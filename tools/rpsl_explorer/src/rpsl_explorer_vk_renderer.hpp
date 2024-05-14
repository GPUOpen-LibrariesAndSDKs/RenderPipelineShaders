// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#include "rpsl_explorer.hpp"
#include "app_framework/afx_vk_renderer.hpp"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_vulkan.h"

class ToolVKRenderer : public RpsAfxVulkanRenderer
{
public:
    ToolVKRenderer(RpslExplorer* pRpslExplorer)
        : m_pRpslExplorer(pRpslExplorer)
    {
    }

    virtual void OnInit(VkCommandBuffer initCmdList, InitTempResources& tempResources) override final
    {
        m_pRpslExplorer->Init(m_hWnd);

        ImGui_ImplVulkan_InitInfo imguiVkInitInfo = {};
        imguiVkInitInfo.Instance                  = m_vkInstance;
        imguiVkInitInfo.PhysicalDevice            = m_physicalDevice;
        imguiVkInitInfo.Device                    = m_device;
        imguiVkInitInfo.QueueFamily               = m_presentQueueFamilyIndex;
        imguiVkInitInfo.Queue                     = m_presentQueue;
        imguiVkInitInfo.DescriptorPool            = m_descriptorPool;
        imguiVkInitInfo.Subpass                   = 0;
        imguiVkInitInfo.MinImageCount             = uint32_t(m_swapChainImages.size());
        imguiVkInitInfo.ImageCount                = uint32_t(m_swapChainImages.size());
        imguiVkInitInfo.MSAASamples               = VK_SAMPLE_COUNT_1_BIT;

        // Create the Render Pass
        VkAttachmentDescription attachment     = {};
        attachment.format                      = m_swapChainFormat.format;
        attachment.samples                     = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp                      = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp                     = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp               = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp              = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout               = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout                 = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference color_attachment = {};
        color_attachment.attachment            = 0;
        color_attachment.layout                = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkSubpassDescription subpass           = {};
        subpass.pipelineBindPoint              = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount           = 1;
        subpass.pColorAttachments              = &color_attachment;
        VkSubpassDependency dependency         = {};
        dependency.srcSubpass                  = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass                  = 0;
        dependency.srcStageMask                = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask                = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask               = 0;
        dependency.dstAccessMask               = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo info            = {};
        info.sType                             = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount                   = 1;
        info.pAttachments                      = &attachment;
        info.subpassCount                      = 1;
        info.pSubpasses                        = &subpass;
        info.dependencyCount                   = 1;
        info.pDependencies                     = &dependency;
        VkResult result                        = vkCreateRenderPass(m_device, &info, nullptr, &m_guiRenderPass);

        ImGui_ImplVulkan_Init(&imguiVkInitInfo, m_guiRenderPass);

        ImGui_ImplVulkan_CreateFontsTexture(initCmdList);

        OnPostResize();
    }

    virtual void OnPostInit() override
    {
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    virtual void OnCleanUp() override
    {
        ImGui_ImplVulkan_Shutdown();

        DestroySwapChainFrameBuffers();

        if (m_guiRenderPass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(m_device, m_guiRenderPass, nullptr);
            m_guiRenderPass = VK_NULL_HANDLE;
        }

        m_pRpslExplorer->CleanUp();
    }

    virtual void OnPreResize() override
    {
        DestroySwapChainFrameBuffers();
    }

    virtual void OnPostResize() override
    {
        if (m_guiRenderPass != VK_NULL_HANDLE)
        {
            VkFramebufferCreateInfo fbCreateInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            fbCreateInfo.renderPass              = m_guiRenderPass;
            fbCreateInfo.attachmentCount         = 1;
            fbCreateInfo.width                   = m_width;
            fbCreateInfo.height                  = m_height;
            fbCreateInfo.layers                  = 1;

            assert(m_guiFrameBuffers.empty());
            m_guiFrameBuffers.resize(m_swapChainImages.size());

            for (uint32_t i = 0; i < m_swapChainImages.size(); i++)
            {
                fbCreateInfo.pAttachments = &m_swapChainImages[i].imageView;
                ThrowIfFailedVK(vkCreateFramebuffer(m_device, &fbCreateInfo, nullptr, &m_guiFrameBuffers[i]));
            }
        }
    }

    virtual void OnUpdate(uint32_t frameIndex) override
    {
        ImGui_ImplVulkan_NewFrame();
        m_pRpslExplorer->RenderImGuiFrame();

        RpsResourceDesc backBufferDesc;
        auto&           swapChainBufferHdls = GetBackBuffers(backBufferDesc);

        RpsConstant               args[]         = {&backBufferDesc};
        const RpsRuntimeResource* argResources[] = {swapChainBufferHdls.data()};

        uint64_t completedFrameIndex = CalcGuaranteedCompletedFrameIndexForRps();

        m_pRpslExplorer->UpdateRpsPipeline(
            frameIndex, completedFrameIndex, uint32_t(std::size(args)), args, argResources);

        m_pRpslExplorer->Tick();
    }

    virtual void OnRender(uint32_t frameIndex) override
    {
        RpsRenderGraph hRenderGraph = m_pRpslExplorer->GetRenderGraph();
        if (hRenderGraph != RPS_NULL_HANDLE)
        {
            ExecuteRenderGraph(frameIndex, hRenderGraph, true, false);
        }

        ActiveCommandList cmdList = BeginCmdList(RPS_AFX_QUEUE_INDEX_GFX);

        VkClearValue clearColor = {};

        VkRenderPassBeginInfo rpBegin = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rpBegin.framebuffer           = m_guiFrameBuffers[m_backBufferIndex];
        rpBegin.clearValueCount       = 1;
        rpBegin.pClearValues          = &clearColor;
        rpBegin.renderPass            = m_guiRenderPass;
        rpBegin.renderArea            = VkRect2D{{0, 0}, {m_width, m_height}};

        vkCmdBeginRenderPass(cmdList, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdList);

        vkCmdEndRenderPass(cmdList);

        EndCmdList(cmdList);

        SubmitCmdLists(&cmdList, 1, true);

        RecycleCmdList(cmdList);
    }

    virtual LRESULT WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool& bHandled) override final
    {
        return m_pRpslExplorer->WindowProc(hWnd, message, wParam, lParam, bHandled);
    }

private:
    void DestroySwapChainFrameBuffers()
    {
        for (auto fb : m_guiFrameBuffers)
        {
            vkDestroyFramebuffer(m_device, fb, nullptr);
        }
        m_guiFrameBuffers = {};
    }

private:
    RpslExplorer*              m_pRpslExplorer = {};
    VkRenderPass               m_guiRenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_guiFrameBuffers;
};
