// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#else
#error "TODO"
#endif

#define RPS_VK_RUNTIME 1

#include "test_mrt_viewport_clear_shared.hpp"

#include "utils/rps_test_win32.hpp"
#include "utils/rps_test_vk_renderer.hpp"

class TestVkMrtViewportClear : public RpsTestVulkanRenderer, public TestRpsMrtViewportClear
{
    static constexpr uint32_t PushConstOffsetDrawId    = 0;
    static constexpr uint32_t PushConstOffsetFlagDepth = 4;

protected :
    virtual void OnInit(VkCommandBuffer initCmdBuf, InitTempResources& tempResources) override
    {
        LoadAssets();

        TestRpsMrtViewportClear::Init(rpsTestUtilCreateDevice(
            [this](auto pCreateInfo, auto phDevice) { return CreateRpsRuntimeDevice(*pCreateInfo, *phDevice); }));
    }

    virtual void OnPostResize() override
    {
    }

    virtual void OnCleanUp() override
    {
        TestRpsMrtViewportClear::OnDestroy();

        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        vkDestroyPipeline(m_device, m_psoMrt5NoDS, nullptr);
        vkDestroyPipeline(m_device, m_psoMrt3DS, nullptr);
        vkDestroyPipeline(m_device, m_psoRtArray, nullptr);
        vkDestroyPipeline(m_device, m_psoRtArrayCube, nullptr);
        vkDestroyPipeline(m_device, m_psoBlt, nullptr);
        vkDestroyPipeline(m_device, m_psoBltCube, nullptr);
        vkDestroyPipeline(m_device, m_psoWriteDepthStencil, nullptr);
        vkDestroyPipeline(m_device, m_psoReadDepthWriteStencil, nullptr);
        vkDestroyPipeline(m_device, m_psoReadDepthStencil, nullptr);
        vkDestroySampler(m_device, m_sampler, nullptr);
    }

    virtual void OnUpdate(uint32_t frameIndex) override
    {
        RpsResourceDesc backBufferDesc;
        auto&           swapChainBufferHdls = GetBackBuffers(backBufferDesc);

        const bool bBufferRTVSupported = true;

        RpsConstant               args[]         = {&backBufferDesc, &bBufferRTVSupported};
        const RpsRuntimeResource* argResources[] = {swapChainBufferHdls.data(), nullptr};

        uint64_t completedFrameIndex = CalcGuaranteedCompletedFrameIndexForRps();

        TestRpsMrtViewportClear::OnUpdate(
            frameIndex, completedFrameIndex, uint32_t(RPS_TEST_COUNTOF(args)), args, argResources);

        RpsTestVulkanRenderer::OnUpdate(frameIndex);
    }

    virtual void OnRender(uint32_t frameIndex) override
    {
        REQUIRE(RPS_SUCCEEDED(ExecuteRenderGraph(frameIndex, GetRpsRenderGraph())));
    }

protected:
    virtual void BindNodes(RpsSubprogram hRpslEntry) override final
    {
        TestRpsMrtViewportClear::BindNodes(hRpslEntry);

        RpsResult result =
            rpsProgramBindNode(hRpslEntry, "test_mrt_with_array", &TestVkMrtViewportClear::DrawMrtWithArray, this);
        REQUIRE(result == RPS_OK);

        result = rpsProgramBindNode(hRpslEntry, "blt_to_swapchain", &TestVkMrtViewportClear::DrawBlt, this);
        REQUIRE(result == RPS_OK);

        result = rpsProgramBindNode(hRpslEntry, "draw_cube_to_swapchain", &TestVkMrtViewportClear::DrawBltCube, this);
        REQUIRE(result == RPS_OK);

        result = rpsProgramBindNode(
            hRpslEntry, "test_bind_dsv_write_depth_stencil", &TestVkMrtViewportClear::BindDsvWriteDepthStencil, this);
        REQUIRE(result == RPS_OK);

        result = rpsProgramBindNode(hRpslEntry,
                                    "test_bind_dsv_read_depth_write_stencil",
                                    &TestVkMrtViewportClear::BindDsvReadDepthWriteStencil,
                                    this);
        REQUIRE(result == RPS_OK);

        result = rpsProgramBindNode(
            hRpslEntry, "test_bind_dsv_read_depth_stencil", &TestVkMrtViewportClear::BindDsvReadDepthStencil, this);
        REQUIRE(result == RPS_OK);
    }

    void Create5MrtNoDS(const RpsCmdCallbackContext* pContext)
    {
        if (m_psoMrt5NoDS == VK_NULL_HANDLE)
        {
            VkRenderPass rp;
            RpsResult    result = rpsVKGetCmdRenderPass(pContext, &rp);
            REQUIRE(result == RPS_OK);

            CreatePSO(L"VSSimple", nullptr, L"PSMrt5", 5, false, rp, &m_psoMrt5NoDS);
        }
    }

