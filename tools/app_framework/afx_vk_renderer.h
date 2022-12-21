// Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the AMD INTERNAL EVALUATION LICENSE.
//
// See file LICENSE.RTF for full license details.

#pragma once

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#else
#error "TODO"
#endif

#include <vulkan/vulkan.h>
#include <mutex>
#include <vector>

#include "afx_renderer.h"
#include "afx_shader_compiler.h"

#ifdef RPS_AFX_REQUIRE_IMGUI

// TODO: ImGui stuff should be platform nonspecifc
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_vulkan.h"

#endif

#pragma comment(lib, "vulkan-1.lib")

inline void _ThrowIfNotSuccessVK(VkResult result, const char* text, const char* file, int line)
{
    if (result != VK_SUCCESS)
    {
        fprintf_s(stderr, "\nVK app failed ( VkResult = 0x%x, `%s` @ `%s` line %d", result, text, file, line);

        assert(false);
        throw result;
    }
}

inline void _ThrowIfFailedVK(VkResult result, const char* text, const char* file, int line)
{
    if (result < 0)
    {
        fprintf_s(stderr, "\nVK app failed ( VkResult = 0x%x, `%s` @ `%s` line %d", result, text, file, line);

        assert(false);
        throw result;
    }
}

inline void _SkipIfNotSuccessVk(VkResult result, const char* text, const char* file, int line)
{
    if (result != VK_SUCCESS)
    {
        fprintf_s(stderr, "\nVK app skipped ( VkResult = 0x%x, `%s` @ `%s` line %d", result, text, file, line);
        exit(0);
    }
}

#define ThrowIfNotSuccessVK(EXPR) _ThrowIfNotSuccessVK(EXPR, #EXPR, __FILE__, __LINE__)
#define ThrowIfFailedVK(EXPR)     _ThrowIfFailedVK(EXPR, #EXPR, __FILE__, __LINE__)
#define SkipIfNotSuccessVk(EXPR)  _SkipIfNotSuccessVk(EXPR, #EXPR, __FILE__, __LINE__)

class RpsAfxVulkanRenderer : public RpsAfxRendererBase
{
public:
    struct DescriptorHeapSizeRequirement
    {
        uint32_t staticCount;
        uint32_t dynamicCountPerFrame;
    };

    struct InitTempResources
    {
        std::vector<VkBuffer>       buffers;
        std::vector<VkImage>        images;
        std::vector<VkDeviceMemory> memory;
    };

    virtual bool Init(void* hWindow) override final
    {
        m_hWnd = (HWND)hWindow;

        RECT clientRect = {};
        ::GetClientRect(m_hWnd, &clientRect);
        m_width  = clientRect.right - clientRect.left;
        m_height = clientRect.bottom - clientRect.top;

        InitVkInstance();
        InitVkDevice();
        CreateSwapChain();
        OnPostResize();

        ActiveCommandList cmdList = BeginCmdList(RPS_AFX_QUEUE_INDEX_GFX);

        // To keep alive until init cmdlist is executed.
        InitTempResources tempResources;
        OnInit(cmdList.cmdBuf, tempResources);

        EndCmdList(cmdList);

        VkSubmitInfo submitInfo       = {};
        submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &cmdList.cmdBuf;

        FlushUploadBuffer();

        vkQueueSubmit(m_queues[RPS_AFX_QUEUE_INDEX_GFX], 1, &submitInfo, VK_NULL_HANDLE);

        RecycleCmdList(cmdList);

        WaitForGpuIdle();

        for (auto& buf : tempResources.buffers)
        {
            vkDestroyBuffer(m_device, buf, nullptr);
        }
        for (auto& img : tempResources.images)
        {
            vkDestroyImage(m_device, img, nullptr);
        }
        for (auto& mem : tempResources.memory)
        {
            vkFreeMemory(m_device, mem, nullptr);
        }

        OnPostInit();

        return true;
    }

    virtual void Tick() override final
    {
        OnUpdate(m_frameCounter);

        WaitForSwapChainBuffer();

        ResetFrameDynamicDescriptorPools();

        ResetCommandPools();

        m_frameConstantUsage = 0;

        OnRender(m_frameCounter);

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType            = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.swapchainCount   = 1;
        presentInfo.pSwapchains      = &m_swapChain;
        presentInfo.pImageIndices    = &m_backBufferIndex;

        if (m_pendingPresentSemaphore != VK_NULL_HANDLE)
        {
            presentInfo.pWaitSemaphores = &m_pendingPresentSemaphore;
            m_pendingPresentSemaphore   = VK_NULL_HANDLE;
        }

        ThrowIfFailedVK(vkQueuePresentKHR(m_presentQueue, &presentInfo));
        m_frameCounter++;
    }

    virtual void CleanUp() override final
    {
        WaitForGpuIdle();

        OnCleanUp();

        for (auto& img : m_swapChainImages)
        {
            vkDestroyImageView(m_device, img.imageView, nullptr);
        }
        m_swapChainImages.clear();
        vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);

        for (auto& ff : m_frameFences)
        {
            vkDestroySemaphore(m_device, ff.imageAcquiredSemaphore, nullptr);
            vkDestroySemaphore(m_device, ff.renderCompleteSemaphore, nullptr);
            vkDestroyFence(m_device, ff.renderCompleteFence, nullptr);
        }
        m_frameFences.clear();

        if (m_surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(m_vkInstance, m_surface, nullptr);
            m_surface = VK_NULL_HANDLE;
        }

        for (auto& memPools : m_staticAssetMemoryPools)
        {
            for (auto& mem : memPools.pools)
            {
                vkFreeMemory(m_device, mem, nullptr);
            }
        }

        for (uint32_t i = 0, numSemaphores = uint32_t(m_queueSemaphores.size()); i < numSemaphores; i++)
        {
            if (m_queueSemaphores[i] != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(m_device, m_queueSemaphores[i], nullptr);
            }
        }
        m_queueSemaphores.clear();

        for (uint32_t i = 0; i < _countof(m_cmdPools); i++)
        {
            for (auto& pools : m_cmdPools[i])
            {
                for (auto& pool : pools)
                {
                    if (!pool.cmdBuffers.empty())
                    {
                        vkFreeCommandBuffers(
                            m_device, pool.cmdPool, uint32_t(pool.cmdBuffers.size()), pool.cmdBuffers.data());
                    }
                    vkDestroyCommandPool(m_device, pool.cmdPool, nullptr);
                }
            }
            m_cmdPools[i].clear();
        }

        if (m_descriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
            m_descriptorPool = VK_NULL_HANDLE;
        }

        if (m_constantBuffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(m_device, m_constantBuffer, nullptr);
            m_constantBuffer = VK_NULL_HANDLE;
        }

        if (m_constantBufferMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(m_device, m_constantBufferMemory, nullptr);
            m_constantBufferMemory = VK_NULL_HANDLE;
        }

        for (auto& dp : m_frameDynamicDescriptorPools)
        {
            for (auto& p : dp.pools)
            {
                vkDestroyDescriptorPool(m_device, p, nullptr);
            }
        }
        m_frameDynamicDescriptorPools.clear();

        vkDestroyDevice(m_device, nullptr);

        if (m_vkDebugMsger != VK_NULL_HANDLE)
        {
            auto pfn_destroyDebugUtils = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                m_vkInstance, "vkDestroyDebugUtilsMessengerEXT");
            if (pfn_destroyDebugUtils != nullptr)
            {
                pfn_destroyDebugUtils(m_vkInstance, m_vkDebugMsger, nullptr);
            }
        }

        vkDestroyInstance(m_vkInstance, nullptr);
    }

    virtual void OnResize(uint32_t width, uint32_t height) override final
    {
        if (m_swapChain != VK_NULL_HANDLE)
        {
            WaitForGpuIdle();

            if ((width > 0 && height > 0) && ((m_width != width) || (m_height != height)))
            {
                OnPreResize();

                m_width  = width;
                m_height = height;

                CreateSwapChain();

                OnPostResize();
            }
        }
    }

