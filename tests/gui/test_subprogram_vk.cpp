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

#include "test_subprogram_shared.hpp"

#include "utils/rps_test_win32.hpp"
#include "utils/rps_test_vk_renderer.hpp"

class TestVkSubprogram : public RpsTestVulkanRenderer, public TestRpsSubprogram
{
protected:
    virtual void OnInit(VkCommandBuffer initCmdBuf, InitTempResources& tempResources) override
    {
        LoadAssets();

        RpsDevice hRpsDevice = rpsTestUtilCreateDevice(
            [this](auto pCreateInfo, auto phDevice) { return CreateRpsRuntimeDevice(*pCreateInfo, *phDevice); });

        TestRpsSubprogram::Init(hRpsDevice);
    }

    virtual void OnPostResize() override
    {
    }

    virtual void OnCleanUp() override
    {
        TestRpsSubprogram::OnDestroy();

        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        vkDestroyPipeline(m_device, m_pipelineStateDrawScene, nullptr);
        vkDestroyPipeline(m_device, m_pipelineStateDrawGUI, nullptr);
    }

    virtual void OnUpdate(uint32_t frameIndex) override
    {
        BindNodes(m_mainRpslProgram, !!((frameIndex >> 6) & 1), !!((frameIndex >> 5) & 1));

        RpsResourceDesc backBufferDesc;
        auto&           swapChainBufferHdls = GetBackBuffers(backBufferDesc);

        RpsViewport viewport = {
            0.0f, 0.0f, float(backBufferDesc.image.width), float(backBufferDesc.image.height), 0.0f, 1.0f};

        RpsConstant               args[]         = {&backBufferDesc, &viewport};
        const RpsRuntimeResource* argResources[] = {swapChainBufferHdls.data()};

        uint64_t completedFrameIndex = CalcGuaranteedCompletedFrameIndexForRps();

        TestRpsSubprogram::OnUpdate(
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
        BindNodes(hRpslEntry, false, false);
    }

    void BindNodes(RpsSubprogram hRpslEntry, bool bUseSceneSubprogram, bool bUseGUISubprogram)
    {
        TestRpsSubprogram::BindNodes(hRpslEntry);

        RpsResult result = bUseSceneSubprogram
                               ? rpsProgramBindNodeSubprogram(hRpslEntry, "DrawScene", m_drawSceneSubprogram)
                               : rpsProgramBindNode(hRpslEntry, "DrawScene", &TestVkSubprogram::DrawScene, this);
        REQUIRE(result == RPS_OK);

        result = bUseGUISubprogram ? rpsProgramBindNodeSubprogram(hRpslEntry, "DrawGUI", m_drawGUISubprogram)
                                   : rpsProgramBindNode(hRpslEntry, "DrawGUI", &TestVkSubprogram::DrawGUI, this);
        REQUIRE(result == RPS_OK);
    }

    void CreateScene(const RpsCmdCallbackContext* pContext)
    {
        if (!m_pipelineStateDrawScene)
        {
            RpsCmdRenderTargetInfo rtInfo;
            RpsResult              result = rpsCmdGetRenderTargetsInfo(pContext, &rtInfo);
            REQUIRE(result == RPS_OK);

            VkRenderPass rp;
            result = rpsVKGetCmdRenderPass(pContext, &rp);
            REQUIRE(result == RPS_OK);

            CreatePSO(L"VS",
                      nullptr,
                      L"PSScene",
                      rtInfo.numRenderTargets,
                      false,
                      false,
                      rtInfo.numSamples,
                      rp,
                      &m_pipelineStateDrawScene);
        }
    }

    void DrawScene(const RpsCmdCallbackContext* pContext,
                   rps::UnusedArg               rt,
                   const float                  color[4],
                   const RpsViewport&           viewport)
    {
        CreateScene(pContext);

        VkCommandBuffer cmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineStateDrawScene);
        vkCmdPushConstants(cmdBuf, m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float) * 4, color);
        vkCmdDraw(cmdBuf, 3, 1, 0, 0);
    }

    void CreateGUI(const RpsCmdCallbackContext* pContext)
    {
        if (!m_pipelineStateDrawGUI)
        {
            RpsCmdRenderTargetInfo rtInfo;
            RpsResult              result = rpsCmdGetRenderTargetsInfo(pContext, &rtInfo);
            REQUIRE(result == RPS_OK);

            VkRenderPass rp;
            result = rpsVKGetCmdRenderPass(pContext, &rp);
            REQUIRE(result == RPS_OK);

            CreatePSO(L"VS", nullptr, L"PSGUI", 1, false, true, rtInfo.numSamples, rp, &m_pipelineStateDrawGUI);
        }
    }

    void DrawGUI(const RpsCmdCallbackContext* pContext,
                 rps::UnusedArg               rt,
                 const RpsViewport&           viewport,
                 const float                  color[4])
    {
        CreateGUI(pContext);

        VkCommandBuffer cmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineStateDrawGUI);
        vkCmdPushConstants(cmdBuf, m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float) * 4, color);
        vkCmdDraw(cmdBuf, 3, 1, 0, 0);
    }

