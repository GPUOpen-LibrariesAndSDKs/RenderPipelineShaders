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

#include "test_multi_queue_shared.hpp"

#include "rps/runtime/vk/rps_vk_runtime.h"
#include "utils/rps_test_win32.hpp"
#include "utils/rps_test_vk_renderer.hpp"

class TestVKMultiQueue : public RpsTestVulkanRenderer, public TestRpsMultiQueue
{
    static constexpr uint32_t CONST_BINDING        = 0u;
    static constexpr uint32_t COMP_IMG_BINDING     = 1u;
    static constexpr uint32_t GFX_INSTANCE_BINDING = 1u;
    static constexpr uint32_t GFX_IMG_BINDING      = 2u;
    static constexpr uint32_t GFX_SAMPLER_BINDING  = 4u;

public:
    TestVKMultiQueue()
    {
    }

protected:
    virtual void OnInit(VkCommandBuffer initCmdList, InitTempResources& tempResources) override
    {
        LoadAssets();

        TestRpsMultiQueue::Init(rpsTestUtilCreateDevice(
            [this](auto pCreateInfo, auto phDevice) { return CreateRpsRuntimeDevice(*pCreateInfo, *phDevice); }));

        auto hMainEntry = rpsRenderGraphGetMainEntry(GetRpsRenderGraph());
        REQUIRE_RPS_OK(
            rpsProgramBindNode(hMainEntry, "UpdateInstanceData", &TestVKMultiQueue::UpdateInstanceData, this));
        REQUIRE_RPS_OK(rpsProgramBindNode(hMainEntry, "Procedural", &TestVKMultiQueue::Procedural, this));
        REQUIRE_RPS_OK(rpsProgramBindNode(hMainEntry, "GenMip", &TestVKMultiQueue::GenMip, this));
        REQUIRE_RPS_OK(rpsProgramBindNode(hMainEntry, "ShadowMap", &TestVKMultiQueue::ShadowMap, this));
        REQUIRE_RPS_OK(rpsProgramBindNode(hMainEntry, "ShadingPass", &TestVKMultiQueue::ShadingPass, this));
    }

    virtual void OnCleanUp() override
    {
        TestRpsMultiQueue::OnDestroy();

        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayoutComp, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayoutGfx, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipelineLayoutComp, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipelineLayoutGfx, nullptr);
        vkDestroyPipeline(m_device, m_pipelineStateProcedural, nullptr);
        vkDestroyPipeline(m_device, m_pipelineStateMipGen, nullptr);
        vkDestroyPipeline(m_device, m_pipelineStateShadowMap, nullptr);
        vkDestroyPipeline(m_device, m_pipelineStateShading, nullptr);
        vkDestroySampler(m_device, m_shadowMapSampler, nullptr);
        vkDestroySampler(m_device, m_proceduralImgSampler, nullptr);
    }

    virtual void OnUpdate(uint32_t frameIndex) override
    {
        Animate(XMUINT2(m_width, m_height));

        UpdatePipeline(frameIndex, CalcGuaranteedCompletedFrameIndexForRps());
    }

    virtual void OnRender(uint32_t frameIndex) override
    {
        ExecuteRenderGraph(frameIndex, GetRpsRenderGraph());
    }