protected:
    virtual bool WaitForGpuIdle() override final
    {
        return VK_SUCCESS == vkDeviceWaitIdle(m_device);
    }

    virtual void OnPostInit()
    {
    }

    void WaitForSwapChainBuffer()
    {
        m_swapChainImageSemaphoreIndex = m_backBufferIndex;

        ThrowIfNotSuccessVK(vkAcquireNextImageKHR(m_device,
                                                  m_swapChain,
                                                  UINT64_MAX,
                                                  m_frameFences[m_backBufferIndex].imageAcquiredSemaphore,
                                                  VK_NULL_HANDLE,
                                                  &m_backBufferIndex));

        if ((m_frameCounter % m_swapChainImages.size()) != m_backBufferIndex)
            m_frameCounter = m_backBufferIndex;

        ThrowIfNotSuccessVK(
            vkWaitForFences(m_device, 1, &m_frameFences[m_backBufferIndex].renderCompleteFence, VK_TRUE, UINT64_MAX));

        ThrowIfNotSuccessVK(vkResetFences(m_device, 1, &m_frameFences[m_backBufferIndex].renderCompleteFence));
    }

    VkImage GetBackBuffer() const
    {
        return m_swapChainImages[m_backBufferIndex].image;
    }

    VkImageView GetBackBufferView() const
    {
        return m_swapChainImages[m_backBufferIndex].imageView;
    }

    const std::vector<RpsRuntimeResource>& GetBackBuffers(RpsResourceDesc& backBufferDesc) const
    {
        backBufferDesc.type              = RPS_RESOURCE_TYPE_IMAGE_2D;
        backBufferDesc.temporalLayers    = uint32_t(m_swapChainImages.size());
        backBufferDesc.flags             = 0;
        backBufferDesc.image.arrayLayers = 1;
        backBufferDesc.image.mipLevels   = 1;
        backBufferDesc.image.format      = rpsFormatFromVK(m_swapChainFormat.format);
        backBufferDesc.image.width       = m_width;
        backBufferDesc.image.height      = m_height;
        backBufferDesc.image.sampleCount = 1;

        return m_swapChainImageRpsResources;
    }

    uint32_t GetBackBuffers(RpsResourceDesc&    backBufferDesc,
                            RpsRuntimeResource* phResources,
                            uint32_t            maxResources) const
    {
        auto runtimeResources = GetBackBuffers(backBufferDesc);

        const uint32_t numResToCopy = std::min(uint32_t(runtimeResources.size()), maxResources);

        std::copy(runtimeResources.begin(), runtimeResources.begin() + numResToCopy, phResources);

        return numResToCopy;
    }

public:
    struct ActiveCommandList
    {
        uint32_t        backBufferIndex;
        uint32_t        queueIndex;
        uint32_t        poolIndex;
        VkCommandBuffer cmdBuf;
        VkCommandPool   cmdPool;

        operator VkCommandBuffer() const
        {
            return cmdBuf;
        }
    };

protected:
    virtual void OnInit(VkCommandBuffer initCmdList, InitTempResources& tempResources)
    {
    }

    virtual void OnCleanUp()
    {
    }

    virtual void OnPreResize()
    {
    }

    virtual void OnPostResize()
    {
        m_frameCounter = 0;
    }

    virtual void OnUpdate(uint32_t frameIndex)
    {
    }

    virtual void OnRender(uint32_t frameIndex)
    {
        ActiveCommandList cmdList = BeginCmdList(RPS_AFX_QUEUE_INDEX_GFX);

        VkImageMemoryBarrier imageBarrier        = {};
        imageBarrier.sType                       = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageBarrier.srcAccessMask               = 0;
        imageBarrier.dstAccessMask               = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarrier.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.oldLayout                   = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout                   = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier.image                       = GetBackBuffer();
        imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBarrier.subresourceRange.layerCount = 1;
        imageBarrier.subresourceRange.levelCount = 1;

        vkCmdPipelineBarrier(cmdList,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &imageBarrier);

        VkClearColorValue clearColor = {{0.0f, 0.2f, 0.4f, 1.0f}};
        vkCmdClearColorImage(cmdList,
                             GetBackBuffer(),
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             &clearColor,
                             1,
                             &imageBarrier.subresourceRange);

        imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarrier.dstAccessMask = 0;
        imageBarrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        vkCmdPipelineBarrier(cmdList,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &imageBarrier);

        EndCmdList(cmdList);

        SubmitCmdLists(&cmdList, 1, true);

        RecycleCmdList(cmdList);
    }

    virtual RpsResult CreateRpsRuntimeDevice(const RpsDeviceCreateInfo& createInfo, RpsDevice& device) override
    {
        RpsVKRuntimeDeviceCreateInfo vkRuntimeDeviceCreateInfo = {};
        vkRuntimeDeviceCreateInfo.pDeviceCreateInfo            = &createInfo;
        vkRuntimeDeviceCreateInfo.hVkDevice                    = m_device;
        vkRuntimeDeviceCreateInfo.hVkPhysicalDevice            = m_physicalDevice;

        RpsRuntimeDeviceCreateInfo runtimeDeviceCreateInfo     = {};
        runtimeDeviceCreateInfo.pUserContext                   = this;
        runtimeDeviceCreateInfo.callbacks.pfnRecordDebugMarker = &RecordDebugMarker;
        runtimeDeviceCreateInfo.callbacks.pfnSetDebugName      = &SetDebugName;

        vkRuntimeDeviceCreateInfo.pRuntimeCreateInfo = &runtimeDeviceCreateInfo;

        return rpsVKRuntimeDeviceCreate(&vkRuntimeDeviceCreateInfo, &device);
    }

    RpsResult ExecuteRenderGraph(uint32_t       frameIndex,
                                 RpsRenderGraph hRenderGraph,
                                 bool           bWaitSwapChain = true,
                                 bool           frameEnd       = true)
    {
        RpsRenderGraphBatchLayout batchLayout = {};
        RpsResult                 result      = rpsRenderGraphGetBatchLayout(hRenderGraph, &batchLayout);
        if (RPS_FAILED(result))
            return result;

        ReserveSemaphores(batchLayout.numFenceSignals);

        for (uint32_t iBatch = 0; iBatch < batchLayout.numCmdBatches; iBatch++)
        {
            auto& batch = batchLayout.pCmdBatches[iBatch];

            ActiveCommandList cmdList = BeginCmdList(RpsAfxQueueIndices(batch.queueIndex));

            RpsRenderGraphRecordCommandInfo recordInfo = {};

            recordInfo.hCmdBuffer    = rpsVKCommandBufferToHandle(cmdList.cmdBuf);
            recordInfo.pUserContext  = this;
            recordInfo.frameIndex    = frameIndex;
            recordInfo.cmdBeginIndex = batch.cmdBegin;
            recordInfo.numCmds       = batch.numCmds;

            if (g_DebugMarkers)
            {
                recordInfo.flags = RPS_RECORD_COMMAND_FLAG_ENABLE_COMMAND_DEBUG_MARKERS;
            }

            result = rpsRenderGraphRecordCommands(hRenderGraph, &recordInfo);
            if (RPS_FAILED(result))
                return result;

            EndCmdList(cmdList);

            SubmitCmdLists(&cmdList,
                           1,
                           frameEnd && ((iBatch + 1) == batchLayout.numCmdBatches),
                           batch.numWaitFences,
                           batchLayout.pWaitFenceIndices + batch.waitFencesBegin,
                           batch.signalFenceIndex,
                           bWaitSwapChain && (iBatch == 0));  // TODO - RPS to mark first access to swapchain image

            RecycleCmdList(cmdList);
        }

        return RPS_OK;
    }

#ifdef RPS_AFX_REQUIRE_IMGUI

    void StartImGuiDraw()
    {
        ImGui_ImplWin32_NewFrame();
        ImGui_ImplVulkan_NewFrame();
        ImGui::NewFrame();
    }

    RpsResult FinishImGuiDraw(RpsRenderGraph hRenderGraph)
    {
        ImGui::Render();

        RpsRenderGraphBatchLayout batchLayout = {};
        RpsResult                 result      = rpsRenderGraphGetBatchLayout(hRenderGraph, &batchLayout);
        REQUIRE_RPS_OK(result);

        ActiveCommandList cmdList = BeginCmdList(RPS_AFX_QUEUE_INDEX_GFX);

        VkImageMemoryBarrier graphToGuiBarrier = {};
        graphToGuiBarrier.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        //TODO Better solution for srcAccessMask
        graphToGuiBarrier.srcAccessMask = 0;
        graphToGuiBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        graphToGuiBarrier.oldLayout =
            m_frameCounter < m_swapChainImages.size() ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        graphToGuiBarrier.newLayout                   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        graphToGuiBarrier.image                       = GetBackBuffer();
        graphToGuiBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        graphToGuiBarrier.subresourceRange.layerCount = 1;
        graphToGuiBarrier.subresourceRange.levelCount = 1;

        vkCmdPipelineBarrier(cmdList,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             {},
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &graphToGuiBarrier);

        VkClearValue clearColor = {};

        VkRenderPassBeginInfo rpBegin = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rpBegin.framebuffer           = m_imguiFrameBuffers[m_backBufferIndex];
        rpBegin.clearValueCount       = 1;
        rpBegin.pClearValues          = &clearColor;
        rpBegin.renderPass            = m_imguiRenderPass;
        rpBegin.renderArea            = VkRect2D{{0, 0}, { m_width, m_height }};

        vkCmdBeginRenderPass(cmdList, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdList);

        vkCmdEndRenderPass(cmdList);

        EndCmdList(cmdList);

        SubmitCmdLists(&cmdList, 1, true);

        RecycleCmdList(cmdList);

        return RPS_OK;
    }