    virtual void Draw5MrtNoDS(const RpsCmdCallbackContext* pContext) override final
    {
        Create5MrtNoDS(pContext);

        VkCommandBuffer cmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);

        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_psoMrt5NoDS);
        vkCmdDraw(cmdBuf, 3, 1, 0, 0);
    }

    void Create3MrtDS(const RpsCmdCallbackContext* pContext)
    {
        if (m_psoMrt3DS == VK_NULL_HANDLE)
        {
            VkRenderPass rp;
            RpsResult    result = rpsVKGetCmdRenderPass(pContext, &rp);
            REQUIRE(result == RPS_OK);

            CreatePSO(L"VSSimple", nullptr, L"PSMrt3", 3, true, rp, &m_psoMrt3DS);
        }
    }

    virtual void Draw3MrtDS(const RpsCmdCallbackContext* pContext) override final
    {
        Create3MrtDS(pContext);

        VkCommandBuffer cmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);

        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_psoMrt3DS);
        vkCmdDraw(cmdBuf, 3, 1, 0, 0);
    }

    void CreateRtArray(const RpsCmdCallbackContext* pContext)
    {
        if (m_psoRtArray == VK_NULL_HANDLE)
        {
            VkRenderPass rp;
            RpsResult    result = rpsVKGetCmdRenderPass(pContext, &rp);
            REQUIRE(result == RPS_OK);

            CreatePSO(L"VSRtArray", L"GSRtArray", L"PSRtArray", 1, false, rp, &m_psoRtArray);
        }
    }

    virtual void DrawRtArray(const RpsCmdCallbackContext* pContext) override final
    {
        CreateRtArray(pContext);

        VkCommandBuffer cmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);

        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_psoRtArray);
        vkCmdDraw(cmdBuf, 3, 1, 0, 0);
    }

    void CreateMrtWithArray(const RpsCmdCallbackContext* pContext)
    {
        if (m_psoRtArrayCube == VK_NULL_HANDLE)
        {
            RpsCmdRenderTargetInfo rtInfo;
            RpsResult              result = rpsCmdGetRenderTargetsInfo(pContext, &rtInfo);
            REQUIRE(result == RPS_OK);

            VkRenderPass rp;
            result = rpsVKGetCmdRenderPass(pContext, &rp);
            REQUIRE(result == RPS_OK);

            CreatePSO(L"VSRtArray",
                      L"GSRtArrayToCube",
                      L"PSRtArrayToCubeMRT",
                      rtInfo.numRenderTargets,
                      rtInfo.depthStencilFormat != RPS_FORMAT_UNKNOWN,
                      rp,
                      &m_psoRtArrayCube);
        }
    }

    void DrawMrtWithArray(const RpsCmdCallbackContext* pContext) override final
    {
        CreateMrtWithArray(pContext);

        VkCommandBuffer cmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);

        VkWriteDescriptorSet writeDescriptorSet[1] = {};

        VkDescriptorSet ds;
        ThrowIfFailedVK(AllocFrameDescriptorSet(&m_descriptorSetLayout, 1, &ds));

        RpsParameterDesc paramDesc;
        RpsResult        result = rpsCmdGetParamDesc(pContext, 2, &paramDesc);
        REQUIRE(result == RPS_OK);
        REQUIRE(paramDesc.arraySize == 12);

        VkImageView imageViews[12];
        result = rpsVKGetCmdArgImageViewArray(pContext, 2, 0, imageViews, paramDesc.arraySize);
        REQUIRE(result == RPS_OK);

        VkDescriptorImageInfo imageInfo[12] = {};
        for (uint32_t i = 0; i < _countof(imageInfo); i++)
        {
            imageInfo[i].sampler     = VK_NULL_HANDLE;
            imageInfo[i].imageView   = imageViews[i];
            imageInfo[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        AppendWriteDescriptorSetImages(&writeDescriptorSet[0], ds, 2, _countof(imageInfo), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfo);

        vkUpdateDescriptorSets(m_device, _countof(writeDescriptorSet), writeDescriptorSet, 0, nullptr);

        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &ds, 0, nullptr);
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_psoRtArrayCube);
        vkCmdDraw(cmdBuf, 3, 1, 0, 0);
    }

    void CreateLargeArray(const RpsCmdCallbackContext* pContext)
    {
    }

    virtual void DrawLargeArray(const RpsCmdCallbackContext* pContext) override final
    {
    }

    void CreateBlt(const RpsCmdCallbackContext* pContext)
    {
        if (m_psoBlt == VK_NULL_HANDLE)
        {
            VkRenderPass rp;
            RpsResult result = rpsVKGetCmdRenderPass(pContext, &rp);
            REQUIRE(result == RPS_OK);

            CreatePSO(L"VSBlt", nullptr, L"PSBlt", 1, false, rp, &m_psoBlt);
        }
    }

    void DrawBlt(const RpsCmdCallbackContext* pContext,
                 RpsResourceAccessInfo        resourceAccessInfo,
                 RpsVkImageViewInfo           src,
                 const ViewportData*          dstViewport)
    {
        CreateBlt(pContext);

        VkCommandBuffer cmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);

        VkDescriptorSet ds;
        ThrowIfFailedVK(AllocFrameDescriptorSet(&m_descriptorSetLayout, 1, &ds));

        RpsCmdViewportInfo viewportScissorInfo = {};
        RpsResult          result              = rpsCmdGetViewportInfo(pContext, &viewportScissorInfo);
        REQUIRE(result == RPS_OK);
        REQUIRE(viewportScissorInfo.numViewports == 1);
        REQUIRE(dstViewport->data.x == viewportScissorInfo.pViewports[0].x);
        REQUIRE(dstViewport->data.y == viewportScissorInfo.pViewports[0].y);
        REQUIRE(dstViewport->data.z == viewportScissorInfo.pViewports[0].width);
        REQUIRE(dstViewport->data.w == viewportScissorInfo.pViewports[0].height);

        if (m_frameCounter < (m_swapChainImages.size() * 2))
        {
            RpsResourceDesc       resourceDesc = {};
            RpsRuntimeResource    rtResource;
            RpsResourceAccessInfo accessInfo;
            REQUIRE(RPS_SUCCEEDED(rpsCmdGetArgResourceDesc(pContext, 0, &resourceDesc)));
            REQUIRE(RPS_SUCCEEDED(rpsCmdGetArgResourceDescArray(pContext, 1, 0, &resourceDesc, 1)));
            REQUIRE(RPS_ERROR_INDEX_OUT_OF_BOUNDS == rpsCmdGetArgResourceDescArray(pContext, 0, 1, &resourceDesc, 1));
            REQUIRE(RPS_ERROR_INDEX_OUT_OF_BOUNDS == rpsCmdGetArgResourceDescArray(pContext, 1, 1, &resourceDesc, 1));
            REQUIRE(RPS_ERROR_TYPE_MISMATCH == rpsCmdGetArgResourceDesc(pContext, 2, &resourceDesc));  // Not a resource
            REQUIRE(RPS_ERROR_INDEX_OUT_OF_BOUNDS == rpsCmdGetArgResourceDescArray(pContext, 4, 1, &resourceDesc, 1));

            REQUIRE(RPS_SUCCEEDED(rpsCmdGetArgRuntimeResource(pContext, 0, &rtResource)));
            REQUIRE(RPS_SUCCEEDED(rpsCmdGetArgRuntimeResource(pContext, 1, &rtResource)));
            REQUIRE(RPS_ERROR_INDEX_OUT_OF_BOUNDS == rpsCmdGetArgRuntimeResourceArray(pContext, 0, 1, &rtResource, 1));
            REQUIRE(RPS_ERROR_INDEX_OUT_OF_BOUNDS == rpsCmdGetArgRuntimeResourceArray(pContext, 1, 1, &rtResource, 1));
            REQUIRE(RPS_ERROR_TYPE_MISMATCH ==
                    rpsCmdGetArgRuntimeResource(pContext, 2, &rtResource));  // Not a resource
            REQUIRE(RPS_ERROR_INDEX_OUT_OF_BOUNDS == rpsCmdGetArgRuntimeResourceArray(pContext, 4, 1, &rtResource, 1));

            REQUIRE(RPS_SUCCEEDED(rpsCmdGetArgResourceAccessInfo(pContext, 0, &accessInfo)));
            REQUIRE(accessInfo.access.accessFlags == (RPS_ACCESS_RENDER_TARGET_BIT | RPS_ACCESS_RENDER_PASS));
            REQUIRE(accessInfo.access.accessStages == RPS_SHADER_STAGE_NONE);
            REQUIRE(accessInfo.range.baseArrayLayer == 0);
            REQUIRE(accessInfo.range.arrayLayers == 1);
            REQUIRE(accessInfo.range.baseMipLevel == 0);
            REQUIRE(accessInfo.range.mipLevels == 1);
            REQUIRE(accessInfo.viewFormat == rpsFormatFromVK(m_swapChainFormat.format));
            REQUIRE(resourceAccessInfo.access.accessFlags == accessInfo.access.accessFlags);
            REQUIRE(resourceAccessInfo.access.accessStages == accessInfo.access.accessStages);
            REQUIRE(resourceAccessInfo.range.baseArrayLayer == accessInfo.range.baseArrayLayer);
            REQUIRE(resourceAccessInfo.range.arrayLayers == accessInfo.range.arrayLayers);
            REQUIRE(resourceAccessInfo.range.baseMipLevel == accessInfo.range.baseMipLevel);
            REQUIRE(resourceAccessInfo.range.mipLevels == accessInfo.range.mipLevels);
            REQUIRE(resourceAccessInfo.viewFormat == accessInfo.viewFormat);

            REQUIRE(RPS_SUCCEEDED(rpsCmdGetArgResourceAccessInfo(pContext, 1, &accessInfo)));
            REQUIRE(RPS_ERROR_INDEX_OUT_OF_BOUNDS ==
                    rpsCmdGetArgResourceAccessInfoArray(pContext, 0, 1, &accessInfo, 1));
            REQUIRE(RPS_ERROR_INDEX_OUT_OF_BOUNDS ==
                    rpsCmdGetArgResourceAccessInfoArray(pContext, 1, 1, &accessInfo, 1));
            REQUIRE(RPS_ERROR_TYPE_MISMATCH ==
                    rpsCmdGetArgResourceAccessInfo(pContext, 2, &accessInfo));  // Not a resource
            REQUIRE(RPS_ERROR_INDEX_OUT_OF_BOUNDS ==
                    rpsCmdGetArgResourceAccessInfoArray(pContext, 4, 1, &accessInfo, 1));


            VkImage      hImage      = VK_NULL_HANDLE;
            VkImageView  hImageView  = VK_NULL_HANDLE;
            VkBuffer     hBuffer     = VK_NULL_HANDLE;
            VkBufferView hBufferView = VK_NULL_HANDLE;

            REQUIRE((RPS_SUCCEEDED(rpsVKGetCmdArgImage(pContext, 0, &hImage)) && (hImage != VK_NULL_HANDLE)));
            hImage = VK_NULL_HANDLE;
            REQUIRE(
                (RPS_SUCCEEDED(rpsVKGetCmdArgImageArray(pContext, 0, 0, &hImage, 1)) && (hImage != VK_NULL_HANDLE)));
            REQUIRE(RPS_ERROR_INDEX_OUT_OF_BOUNDS == rpsVKGetCmdArgImageArray(pContext, 0, 1, &hImage, 1));
            REQUIRE(RPS_ERROR_INDEX_OUT_OF_BOUNDS == rpsVKGetCmdArgImageArray(pContext, 0, 0, &hImage, 2));

            REQUIRE(
                (RPS_SUCCEEDED(rpsVKGetCmdArgImageView(pContext, 0, &hImageView)) && (hImageView != VK_NULL_HANDLE)));
            hImageView = VK_NULL_HANDLE;
            REQUIRE((RPS_SUCCEEDED(rpsVKGetCmdArgImageViewArray(pContext, 0, 0, &hImageView, 1)) &&
                     (hImageView != VK_NULL_HANDLE)));
            REQUIRE(RPS_ERROR_INDEX_OUT_OF_BOUNDS == rpsVKGetCmdArgImageViewArray(pContext, 0, 1, &hImageView, 1));
            REQUIRE(RPS_ERROR_INDEX_OUT_OF_BOUNDS == rpsVKGetCmdArgImageViewArray(pContext, 0, 0, &hImageView, 2));

            REQUIRE(RPS_ERROR_TYPE_MISMATCH == rpsVKGetCmdArgBuffer(pContext, 0, &hBuffer));
            REQUIRE(RPS_ERROR_TYPE_MISMATCH == rpsVKGetCmdArgBufferArray(pContext, 0, 0, &hBuffer, 1));
            REQUIRE(RPS_ERROR_TYPE_MISMATCH == rpsVKGetCmdArgBufferView(pContext, 0, &hBufferView));
            REQUIRE(RPS_ERROR_TYPE_MISMATCH == rpsVKGetCmdArgBufferViewArray(pContext, 0, 0, &hBufferView, 1));

            REQUIRE(RPS_ERROR_TYPE_MISMATCH == rpsVKGetCmdArgBuffer(pContext, 1, &hBuffer));
            REQUIRE(RPS_ERROR_TYPE_MISMATCH == rpsVKGetCmdArgBufferView(pContext, 1, &hBufferView));

            hImage     = VK_NULL_HANDLE;
            hImageView = VK_NULL_HANDLE;
            REQUIRE((RPS_SUCCEEDED(rpsVKGetCmdArgImage(pContext, 1, &hImage)) && (hImage != VK_NULL_HANDLE)));
            REQUIRE(RPS_ERROR_TYPE_MISMATCH == rpsVKGetCmdArgImage(pContext, 2, &hImage));
        }

        VkWriteDescriptorSet writeDescriptorSet[1] = {};

        VkDescriptorImageInfo imageInfo = {VK_NULL_HANDLE, src.hImageView, src.layout};
        AppendWriteDescriptorSetImages(&writeDescriptorSet[0], ds, 1, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageInfo);

        vkUpdateDescriptorSets(m_device, _countof(writeDescriptorSet), writeDescriptorSet, 0, nullptr);

        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &ds, 0, nullptr);
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_psoBlt);
        vkCmdDraw(cmdBuf, 3, 1, 0, 0);
    }

    void CreateBltCube(const RpsCmdCallbackContext* pContext)
    {
        if (!m_psoBltCube)
        {
            RpsCmdRenderTargetInfo rtInfo;
            RpsResult              result = rpsCmdGetRenderTargetsInfo(pContext, &rtInfo);
            REQUIRE(result == RPS_OK);

            VkRenderPass rp;
            result = rpsVKGetCmdRenderPass(pContext, &rp);
            REQUIRE(result == RPS_OK);

            CreatePSO(L"VSBlt", nullptr, L"PSBltCube", rtInfo.numRenderTargets, rtInfo.depthStencilFormat != RPS_FORMAT_UNKNOWN, rp, &m_psoBltCube);
        }
    }

    void DrawBltCube(const RpsCmdCallbackContext* pContext,
                     rps::UnusedArg               dst,
                     VkImageView                  src,
                     const ViewportData&          dstViewport)
    {
        CreateBltCube(pContext);

        VkCommandBuffer cmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);

        VkDescriptorSet ds;
        ThrowIfFailedVK(AllocFrameDescriptorSet(&m_descriptorSetLayout, 1, &ds));

        VkWriteDescriptorSet writeDescriptorSet[1] = {};

        VkDescriptorImageInfo imageInfo = {VK_NULL_HANDLE, src, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        AppendWriteDescriptorSetImages(&writeDescriptorSet[0], ds, 1, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageInfo);

        vkUpdateDescriptorSets(m_device, _countof(writeDescriptorSet), writeDescriptorSet, 0, nullptr);

        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &ds, 0, nullptr);
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_psoBltCube);
        vkCmdDraw(cmdBuf, 3, 1, 0, 0);
    }

    void BindDsvWriteDepthStencil(const RpsCmdCallbackContext* pContext)
    {
        if (!m_psoWriteDepthStencil)
        {
            RpsCmdRenderTargetInfo rtInfo;
            RpsResult              result = rpsCmdGetRenderTargetsInfo(pContext, &rtInfo);
            REQUIRE(result == RPS_OK);

            VkRenderPass rp;
            result = rpsVKGetCmdRenderPass(pContext, &rp);
            REQUIRE(result == RPS_OK);

            VkPipelineDepthStencilStateCreateInfo dsStateInfo;
            dsStateInfo.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            dsStateInfo.pNext                 = NULL;
            dsStateInfo.flags                 = 0;
            dsStateInfo.depthTestEnable       = VK_TRUE;
            dsStateInfo.depthWriteEnable      = VK_TRUE;
            dsStateInfo.depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL;
            dsStateInfo.depthBoundsTestEnable = VK_FALSE;
            dsStateInfo.stencilTestEnable     = VK_TRUE;
            dsStateInfo.back.failOp           = VK_STENCIL_OP_KEEP;
            dsStateInfo.back.passOp           = VK_STENCIL_OP_REPLACE;
            dsStateInfo.back.compareOp        = VK_COMPARE_OP_ALWAYS;
            dsStateInfo.back.compareMask      = 0;
            dsStateInfo.back.reference        = 0;
            dsStateInfo.back.depthFailOp      = VK_STENCIL_OP_KEEP;
            dsStateInfo.back.writeMask        = 0xff;
            dsStateInfo.minDepthBounds        = 0;
            dsStateInfo.maxDepthBounds        = 0;
            dsStateInfo.front                 = dsStateInfo.back;

            CreatePSO(L"VSSimple",
                      nullptr,
                      L"PSWriteDepthStencil",
                      rtInfo.numRenderTargets,
                      rtInfo.depthStencilFormat != RPS_FORMAT_UNKNOWN,
                      rp,
                      &m_psoWriteDepthStencil,
                      &dsStateInfo);
        }

        VkCommandBuffer cmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);

        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_psoWriteDepthStencil);

        uint32_t drawId = 0;
        vkCmdPushConstants(cmdBuf,
                           m_pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           PushConstOffsetDrawId,
                           sizeof(drawId),
                           &drawId);
        vkCmdSetStencilReference(cmdBuf, VK_STENCIL_FACE_FRONT_AND_BACK, 0x1);
        vkCmdDraw(cmdBuf, 3, 1, 0, 0);

        drawId = 1;
        vkCmdPushConstants(cmdBuf,
                           m_pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           PushConstOffsetDrawId,
                           sizeof(drawId),
                           &drawId);
        vkCmdSetStencilReference(cmdBuf, VK_STENCIL_FACE_FRONT_AND_BACK, 0x2);
        vkCmdDraw(cmdBuf, 3, 1, 0, 0);
    }

    void BindDsvReadDepthWriteStencil(const RpsCmdCallbackContext* pContext, RpsVkImageViewInfo depthSrv)
    {
        if (!m_psoReadDepthWriteStencil)
        {
            RpsCmdRenderTargetInfo rtInfo;
            RpsResult              result = rpsCmdGetRenderTargetsInfo(pContext, &rtInfo);
            REQUIRE(result == RPS_OK);

            VkRenderPass rp;
            result = rpsVKGetCmdRenderPass(pContext, &rp);
            REQUIRE(result == RPS_OK);

            VkPipelineDepthStencilStateCreateInfo dsStateInfo;
            dsStateInfo.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            dsStateInfo.pNext                 = NULL;
            dsStateInfo.flags                 = 0;
            dsStateInfo.depthTestEnable       = VK_TRUE;
            dsStateInfo.depthWriteEnable      = VK_FALSE;
            dsStateInfo.depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL;
            dsStateInfo.depthBoundsTestEnable = VK_FALSE;
            dsStateInfo.stencilTestEnable     = VK_TRUE;
            dsStateInfo.back.failOp           = VK_STENCIL_OP_KEEP;
            dsStateInfo.back.passOp           = VK_STENCIL_OP_INCREMENT_AND_WRAP;
            dsStateInfo.back.compareOp        = VK_COMPARE_OP_EQUAL;
            dsStateInfo.back.compareMask      = 0xff;
            dsStateInfo.back.reference        = 0x2;
            dsStateInfo.back.depthFailOp      = VK_STENCIL_OP_KEEP;
            dsStateInfo.back.writeMask        = 0xff;
            dsStateInfo.minDepthBounds        = 0;
            dsStateInfo.maxDepthBounds        = 0;
            dsStateInfo.front                 = dsStateInfo.back;

            CreatePSO(L"VSSimpleFlatDepth",
                      nullptr,
                      L"PSReadDepthWriteStencil",
                      rtInfo.numRenderTargets,
                      rtInfo.depthStencilFormat != RPS_FORMAT_UNKNOWN,
                      rp,
                      &m_psoReadDepthWriteStencil,
                      &dsStateInfo);
        }

        VkCommandBuffer cmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);

        VkDescriptorSet ds;
        ThrowIfFailedVK(AllocFrameDescriptorSet(&m_descriptorSetLayout, 1, &ds));

        VkWriteDescriptorSet writeDescriptorSet[1] = {};

        VkDescriptorImageInfo imageInfo = {VK_NULL_HANDLE, depthSrv.hImageView, depthSrv.layout};
        AppendWriteDescriptorSetImages(&writeDescriptorSet[0], ds, 1, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageInfo);

        vkUpdateDescriptorSets(m_device, _countof(writeDescriptorSet), writeDescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &ds, 0, nullptr);

        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_psoReadDepthWriteStencil);
        float depth = 0.25f;
        vkCmdPushConstants(cmdBuf,
                           m_pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           PushConstOffsetFlagDepth,
                           sizeof(depth),
                           &depth);
        vkCmdSetStencilReference(cmdBuf, VK_STENCIL_FACE_FRONT_AND_BACK, 0x2);
        vkCmdDraw(cmdBuf, 3, 1, 0, 0);
    }

    void BindDsvReadDepthStencil(const RpsCmdCallbackContext* pContext,
                                 RpsVkImageViewInfo           depthSrv,
                                 RpsVkImageViewInfo           stencilSrv)
    {
        if (!m_psoReadDepthStencil)
        {
            RpsCmdRenderTargetInfo rtInfo;
            RpsResult              result = rpsCmdGetRenderTargetsInfo(pContext, &rtInfo);
            REQUIRE(result == RPS_OK);

            VkRenderPass rp;
            result = rpsVKGetCmdRenderPass(pContext, &rp);
            REQUIRE(result == RPS_OK);

            VkPipelineDepthStencilStateCreateInfo dsStateInfo;
            dsStateInfo.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            dsStateInfo.pNext                 = NULL;
            dsStateInfo.flags                 = 0;
            dsStateInfo.depthTestEnable       = VK_TRUE;
            dsStateInfo.depthWriteEnable      = VK_FALSE;
            dsStateInfo.depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL;
            dsStateInfo.depthBoundsTestEnable = VK_FALSE;
            dsStateInfo.stencilTestEnable     = VK_TRUE;
            dsStateInfo.back.failOp           = VK_STENCIL_OP_KEEP;
            dsStateInfo.back.passOp           = VK_STENCIL_OP_KEEP;
            dsStateInfo.back.compareOp        = VK_COMPARE_OP_EQUAL;
            dsStateInfo.back.compareMask      = 0xff;
            dsStateInfo.back.reference        = 0x3;
            dsStateInfo.back.depthFailOp      = VK_STENCIL_OP_KEEP;
            dsStateInfo.back.writeMask        = 0x0;
            dsStateInfo.minDepthBounds        = 0;
            dsStateInfo.maxDepthBounds        = 0;
            dsStateInfo.front                 = dsStateInfo.back;

            CreatePSO(L"VSSimpleFlatDepth",
                      nullptr,
                      L"PSReadDepthStencil",
                      rtInfo.numRenderTargets,
                      rtInfo.depthStencilFormat != RPS_FORMAT_UNKNOWN,
                      rp,
                      &m_psoReadDepthStencil,
                      &dsStateInfo);
        }

        VkCommandBuffer cmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);

        VkDescriptorSet ds;
        ThrowIfFailedVK(AllocFrameDescriptorSet(&m_descriptorSetLayout, 1, &ds));

        VkWriteDescriptorSet writeDescriptorSet[2] = {};

        VkDescriptorImageInfo depthImageInfo   = {VK_NULL_HANDLE, depthSrv.hImageView, depthSrv.layout};
        VkDescriptorImageInfo stencilImageInfo = {VK_NULL_HANDLE, stencilSrv.hImageView, stencilSrv.layout};
        AppendWriteDescriptorSetImages(
            &writeDescriptorSet[0], ds, 1, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &depthImageInfo);
        AppendWriteDescriptorSetImages(
            &writeDescriptorSet[1], ds, 3, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &stencilImageInfo);

        vkUpdateDescriptorSets(m_device, _countof(writeDescriptorSet), writeDescriptorSet, 0, nullptr);

        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &ds, 0, nullptr);

        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_psoReadDepthStencil);
        const float depth = 0.5f;
        vkCmdPushConstants(cmdBuf,
                           m_pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           PushConstOffsetFlagDepth,
                           sizeof(depth),
                           &depth);
        vkCmdSetStencilReference(cmdBuf, VK_STENCIL_FACE_FRONT_AND_BACK, 0x3);
        vkCmdDraw(cmdBuf, 3, 1, 0, 0);
    }

