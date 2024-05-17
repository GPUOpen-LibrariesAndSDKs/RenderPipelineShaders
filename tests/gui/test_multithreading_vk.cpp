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

#include "test_multithreading_shared.hpp"

#include "rps/runtime/vk/rps_vk_runtime.h"
#include "utils/rps_test_win32.hpp"
#include "utils/rps_test_vk_renderer.hpp"

class TestVkMultithreading : public RpsTestVulkanRenderer, public TestRpsMultithreading
{
protected:
    virtual void OnInit(VkCommandBuffer initCmdBuf, InitTempResources& tempResources) override
    {
        LoadAssets(initCmdBuf, tempResources);

        TestRpsMultithreading::Init(rpsTestUtilCreateDevice(
            [this](auto pCreateInfo, auto phDevice) { return CreateRpsRuntimeDevice(*pCreateInfo, *phDevice); }));
    }

    virtual void OnCleanUp() override
    {
        TestRpsMultithreading::OnDestroy();

        vkDestroyPipeline(m_device, m_geoPipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    }

    virtual void OnUpdate(uint32_t frameIndex) override
    {
        UpdatePipeline(frameIndex, CalcGuaranteedCompletedFrameIndexForRps());

        if (frameIndex < 50 * MAX_THREADS)
        {
            SetRenderJobCount(frameIndex / 50 + 1);
        }
    }

    virtual RpsRuntimeCommandBuffer AcquireNewCommandBuffer(uint32_t* pInsertAfter) override final
    {
        m_activePrimaryCmdBufs.emplace_back(BeginCmdList(RPS_AFX_QUEUE_INDEX_GFX));
        return rpsVKCommandBufferToHandle(m_activePrimaryCmdBufs.back().cmdBuf);
    }

    virtual void OnRender(uint32_t frameIndex) override
    {
        assert(m_activePrimaryCmdBufs.empty());

        TestRpsMultithreading::OnRender(frameIndex, m_numPasses);

        for (auto& cl : m_activePrimaryCmdBufs)
        {
            EndCmdList(cl);
        }

        SubmitCmdLists(m_activePrimaryCmdBufs.data(), uint32_t(m_activePrimaryCmdBufs.size()), true);

        for (auto& secondaryCmdBuf : m_activeSecondaryCmdBufs)
        {
            RecycleCmdList(secondaryCmdBuf);
        }
        m_activeSecondaryCmdBufs.clear();

        for (uint32_t i = 0; i < m_activePrimaryCmdBufs.size(); i++)
        {
            RecycleCmdList(m_activePrimaryCmdBufs[i]);
        }
        m_activePrimaryCmdBufs.clear();
    }

    virtual void OnKeyUp(char key) override
    {
        if (key >= '1' && key <= '8')
            SetRenderJobCount(key - '1');
    }

    void SetRenderJobCount(uint32_t count)
    {
        m_renderJobs = std::max(1u, count);

        char buf[256];
        sprintf_s(buf, "TestVkMultithreading - %d workers on %d threads", m_renderJobs, m_threadPool.GetNumThreads());
        SetWindowText(m_hWnd, buf);
    }

protected:
    virtual void DrawGeometryPass(const RpsCmdCallbackContext* pContext) override final
    {
        // begin cmd rp
        RpsCmdRenderPassBeginInfo passBeginInfo = {};
        passBeginInfo.flags                     = RPS_RUNTIME_RENDER_PASS_EXECUTE_SECONDARY_COMMAND_BUFFERS;
        THREAD_SAFE_REQUIRE(rpsCmdBeginRenderPass(pContext, &passBeginInfo) == RPS_OK);

        if (m_geoPipeline == VK_NULL_HANDLE)
        {
            std::lock_guard<std::mutex> lock(m_cmdListMutex);
            if (m_geoPipeline == VK_NULL_HANDLE)
            {
                VkRenderPass rp;
                RpsResult    result = rpsVKGetCmdRenderPass(pContext, &rp);
                THREAD_SAFE_REQUIRE(result == RPS_OK);
                CreatePipeline(c_Shader, rp, &m_geoPipeline);
            }
        }

        const uint32_t numThreads = std::max(1u, std::min(MAX_THREADS, m_renderJobs));

        RpsAfxThreadPool::WaitHandle  waitHandles[MAX_THREADS];
        VkCommandBuffer               vkCmdBufs[MAX_THREADS];

        VkCommandBufferInheritanceInfo cmdBufInheritanceInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
        RpsResult                      result = rpsVKGetCmdRenderPass(pContext, &cmdBufInheritanceInfo.renderPass);
        THREAD_SAFE_REQUIRE(result == RPS_OK);

        CmdRangeContext* pRangeContext = static_cast<CmdRangeContext*>(pContext->pUserRecordContext);

        std::atomic_int32_t failCount    = {};
        std::atomic_int32_t executeCount = {};

        for (uint32_t i = 0; i < numThreads; i++)
        {
            ActiveCommandList hNewCmdBuf = BeginCmdList(RPS_AFX_QUEUE_INDEX_GFX, &cmdBufInheritanceInfo);

            const RpsCmdCallbackContext* pLocalContext = {};

            {
                std::lock_guard<std::mutex> lock(m_cmdListMutex);
                result = rpsCmdCloneContext(pContext, rpsVKCommandBufferToHandle(hNewCmdBuf), &pLocalContext);
                THREAD_SAFE_REQUIRE(result == RPS_OK);
            }

            vkCmdBufs[i] = hNewCmdBuf;

            waitHandles[i] = m_threadPool.EnqueueJob([this,
                                                      pLocalContext,
                                                      hNewCmdBuf,
                                                      i,
                                                      numThreads,
                                                      &failCount,
                                                      &executeCount,
                                                      batchId = pRangeContext->BatchIndex]() {
                ActiveCommandList cmdBuf = hNewCmdBuf;

                uint32_t numTrianglesPerThread = uint32_t(m_triangleData.size() + numThreads - 1) / numThreads;

                uint32_t beginIndex = numTrianglesPerThread * i;
                uint32_t endIndex   = std::min(uint32_t(m_triangleData.size()), beginIndex + numTrianglesPerThread);

                assert(cmdBuf == rpsVKCommandBufferFromHandle(pLocalContext->hCommandBuffer));

                RpsCmdRenderPassBeginInfo rpBeginInfo = {};
                rpBeginInfo.flags                     = RPS_RUNTIME_RENDER_PASS_SECONDARY_COMMAND_BUFFER;

                RpsResult threadResult = rpsCmdBeginRenderPass(pLocalContext, &rpBeginInfo);
                if (threadResult != RPS_OK)
                    failCount++;

                const float aspectRatio = m_height / static_cast<float>(m_width);

                auto           tid = batchId * numThreads + i;
                const XMVECTOR threadColorTint =
                    XMVectorSet(float((tid / 7) & 1), float((tid / 13) & 1), float((tid / 25) & 1), 1.0f);

                for (uint32_t triangleIdx = beginIndex; triangleIdx < endIndex; triangleIdx++)
                {
                    TriangleDataGPU cbData = {};

                    TriangleDataCPU* pTriangle = &m_triangleData[triangleIdx];
                    pTriangle->Offset.x =
                        fmod(pTriangle->Offset.x + pTriangle->Speed + m_runwayLength * 0.5f, m_runwayLength) -
                        m_runwayLength * 0.5f;

                    cbData.Pos         = pTriangle->Offset;
                    cbData.AspectRatio = aspectRatio;
                    cbData.Scale       = pTriangle->Scale;
                    XMStoreFloat3(&cbData.Color, XMVectorLerp(XMLoadFloat3(&pTriangle->Color), threadColorTint, 0.7f));

                    vkCmdPushConstants(
                        cmdBuf, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(TriangleDataGPU), &cbData);
                    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_geoPipeline);
                    vkCmdDraw(cmdBuf, 3, 1, 0, 0);
                }

                threadResult = rpsCmdEndRenderPass(pLocalContext);
                if (threadResult != RPS_OK)
                    failCount++;

                EndCmdList(cmdBuf);

                {
                    std::lock_guard<std::mutex> lock(m_cmdListMutex);
                    m_activeSecondaryCmdBufs.emplace_back(cmdBuf);
                }
                executeCount++;
            });
        }

        // For vulkan secondary cmd buffers we need to wait before execute them on the primary cmd buffer.
        m_threadPool.WaitForJobs(waitHandles, numThreads);

        THREAD_SAFE_REQUIRE(failCount == 0);
        THREAD_SAFE_REQUIRE(executeCount == numThreads);

        VkCommandBuffer cmdBufPrimary = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);
        vkCmdExecuteCommands(cmdBufPrimary, numThreads, vkCmdBufs);