#endif

    void SubmitCmdLists(ActiveCommandList* pCmdLists,
                        uint32_t           numCmdLists,
                        bool               frameEnd,
                        uint32_t           waitSemaphoreCount    = 0,
                        const uint32_t*    pWaitSemaphoreIndices = nullptr,
                        uint32_t           signalSemaphoreIndex  = UINT32_MAX,
                        bool               bWaitSwapChain        = false)

    {
        assert(numCmdLists > 0);

        FlushUploadBuffer();

        VkCommandBuffer* pCmdBufs = &pCmdLists[0].cmdBuf;
        if (numCmdLists > 1)
        {
            m_cmdBufsToSubmit.resize(numCmdLists);
            for (uint32_t i = 0; i < m_cmdBufsToSubmit.size(); i++)
            {
                m_cmdBufsToSubmit[i] = pCmdLists[i].cmdBuf;
            }
            pCmdBufs = m_cmdBufsToSubmit.data();
        }

        VkPipelineStageFlags submitWaitStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

        uint32_t    numWaitSemaphores                  = 0;
        VkSemaphore waitSemaphores[RPS_MAX_QUEUES + 1] = {};

        if (bWaitSwapChain)
        {
            waitSemaphores[numWaitSemaphores] = m_frameFences[m_swapChainImageSemaphoreIndex].imageAcquiredSemaphore;
            ++numWaitSemaphores;
        }

        assert(waitSemaphoreCount <= RPS_MAX_QUEUES);

        for (uint32_t i = 0; (i < waitSemaphoreCount) && (i < RPS_MAX_QUEUES); i++)
        {
            assert(pWaitSemaphoreIndices[i] < m_queueSemaphores.size());
            waitSemaphores[numWaitSemaphores] = m_queueSemaphores[pWaitSemaphoreIndices[i]];
            ++numWaitSemaphores;
        }

        VkFence submitFence = VK_NULL_HANDLE;

        uint32_t    numSignalSemaphores = 0;
        VkSemaphore signalSemaphores[2] = {};

        if (frameEnd)
        {
            if (m_presentQueue != m_queues[pCmdLists->queueIndex])
            {
                m_pendingPresentSemaphore             = m_frameFences[m_backBufferIndex].renderCompleteSemaphore;
                signalSemaphores[numSignalSemaphores] = m_pendingPresentSemaphore;
                ++numSignalSemaphores;
            }

            submitFence = m_frameFences[m_backBufferIndex].renderCompleteFence;
        }

        if (signalSemaphoreIndex != UINT32_MAX)
        {
            signalSemaphores[numSignalSemaphores] = m_queueSemaphores[signalSemaphoreIndex];
            ++numSignalSemaphores;
        }

        VkSubmitInfo submitInfo         = {};
        submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount   = numCmdLists;
        submitInfo.pCommandBuffers      = pCmdBufs;
        submitInfo.pWaitSemaphores      = waitSemaphores;
        submitInfo.waitSemaphoreCount   = numWaitSemaphores;
        submitInfo.pSignalSemaphores    = signalSemaphores;
        submitInfo.signalSemaphoreCount = numSignalSemaphores;
        submitInfo.pWaitDstStageMask    = &submitWaitStage;

        vkQueueSubmit(m_queues[pCmdLists->queueIndex], 1, &submitInfo, submitFence);
    }

    ActiveCommandList BeginCmdList(RpsAfxQueueIndices                    queueIndex,
                                   const VkCommandBufferInheritanceInfo* pInheritanceInfo = nullptr)
    {
        ActiveCommandList result = {};
        result.backBufferIndex   = m_backBufferIndex;
        result.queueIndex        = queueIndex;
        result.cmdPool           = VK_NULL_HANDLE;

        std::lock_guard<std::mutex> lock(m_cmdListMutex);

        if (m_cmdPools[queueIndex].size() <= m_swapChainImages.size())
        {
            m_cmdPools[queueIndex].resize(m_swapChainImages.size());
        }

        uint32_t freeIdx = 0;
        for (; freeIdx < m_cmdPools[queueIndex][m_backBufferIndex].size(); freeIdx++)
        {
            if (!m_cmdPools[queueIndex][m_backBufferIndex][freeIdx].inUse)
                break;
        }

        if (freeIdx == m_cmdPools[queueIndex][m_backBufferIndex].size())
        {
            VkCommandPoolCreateInfo cmdPoolInfo = {};
            cmdPoolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            cmdPoolInfo.queueFamilyIndex        = m_rpsQueueIndexToVkQueueFamilyMap[queueIndex];

            CommandPool newPool = {};
            ThrowIfNotSuccessVK(vkCreateCommandPool(m_device, &cmdPoolInfo, nullptr, &newPool.cmdPool));

            m_cmdPools[queueIndex][m_backBufferIndex].emplace_back(newPool);
        }

        CommandPool* pPool = &m_cmdPools[queueIndex][m_backBufferIndex][freeIdx];
        pPool->inUse       = true;
        result.poolIndex   = freeIdx;
        result.cmdPool     = pPool->cmdPool;

        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandBufferCount          = 1;
        allocInfo.commandPool                 = result.cmdPool;
        allocInfo.level =
            (pInheritanceInfo == nullptr) ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;

        ThrowIfNotSuccessVK(vkAllocateCommandBuffers(m_device, &allocInfo, &result.cmdBuf));

        pPool->cmdBuffers.push_back(result.cmdBuf);

        VkCommandBufferBeginInfo cmdBeginInfo = {};
        cmdBeginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBeginInfo.pInheritanceInfo         = pInheritanceInfo;
        cmdBeginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (pInheritanceInfo)
            cmdBeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

        ThrowIfNotSuccessVK(vkBeginCommandBuffer(result.cmdBuf, &cmdBeginInfo));

        return result;
    }

    void EndCmdList(ActiveCommandList& cmdList)
    {
        assert(cmdList.cmdBuf != VK_NULL_HANDLE);
        assert(cmdList.backBufferIndex == m_backBufferIndex);

        std::lock_guard<std::mutex> lock(m_cmdListMutex);

        ThrowIfNotSuccessVK(vkEndCommandBuffer(cmdList.cmdBuf));

        m_cmdPools[cmdList.queueIndex][m_backBufferIndex][cmdList.poolIndex].inUse = false;
        cmdList.cmdPool                                                            = VK_NULL_HANDLE;
    }

    void RecycleCmdList(ActiveCommandList& cmdList)
    {
        cmdList.cmdBuf = VK_NULL_HANDLE;
    }

    VkResult AllocFrameDescriptorSet(VkDescriptorSetLayout* pLayouts, uint32_t numSets, VkDescriptorSet* pSets)
    {
        VkResult result = VK_SUCCESS;

        auto& poolInfo = m_frameDynamicDescriptorPools[m_backBufferIndex];

        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorSetCount          = numSets;
        allocInfo.pSetLayouts                 = pLayouts;

        for (uint32_t iTry = 0; iTry < 2; iTry++)
        {
            if (!poolInfo.pools.empty())
            {
                allocInfo.descriptorPool = poolInfo.pools[poolInfo.current];
                result                   = vkAllocateDescriptorSets(m_device, &allocInfo, pSets);
                if (result != VK_ERROR_OUT_OF_POOL_MEMORY)
                    return result;
            }

            if (iTry == 0)
            {
                if ((poolInfo.current + 1) < poolInfo.pools.size())
                {
                    poolInfo.current++;
                }
                else
                {
                    VkDescriptorPoolCreateInfo dpInfo = {};
                    dpInfo.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
                    dpInfo.maxSets                    = m_defaultFrameDynamicDescriptorPoolMaxSets;
                    dpInfo.pPoolSizes                 = m_defaultFrameDynamicDescriptorPoolSizes.data();
                    dpInfo.poolSizeCount              = (uint32_t)m_defaultFrameDynamicDescriptorPoolSizes.size();

                    VkDescriptorPool newPool;
                    ThrowIfFailedVK(vkCreateDescriptorPool(m_device, &dpInfo, nullptr, &newPool));
                    poolInfo.pools.push_back(newPool);
                }
            }
        }

        return result;
    }

    void ResetFrameDynamicDescriptorPools()
    {
        m_frameDynamicDescriptorPools[m_backBufferIndex].current = 0;
        for (auto& pool : m_frameDynamicDescriptorPools[m_backBufferIndex].pools)
        {
            ThrowIfFailedVK(vkResetDescriptorPool(m_device, pool, 0));
        }
    }

    void ResetCommandPools()
    {
        for (uint32_t iQ = 0; iQ < RPS_AFX_QUEUE_INDEX_COUNT; iQ++)
        {
            if (m_backBufferIndex < m_cmdPools[iQ].size())
            {
                for (auto& pool : m_cmdPools[iQ][m_backBufferIndex])
                {
                    if (!pool.cmdBuffers.empty())
                    {
                        vkFreeCommandBuffers(
                            m_device, pool.cmdPool, uint32_t(pool.cmdBuffers.size()), pool.cmdBuffers.data());
                        pool.cmdBuffers.clear();
                    }

                    ThrowIfFailedVK(vkResetCommandPool(m_device, pool.cmdPool, 0));
                }
            }
        }
    }

    void AppendWriteDescriptorSet(VkWriteDescriptorSet*         pOut,
                                  VkDescriptorSet               dstSet,
                                  uint32_t                      binding,
                                  uint32_t                      count,
                                  VkDescriptorType              type,
                                  uint32_t                      dstArrayElement,
                                  const VkDescriptorImageInfo*  pImageInfos,
                                  const VkDescriptorBufferInfo* pBufferInfos,
                                  const VkBufferView*           pTexelBufferViews)
    {
        pOut->sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        pOut->dstSet           = dstSet;
        pOut->dstBinding       = binding;
        pOut->descriptorCount  = count;
        pOut->descriptorType   = type;
        pOut->dstArrayElement  = dstArrayElement;
        pOut->pImageInfo       = pImageInfos;
        pOut->pBufferInfo      = pBufferInfos;
        pOut->pTexelBufferView = pTexelBufferViews;
    }

    void AppendWriteDescriptorSetBuffers(VkWriteDescriptorSet*         pOut,
                                         VkDescriptorSet               dstSet,
                                         uint32_t                      binding,
                                         uint32_t                      count,
                                         VkDescriptorType              type,
                                         const VkDescriptorBufferInfo* pBufferInfos)
    {
        AppendWriteDescriptorSet(pOut, dstSet, binding, count, type, 0, nullptr, pBufferInfos, nullptr);
    }

    void AppendWriteDescriptorSetImages(VkWriteDescriptorSet*        pOut,
                                        VkDescriptorSet              dstSet,
                                        uint32_t                     binding,
                                        uint32_t                     count,
                                        VkDescriptorType             type,
                                        const VkDescriptorImageInfo* pImageInfos)
    {
        AppendWriteDescriptorSet(pOut, dstSet, binding, count, type, 0, pImageInfos, nullptr, nullptr);
    }

    VkDescriptorBufferInfo AllocAndWriteFrameConstants(const void* pSrcData, uint32_t size)
    {
        VkDescriptorBufferInfo result = {};

        uint32_t allocSize = (size + (uint32_t)m_physicalDeviceProperties.limits.minUniformBufferOffsetAlignment - 1) &
                             ~((uint32_t)m_physicalDeviceProperties.limits.minUniformBufferOffsetAlignment - 1);

        uint32_t newOffset = m_frameConstantUsage + allocSize;
        if (newOffset > m_maxConstantSizePerFrame)
        {
            throw;
        }

        const uint32_t totalOffset = m_maxConstantSizePerFrame * m_backBufferIndex + m_frameConstantUsage;
        memcpy(m_constantBufferCpuVA + totalOffset, pSrcData, size);

        m_frameConstantUsage = newOffset;
        result.buffer        = m_constantBuffer;
        result.offset        = totalOffset;
        result.range         = size;

        return result;
    }

    void FlushUploadBuffer()
    {
        if (m_constantBufferNeedsFlushAfterUpdate)
        {
            VkMappedMemoryRange currRange = {};
            currRange.sType               = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            currRange.memory              = m_constantBufferMemory;
            currRange.offset              = m_maxConstantSizePerFrame * m_backBufferIndex;
            currRange.size = (m_frameConstantUsage + m_physicalDeviceProperties.limits.nonCoherentAtomSize - 1) &
                             ~(m_physicalDeviceProperties.limits.nonCoherentAtomSize - 1);

            ThrowIfFailedVK(vkFlushMappedMemoryRanges(m_device, 1, &currRange));
        }
    }

    VkBuffer CreateAndBindStaticBuffer(VkDeviceSize size, VkBufferUsageFlags usage)
    {
        VkBufferCreateInfo bufCI    = {};
        bufCI.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufCI.usage                 = usage;
        bufCI.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
        bufCI.queueFamilyIndexCount = 0;
        bufCI.pQueueFamilyIndices   = nullptr;
        bufCI.size                  = size;

        VkBuffer buf;
        ThrowIfFailedVK(vkCreateBuffer(m_device, &bufCI, nullptr, &buf));

        AllocAndBindStaticMemory(buf);

        return buf;
    }

    VkImage CreateAndBindStaticImage(VkImageType       type,
                                     VkImageUsageFlags usage,
                                     VkFormat          format,
                                     uint32_t          width,
                                     uint32_t          height,
                                     uint32_t          depth,
                                     uint32_t          mipLevels,
                                     uint32_t          arrayLayers)
    {
        VkImageCreateInfo imgCI = {};
        imgCI.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgCI.imageType         = VK_IMAGE_TYPE_2D;
        imgCI.format            = format;
        imgCI.mipLevels         = mipLevels;
        imgCI.arrayLayers       = arrayLayers;
        imgCI.samples           = VK_SAMPLE_COUNT_1_BIT;
        imgCI.tiling            = VK_IMAGE_TILING_OPTIMAL;
        imgCI.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
        imgCI.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
        imgCI.extent            = {width, height, depth};
        imgCI.usage             = usage;

        VkImage img;
        ThrowIfFailedVK(vkCreateImage(m_device, &imgCI, nullptr, &img));

        AllocAndBindStaticMemory(img);

        return img;
    }

    void AllocAndBindStaticMemory(VkImage image)
    {
        VkMemoryRequirements req = {};
        vkGetImageMemoryRequirements(m_device, image, &req);

        VkDeviceSize   offset;
        VkDeviceMemory mem = AllocStaticMemory(req, &offset);

        ThrowIfFailedVK(vkBindImageMemory(m_device, image, mem, offset));
    }

    void AllocAndBindStaticMemory(VkBuffer buffer)
    {
        VkMemoryRequirements req = {};
        vkGetBufferMemoryRequirements(m_device, buffer, &req);

        VkDeviceSize   offset;
        VkDeviceMemory mem = AllocStaticMemory(req, &offset);

        ThrowIfFailedVK(vkBindBufferMemory(m_device, buffer, mem, offset));
    }

    uint32_t FindMemoryTypeIndex(uint32_t bitMask, bool preferLocal, bool needCpuWrite, bool needCpuRead)
    {
        uint32_t typeIdx = UINT32_MAX;
        for (uint32_t iType = 0; iType < m_deviceMemoryProperties.memoryTypeCount; iType++)
        {
            if (bitMask & (1 << iType))
            {
                const auto memTypeFlags = m_deviceMemoryProperties.memoryTypes[iType].propertyFlags;

                // Require visible
                if ((needCpuWrite || needCpuRead) && !(memTypeFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
                {
                    continue;
                }

                typeIdx = iType;

                // Prefer local
                if (preferLocal && (memTypeFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
                {
                    break;
                }

                // Prefer cached
                if (needCpuRead && (memTypeFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT))
                {
                    break;
                }
            }
        }
        return typeIdx;
    }

    VkDeviceMemory AllocStaticMemory(const VkMemoryRequirements& req, VkDeviceSize* pOffset)
    {
        uint32_t typeIdx = FindMemoryTypeIndex(req.memoryTypeBits, true, false, false);

        auto& pool = m_staticAssetMemoryPools[typeIdx];

        VkDeviceSize alignedOffset = ((pool.lastUsage + req.alignment - 1) & ~(req.alignment - 1));
        if ((alignedOffset + req.size) > pool.lastCapacity)
        {
            static const VkDeviceSize DEFAULT_POOL_SIZE = 64 * 1024 * 1024;

            VkMemoryAllocateInfo ai = {};
            ai.sType                = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            ai.memoryTypeIndex      = typeIdx;
            ai.allocationSize       = std::max(req.size, DEFAULT_POOL_SIZE);

            VkDeviceMemory newMem;
            ThrowIfFailedVK(vkAllocateMemory(m_device, &ai, nullptr, &newMem));

            pool.pools.push_back(newMem);
            pool.lastCapacity = ai.allocationSize;
            pool.lastUsage    = 0;
            alignedOffset     = 0;
        }

        pool.lastUsage = (alignedOffset + req.size);
        *pOffset       = alignedOffset;
        return pool.pools.back();
    }

    uint64_t CalcGuaranteedCompletedFrameIndexForRps() const
    {
        // For VK we wait for swapchain before submitting, so max queued frame count is swapChainImages + 1.
        const uint32_t maxQueuedFrames = uint32_t(m_swapChainImages.size() + 1);

        return (m_frameCounter > maxQueuedFrames) ? m_frameCounter - maxQueuedFrames
                                                  : RPS_GPU_COMPLETED_FRAME_INDEX_NONE;
    }

    void CreateStaticCheckerboardTexture(VkImageView&       textureView,
                                         VkImage&           texture,
                                         VkCommandBuffer    initCmdBuf,
                                         InitTempResources& tempResources,
                                         uint32_t           width,
                                         uint32_t           height,
                                         const float        tintColor[4])
    {
        // Texture data contains 4 channels (RGBA) with unnormalized 8-bit values, this is the most commonly supported format
        VkFormat format           = VK_FORMAT_R8G8B8A8_UNORM;
        uint32_t texturePixelSize = 4;

        const uint32_t rowPitch    = width * texturePixelSize;
        const uint32_t cellPitch   = rowPitch >> 3;  // The width of a cell in the checkboard texture.
        const uint32_t cellHeight  = width >> 3;     // The height of a cell in the checkerboard texture.
        const uint32_t textureSize = rowPitch * height;

        std::vector<UINT8> data(textureSize);
        uint8_t*           textureData = &data[0];

#define RPS_AFX_SCALE_BYTE(B, S) (std::max<uint8_t>(0, std::min<int32_t>(0xff, (int32_t((B) * (S))))))

        for (uint32_t n = 0; n < textureSize; n += texturePixelSize)
        {
            uint32_t x = n % rowPitch;
            uint32_t y = n / rowPitch;
            uint32_t i = x / cellPitch;
            uint32_t j = y / cellHeight;

            if (i % 2 == j % 2)
            {
                textureData[n]     = RPS_AFX_SCALE_BYTE(0xa0, tintColor[0]);  // R
                textureData[n + 1] = RPS_AFX_SCALE_BYTE(0xa0, tintColor[1]);  // G
                textureData[n + 2] = RPS_AFX_SCALE_BYTE(0xa0, tintColor[2]);  // B
                textureData[n + 3] = RPS_AFX_SCALE_BYTE(0xff, tintColor[3]);  // A
            }
            else
            {
                textureData[n]     = RPS_AFX_SCALE_BYTE(0xff, tintColor[0]);  // R
                textureData[n + 1] = RPS_AFX_SCALE_BYTE(0xff, tintColor[1]);  // G
                textureData[n + 2] = RPS_AFX_SCALE_BYTE(0xff, tintColor[2]);  // B
                textureData[n + 3] = RPS_AFX_SCALE_BYTE(0xff, tintColor[3]);  // A
            }
        }

#undef RPS_AFX_SCALE_BYTE

        {
            auto textureDataUploadBuf = AllocAndWriteFrameConstants(textureData, textureSize);

            texture = CreateAndBindStaticImage(VK_IMAGE_TYPE_2D,
                                               VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                               format,
                                               width,
                                               height,
                                               1,
                                               1,
                                               1);

            VkBufferImageCopy bufferCopyRegion               = {};
            bufferCopyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel       = 0;
            bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
            bufferCopyRegion.imageSubresource.layerCount     = 1;
            bufferCopyRegion.imageExtent.width               = width;
            bufferCopyRegion.imageExtent.height              = height;
            bufferCopyRegion.imageExtent.depth               = 1;
            bufferCopyRegion.bufferOffset                    = textureDataUploadBuf.offset;

            VkImageSubresourceRange subresourceRange = {};

            // Transition the texture image layout to transfer target, so we can safely copy our buffer data to it.
            VkImageMemoryBarrier imageMemoryBarrier          = {};
            imageMemoryBarrier.sType                         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.image                         = texture;
            imageMemoryBarrier.subresourceRange.aspectMask   = VK_IMAGE_ASPECT_COLOR_BIT;
            imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
            imageMemoryBarrier.subresourceRange.levelCount   = 1;
            imageMemoryBarrier.subresourceRange.layerCount   = 1;
            imageMemoryBarrier.srcAccessMask                 = 0;
            imageMemoryBarrier.dstAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.oldLayout                     = VK_IMAGE_LAYOUT_UNDEFINED;
            imageMemoryBarrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

            // Insert a memory dependency at the proper pipeline stages that will execute the image layout transition
            // Source pipeline stage is host write/read exection (VK_PIPELINE_STAGE_HOST_BIT)
            // Destination pipeline stage is copy command exection (VK_PIPELINE_STAGE_TRANSFER_BIT)
            vkCmdPipelineBarrier(initCmdBuf,
                                 VK_PIPELINE_STAGE_HOST_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &imageMemoryBarrier);

            // Copy mip levels from staging buffer
            vkCmdCopyBufferToImage(initCmdBuf,
                                   textureDataUploadBuf.buffer,
                                   texture,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   1,
                                   &bufferCopyRegion);

            // Once the data has been uploaded we transfer to the texture image to the shader read layout, so it can be sampled from
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            imageMemoryBarrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageMemoryBarrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            vkCmdPipelineBarrier(initCmdBuf,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &imageMemoryBarrier);
        }

        // Create image view
        // Textures are not directly accessed by the shaders and
        // are abstracted by image views containing additional
        // information and sub resource ranges
        VkImageViewCreateInfo view = {};
        view.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.viewType              = VK_IMAGE_VIEW_TYPE_2D;
        view.format                = format;
        view.components            = {
            VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
        view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view.subresourceRange.layerCount = 1;
        view.subresourceRange.levelCount = 1;
        view.image                       = texture;
        ThrowIfFailedVK(vkCreateImageView(m_device, &view, nullptr, &textureView));
    }

    void ReserveSemaphores(uint32_t numSyncs)
    {
        const uint32_t oldSize = uint32_t(m_queueSemaphores.size());
        if (numSyncs > oldSize)
        {
            m_queueSemaphores.resize(numSyncs, VK_NULL_HANDLE);
        }

        VkSemaphoreCreateInfo semaphoreCI = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

        for (size_t i = oldSize; i < numSyncs; i++)
        {
            ThrowIfFailedVK(vkCreateSemaphore(m_device, &semaphoreCI, nullptr, &m_queueSemaphores[i]));
        }
    }

    VkSemaphore GetSemaphore(uint32_t index) const
    {
        return m_queueSemaphores[index];
    }

private:
    static VKAPI_ATTR VkBool32 VKAPI_CALL
    ValidationDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
                            VkDebugUtilsMessageTypeFlagsEXT             messageType,
                            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                            void*                                       pUserData)
    {
        static const struct
        {
            VkDebugUtilsMessageSeverityFlagBitsEXT severity;
            const char*                            name;
        } s_severityMap[] = {
            {VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "VERBOSE"},
            {VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, "INFO"},
            {VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "WARNING"},
            {VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "ERROR"},
        };

        const char* severityName = "";
        for (uint32_t i = 0; i < _countof(s_severityMap); i++)
        {
            if (s_severityMap[i].severity & messageSeverity)
            {
                severityName = s_severityMap[i].name;
                break;
            }
        }

        char buf[2048];
        sprintf_s(buf, "\n\n[VK %s]: %s", severityName, pCallbackData->pMessage);
        fprintf_s(stderr, "%s", buf);
        ::OutputDebugStringA(buf);

        // FAIL(buf);

        return VK_FALSE;
    }

    static void RecordDebugMarker(void* pUserContext, const RpsRuntimeOpRecordDebugMarkerArgs* pArgs)
    {
        auto hCmdBuf = rpsVKCommandBufferFromHandle(pArgs->hCommandBuffer);
        auto pThis   = static_cast<RpsAfxVulkanRenderer*>(pUserContext);

        VkDebugUtilsLabelEXT labelInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};

        switch (pArgs->mode)
        {
        case RPS_RUNTIME_DEBUG_MARKER_BEGIN:
            if (pThis->vkCmdBeginDebugUtilsLabel)
            {
                labelInfo.pLabelName = pArgs->text;
                pThis->vkCmdBeginDebugUtilsLabel(hCmdBuf, &labelInfo);
            }
            break;
        case RPS_RUNTIME_DEBUG_MARKER_END:
            if (pThis->vkCmdEndDebugUtilsLabel)
            {
                pThis->vkCmdEndDebugUtilsLabel(hCmdBuf);
            }
            break;
        case RPS_RUNTIME_DEBUG_MARKER_LABEL:
            if (pThis->vkCmdInsertDebugUtilsLabel)
            {
                labelInfo.pLabelName = pArgs->text;
                pThis->vkCmdInsertDebugUtilsLabel(hCmdBuf, &labelInfo);
            }
            break;
        }
    }

    static void SetDebugName(void* pUserContext, const RpsRuntimeOpSetDebugNameArgs* pArgs)
    {
        auto pThis = static_cast<RpsAfxVulkanRenderer*>(pUserContext);

        if (pThis->vkSetDebugUtilsObjectName)
        {
            VkDebugUtilsObjectNameInfoEXT objNameInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};

            objNameInfo.objectHandle = reinterpret_cast<uintptr_t>(static_cast<void*>(pArgs->hResource.ptr));
            objNameInfo.objectType =
                (pArgs->resourceType == RPS_RESOURCE_TYPE_BUFFER) ? VK_OBJECT_TYPE_BUFFER : VK_OBJECT_TYPE_IMAGE;
            objNameInfo.pObjectName = pArgs->name;

            pThis->vkSetDebugUtilsObjectName(pThis->m_device, &objNameInfo);
        }
    }

    void InitVkInstance()
    {
        // Query instance layers.
        uint32_t instanceLayerPropertyCount = 0;
        SkipIfNotSuccessVk(vkEnumerateInstanceLayerProperties(&instanceLayerPropertyCount, nullptr));
        std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerPropertyCount);
        if (instanceLayerPropertyCount > 0)
        {
            ThrowIfNotSuccessVK(
                vkEnumerateInstanceLayerProperties(&instanceLayerPropertyCount, instanceLayerProperties.data()));
        }

        // Query instance extensions.
        //
        uint32_t instanceExtensionPropertyCount = 0;

        SkipIfNotSuccessVk(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionPropertyCount, nullptr));

        std::vector<VkExtensionProperties> instanceExtensionProperties(instanceExtensionPropertyCount);
        if (instanceExtensionPropertyCount > 0)
        {
            ThrowIfNotSuccessVK(vkEnumerateInstanceExtensionProperties(
                nullptr, &instanceExtensionPropertyCount, instanceExtensionProperties.data()));
        }

        auto findLayer = [&](const char* name) {
            return std::find_if(instanceLayerProperties.begin(),
                                instanceLayerProperties.end(),
                                [name](const VkLayerProperties& layerProps) {
                                    return strcmp(layerProps.layerName, name) == 0;
                                }) != instanceLayerProperties.end();
        };
        auto findExt = [&](const char* name) {
            return std::find_if(instanceExtensionProperties.begin(),
                                instanceExtensionProperties.end(),
                                [name](const VkExtensionProperties& extensionProps) {
                                    return strcmp(extensionProps.extensionName, name) == 0;
                                }) != instanceExtensionProperties.end();
        };

        std::vector<const char*> layerNames;
        layerNames.reserve(8);
        std::vector<const char*> instanceExtNames;
        instanceExtNames.reserve(16);

        auto findAndAddExt = [&](const char* name) {
            if (findExt(name))
            {
                instanceExtNames.push_back(name);
            }
        };

        findAndAddExt(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
        findAndAddExt(VK_KHR_SURFACE_EXTENSION_NAME);

        VkInstanceCreateInfo instanceInfo = {};
        instanceInfo.sType                = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

#if defined(_DEBUG)

        const char* validationLayerName = "VK_LAYER_KHRONOS_validation";

        VkValidationFeaturesEXT            validationFeatures  = {};
        const VkValidationFeatureEnableEXT featuresRequested[] = {
            VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT, VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT};

        if (findLayer(validationLayerName) && findExt(VK_EXT_DEBUG_REPORT_EXTENSION_NAME))
        {
            layerNames.push_back(validationLayerName);
            instanceExtNames.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

            if (m_bGpuValidation)
            {
                validationFeatures.sType                         = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
                validationFeatures.enabledValidationFeatureCount = _countof(featuresRequested);
                validationFeatures.pEnabledValidationFeatures    = featuresRequested;

                validationFeatures.pNext = instanceInfo.pNext;
                instanceInfo.pNext       = &validationFeatures;
            }
        }
#endif

        findAndAddExt(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        instanceInfo.enabledLayerCount       = (uint32_t)layerNames.size();
        instanceInfo.ppEnabledLayerNames     = layerNames.data();
        instanceInfo.enabledExtensionCount   = (uint32_t)instanceExtNames.size();
        instanceInfo.ppEnabledExtensionNames = instanceExtNames.data();

        VkApplicationInfo appInfo     = {};
        appInfo.sType                 = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.apiVersion            = VK_API_VERSION_1_2;
        appInfo.pApplicationName      = "RPS app";
        appInfo.pEngineName           = "RPS_AFX";
        instanceInfo.pApplicationInfo = &appInfo;

        SkipIfNotSuccessVk(vkCreateInstance(&instanceInfo, nullptr, &m_vkInstance));

        VkDebugUtilsMessengerCreateInfoEXT debugMsgerCI{};
        debugMsgerCI.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugMsgerCI.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        if (!m_bValidationErrorOnly)
        {
            debugMsgerCI.messageSeverity |=
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        }

        debugMsgerCI.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugMsgerCI.pfnUserCallback = ValidationDebugCallback;

        auto pfn_createDebugUtils =
            (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_vkInstance, "vkCreateDebugUtilsMessengerEXT");
        if (pfn_createDebugUtils != nullptr)
        {
            ThrowIfNotSuccessVK(pfn_createDebugUtils(m_vkInstance, &debugMsgerCI, nullptr, &m_vkDebugMsger));
        }

        uint32_t gpuCount = 0;
        ThrowIfNotSuccessVK(vkEnumeratePhysicalDevices(m_vkInstance, &gpuCount, nullptr));

        if (gpuCount > 0)
        {
            std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
            ThrowIfNotSuccessVK(vkEnumeratePhysicalDevices(m_vkInstance, &gpuCount, physicalDevices.data()));
            m_physicalDevice = physicalDevices[0];
        }
    }

    void InitVkDevice()
    {
        vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_deviceMemoryProperties);
        m_staticAssetMemoryPools.resize(m_deviceMemoryProperties.memoryTypeCount, {});

        // Queue family properties, used for setting up requested queues upon device creation
        uint32_t queueFamilyCount;
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
        assert(queueFamilyCount > 0);
        m_queueFamilyProperties.resize(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, m_queueFamilyProperties.data());

        vkGetPhysicalDeviceProperties(m_physicalDevice, &m_physicalDeviceProperties);

        uint32_t deviceExtCount;
        ThrowIfFailedVK(vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &deviceExtCount, nullptr));
        std::vector<VkExtensionProperties> deviceExtProps(deviceExtCount);
        ThrowIfFailedVK(
            vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &deviceExtCount, deviceExtProps.data()));

        std::vector<const char*> deviceExts = {};

        auto findExt = [&](const char* name) {
            return std::find_if(deviceExtProps.begin(),
                                deviceExtProps.end(),
                                [name](const VkExtensionProperties& extensionProps) {
                                    return strcmp(extensionProps.extensionName, name) == 0;
                                }) != deviceExtProps.end();
        };

        auto findAndAddExt = [&](const char* name) {
            if (findExt(name))
            {
                deviceExts.push_back(name);
            }
        };

        findAndAddExt(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        findAndAddExt(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
#ifdef VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
        findAndAddExt(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
#endif

#if defined(_WIN32)
        // Crate a Win32 Surface
        VkWin32SurfaceCreateInfoKHR createInfo = {};
        createInfo.sType                       = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.pNext                       = NULL;
        createInfo.hinstance                   = NULL;
        createInfo.hwnd                        = m_hWnd;
        ThrowIfFailedVK(vkCreateWin32SurfaceKHR(m_vkInstance, &createInfo, NULL, &m_surface));
#else
#error platform not supported
#endif

        struct QueueFamilyIndexSelection
        {
            uint32_t first    = UINT32_MAX;
            uint32_t prefered = UINT32_MAX;

            uint32_t Get() const
            {
                return (prefered != UINT32_MAX) ? prefered : first;
            }
        };

        QueueFamilyIndexSelection presentQueueSel;
        QueueFamilyIndexSelection graphicsQueueSel;
        QueueFamilyIndexSelection computeQueueSel;
        QueueFamilyIndexSelection copyQueueSel;

        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            VkBool32 supportsPresent;
            ThrowIfFailedVK(vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, m_surface, &supportsPresent));

            bool hasGfx     = (m_queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
            bool hasCompute = (m_queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
            bool hasCopy    = (m_queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) != 0;

            if (supportsPresent)
            {
                if (presentQueueSel.first == UINT32_MAX)
                    presentQueueSel.first = i;

                if (hasGfx)
                    presentQueueSel.prefered = i;
            }

            if (hasGfx)
            {
                if (graphicsQueueSel.first == UINT32_MAX)
                    graphicsQueueSel.first = i;

                if (supportsPresent)
                    graphicsQueueSel.prefered = i;
            }

            if (hasCompute)
            {
                if (computeQueueSel.first == UINT32_MAX)
                    computeQueueSel.first = i;

                if (!hasGfx)
                    computeQueueSel.prefered = i;
            }

            if (hasCopy)
            {
                if (copyQueueSel.first == UINT32_MAX)
                    copyQueueSel.first = i;

                if (!hasCompute)
                    copyQueueSel.prefered = i;
            }
        }

        m_rpsQueueIndexToVkQueueFamilyMap[RPS_AFX_QUEUE_INDEX_GFX]     = graphicsQueueSel.Get();
        m_rpsQueueIndexToVkQueueFamilyMap[RPS_AFX_QUEUE_INDEX_COMPUTE] = computeQueueSel.Get();
        m_rpsQueueIndexToVkQueueFamilyMap[RPS_AFX_QUEUE_INDEX_COPY]    = copyQueueSel.Get();

        float queuePriorities[1] = {0.0};

        VkDeviceQueueCreateInfo queueCI[RPS_AFX_QUEUE_INDEX_COUNT] = {};
        for (uint32_t queueIdx = RPS_AFX_QUEUE_INDEX_GFX; queueIdx < RPS_AFX_QUEUE_INDEX_COUNT; queueIdx++)
        {
            queueCI[queueIdx].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCI[queueIdx].pNext            = NULL;
            queueCI[queueIdx].queueCount       = 1;
            queueCI[queueIdx].pQueuePriorities = queuePriorities;
            queueCI[queueIdx].queueFamilyIndex = m_rpsQueueIndexToVkQueueFamilyMap[queueIdx];
        }

        VkPhysicalDeviceFeatures physicalDeviceFeatures       = {};
        physicalDeviceFeatures.fillModeNonSolid               = VK_TRUE;
        physicalDeviceFeatures.pipelineStatisticsQuery        = VK_TRUE;
        physicalDeviceFeatures.fragmentStoresAndAtomics       = VK_TRUE;
        physicalDeviceFeatures.vertexPipelineStoresAndAtomics = VK_TRUE;
        physicalDeviceFeatures.shaderImageGatherExtended      = VK_TRUE;
        physicalDeviceFeatures.samplerAnisotropy              = VK_TRUE;
        physicalDeviceFeatures.geometryShader                 = VK_TRUE;
        physicalDeviceFeatures.multiDrawIndirect              = VK_TRUE;
        physicalDeviceFeatures.imageCubeArray                 = VK_TRUE;
        physicalDeviceFeatures.multiViewport                  = VK_TRUE;
        physicalDeviceFeatures.sampleRateShading              = VK_TRUE;

        VkPhysicalDeviceVulkan12Features vk12Features = {};
        vk12Features.sType                            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        vk12Features.separateDepthStencilLayouts      = VK_TRUE;

#ifdef VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME

#ifdef VK_API_VERSION_1_3
        VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_feature = {};
        dynamic_rendering_feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
#else
        VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_feature = {};
        dynamic_rendering_feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
#endif
        dynamic_rendering_feature.dynamicRendering = VK_TRUE;
        vk12Features.pNext                         = &dynamic_rendering_feature;

#endif

        VkDeviceCreateInfo device_info      = {};
        device_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.queueCreateInfoCount    = sizeof(queueCI) / sizeof(queueCI[0]);
        device_info.pQueueCreateInfos       = queueCI;
        device_info.enabledExtensionCount   = (uint32_t)deviceExts.size();
        device_info.ppEnabledExtensionNames = deviceExts.empty() ? NULL : deviceExts.data();
        device_info.pEnabledFeatures        = &physicalDeviceFeatures;
        device_info.pNext                   = &vk12Features;

        ThrowIfFailedVK(vkCreateDevice(m_physicalDevice, &device_info, NULL, &m_device));

        // get queues

        vkGetDeviceQueue(m_device, presentQueueSel.Get(), 0, &m_presentQueue);

        for (uint32_t queueIdx = RPS_AFX_QUEUE_INDEX_GFX; queueIdx < RPS_AFX_QUEUE_INDEX_COUNT; queueIdx++)
        {
            vkGetDeviceQueue(m_device, m_rpsQueueIndexToVkQueueFamilyMap[queueIdx], 0, &m_queues[queueIdx]);
        }

        // Init the extensions (if they have been enabled successfuly)
        vkCmdBeginDebugUtilsLabel =
            (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetDeviceProcAddr(m_device, "vkCmdBeginDebugUtilsLabelEXT");
        vkCmdEndDebugUtilsLabel =
            (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetDeviceProcAddr(m_device, "vkCmdEndDebugUtilsLabelEXT");
        vkCmdInsertDebugUtilsLabel =
            (PFN_vkCmdInsertDebugUtilsLabelEXT)vkGetDeviceProcAddr(m_device, "vkCmdInsertDebugUtilsLabelEXT");
        vkSetDebugUtilsObjectName =
            (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(m_device, "vkSetDebugUtilsObjectNameEXT");

        // Create the default descriptor pool (mostly for ImGUI)
        VkDescriptorPoolCreateInfo dpInfo = {};
        dpInfo.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dpInfo.maxSets                    = m_defaultFrameDynamicDescriptorPoolMaxSets;
        dpInfo.pPoolSizes                 = m_defaultFrameDynamicDescriptorPoolSizes.data();
        dpInfo.poolSizeCount              = (uint32_t)m_defaultFrameDynamicDescriptorPoolSizes.size();

        ThrowIfFailedVK(vkCreateDescriptorPool(m_device, &dpInfo, nullptr, &m_descriptorPool));
    }

    void CreateSwapChain()
    {
        uint32_t oldImageCount = (uint32_t)m_swapChainImages.size();

        VkSwapchainKHR oldSwapchain = m_swapChain;

        // Get physical device surface properties and formats
        VkSurfaceCapabilitiesKHR surfCaps;
        ThrowIfFailedVK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &surfCaps));

        // Get available present modes
        uint32_t presentModeCount;
        ThrowIfFailedVK(
            vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, NULL));
        assert(presentModeCount > 0);

        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        ThrowIfFailedVK(vkGetPhysicalDeviceSurfacePresentModesKHR(
            m_physicalDevice, m_surface, &presentModeCount, presentModes.data()));

        VkExtent2D swapchainExtent = {};
        // If width (and height) equals the special value 0xFFFFFFFF, the size of the surface will be set by the swapchain
        if (surfCaps.currentExtent.width == (uint32_t)-1)
        {
            // If the surface size is undefined, the size is set to
            // the size of the images requested.
            swapchainExtent.width  = m_width;
            swapchainExtent.height = m_height;
        }
        else
        {
            // If the surface size is defined, the swap chain size must match
            swapchainExtent = surfCaps.currentExtent;
            m_width         = surfCaps.currentExtent.width;
            m_height        = surfCaps.currentExtent.height;
        }

        // Select a present mode for the swapchain

        // The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
        // This mode waits for the vertical blank ("v-sync")
        VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

        // If v-sync is not requested, try to find a mailbox mode
        // It's the lowest latency non-tearing present mode available
        if (!m_bVSync)
        {
            for (size_t i = 0; i < presentModeCount; i++)
            {
                if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
                {
                    swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                    break;
                }
                if ((swapchainPresentMode != VK_PRESENT_MODE_MAILBOX_KHR) &&
                    (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR))
                {
                    swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
                }
            }
        }

        // Determine the number of images
        uint32_t desiredNumberOfSwapchainImages = surfCaps.minImageCount + 1;
        if ((surfCaps.maxImageCount > 0) && (desiredNumberOfSwapchainImages > surfCaps.maxImageCount))
        {
            desiredNumberOfSwapchainImages = surfCaps.maxImageCount;
        }

        // Find the transformation of the surface
        VkSurfaceTransformFlagsKHR preTransform;
        if (surfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        {
            // We prefer a non-rotated transform
            preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        }
        else
        {
            preTransform = surfCaps.currentTransform;
        }

        uint32_t surfaceFormatCount = 0;
        ThrowIfFailedVK(
            vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &surfaceFormatCount, nullptr));
        std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
        ThrowIfFailedVK(vkGetPhysicalDeviceSurfaceFormatsKHR(
            m_physicalDevice, m_surface, &surfaceFormatCount, surfaceFormats.data()));
        m_swapChainFormat = surfaceFormats[0];

        // Find a supported composite alpha format (not all devices support alpha opaque)
        VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        // Simply select the first composite alpha format available
        std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        };
        for (auto& compositeAlphaFlag : compositeAlphaFlags)
        {
            if (surfCaps.supportedCompositeAlpha & compositeAlphaFlag)
            {
                compositeAlpha = compositeAlphaFlag;
                break;
            };
        }

        VkSwapchainCreateInfoKHR swapchainCI = {};
        swapchainCI.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainCI.pNext                    = NULL;
        swapchainCI.surface                  = m_surface;
        swapchainCI.minImageCount            = desiredNumberOfSwapchainImages;
        swapchainCI.imageFormat              = m_swapChainFormat.format;
        swapchainCI.imageColorSpace          = m_swapChainFormat.colorSpace;
        swapchainCI.imageExtent              = {swapchainExtent.width, swapchainExtent.height};
        swapchainCI.imageUsage               = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        swapchainCI.preTransform             = (VkSurfaceTransformFlagBitsKHR)preTransform;
        swapchainCI.imageArrayLayers         = 1;
        swapchainCI.imageSharingMode         = VK_SHARING_MODE_EXCLUSIVE;
        swapchainCI.queueFamilyIndexCount    = 0;
        swapchainCI.pQueueFamilyIndices      = NULL;
        swapchainCI.presentMode              = swapchainPresentMode;
        swapchainCI.oldSwapchain             = oldSwapchain;
        // Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
        swapchainCI.clipped        = VK_TRUE;
        swapchainCI.compositeAlpha = compositeAlpha;

        // Enable transfer source on swap chain images if supported
        if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
        {
            swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }

        // Enable transfer destination on swap chain images if supported
        if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        {
            swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }

        ThrowIfFailedVK(vkCreateSwapchainKHR(m_device, &swapchainCI, nullptr, &m_swapChain));

        // If an existing swap chain is re-created, destroy the old swap chain
        // This also cleans up all the presentable images
        if (oldSwapchain != VK_NULL_HANDLE)
        {
            for (auto& swapChainImgs : m_swapChainImages)
            {
                vkDestroyImageView(m_device, swapChainImgs.imageView, nullptr);
            }
            vkDestroySwapchainKHR(m_device, oldSwapchain, nullptr);
        }

        uint32_t numImages = 0;
        ThrowIfNotSuccessVK(vkGetSwapchainImagesKHR(m_device, m_swapChain, &numImages, nullptr));
        std::vector<VkImage> backBuffers(numImages);
        ThrowIfNotSuccessVK(vkGetSwapchainImagesKHR(m_device, m_swapChain, &numImages, backBuffers.data()));

        m_swapChainImages.resize(numImages);

        if (m_frameFences.size() < numImages)
        {
            uint32_t oldSize = (uint32_t)m_frameFences.size();
            m_frameFences.resize(numImages);

            VkSemaphoreCreateInfo semaphoreCI = {};
            semaphoreCI.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            VkFenceCreateInfo fenceCI         = {};
            fenceCI.sType                     = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceCI.flags                     = VK_FENCE_CREATE_SIGNALED_BIT;

            for (uint32_t i = oldSize; i < m_frameFences.size(); i++)
            {
                vkCreateSemaphore(m_device, &semaphoreCI, nullptr, &m_frameFences[i].imageAcquiredSemaphore);
                vkCreateFence(m_device, &fenceCI, nullptr, &m_frameFences[i].renderCompleteFence);
            }
        }

        if (m_frameDynamicDescriptorPools.size() < numImages)
        {
            uint32_t oldSize = (uint32_t)m_frameDynamicDescriptorPools.size();
            m_frameDynamicDescriptorPools.resize(numImages);
        }

        if (oldImageCount < numImages)
        {
            if (m_constantBuffer != VK_NULL_HANDLE)
            {
                vkDestroyBuffer(m_device, m_constantBuffer, nullptr);
                vkFreeMemory(m_device, m_constantBufferMemory, nullptr);
            }

            // Create shared dynamic constant buffer
            VkBufferCreateInfo bufCI    = {};
            bufCI.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufCI.usage                 = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufCI.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
            bufCI.queueFamilyIndexCount = 0;
            bufCI.pQueueFamilyIndices   = nullptr;
            bufCI.size                  = m_maxConstantSizePerFrame * numImages;

            ThrowIfFailedVK(vkCreateBuffer(m_device, &bufCI, nullptr, &m_constantBuffer));

            VkMemoryRequirements req;
            vkGetBufferMemoryRequirements(m_device, m_constantBuffer, &req);

            VkMemoryAllocateInfo ai = {};
            ai.sType                = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            ai.memoryTypeIndex      = FindMemoryTypeIndex(req.memoryTypeBits, true, true, false);
            ai.allocationSize       = req.size;

            m_constantBufferNeedsFlushAfterUpdate =
                !(m_deviceMemoryProperties.memoryTypes[ai.memoryTypeIndex].propertyFlags &
                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            ThrowIfFailedVK(vkAllocateMemory(m_device, &ai, nullptr, &m_constantBufferMemory));
            ThrowIfFailedVK(vkBindBufferMemory(m_device, m_constantBuffer, m_constantBufferMemory, 0));
            ThrowIfFailedVK(
                vkMapMemory(m_device, m_constantBufferMemory, 0, req.size, 0, (void**)&m_constantBufferCpuVA));
        }

        VkImageViewCreateInfo imageViewInfo = {};
        imageViewInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.pNext                 = NULL;
        imageViewInfo.format                = m_swapChainFormat.format;
        imageViewInfo.components            = {
            VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
        imageViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewInfo.subresourceRange.baseMipLevel   = 0;
        imageViewInfo.subresourceRange.levelCount     = 1;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount     = 1;
        imageViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.flags                           = 0;

        m_swapChainImageRpsResources.resize(m_swapChainImages.size());

        for (uint32_t iBuf = 0; iBuf < numImages; iBuf++)
        {
            m_swapChainImages[iBuf].image = backBuffers[iBuf];
            imageViewInfo.image           = backBuffers[iBuf];
            vkCreateImageView(m_device, &imageViewInfo, nullptr, &m_swapChainImages[iBuf].imageView);

            m_swapChainImageRpsResources[iBuf] = rpsVKImageToHandle(backBuffers[iBuf]);
        }

        m_backBufferIndex = 0;
    }

protected:
    bool DxcCompileToSpirv(const char*           shaderCode,
                           const WCHAR*          pShaderEntryPoint,
                           const WCHAR*          pProfile,
                           std::wstring          compilerParams,
                           const DxcDefine*      pDefines,
                           uint32_t              definesCount,
                           std::vector<uint8_t>& outSpvData)
    {
        std::wstring scp = L"-spirv -fspv-target-env=vulkan1.1 " + compilerParams;
        return DxcCompile(shaderCode, pShaderEntryPoint, pProfile, scp.c_str(), pDefines, definesCount, outSpvData);
    }

protected:
    bool             m_bGpuValidation       = true;
    bool             m_bValidationErrorOnly = true;
    bool             m_bVSync               = g_VSync;
    HWND             m_hWnd                 = NULL;
    UINT             m_width                = 0;
    UINT             m_height               = 0;
    VkInstance       m_vkInstance           = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice       = VK_NULL_HANDLE;
    VkDevice         m_device               = VK_NULL_HANDLE;

    VkDebugUtilsMessengerEXT m_vkDebugMsger = VK_NULL_HANDLE;

    std::vector<VkQueueFamilyProperties> m_queueFamilyProperties    = {};
    VkPhysicalDeviceMemoryProperties     m_deviceMemoryProperties   = {};
    VkPhysicalDeviceProperties           m_physicalDeviceProperties = {};

    struct SwapChainImages
    {
        VkImage     image;
        VkImageView imageView;
    };

    struct FrameFences
    {
        VkFence     renderCompleteFence;
        VkSemaphore renderCompleteSemaphore;
        VkSemaphore imageAcquiredSemaphore;
    };

    std::vector<SwapChainImages>    m_swapChainImages;
    std::vector<RpsRuntimeResource> m_swapChainImageRpsResources;
    std::vector<FrameFences>        m_frameFences;
    VkSurfaceKHR                    m_surface                      = VK_NULL_HANDLE;
    VkSurfaceFormatKHR              m_swapChainFormat              = {};
    VkSwapchainKHR                  m_swapChain                    = VK_NULL_HANDLE;
    uint32_t                        m_backBufferIndex              = 0;
    uint32_t                        m_swapChainImageSemaphoreIndex = 0;

    uint32_t m_frameCounter = 0;

    struct StaticMemoryPool
    {
        std::vector<VkDeviceMemory> pools;
        VkDeviceSize                lastUsage;
        VkDeviceSize                lastCapacity;
    };

    std::vector<StaticMemoryPool> m_staticAssetMemoryPools;

    struct CommandPool
    {
        bool                         inUse;
        VkCommandPool                cmdPool;
        std::vector<VkCommandBuffer> cmdBuffers;
    };

    uint32_t                              m_presentQueueFamilyIndex   = {};
    VkQueue                               m_presentQueue              = VK_NULL_HANDLE;
    VkQueue                               m_queues[RPS_AFX_QUEUE_INDEX_COUNT] = {};
    std::vector<VkSemaphore>              m_queueSemaphores;
    VkSemaphore                           m_pendingPresentSemaphore = VK_NULL_HANDLE;
    uint32_t                              m_rpsQueueIndexToVkQueueFamilyMap[RPS_AFX_QUEUE_INDEX_COUNT];
    std::vector<std::vector<CommandPool>> m_cmdPools[RPS_AFX_QUEUE_INDEX_COUNT];
    std::mutex                            m_cmdListMutex;
    VkDescriptorPool                      m_descriptorPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer>          m_cmdBufsToSubmit;

    VkBuffer       m_constantBuffer                      = VK_NULL_HANDLE;
    VkDeviceMemory m_constantBufferMemory                = VK_NULL_HANDLE;
    uint8_t*       m_constantBufferCpuVA                 = nullptr;
    bool           m_constantBufferNeedsFlushAfterUpdate = false;
    uint32_t       m_frameConstantUsage                  = 0;
    uint32_t       m_maxConstantSizePerFrame             = 16 * 1024 * 1024;

    struct FrameDynamicDescriptorPools
    {
        std::vector<VkDescriptorPool> pools;
        uint32_t                      current;
    };

    std::vector<FrameDynamicDescriptorPools> m_frameDynamicDescriptorPools;
    //TODO Let this be automatically determined
    uint32_t                          m_defaultFrameDynamicDescriptorPoolMaxSets = 1024;
    std::vector<VkDescriptorPoolSize> m_defaultFrameDynamicDescriptorPoolSizes   = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 64},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 512},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 512},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 256},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 256},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 256},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 512},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 128},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 128},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 64},
#if 0 // Not using these:
        { VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT,   0 },
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 0 },
#endif
    };

    VkRenderPass               m_imguiRenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_imguiFrameBuffers;

    void DestroySwapChainFrameBuffers()
    {
        for (VkFramebuffer fb : m_imguiFrameBuffers)
        {
            vkDestroyFramebuffer(m_device, fb, nullptr);
        }
        m_imguiFrameBuffers = {};
    }

protected:
    PFN_vkCmdBeginDebugUtilsLabelEXT  vkCmdBeginDebugUtilsLabel;
    PFN_vkCmdEndDebugUtilsLabelEXT    vkCmdEndDebugUtilsLabel;
    PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabel;
    PFN_vkSetDebugUtilsObjectNameEXT  vkSetDebugUtilsObjectName;
};