private:
    void LoadAssets()
    {
        OnPostResize();

        VkSamplerCreateInfo samplerCI = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerCI.magFilter           = VK_FILTER_LINEAR;
        samplerCI.minFilter           = VK_FILTER_LINEAR;
        samplerCI.mipmapMode          = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCI.addressModeU        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.addressModeV        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.addressModeW        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.mipLodBias          = 0.0f;
        samplerCI.compareOp           = VK_COMPARE_OP_NEVER;
        samplerCI.minLod              = 0.0f;
        samplerCI.maxLod              = FLT_MAX;
        samplerCI.maxAnisotropy       = 1.0;
        samplerCI.anisotropyEnable    = VK_FALSE;
        samplerCI.borderColor         = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

        ThrowIfFailedVK(vkCreateSampler(m_device, &samplerCI, nullptr, &m_sampler));

        VkDescriptorSetLayoutBinding sharedLayoutBindings[4] = {};
        sharedLayoutBindings[0].binding                      = 1;
        sharedLayoutBindings[0].descriptorCount              = 1;
        sharedLayoutBindings[0].descriptorType               = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        sharedLayoutBindings[0].stageFlags                   = VK_SHADER_STAGE_FRAGMENT_BIT;
        sharedLayoutBindings[1].binding                      = 2;
        sharedLayoutBindings[1].descriptorCount              = 12;
        sharedLayoutBindings[1].descriptorType               = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        sharedLayoutBindings[1].stageFlags                   = VK_SHADER_STAGE_FRAGMENT_BIT;
        sharedLayoutBindings[2].binding                      = 0;
        sharedLayoutBindings[2].descriptorCount              = 1;
        sharedLayoutBindings[2].descriptorType               = VK_DESCRIPTOR_TYPE_SAMPLER;
        sharedLayoutBindings[2].stageFlags                   = VK_SHADER_STAGE_FRAGMENT_BIT;
        sharedLayoutBindings[2].pImmutableSamplers           = &m_sampler;
        sharedLayoutBindings[3].binding                      = 3;
        sharedLayoutBindings[3].descriptorCount              = 1;
        sharedLayoutBindings[3].descriptorType               = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        sharedLayoutBindings[3].stageFlags                   = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo setLayoutCI = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        setLayoutCI.pBindings                       = sharedLayoutBindings;
        setLayoutCI.bindingCount                    = _countof(sharedLayoutBindings);

        ThrowIfFailedVK(vkCreateDescriptorSetLayout(m_device, &setLayoutCI, nullptr, &m_descriptorSetLayout));

        VkPushConstantRange pushConstRanges[] = {{VK_SHADER_STAGE_FRAGMENT_BIT, 0, 8},
                                                 {VK_SHADER_STAGE_VERTEX_BIT, 0, 8}};

        VkPipelineLayoutCreateInfo plCI = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        plCI.setLayoutCount             = 1;
        plCI.pSetLayouts                = &m_descriptorSetLayout;
        plCI.pushConstantRangeCount     = RPS_TEST_COUNTOF(pushConstRanges);
        plCI.pPushConstantRanges        = pushConstRanges;

        ThrowIfFailedVK(vkCreatePipelineLayout(m_device, &plCI, nullptr, &m_pipelineLayout));
    }

    void CreatePSO(const WCHAR*                                 vsEntry,
                   const WCHAR*                                 gsEntry,
                   const WCHAR*                                 psEntry,
                   uint32_t                                     numColorAttachments,
                   bool                                         bDepth,
                   VkRenderPass                                 renderPass,
                   VkPipeline*                                  pPso,
                   const VkPipelineDepthStencilStateCreateInfo* pCustomDSInfo = nullptr)
    {
        VkPipelineVertexInputStateCreateInfo vi = {};
        vi.sType                                = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vi.pNext                                = NULL;
        vi.flags                                = 0;
        vi.vertexBindingDescriptionCount        = 0;
        vi.pVertexBindingDescriptions           = nullptr;
        vi.vertexAttributeDescriptionCount      = 0;
        vi.pVertexAttributeDescriptions         = nullptr;

        // input assembly state
        //
        VkPipelineInputAssemblyStateCreateInfo ia;
        ia.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.pNext                  = NULL;
        ia.flags                  = 0;
        ia.primitiveRestartEnable = VK_FALSE;
        ia.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // rasterizer state
        VkPipelineRasterizationStateCreateInfo rs;
        rs.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.pNext                   = NULL;
        rs.flags                   = 0;
        rs.polygonMode             = VK_POLYGON_MODE_FILL;
        rs.cullMode                = VK_CULL_MODE_NONE;
        rs.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rs.depthClampEnable        = VK_FALSE;
        rs.rasterizerDiscardEnable = VK_FALSE;
        rs.depthBiasEnable         = VK_FALSE;
        rs.depthBiasConstantFactor = 0;
        rs.depthBiasClamp          = 0;
        rs.depthBiasSlopeFactor    = 0;
        rs.lineWidth               = 1.0f;

        VkPipelineColorBlendAttachmentState bs[8] = {};
        bs[0].blendEnable                         = VK_FALSE;
        bs[0].srcColorBlendFactor                 = VK_BLEND_FACTOR_ONE;
        bs[0].dstColorBlendFactor                 = VK_BLEND_FACTOR_ZERO;
        bs[0].colorBlendOp                        = VK_BLEND_OP_ADD;
        bs[0].srcAlphaBlendFactor                 = VK_BLEND_FACTOR_ONE;
        bs[0].dstAlphaBlendFactor                 = VK_BLEND_FACTOR_ZERO;
        bs[0].alphaBlendOp                        = VK_BLEND_OP_ADD;
        bs[0].colorWriteMask                      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        for (uint32_t i = 1; i < numColorAttachments; i++)
        {
            bs[i] = bs[0];
        }

        // Color blend state
        VkPipelineColorBlendStateCreateInfo cb;
        cb.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.flags             = 0;
        cb.pNext             = NULL;
        cb.attachmentCount   = numColorAttachments;
        cb.pAttachments      = bs;
        cb.logicOpEnable     = VK_FALSE;
        cb.logicOp           = VK_LOGIC_OP_NO_OP;
        cb.blendConstants[0] = 1.0f;
        cb.blendConstants[1] = 1.0f;
        cb.blendConstants[2] = 1.0f;
        cb.blendConstants[3] = 1.0f;

        VkDynamicState dynamicStateEnables[] = {
            VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_STENCIL_REFERENCE};
        VkPipelineDynamicStateCreateInfo dynamicState        = {};
        dynamicState.sType                                   = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.pNext                                   = NULL;
        dynamicState.pDynamicStates                          = dynamicStateEnables;
        dynamicState.dynamicStateCount                       = RPS_TEST_COUNTOF(dynamicStateEnables);

        // view port state

        VkPipelineViewportStateCreateInfo vp = {};
        vp.sType                             = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vp.pNext                             = NULL;
        vp.flags                             = 0;
        vp.viewportCount                     = 1;
        vp.scissorCount                      = 1;
        vp.pScissors                         = NULL;
        vp.pViewports                        = NULL;

        // depth stencil state

        VkPipelineDepthStencilStateCreateInfo ds;
        ds.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        ds.pNext                 = NULL;
        ds.flags                 = 0;
        ds.depthTestEnable       = bDepth ? VK_TRUE : VK_FALSE;
        ds.depthWriteEnable      = bDepth ? VK_TRUE : VK_FALSE;
        ds.depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL;
        ds.depthBoundsTestEnable = VK_FALSE;
        ds.stencilTestEnable     = VK_FALSE;
        ds.back.failOp           = VK_STENCIL_OP_KEEP;
        ds.back.passOp           = VK_STENCIL_OP_KEEP;
        ds.back.compareOp        = VK_COMPARE_OP_ALWAYS;
        ds.back.compareMask      = 0;
        ds.back.reference        = 0;
        ds.back.depthFailOp      = VK_STENCIL_OP_KEEP;
        ds.back.writeMask        = 0;
        ds.minDepthBounds        = 0;
        ds.maxDepthBounds        = 0;
        ds.stencilTestEnable     = VK_FALSE;
        ds.front                 = ds.back;

        // multi sample state

        VkPipelineMultisampleStateCreateInfo ms;
        ms.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.pNext                 = NULL;
        ms.flags                 = 0;
        ms.pSampleMask           = NULL;
        ms.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
        ms.sampleShadingEnable   = VK_FALSE;
        ms.alphaToCoverageEnable = VK_FALSE;
        ms.alphaToOneEnable      = VK_FALSE;
        ms.minSampleShading      = 0.0;

        VkShaderModule       vsModule = VK_NULL_HANDLE, gsModule = VK_NULL_HANDLE, psModule = VK_NULL_HANDLE;
        std::vector<uint8_t> vsCode, gsCode, psCode;

        const DxcDefine defs[] = {{L"VULKAN", L"1"}};

        DxcCompileToSpirv(c_Shader, vsEntry, L"vs_6_0", L"", defs, 1, vsCode);
        DxcCompileToSpirv(c_Shader, psEntry, L"ps_6_0", L"", defs, 1, psCode);

        if (gsEntry)
        {
            DxcCompileToSpirv(c_Shader, gsEntry, L"gs_6_0", L"", defs, 1, gsCode);
        }

        VkShaderModuleCreateInfo smCI = {};
        smCI.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

        smCI.pCode    = reinterpret_cast<const uint32_t*>(vsCode.data());
        smCI.codeSize = vsCode.size();

        ThrowIfFailedVK(vkCreateShaderModule(m_device, &smCI, nullptr, &vsModule));

        smCI.pCode    = reinterpret_cast<const uint32_t*>(psCode.data());
        smCI.codeSize = psCode.size();

        ThrowIfFailedVK(vkCreateShaderModule(m_device, &smCI, nullptr, &psModule));

        char vsName[128];
        char psName[128];
        sprintf_s(vsName, "%S", vsEntry);
        sprintf_s(psName, "%S", psEntry);

        VkPipelineShaderStageCreateInfo shaderStages[3] = {};
        shaderStages[0].sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].module                          = vsModule;
        shaderStages[0].pName                           = vsName;
        shaderStages[0].stage                           = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[1].sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].module                          = psModule;
        shaderStages[1].pName                           = psName;
        shaderStages[1].stage                           = VK_SHADER_STAGE_FRAGMENT_BIT;

        uint32_t numShaderStages = 2;

        char gsName[128];
        if (gsEntry)
        {
            smCI.pCode    = reinterpret_cast<const uint32_t*>(gsCode.data());
            smCI.codeSize = gsCode.size();

            ThrowIfFailedVK(vkCreateShaderModule(m_device, &smCI, nullptr, &gsModule));

            sprintf_s(gsName, "%S", gsEntry);

            shaderStages[2].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStages[2].module = gsModule;
            shaderStages[2].pName  = gsName;
            shaderStages[2].stage  = VK_SHADER_STAGE_GEOMETRY_BIT;

            numShaderStages = 3;
        }

        VkGraphicsPipelineCreateInfo psoCI = {};
        psoCI.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        psoCI.pNext                        = NULL;
        psoCI.layout                       = m_pipelineLayout;
        psoCI.basePipelineHandle           = VK_NULL_HANDLE;
        psoCI.basePipelineIndex            = 0;
        psoCI.flags                        = 0;
        psoCI.pVertexInputState            = &vi;
        psoCI.pInputAssemblyState          = &ia;
        psoCI.pRasterizationState          = &rs;
        psoCI.pColorBlendState             = &cb;
        psoCI.pTessellationState           = NULL;
        psoCI.pMultisampleState            = &ms;
        psoCI.pDynamicState                = &dynamicState;
        psoCI.pViewportState               = &vp;
        psoCI.pDepthStencilState           = pCustomDSInfo ? pCustomDSInfo : &ds;
        psoCI.pStages                      = shaderStages;
        psoCI.stageCount                   = numShaderStages;
        psoCI.renderPass                   = renderPass;
        psoCI.subpass                      = 0;

        ThrowIfFailedVK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &psoCI, nullptr, pPso));

        vkDestroyShaderModule(m_device, vsModule, nullptr);
        vkDestroyShaderModule(m_device, psModule, nullptr);
        if (gsModule != RPS_NULL_HANDLE)
        {
            vkDestroyShaderModule(m_device, gsModule, nullptr);
        }
    }