        THREAD_SAFE_REQUIRE(rpsCmdEndRenderPass(pContext) == RPS_OK);
    }

private:
    void LoadAssets(VkCommandBuffer initCmdBuf, InitTempResources& tempResources)
    {
        OnPostResize();

        VkPushConstantRange pushConstRanges[1] = {};
        pushConstRanges[0].offset              = 0;
        pushConstRanges[0].size                = 7 * 4;
        pushConstRanges[0].stageFlags          = VK_SHADER_STAGE_VERTEX_BIT;

        VkPipelineLayoutCreateInfo plCI = {};
        plCI.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plCI.pPushConstantRanges        = pushConstRanges;
        plCI.pushConstantRangeCount     = _countof(pushConstRanges);

        ThrowIfFailedVK(vkCreatePipelineLayout(m_device, &plCI, nullptr, &m_pipelineLayout));
    }

    void CreatePipeline(const char* pShaderCode, VkRenderPass renderPass, VkPipeline* pPipeline)
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

        VkPipelineColorBlendAttachmentState bs[1] = {};
        bs[0].blendEnable                         = VK_FALSE;
        bs[0].srcColorBlendFactor                 = VK_BLEND_FACTOR_ONE;
        bs[0].dstColorBlendFactor                 = VK_BLEND_FACTOR_ZERO;
        bs[0].colorBlendOp                        = VK_BLEND_OP_ADD;
        bs[0].srcAlphaBlendFactor                 = VK_BLEND_FACTOR_ONE;
        bs[0].dstAlphaBlendFactor                 = VK_BLEND_FACTOR_ZERO;
        bs[0].alphaBlendOp                        = VK_BLEND_OP_ADD;
        bs[0].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        // Color blend state
        VkPipelineColorBlendStateCreateInfo cb;
        cb.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.flags             = 0;
        cb.pNext             = NULL;
        cb.attachmentCount   = 1;
        cb.pAttachments      = bs;
        cb.logicOpEnable     = VK_FALSE;
        cb.logicOp           = VK_LOGIC_OP_NO_OP;
        cb.blendConstants[0] = 1.0f;
        cb.blendConstants[1] = 1.0f;
        cb.blendConstants[2] = 1.0f;
        cb.blendConstants[3] = 1.0f;

        std::vector<VkDynamicState>      dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState        = {};
        dynamicState.sType                                   = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.pNext                                   = NULL;
        dynamicState.pDynamicStates                          = dynamicStateEnables.data();
        dynamicState.dynamicStateCount                       = (uint32_t)dynamicStateEnables.size();

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
        ds.depthTestEnable       = VK_FALSE;
        ds.depthWriteEnable      = VK_FALSE;
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

        VkShaderModule       vsModule, psModule;
        std::vector<uint8_t> vsCode, psCode;

        const DxcDefine defs[] = {{L"VULKAN", L"1"}};

        DxcCompileToSpirv(pShaderCode, L"VSMain", L"vs_6_0", L"", defs, _countof(defs), vsCode);
        DxcCompileToSpirv(pShaderCode, L"PSMain", L"ps_6_0", L"", defs, _countof(defs), psCode);

        VkShaderModuleCreateInfo smCI = {};
        smCI.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

        smCI.pCode    = reinterpret_cast<const uint32_t*>(vsCode.data());
        smCI.codeSize = vsCode.size();

        ThrowIfFailedVK(vkCreateShaderModule(m_device, &smCI, nullptr, &vsModule));

        smCI.pCode    = reinterpret_cast<const uint32_t*>(psCode.data());
        smCI.codeSize = psCode.size();

        ThrowIfFailedVK(vkCreateShaderModule(m_device, &smCI, nullptr, &psModule));

        VkPipelineShaderStageCreateInfo shaderStages[2] = {};
        shaderStages[0].sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].module                          = vsModule;
        shaderStages[0].pName                           = "VSMain";
        shaderStages[0].stage                           = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[1].sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].module                          = psModule;
        shaderStages[1].pName                           = "PSMain";
        shaderStages[1].stage                           = VK_SHADER_STAGE_FRAGMENT_BIT;

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
        psoCI.pDepthStencilState           = &ds;
        psoCI.pStages                      = shaderStages;
        psoCI.stageCount                   = _countof(shaderStages);
        psoCI.renderPass                   = renderPass;
        psoCI.subpass                      = 0;

        ThrowIfFailedVK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &psoCI, nullptr, pPipeline));

        vkDestroyShaderModule(m_device, vsModule, nullptr);
        vkDestroyShaderModule(m_device, psModule, nullptr);
    }

    void UpdatePipeline(uint64_t frameIndex, uint64_t completedFrameIndex)
    {
        RpsRuntimeResource backBuffers[16];

        if (m_swapChainImages.size() > RPS_TEST_COUNTOF(backBuffers))
            throw;

        for (uint32_t i = 0; i < m_swapChainImages.size(); i++)
        {
            backBuffers[i] = rpsVKImageToHandle(m_swapChainImages[i].image);
        }

        RpsResourceDesc backBufferDesc   = {};
        backBufferDesc.type              = RPS_RESOURCE_TYPE_IMAGE_2D;
        backBufferDesc.temporalLayers    = uint32_t(m_swapChainImages.size());
        backBufferDesc.image.arrayLayers = 1;
        backBufferDesc.image.mipLevels   = 1;
        backBufferDesc.image.format      = rpsFormatFromVK(m_swapChainFormat.format);
        backBufferDesc.image.width       = m_width;
        backBufferDesc.image.height      = m_height;
        backBufferDesc.image.sampleCount = 1;

        TestRpsMultithreading::UpdateRpsPipeline(frameIndex, completedFrameIndex, backBufferDesc, backBuffers);
    }

private:
    VkPipeline       m_geoPipeline    = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

    std::vector<ActiveCommandList> m_activeSecondaryCmdBufs;
    std::vector<ActiveCommandList> m_activePrimaryCmdBufs;
};

TEST_CASE(TEST_APP_NAME)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#if defined(BREAK_AT_ALLOC_ID)
    _CrtSetBreakAlloc(BREAK_AT_ALLOC_ID);
#endif

    TestVkMultithreading renderer;

    RpsTestRunWindowInfo runInfo = {};
    runInfo.title                = TEXT(TEST_APP_NAME);
    runInfo.numFramesToRender    = g_exitAfterFrame;
    runInfo.width                = 1280;
    runInfo.height               = 720;
    runInfo.pRenderer            = &renderer;
    RpsTestRunWindowApp(&runInfo);
}