private:
    void UpdateInstanceData(const RpsCmdCallbackContext*  pContext,
                            RpsVkDeviceMemoryRange uploadBuffer,
                            RpsVkDeviceMemoryRange constantBuffer)
    {
        void* pData = nullptr;
        REQUIRE(VK_SUCCESS ==
                vkMapMemory(m_device, uploadBuffer.hMemory, uploadBuffer.offset, uploadBuffer.size, 0, &pData));

        size_t sizeToCopy = std::min(uploadBuffer.size, m_instanceDataGpu.size() * sizeof(InstanceDataGPU));

        memcpy(pData, m_instanceDataGpu.data(), sizeToCopy);

        vkUnmapMemory(m_device, uploadBuffer.hMemory);

        REQUIRE(VK_SUCCESS ==
                vkMapMemory(m_device, constantBuffer.hMemory, constantBuffer.offset, constantBuffer.size, 0, &pData));

        memcpy(pData, &m_cbufferData, sizeof(m_cbufferData));

        vkUnmapMemory(m_device, constantBuffer.hMemory);
    }

    void Procedural(const RpsCmdCallbackContext* pContext,
                    VkImageView                  proceduralTextureUav,
                    VkBuffer                     constantBuffer,
                    const XMUINT2&               outputDim)
    {
        VkCommandBuffer hCmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);

        VkDescriptorSet ds;
        ThrowIfFailedVK(AllocFrameDescriptorSet(&m_descriptorSetLayoutComp, 1, &ds));

        VkWriteDescriptorSet writeDescriptorSet[2] = {};

        VkDescriptorBufferInfo constBufferInfo = {constantBuffer, 0, VK_WHOLE_SIZE};
        VkDescriptorImageInfo imageInfo = {VK_NULL_HANDLE, proceduralTextureUav, VK_IMAGE_LAYOUT_GENERAL};
        AppendWriteDescriptorSetBuffers(
            &writeDescriptorSet[0], ds, CONST_BINDING, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &constBufferInfo);
        AppendWriteDescriptorSetImages(
            &writeDescriptorSet[1], ds, COMP_IMG_BINDING, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageInfo);

        vkUpdateDescriptorSets(m_device, RPS_TEST_COUNTOF(writeDescriptorSet), writeDescriptorSet, 0, nullptr);

        vkCmdBindDescriptorSets(hCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayoutComp, 0, 1, &ds, 0, nullptr);
        vkCmdBindPipeline(hCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineStateProcedural);
        vkCmdDispatch(hCmdBuf, DivRoundUp(outputDim.x, 8), DivRoundUp(outputDim.y, 8), 1);
    }

    void GenMip(const RpsCmdCallbackContext* pContext, VkImageView outMip, VkImageView inMip, const XMUINT2& outputDim)
    {
        VkCommandBuffer hCmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);

        VkDescriptorSet ds;
        ThrowIfFailedVK(AllocFrameDescriptorSet(&m_descriptorSetLayoutComp, 1, &ds));

        VkDescriptorImageInfo imageInfos[2] = {
            {VK_NULL_HANDLE, outMip, VK_IMAGE_LAYOUT_GENERAL},
            {VK_NULL_HANDLE, inMip, VK_IMAGE_LAYOUT_GENERAL},
        };

        VkWriteDescriptorSet writeDescriptorSet[1] = {};
        AppendWriteDescriptorSetImages(
            &writeDescriptorSet[0], ds, COMP_IMG_BINDING, 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, imageInfos);

        vkUpdateDescriptorSets(m_device, RPS_TEST_COUNTOF(writeDescriptorSet), writeDescriptorSet, 0, nullptr);

        vkCmdBindDescriptorSets(hCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayoutComp, 0, 1, &ds, 0, nullptr);
        vkCmdBindPipeline(hCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineStateMipGen);
        vkCmdDispatch(hCmdBuf, DivRoundUp(outputDim.x, 8), DivRoundUp(outputDim.y, 8), 1);
    }

    void ShadowMap(const RpsCmdCallbackContext* pContext, VkBuffer instanceBuffer, VkBuffer constantBuffer)
    {
        if (!m_pipelineStateShadowMap)
        {
            RpsCmdRenderTargetInfo renderTargetInfo;
            REQUIRE_RPS_OK(rpsCmdGetRenderTargetsInfo(pContext, &renderTargetInfo));
            VkRenderPass hRenderPass = VK_NULL_HANDLE;
            REQUIRE_RPS_OK(rpsVKGetCmdRenderPass(pContext, &hRenderPass));

            CreateGfxPSO(L"VSShadow", nullptr, nullptr, &renderTargetInfo, m_pipelineLayoutGfx, hRenderPass, &m_pipelineStateShadowMap);
        }

        VkCommandBuffer hCmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);

        VkDescriptorSet ds;
        ThrowIfFailedVK(AllocFrameDescriptorSet(&m_descriptorSetLayoutGfx, 1, &ds));

        VkDescriptorBufferInfo constBufferInfo = {constantBuffer, 0, VK_WHOLE_SIZE};
        VkDescriptorBufferInfo instanceBufferInfo = {instanceBuffer, 0, VK_WHOLE_SIZE};


        VkWriteDescriptorSet writeDescriptorSet[2] = {};
        AppendWriteDescriptorSetBuffers(
            &writeDescriptorSet[0], ds, CONST_BINDING, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &constBufferInfo);
        AppendWriteDescriptorSetBuffers(
            &writeDescriptorSet[1], ds, GFX_INSTANCE_BINDING, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &instanceBufferInfo);

        vkUpdateDescriptorSets(m_device, RPS_TEST_COUNTOF(writeDescriptorSet), writeDescriptorSet, 0, nullptr);

        vkCmdBindDescriptorSets(hCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayoutGfx, 0, 1, &ds, 0, nullptr);
        vkCmdBindPipeline(hCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineStateShadowMap);
        vkCmdDraw(hCmdBuf, 36, uint32_t(m_instanceData.size()), 0, 0);
    }

    void ShadingPass(const RpsCmdCallbackContext* pContext,
                     rps::UnusedArg               colorBuffer,
                     rps::UnusedArg               depthBuffer,
                     VkBuffer                     instanceBuffer,
                     VkImageView                  shadowMap,
                     VkImageView                  proceduralTexture,
                     VkBuffer                     constantBuffer)
    {
        if (!m_pipelineStateShading)
        {
            RpsCmdRenderTargetInfo renderTargetInfo;
            REQUIRE_RPS_OK(rpsCmdGetRenderTargetsInfo(pContext, &renderTargetInfo));
            VkRenderPass hRenderPass = VK_NULL_HANDLE;
            REQUIRE_RPS_OK(rpsVKGetCmdRenderPass(pContext, &hRenderPass));
            CreateGfxPSO(L"VSShading",
                         L"PSShading",
                         nullptr,
                         &renderTargetInfo,
                         m_pipelineLayoutGfx,
                         hRenderPass,
                         &m_pipelineStateShading);
        }

        VkCommandBuffer hCmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);

        VkDescriptorSet ds;
        ThrowIfFailedVK(AllocFrameDescriptorSet(&m_descriptorSetLayoutGfx, 1, &ds));

        VkDescriptorBufferInfo constBufferInfo    = {constantBuffer, 0, VK_WHOLE_SIZE};
        VkDescriptorBufferInfo instanceBufferInfo = {instanceBuffer, 0, VK_WHOLE_SIZE};

        VkWriteDescriptorSet writeDescriptorSet[4] = {};
        AppendWriteDescriptorSetBuffers(
            &writeDescriptorSet[0], ds, CONST_BINDING, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &constBufferInfo);
        AppendWriteDescriptorSetBuffers(&writeDescriptorSet[1],
                                        ds,
                                        GFX_INSTANCE_BINDING,
                                        1,
                                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                        &instanceBufferInfo);

        VkDescriptorImageInfo shadowMapImageInfo = {
            VK_NULL_HANDLE, shadowMap, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo proceduralImageInfo = {
            VK_NULL_HANDLE, proceduralTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

        AppendWriteDescriptorSetImages(
            &writeDescriptorSet[2], ds, GFX_IMG_BINDING, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &shadowMapImageInfo);
        AppendWriteDescriptorSetImages(
            &writeDescriptorSet[3], ds, GFX_IMG_BINDING + 1, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &proceduralImageInfo);

        vkUpdateDescriptorSets(m_device, RPS_TEST_COUNTOF(writeDescriptorSet), writeDescriptorSet, 0, nullptr);

        vkCmdBindDescriptorSets(hCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayoutGfx, 0, 1, &ds, 0, nullptr);
        vkCmdBindPipeline(hCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineStateShading);
        vkCmdDraw(hCmdBuf, 36, uint32_t(m_instanceData.size()), 0, 0);
    }

private:
    void LoadAssets()
    {
        OnPostResize();

        CreatePsoLayouts();

        CreateComputePSO(L"CSProcedural", m_pipelineLayoutComp, &m_pipelineStateProcedural);
        CreateComputePSO(L"CSMipGen", m_pipelineLayoutComp, &m_pipelineStateMipGen);
    }

    void CreatePsoLayouts()
    {
        {
            VkSamplerCreateInfo samplerCI = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
            samplerCI.magFilter           = VK_FILTER_LINEAR;
            samplerCI.minFilter           = VK_FILTER_LINEAR;
            samplerCI.mipmapMode          = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            samplerCI.addressModeU        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCI.addressModeV        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCI.addressModeW        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCI.mipLodBias          = 0.0f;
            samplerCI.compareEnable       = VK_TRUE;
            samplerCI.compareOp           = VK_COMPARE_OP_LESS;
            samplerCI.minLod              = 0.0f;
            samplerCI.maxLod              = FLT_MAX;
            samplerCI.maxAnisotropy       = 1.0;
            samplerCI.anisotropyEnable    = VK_FALSE;
            samplerCI.borderColor         = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

            ThrowIfFailedVK(vkCreateSampler(m_device, &samplerCI, nullptr, &m_shadowMapSampler));

            samplerCI.magFilter        = VK_FILTER_LINEAR;
            samplerCI.minFilter        = VK_FILTER_LINEAR;
            samplerCI.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerCI.addressModeU     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerCI.addressModeV     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerCI.addressModeW     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerCI.mipLodBias       = 0.0f;
            samplerCI.compareEnable    = VK_FALSE;
            samplerCI.compareOp        = VK_COMPARE_OP_NEVER;
            samplerCI.minLod           = 0.0f;
            samplerCI.maxLod           = FLT_MAX;
            samplerCI.maxAnisotropy    = 1.0;
            samplerCI.anisotropyEnable = VK_FALSE;
            samplerCI.borderColor      = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

            ThrowIfFailedVK(vkCreateSampler(m_device, &samplerCI, nullptr, &m_proceduralImgSampler));

            VkSampler samplers[] = {m_shadowMapSampler, m_proceduralImgSampler};

            VkDescriptorSetLayoutBinding sharedLayoutBindings[6] = {};
            sharedLayoutBindings[0].binding                      = CONST_BINDING;
            sharedLayoutBindings[0].descriptorCount              = 1;
            sharedLayoutBindings[0].descriptorType               = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            sharedLayoutBindings[0].stageFlags                   = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            sharedLayoutBindings[1].binding                      = GFX_INSTANCE_BINDING;
            sharedLayoutBindings[1].descriptorCount              = 1;
            sharedLayoutBindings[1].descriptorType               = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            sharedLayoutBindings[1].stageFlags                   = VK_SHADER_STAGE_VERTEX_BIT;
            sharedLayoutBindings[2].binding                      = GFX_IMG_BINDING;
            sharedLayoutBindings[2].descriptorCount              = 1;
            sharedLayoutBindings[2].descriptorType               = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            sharedLayoutBindings[2].stageFlags                   = VK_SHADER_STAGE_FRAGMENT_BIT;
            sharedLayoutBindings[3].binding                      = GFX_IMG_BINDING + 1;
            sharedLayoutBindings[3].descriptorCount              = 1;
            sharedLayoutBindings[3].descriptorType               = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            sharedLayoutBindings[3].stageFlags                   = VK_SHADER_STAGE_FRAGMENT_BIT;
            sharedLayoutBindings[4].binding                      = GFX_SAMPLER_BINDING;
            sharedLayoutBindings[4].descriptorCount              = 1;
            sharedLayoutBindings[4].descriptorType               = VK_DESCRIPTOR_TYPE_SAMPLER;
            sharedLayoutBindings[4].stageFlags                   = VK_SHADER_STAGE_FRAGMENT_BIT;
            sharedLayoutBindings[4].pImmutableSamplers           = &m_shadowMapSampler;
            sharedLayoutBindings[5].binding                      = GFX_SAMPLER_BINDING + 1;
            sharedLayoutBindings[5].descriptorCount              = 1;
            sharedLayoutBindings[5].descriptorType               = VK_DESCRIPTOR_TYPE_SAMPLER;
            sharedLayoutBindings[5].stageFlags                   = VK_SHADER_STAGE_FRAGMENT_BIT;
            sharedLayoutBindings[5].pImmutableSamplers           = &m_proceduralImgSampler;

            VkDescriptorSetLayoutCreateInfo setLayoutCI = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            setLayoutCI.pBindings                       = sharedLayoutBindings;
            setLayoutCI.bindingCount                    = _countof(sharedLayoutBindings);

            ThrowIfFailedVK(vkCreateDescriptorSetLayout(m_device, &setLayoutCI, nullptr, &m_descriptorSetLayoutGfx));

            VkPipelineLayoutCreateInfo plCI = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            plCI.setLayoutCount             = 1;
            plCI.pSetLayouts                = &m_descriptorSetLayoutGfx;

            ThrowIfFailedVK(vkCreatePipelineLayout(m_device, &plCI, nullptr, &m_pipelineLayoutGfx));
        }

        {
            VkDescriptorSetLayoutBinding sharedLayoutBindings[3] = {};
            sharedLayoutBindings[0].binding                      = CONST_BINDING;
            sharedLayoutBindings[0].descriptorCount              = 1;
            sharedLayoutBindings[0].descriptorType               = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            sharedLayoutBindings[0].stageFlags                   = VK_SHADER_STAGE_COMPUTE_BIT;
            sharedLayoutBindings[1].binding                      = COMP_IMG_BINDING;
            sharedLayoutBindings[1].descriptorCount              = 1;
            sharedLayoutBindings[1].descriptorType               = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            sharedLayoutBindings[1].stageFlags                   = VK_SHADER_STAGE_COMPUTE_BIT;
            sharedLayoutBindings[2].binding                      = COMP_IMG_BINDING + 1;
            sharedLayoutBindings[2].descriptorCount              = 1;
            sharedLayoutBindings[2].descriptorType               = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            sharedLayoutBindings[2].stageFlags                   = VK_SHADER_STAGE_COMPUTE_BIT;

            VkDescriptorSetLayoutCreateInfo setLayoutCI = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            setLayoutCI.pBindings                       = sharedLayoutBindings;
            setLayoutCI.bindingCount                    = _countof(sharedLayoutBindings);

            ThrowIfFailedVK(vkCreateDescriptorSetLayout(m_device, &setLayoutCI, nullptr, &m_descriptorSetLayoutComp));

            VkPipelineLayoutCreateInfo plCI = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            plCI.setLayoutCount             = 1;
            plCI.pSetLayouts                = &m_descriptorSetLayoutComp;

            ThrowIfFailedVK(vkCreatePipelineLayout(m_device, &plCI, nullptr, &m_pipelineLayoutComp));
        }
    }

    void CreateComputePSO(LPCWSTR csEntry, VkPipelineLayout hPipelineLayout, VkPipeline* phPipeline)
    {
        const DxcDefine defs[] = {{L"VULKAN", L"1"}};

        std::vector<uint8_t> csCode;
        DxcCompileToSpirv(c_Shader, csEntry, L"cs_6_0", L"", defs, RPS_TEST_COUNTOF(defs), csCode);

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
        compPsoCI.layout                      = hPipelineLayout;

        ThrowIfFailedVK(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &compPsoCI, nullptr, phPipeline));
        vkDestroyShaderModule(m_device, csModule, nullptr);
    }

    void CreateGfxPSO(LPCWSTR                       vsEntry,
                      LPCWSTR                       psEntry,
                      LPCWSTR                       gsEntry,
                      const RpsCmdRenderTargetInfo* pRtInfo,
                      VkPipelineLayout              hPipelineLayout,
                      VkRenderPass                  hRenderPass,
                      VkPipeline*                   pPso)
    {
        VkPipelineVertexInputStateCreateInfo vi = {};
        vi.sType                                = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo ia = {};
        ia.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.topology                               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // rasterizer state
        VkPipelineRasterizationStateCreateInfo rs = {};
        rs.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.polygonMode                            = VK_POLYGON_MODE_FILL;
        rs.cullMode                               = VK_CULL_MODE_NONE;
        rs.frontFace                              = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rs.lineWidth                              = 1.0f;

        VkPipelineColorBlendAttachmentState bs[8] = {};
        bs[0].blendEnable                         = VK_FALSE;
        bs[0].srcColorBlendFactor                 = VK_BLEND_FACTOR_ONE;
        bs[0].dstColorBlendFactor                 = VK_BLEND_FACTOR_ZERO;
        bs[0].colorBlendOp                        = VK_BLEND_OP_ADD;
        bs[0].srcAlphaBlendFactor                 = VK_BLEND_FACTOR_ONE;
        bs[0].dstAlphaBlendFactor                 = VK_BLEND_FACTOR_ZERO;
        bs[0].alphaBlendOp                        = VK_BLEND_OP_ADD;
        bs[0].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        for (uint32_t i = 1; i < pRtInfo->numRenderTargets; i++)
        {
            bs[i] = bs[0];
        }

        // Color blend state
        VkPipelineColorBlendStateCreateInfo cb;
        cb.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.flags             = 0;
        cb.pNext             = NULL;
        cb.attachmentCount   = pRtInfo->numRenderTargets;
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

        const bool bDepth = (pRtInfo->depthStencilFormat != RPS_FORMAT_UNKNOWN);

        VkPipelineDepthStencilStateCreateInfo ds = {};
        ds.sType                                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        ds.depthTestEnable                       = bDepth ? VK_TRUE : VK_FALSE;
        ds.depthWriteEnable                      = bDepth ? VK_TRUE : VK_FALSE;
        ds.depthCompareOp                        = VK_COMPARE_OP_LESS_OR_EQUAL;
        ds.depthBoundsTestEnable                 = VK_FALSE;
        ds.stencilTestEnable                     = VK_FALSE;
        ds.back.failOp                           = VK_STENCIL_OP_KEEP;
        ds.back.passOp                           = VK_STENCIL_OP_KEEP;
        ds.back.compareOp                        = VK_COMPARE_OP_ALWAYS;
        ds.back.compareMask                      = 0;
        ds.back.reference                        = 0;
        ds.back.depthFailOp                      = VK_STENCIL_OP_KEEP;
        ds.back.writeMask                        = 0;
        ds.minDepthBounds                        = 0;
        ds.maxDepthBounds                        = 0;
        ds.stencilTestEnable                     = VK_FALSE;
        ds.front                                 = ds.back;

        // multi sample state

        VkPipelineMultisampleStateCreateInfo ms;
        ms.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.pNext                 = NULL;
        ms.flags                 = 0;
        ms.pSampleMask           = NULL;
        ms.rasterizationSamples  = static_cast<VkSampleCountFlagBits>(pRtInfo->numSamples);
        ms.sampleShadingEnable   = (pRtInfo->numSamples > 1) ? VK_TRUE : VK_FALSE;
        ms.alphaToCoverageEnable = VK_FALSE;
        ms.alphaToOneEnable      = VK_FALSE;
        ms.minSampleShading      = (pRtInfo->numSamples > 1) ? 1.0f : 0.0f;

        VkShaderModule       vsModule = VK_NULL_HANDLE, gsModule = VK_NULL_HANDLE, psModule = VK_NULL_HANDLE;
        std::vector<uint8_t> vsCode, gsCode, psCode;

        const DxcDefine defs[] = {{L"VULKAN", L"1"}};

        DxcCompileToSpirv(c_Shader, vsEntry, L"vs_6_0", L"", defs, _countof(defs), vsCode);

        if (psEntry)
        {
            DxcCompileToSpirv(c_Shader, psEntry, L"ps_6_0", L"", defs, _countof(defs), psCode);
        }

        if (gsEntry)
        {
            DxcCompileToSpirv(c_Shader, gsEntry, L"gs_6_0", L"", defs, _countof(defs), gsCode);
        }

        VkShaderModuleCreateInfo smCI = {};
        smCI.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

        smCI.pCode    = reinterpret_cast<const uint32_t*>(vsCode.data());
        smCI.codeSize = vsCode.size();

        ThrowIfFailedVK(vkCreateShaderModule(m_device, &smCI, nullptr, &vsModule));

        char vsName[128];
        sprintf_s(vsName, "%S", vsEntry);

        VkPipelineShaderStageCreateInfo shaderStages[3] = {};
        shaderStages[0].sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].module                          = vsModule;
        shaderStages[0].pName                           = vsName;
        shaderStages[0].stage                           = VK_SHADER_STAGE_VERTEX_BIT;

        uint32_t numShaderStages = 1;

        char psName[128];
        if (psEntry)
        {
            smCI.pCode    = reinterpret_cast<const uint32_t*>(psCode.data());
            smCI.codeSize = psCode.size();

            ThrowIfFailedVK(vkCreateShaderModule(m_device, &smCI, nullptr, &psModule));

            sprintf_s(psName, "%S", psEntry);

            shaderStages[numShaderStages].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStages[numShaderStages].module = psModule;
            shaderStages[numShaderStages].pName  = psName;
            shaderStages[numShaderStages].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;

            numShaderStages++;
        }

        char gsName[128];
        if (gsEntry)
        {
            smCI.pCode    = reinterpret_cast<const uint32_t*>(gsCode.data());
            smCI.codeSize = gsCode.size();

            ThrowIfFailedVK(vkCreateShaderModule(m_device, &smCI, nullptr, &gsModule));

            sprintf_s(gsName, "%S", gsEntry);

            shaderStages[numShaderStages].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStages[numShaderStages].module = gsModule;
            shaderStages[numShaderStages].pName  = gsName;
            shaderStages[numShaderStages].stage  = VK_SHADER_STAGE_GEOMETRY_BIT;

            numShaderStages++;
        }

        VkGraphicsPipelineCreateInfo psoCI = {};
        psoCI.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        psoCI.pNext                        = NULL;
        psoCI.layout                       = hPipelineLayout;
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
        psoCI.renderPass                   = hRenderPass;
        psoCI.subpass                      = 0;

        ThrowIfFailedVK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &psoCI, nullptr, pPso));

        vkDestroyShaderModule(m_device, vsModule, nullptr);
        vkDestroyShaderModule(m_device, psModule, nullptr);
        if (gsModule != RPS_NULL_HANDLE)
        {
            vkDestroyShaderModule(m_device, gsModule, nullptr);
        }
    }

    void UpdatePipeline(uint64_t frameIndex, uint64_t completedFrameIndex)
    {
        RpsResourceDesc                        backBufferDesc;
        const std::vector<RpsRuntimeResource>& backBuffers = GetBackBuffers(backBufferDesc);

        TestRpsMultiQueue::UpdateRpsPipeline(frameIndex, completedFrameIndex, backBufferDesc, backBuffers.data());
    }

