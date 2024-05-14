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

#include "test_output_resource_shared.hpp"

#include "utils/rps_test_common.h"
#include "utils/rps_test_win32.hpp"
#include "utils/rps_test_vk_renderer.hpp"

#include <DirectXMath.h>

class TestVkOutputResource : public RpsTestVulkanRenderer, public TestRpsOutputResource
{
protected:
    virtual void OnInit(VkCommandBuffer initCmdBuf, InitTempResources& tempResources) override
    {
        LoadAssets(initCmdBuf, tempResources);

        TestRpsOutputResource::OnInit();

        auto coloredTriangelEntry = rpsRenderGraphGetMainEntry(GetRpsRenderGraphColoredTriangle());

        RpsResult result =
            rpsProgramBindNode(coloredTriangelEntry, "DrawTriangle", &TestVkOutputResource::DrawTriangle, this);
        REQUIRE(result == RPS_OK);

        result = rpsProgramBindNode(coloredTriangelEntry, "Blt", &TestVkOutputResource::DrawBlt, this);
        REQUIRE(result == RPS_OK);

        auto tintedQuadEntry = rpsRenderGraphGetMainEntry(GetRpsRenderGraphTintedQuads());

        result = rpsProgramBindNode(tintedQuadEntry, "Blt", &TestVkOutputResource::DrawBlt, this);
        REQUIRE(result == RPS_OK);
    }

    virtual void OnCleanUp() override
    {
        TestRpsOutputResource::OnCleanUp();

        vkDestroyPipeline(m_device, m_psoBlt, nullptr);
        vkDestroyPipeline(m_device, m_psoDrawTriangle, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        vkDestroySampler(m_device, m_defaultSampler, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_sharedDescriptorSetLayout, nullptr);
    }

    virtual void OnUpdate(uint32_t frameIndex) override
    {
        TestRpsOutputResource::OnUpdate(frameIndex, m_width, m_height);
        UpdatePipeline(frameIndex, CalcGuaranteedCompletedFrameIndexForRps());
    }

    virtual void OnRender(uint32_t frameIndex) override
    {
        if (m_triangleActive)
        {
            REQUIRE(RPS_SUCCEEDED(ExecuteRenderGraph(frameIndex, GetRpsRenderGraphColoredTriangle(), true, false)));
        }

        REQUIRE(RPS_SUCCEEDED(ExecuteRenderGraph(frameIndex, GetRpsRenderGraphTintedQuads(), !m_triangleActive, true)));
    }

protected:
    virtual void CreateRpsDevice(RpsDevice& rpsDeviceOut) override final
    {
        rpsDeviceOut = rpsTestUtilCreateDevice(
            [this](auto pCreateInfo, auto phDevice) { return CreateRpsRuntimeDevice(*pCreateInfo, *phDevice); });
    }

    void DrawTriangle(const RpsCmdCallbackContext* pContext)
    {
        if (m_psoDrawTriangle == RPS_NULL_HANDLE)
        {
            VkRenderPass rp;
            RpsResult    result = rpsVKGetCmdRenderPass(pContext, &rp);
            REQUIRE(result == RPS_OK);

            CreatePipeline(c_Shader, rp, &m_psoDrawTriangle, L"VSTriangle", L"PSTriangle");
        }

        VkCommandBuffer cmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);

        ConstantData cbData = {
            {},
            float(m_width) / m_height,
            float(std::chrono::duration_cast<std::chrono::duration<double>>(m_triangleAnimationTime).count())};

        vkCmdPushConstants(cmdBuf, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(cbData), &cbData);
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_psoDrawTriangle);
        vkCmdDraw(cmdBuf, 3, 1, 0, 0);
    }

    void DrawBlt(const RpsCmdCallbackContext* pContext, const XMFLOAT3& tint, VkImageView srcImage)
    {
        if (m_psoBlt == RPS_NULL_HANDLE)
        {
            VkRenderPass rp;
            RpsResult    result = rpsVKGetCmdRenderPass(pContext, &rp);
            REQUIRE(result == RPS_OK);

            CreatePipeline(c_Shader, rp, &m_psoBlt, L"VSBlt", L"PSBlt");
        }

        VkCommandBuffer cmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);

        ConstantData cbData = {
            tint,
            float(m_width) / m_height,
            (tint.x + tint.y + tint.z) > 2.5f ? 0 : float(RpsAfxCpuTimer::SecondsSinceEpoch().count())};

        vkCmdPushConstants(cmdBuf, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(cbData), &cbData);

        VkDescriptorSet ds;
        ThrowIfFailedVK(AllocFrameDescriptorSet(&m_sharedDescriptorSetLayout, 1, &ds));

        VkWriteDescriptorSet  writeDescriptorSet[1] = {};
        VkDescriptorImageInfo imageInfo = {VK_NULL_HANDLE, srcImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        AppendWriteDescriptorSetImages(&writeDescriptorSet[0], ds, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageInfo);

        vkUpdateDescriptorSets(m_device, _countof(writeDescriptorSet), writeDescriptorSet, 0, nullptr);

        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &ds, 0, nullptr);
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_psoBlt);
        vkCmdDraw(cmdBuf, 3, 1, 0, 0);
    }

