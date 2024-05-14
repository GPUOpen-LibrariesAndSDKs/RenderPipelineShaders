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

#include "test_built_in_nodes_shared.hpp"

#include "utils/rps_test_win32.hpp"
#include "utils/rps_test_vk_renderer.hpp"

class TestVkBuiltInNodes : public RpsTestVulkanRenderer, public TestRpsBuiltInNodes
{
protected:
    virtual void OnInit(VkCommandBuffer initCmdBuf, InitTempResources& tempResources) override
    {
        LoadAssets();

        TestRpsBuiltInNodes::Init(rpsTestUtilCreateDevice(
            [this](auto pCreateInfo, auto phDevice) { return CreateRpsRuntimeDevice(*pCreateInfo, *phDevice); }));
    }

    virtual void OnPostResize() override
    {
    }

    virtual void OnCleanUp() override
    {
        TestRpsBuiltInNodes::OnDestroy();

        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        vkDestroyPipeline(m_device, m_psoFillUV, nullptr);
        vkDestroyPipeline(m_device, m_psoMSAAQuad, nullptr);
        vkDestroyPipeline(m_device, m_psoBlt, nullptr);
        vkDestroySampler(m_device, m_sampler, nullptr);
    }

    virtual void OnUpdate(uint32_t frameIndex) override
    {
        RpsResourceDesc backBufferDesc;
        auto&           swapChainBufferHdls = GetBackBuffers(backBufferDesc);

        RpsBool bTestMinMax = RPS_FALSE;

        RpsConstant               args[]         = {&backBufferDesc, &bTestMinMax};
        const RpsRuntimeResource* argResources[] = {swapChainBufferHdls.data()};

        uint64_t completedFrameIndex = CalcGuaranteedCompletedFrameIndexForRps();

        TestRpsBuiltInNodes::OnUpdate(
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
        TestRpsBuiltInNodes::BindNodes(hRpslEntry);

        RpsResult result =
            rpsProgramBindNode(hRpslEntry, "blt_to_swapchain", &TestVkBuiltInNodes::DrawBlt, this);
        REQUIRE(result == RPS_OK);

        result = rpsProgramBindNode(hRpslEntry, "fill_uv", &TestVkBuiltInNodes::DrawFillUV, this);
        REQUIRE(result == RPS_OK);

        result = rpsProgramBindNode(hRpslEntry, "msaa_quad", &TestVkBuiltInNodes::DrawMSAAQuad, this);
        REQUIRE(result == RPS_OK);
    }

    void CreateFillUV(const RpsCmdCallbackContext* pContext)
    {
        if (m_psoFillUV == VK_NULL_HANDLE)
        {
            CreateComputePSO(L"CSFillUV", &m_psoFillUV);
        }
    }

    void DrawFillUV(const RpsCmdCallbackContext* pContext, VkImageView dst, float cbData)
    {
        CreateFillUV(pContext);

        VkCommandBuffer cmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);

        VkWriteDescriptorSet writeDescriptorSet[1] = {};

        VkDescriptorSet ds;
        ThrowIfFailedVK(AllocFrameDescriptorSet(&m_descriptorSetLayout, 1, &ds));

        VkDescriptorImageInfo imageInfo = {};
        imageInfo.sampler               = VK_NULL_HANDLE;
        imageInfo.imageView             = dst;
        imageInfo.imageLayout           = VK_IMAGE_LAYOUT_GENERAL;

        auto pViewInfo = static_cast<const RpsImageView*>(rpsCmdGetArg(pContext, 0));

        RpsResourceDesc resourceDesc = {};
        RpsResult       result       = rpsCmdGetArgResourceDesc(pContext, 0, &resourceDesc);
        REQUIRE(result == RPS_OK);

        uint32_t w = std::max(1u, resourceDesc.image.width >> pViewInfo->subresourceRange.baseMipLevel);
        uint32_t h = std::max(1u, resourceDesc.image.height >> pViewInfo->subresourceRange.baseMipLevel);

        AppendWriteDescriptorSetImages(&writeDescriptorSet[0], ds, 2, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageInfo);
        vkUpdateDescriptorSets(m_device, _countof(writeDescriptorSet), writeDescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &ds, 0, nullptr);
        vkCmdPushConstants(cmdBuf, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(cbData), &cbData);
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_psoFillUV);
        vkCmdDispatch(cmdBuf, (w + 7) / 8, (h + 7) / 8, 1);
    }

    void CreateMSAAQuad(const RpsCmdCallbackContext* pContext)
    {
        if (!m_psoMSAAQuad)
        {
            RpsCmdRenderTargetInfo rtInfo;
            RpsResult              result = rpsCmdGetRenderTargetsInfo(pContext, &rtInfo);
            REQUIRE(result == RPS_OK);

            VkRenderPass rp;
            result = rpsVKGetCmdRenderPass(pContext, &rp);
            REQUIRE(result == RPS_OK);

            CreatePSO(L"VSBlt", nullptr, L"PSColorSample", rtInfo.numRenderTargets, false, rtInfo.numSamples, rp, &m_psoMSAAQuad);
        }
    }

    void DrawMSAAQuad(const RpsCmdCallbackContext* pContext)
    {
        CreateMSAAQuad(pContext);

        VkCommandBuffer cmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_psoMSAAQuad);
        vkCmdDraw(cmdBuf, 3, 1, 0, 0);
    }

    void CreateBlt(const RpsCmdCallbackContext* pContext)
    {
        if (m_psoBlt == VK_NULL_HANDLE)
        {
            VkRenderPass rp;
            RpsResult    result = rpsVKGetCmdRenderPass(pContext, &rp);
            REQUIRE(result == RPS_OK);

            CreatePSO(L"VSBlt", nullptr, L"PSBlt", 1, false, 1, rp, &m_psoBlt);
        }
    }

    void DrawBlt(const RpsCmdCallbackContext* pContext,
                 rps::UnusedArg                 dst,
                 VkImageView                  src,
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

        VkWriteDescriptorSet writeDescriptorSet[1] = {};

        VkDescriptorImageInfo imageInfo = {VK_NULL_HANDLE, src, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        AppendWriteDescriptorSetImages(&writeDescriptorSet[0], ds, 1, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageInfo);

        vkUpdateDescriptorSets(m_device, _countof(writeDescriptorSet), writeDescriptorSet, 0, nullptr);

        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &ds, 0, nullptr);
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_psoBlt);
        vkCmdDraw(cmdBuf, 3, 1, 0, 0);
    }