private:
    VkDescriptorSetLayout m_descriptorSetLayout      = VK_NULL_HANDLE;
    VkPipelineLayout      m_pipelineLayout           = VK_NULL_HANDLE;
    VkPipeline            m_psoMrt5NoDS              = VK_NULL_HANDLE;
    VkPipeline            m_psoMrt3DS                = VK_NULL_HANDLE;
    VkPipeline            m_psoRtArray               = VK_NULL_HANDLE;
    VkPipeline            m_psoRtArrayCube           = VK_NULL_HANDLE;
    VkPipeline            m_psoBlt                   = VK_NULL_HANDLE;
    VkPipeline            m_psoBltCube               = VK_NULL_HANDLE;
    VkPipeline            m_psoWriteDepthStencil     = VK_NULL_HANDLE;
    VkPipeline            m_psoReadDepthWriteStencil = VK_NULL_HANDLE;
    VkPipeline            m_psoReadDepthStencil      = VK_NULL_HANDLE;
    VkSampler             m_sampler                  = VK_NULL_HANDLE;
};

TEST_CASE(TEST_APP_NAME)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#if defined(BREAK_AT_ALLOC_ID)
    _CrtSetBreakAlloc(BREAK_AT_ALLOC_ID);
#endif

    TestVkMrtViewportClear renderer;

    RpsTestRunWindowInfo runInfo = {};
    runInfo.title                = TEXT(TEST_APP_NAME);
    runInfo.numFramesToRender    = g_exitAfterFrame;
    runInfo.width                = 1280;
    runInfo.height               = 720;
    runInfo.pRenderer            = &renderer;
    RpsTestRunWindowApp(&runInfo);
}