private:
    void LoadAssets()
    {
        OnPostResize();

        VkPushConstantRange pushConstRanges[1] = {};
        pushConstRanges[0].offset              = 0;
        pushConstRanges[0].size                = 4 * 4;
        pushConstRanges[0].stageFlags          = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkPipelineLayoutCreateInfo plCI = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        plCI.setLayoutCount             = 0;
        plCI.pSetLayouts                = nullptr;
        plCI.pPushConstantRanges        = pushConstRanges;
        plCI.pushConstantRangeCount     = _countof(pushConstRanges);

        ThrowIfFailedVK(vkCreatePipelineLayout(m_device, &plCI, nullptr, &m_pipelineLayout));
    }

    void CreatePSO(const WCHAR* vsEntry,
                   const WCHAR* gsEntry,
                   const WCHAR* psEntry,
                   uint32_t     numColorAttachments,
                   bool         bDepth,
                   bool         bAlphaBlend,
                   uint32_t     sampleCount,
                   VkRenderPass renderPass,
                   VkPipeline*  pPso)
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

        bs[0].blendEnable         = bAlphaBlend ? VK_TRUE : VK_FALSE;
        bs[0].srcColorBlendFactor = bAlphaBlend ? VK_BLEND_FACTOR_SRC_ALPHA : VK_BLEND_FACTOR_ONE;
        bs[0].dstColorBlendFactor = bAlphaBlend ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA : VK_BLEND_FACTOR_ZERO;
        bs[0].colorBlendOp        = VK_BLEND_OP_ADD;
        bs[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        bs[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        bs[0].alphaBlendOp        = VK_BLEND_OP_ADD;
        bs[0].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

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
        ms.rasterizationSamples  = static_cast<VkSampleCountFlagBits>(sampleCount);
        ms.sampleShadingEnable   = (sampleCount > 1) ? VK_TRUE : VK_FALSE;
        ms.alphaToCoverageEnable = VK_FALSE;
        ms.alphaToOneEnable      = VK_FALSE;
        ms.minSampleShading      = (sampleCount > 1) ? 1.0f : 0.0f;

        VkShaderModule       vsModule = VK_NULL_HANDLE, gsModule = VK_NULL_HANDLE, psModule = VK_NULL_HANDLE;
        std::vector<uint8_t> vsCode, gsCode, psCode;

        const DxcDefine defs[] = {{L"VULKAN", L"1"}};

        DxcCompileToSpirv(c_Shader, vsEntry, L"vs_6_0", L"", defs, _countof(defs), vsCode);
        DxcCompileToSpirv(c_Shader, psEntry, L"ps_6_0", L"", defs, _countof(defs), psCode);

        if (gsEntry)
        {
            DxcCompileToSpirv(c_Shader, gsEntry, L"gs_6_0", L"", defs, _countof(defs), gsCode);
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

        if (gsEntry)
        {
            smCI.pCode    = reinterpret_cast<const uint32_t*>(gsCode.data());
            smCI.codeSize = gsCode.size();

            ThrowIfFailedVK(vkCreateShaderModule(m_device, &smCI, nullptr, &gsModule));

            char gsName[128];
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
        psoCI.pDepthStencilState           = &ds;
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

    void CreateComputePSO(const WCHAR* csEntry, VkPipeline* pPso)
    {
        const DxcDefine defs[] = {{L"VULKAN", L"1"}};

        std::vector<uint8_t> csCode;
        DxcCompileToSpirv(c_Shader, csEntry, L"cs_6_0", L"", defs, _countof(defs), csCode);

        VkShaderModuleCreateInfo smCI = {};
        smCI.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        smCI.pCode                    = reinterpret_cast<const uint32_t*>(csCode.data());
        smCI.codeSize                 = csCode.size();

        VkShaderModule csModule;
        ThrowIfFailedVK(vkCreateShaderModule(m_device, &smCI, nullptr, &csModule));

        char csName[128];
        sprintf_s(csName, "%S", csEntry);

        VkComputePipelineCreateInfo compPsoCI = {};
        compPsoCI.sType                       = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        compPsoCI.stage.sType                 = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        compPsoCI.stage.module                = csModule;
        compPsoCI.stage.pName                 = csName;
        compPsoCI.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
        compPsoCI.layout                      = m_pipelineLayout;

        ThrowIfFailedVK(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &compPsoCI, nullptr, pPso));
        vkDestroyShaderModule(m_device, csModule, nullptr);
    }

private:
    VkPipelineLayout m_pipelineLayout         = VK_NULL_HANDLE;
    VkPipeline       m_pipelineStateDrawScene = VK_NULL_HANDLE;
    VkPipeline       m_pipelineStateDrawGUI   = VK_NULL_HANDLE;
};

TEST_CASE(TEST_APP_NAME)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#if defined(BREAK_AT_ALLOC_ID)
    _CrtSetBreakAlloc(BREAK_AT_ALLOC_ID);
#endif

    TestVkSubprogram renderer;

    RpsTestRunWindowInfo runInfo = {};
    runInfo.title                = TEXT(TEST_APP_NAME);
    runInfo.numFramesToRender    = g_exitAfterFrame;
    runInfo.width                = 1280;
    runInfo.height               = 720;
    runInfo.pRenderer            = &renderer;
    RpsTestRunWindowApp(&runInfo);
}