private:
    void LoadAssets(VkCommandBuffer initCmdBuf, InitTempResources& tempResources)
    {
        OnPostResize();

        VkSamplerCreateInfo sampCI = {};
        sampCI.sType               = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampCI.magFilter           = VK_FILTER_LINEAR;
        sampCI.minFilter           = VK_FILTER_LINEAR;
        sampCI.mipmapMode          = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampCI.addressModeU        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampCI.addressModeV        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampCI.addressModeW        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampCI.mipLodBias          = 0.0f;
        sampCI.compareOp           = VK_COMPARE_OP_NEVER;
        sampCI.minLod              = 0.0f;
        sampCI.maxLod              = FLT_MAX;
        sampCI.maxAnisotropy       = 1.0;
        sampCI.anisotropyEnable    = VK_FALSE;
        sampCI.borderColor         = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        ThrowIfFailedVK(vkCreateSampler(m_device, &sampCI, nullptr, &m_defaultSampler));

        VkDescriptorSetLayoutBinding sharedLayoutBindings[2] = {};
        sharedLayoutBindings[0].binding                      = 0;
        sharedLayoutBindings[0].descriptorCount              = 1;
        sharedLayoutBindings[0].descriptorType               = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        sharedLayoutBindings[0].stageFlags                   = VK_SHADER_STAGE_FRAGMENT_BIT;
        sharedLayoutBindings[1].binding                      = 1;
        sharedLayoutBindings[1].descriptorCount              = 1;
        sharedLayoutBindings[1].descriptorType               = VK_DESCRIPTOR_TYPE_SAMPLER;
        sharedLayoutBindings[1].stageFlags                   = VK_SHADER_STAGE_FRAGMENT_BIT;
        sharedLayoutBindings[1].pImmutableSamplers           = &m_defaultSampler;

        VkDescriptorSetLayoutCreateInfo setLayoutCI = {};
        setLayoutCI.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        setLayoutCI.pBindings                       = sharedLayoutBindings;
        setLayoutCI.bindingCount                    = _countof(sharedLayoutBindings);

        ThrowIfFailedVK(vkCreateDescriptorSetLayout(m_device, &setLayoutCI, nullptr, &m_sharedDescriptorSetLayout));

        VkPushConstantRange pushConstRanges[1] = {};
        pushConstRanges[0].offset              = 0;
        pushConstRanges[0].size                = sizeof(ConstantData);
        pushConstRanges[0].stageFlags          = VK_SHADER_STAGE_VERTEX_BIT;

        VkPipelineLayoutCreateInfo plCI = {};
        plCI.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plCI.setLayoutCount             = 1;
        plCI.pSetLayouts                = &m_sharedDescriptorSetLayout;
        plCI.pushConstantRangeCount     = 1;
        plCI.pPushConstantRanges        = pushConstRanges;

        ThrowIfFailedVK(vkCreatePipelineLayout(m_device, &plCI, nullptr, &m_pipelineLayout));
    }

    void CreatePipeline(
        const char* pShaderCode, VkRenderPass renderPass, VkPipeline* pPipeline, LPCWSTR vsEntry, LPCWSTR psEntry)
    {
        VkPipelineVertexInputStateCreateInfo vi = {};
        vi.sType                                = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

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

        DxcDefine defs[] = {{L"VULKAN", L"1"}};

        DxcCompileToSpirv(pShaderCode, vsEntry, L"vs_6_0", L"", defs, RPS_TEST_COUNTOF(defs), vsCode);
        DxcCompileToSpirv(pShaderCode, psEntry, L"ps_6_0", L"", defs, RPS_TEST_COUNTOF(defs), psCode);

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

        VkPipelineShaderStageCreateInfo shaderStages[2] = {};
        shaderStages[0].sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].module                          = vsModule;
        shaderStages[0].pName                           = vsName;
        shaderStages[0].stage                           = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[1].sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].module                          = psModule;
        shaderStages[1].pName                           = psName;
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

        TestRpsOutputResource::UpdateRpsPipeline(frameIndex, completedFrameIndex, backBufferDesc, backBuffers);
    }

private:
    VkPipeline m_psoDrawTriangle = VK_NULL_HANDLE;
    VkPipeline m_psoBlt          = VK_NULL_HANDLE;

    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

    VkSampler m_defaultSampler = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_sharedDescriptorSetLayout = VK_NULL_HANDLE;

    VkImage     m_checkerboardTexture     = RPS_NULL_HANDLE;
    VkImageView m_checkerboardTextureView = RPS_NULL_HANDLE;
    VkBuffer    m_vertexBuffer            = RPS_NULL_HANDLE;

    VkDeviceSize m_triangleVbOffset = 0;
    VkDeviceSize m_quadVbOffset     = 0;
};

TEST_CASE(TEST_APP_NAME)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#if defined(BREAK_AT_ALLOC_ID)
    _CrtSetBreakAlloc(BREAK_AT_ALLOC_ID);
#endif

    TestVkOutputResource renderer;

    RpsTestRunWindowInfo runInfo = {};
    runInfo.title                = TEXT(TEST_APP_NAME);
    runInfo.numFramesToRender    = g_exitAfterFrame;
    runInfo.width                = 1280;
    runInfo.height               = 720;
    runInfo.pRenderer            = &renderer;
    RpsTestRunWindowApp(&runInfo);
}