private:
    VkDescriptorSetLayout m_descriptorSetLayoutComp = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayoutGfx  = VK_NULL_HANDLE;
    VkPipelineLayout      m_pipelineLayoutComp      = VK_NULL_HANDLE;
    VkPipelineLayout      m_pipelineLayoutGfx       = VK_NULL_HANDLE;
    VkPipeline            m_pipelineStateProcedural = VK_NULL_HANDLE;
    VkPipeline            m_pipelineStateMipGen     = VK_NULL_HANDLE;
    VkPipeline            m_pipelineStateShadowMap  = VK_NULL_HANDLE;
    VkPipeline            m_pipelineStateShading    = VK_NULL_HANDLE;
    VkSampler             m_shadowMapSampler        = VK_NULL_HANDLE;
    VkSampler             m_proceduralImgSampler    = VK_NULL_HANDLE;
};

TEST_CASE(TEST_APP_NAME)
{
#if _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    TestVKMultiQueue renderer;

    RpsTestRunWindowInfo runInfo = {};
    runInfo.title                = TEXT(TEST_APP_NAME);
    runInfo.numFramesToRender    = g_exitAfterFrame;
    runInfo.width                = 1280;
    runInfo.height               = 720;
    runInfo.pRenderer            = &renderer;
    RpsTestRunWindowApp(&runInfo);
}