private:
    void LoadAssets()
    {
        OnPostResize();

        VkSamplerCreateInfo samplerCI = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerCI.magFilter           = VK_FILTER_NEAREST;
        samplerCI.minFilter           = VK_FILTER_NEAREST;
        samplerCI.mipmapMode          = VK_SAMPLER_MIPMAP_MODE_NEAREST;
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

        VkDescriptorSetLayoutBinding sharedLayoutBindings[3] = {};
        sharedLayoutBindings[0].binding                      = 1;
        sharedLayoutBindings[0].descriptorCount              = 1;
        sharedLayoutBindings[0].descriptorType               = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        sharedLayoutBindings[0].stageFlags                   = VK_SHADER_STAGE_FRAGMENT_BIT;
        sharedLayoutBindings[1].binding                      = 2;
        sharedLayoutBindings[1].descriptorCount              = 1;
        sharedLayoutBindings[1].descriptorType               = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        sharedLayoutBindings[1].stageFlags                   = VK_SHADER_STAGE_COMPUTE_BIT;
        sharedLayoutBindings[2].binding                      = 0;
        sharedLayoutBindings[2].descriptorCount              = 1;
        sharedLayoutBindings[2].descriptorType               = VK_DESCRIPTOR_TYPE_SAMPLER;
        sharedLayoutBindings[2].stageFlags                   = VK_SHADER_STAGE_FRAGMENT_BIT;
        sharedLayoutBindings[2].pImmutableSamplers           = &m_sampler;

        VkDescriptorSetLayoutCreateInfo setLayoutCI = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        setLayoutCI.pBindings                       = sharedLayoutBindings;
        setLayoutCI.bindingCount                    = _countof(sharedLayoutBindings);

        ThrowIfFailedVK(vkCreateDescriptorSetLayout(m_device, &setLayoutCI, nullptr, &m_descriptorSetLayout));

        VkPushConstantRange pushConstRanges[1] = {};
        pushConstRanges[0].offset              = 0;
        pushConstRanges[0].size                = 4;
        pushConstRanges[0].stageFlags          = VK_SHADER_STAGE_COMPUTE_BIT;

        VkPipelineLayoutCreateInfo plCI = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        plCI.setLayoutCount             = 1;
        plCI.pSetLayouts                = &m_descriptorSetLayout;
        plCI.pPushConstantRanges        = pushConstRanges;
        plCI.pushConstantRangeCount     = _countof(pushConstRanges);

        ThrowIfFailedVK(vkCreatePipelineLayout(m_device, &plCI, nullptr, &m_pipelineLayout));
    }

    void CreatePSO(
        const WCHAR* vsEntry, const WCHAR* gsEntry, const WCHAR* psEntry, uint32_t numColorAttachments, bool bDepth, uint32_t sampleCount, VkRenderPass renderPass, VkPipeline* pPso)
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
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout      m_pipelineLayout      = VK_NULL_HANDLE;
    VkPipeline            m_psoFillUV           = VK_NULL_HANDLE;
    VkPipeline            m_psoMSAAQuad         = VK_NULL_HANDLE;
    VkPipeline            m_psoBlt              = VK_NULL_HANDLE;
    VkSampler             m_sampler             = VK_NULL_HANDLE;
};

TEST_CASE(TEST_APP_NAME)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#if defined(BREAK_AT_ALLOC_ID)
    _CrtSetBreakAlloc(BREAK_AT_ALLOC_ID);
#endif

    TestVkBuiltInNodes renderer;

    RpsTestRunWindowInfo runInfo = {};
    runInfo.title                = TEXT(TEST_APP_NAME);
    runInfo.numFramesToRender    = g_exitAfterFrame;
    runInfo.width                = 1280;
    runInfo.height               = 720;
    runInfo.pRenderer            = &renderer;
    RpsTestRunWindowApp(&runInfo);
}
