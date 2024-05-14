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

#include "test_downsample_shared.hpp"
#include "utils/rps_test_win32.hpp"
#include "utils/rps_test_vk_renderer.hpp"

class TestVkDownsample : public RpsTestVulkanRenderer, public TestRpsDownsample
{
protected:
    virtual void OnInit(VkCommandBuffer initCmdList, InitTempResources& tempResources) override
    {
        LoadAssets(initCmdList, tempResources);

        TestRpsDownsample::OnInit(rpsTestUtilCreateDevice(
            [this](auto pCreateInfo, auto phDevice) { return CreateRpsRuntimeDevice(*pCreateInfo, *phDevice); }));
    }

    virtual void BindNodes(RpsSubprogram hRpslEntry) override
    {
        RpsResult result =
            rpsProgramBindNode(rpsRenderGraphGetMainEntry(GetRpsRenderGraph()), "Quads", &DrawQuadsCb, this);
        REQUIRE(result == RPS_OK);

        result = rpsProgramBindNode(rpsRenderGraphGetMainEntry(GetRpsRenderGraph()), "Geo", &DrawGeoCb, this);
        REQUIRE(result == RPS_OK);

        result =
            rpsProgramBindNode(rpsRenderGraphGetMainEntry(GetRpsRenderGraph()), "Downsample", &DrawDownsampleCb, this);
        REQUIRE(result == RPS_OK);

        result = rpsProgramBindNode(
            rpsRenderGraphGetMainEntry(GetRpsRenderGraph()), "DownsampleCompute", &ComputeDownsampleCb, this);
        REQUIRE(result == RPS_OK);
    }

    virtual void OnCleanUp() override
    {
        TestRpsDownsample::OnDestroy();

        vkDestroyImageView(m_device, m_checkerTextureView, nullptr);
        vkDestroyImage(m_device, m_checkerTexture, nullptr);
        vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
        vkDestroyPipeline(m_device, m_psoCompute, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
        vkDestroySampler(m_device, m_sampler, nullptr);

        if (m_psoDefault != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(m_device, m_psoDefault, nullptr);
        }

        if (m_psoGfxDownsample != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(m_device, m_psoGfxDownsample, nullptr);
        }
    }

    virtual void OnUpdate(uint32_t frameIndex) override
    {
        TestRpsDownsample::OnUpdate(frameIndex, m_width, m_height);
        UpdatePipeline(frameIndex, CalcGuaranteedCompletedFrameIndexForRps());
    }

    virtual void OnRender(uint32_t frameIndex) override
    {
        ExecuteRenderGraph(frameIndex, GetRpsRenderGraph());
    }

private:
    void CreateDefaultPso(const RpsCmdCallbackContext* pContext)
    {
        if (!m_psoDefault)
        {
            VkRenderPass rp;
            auto         result = rpsVKGetCmdRenderPass(pContext, &rp);
            REQUIRE(result == RPS_OK);
            CreateGraphicsPipeline(c_DefaultShader, L"VSMain", L"PSMain", rp, &m_psoDefault);
        }
    }

    void CreateGfxDownsamplePso(const RpsCmdCallbackContext* pContext)
    {
        if (!m_psoGfxDownsample)
        {
            VkRenderPass rp;
            auto         result = rpsVKGetCmdRenderPass(pContext, &rp);
            REQUIRE(result == RPS_OK);
            CreateGraphicsPipeline(c_DownsampleShader, L"VSMain", L"PSMain", rp, &m_psoGfxDownsample);
        }
    }

    void DrawGeo(const RpsCmdCallbackContext* pContext)
    {
        CreateDefaultPso(pContext);

        VkCommandBuffer cmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);

        RpsVariable cmdArg        = rpsCmdGetArg(pContext, 1);
        uint32_t    triangleIndex = *static_cast<uint32_t*>(cmdArg);

        uint32_t triangleDataIndex = triangleIndex % _countof(m_TriangleCbData);

        VkDescriptorSet ds;
        ThrowIfFailedVK(AllocFrameDescriptorSet(&m_descriptorSetLayout, 1, &ds));

        VkWriteDescriptorSet writeDescriptorSet[2] = {};

        VkDescriptorBufferInfo bufInfo =
            AllocAndWriteFrameConstants(&m_TriangleCbData[triangleDataIndex], sizeof(GeoConstantBuffer));
        AppendWriteDescriptorSetBuffers(&writeDescriptorSet[0], ds, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufInfo);

        VkDescriptorImageInfo imageInfo = {
            VK_NULL_HANDLE, m_checkerTextureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        AppendWriteDescriptorSetImages(&writeDescriptorSet[1], ds, 1, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageInfo);

        vkUpdateDescriptorSets(m_device, _countof(writeDescriptorSet), writeDescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &ds, 0, nullptr);

        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_psoDefault);
        vkCmdBindVertexBuffers(
            cmdBuf, 0 /* firstBinding */, 1 /* bindingCount */, &m_vertexBuffer, &m_trisBufferOffset);

        vkCmdDraw(cmdBuf, 3 * (triangleDataIndex + 1), 1, 0, 0);
    }

    void DrawQuads(const RpsCmdCallbackContext* pContext)
    {
        CreateDefaultPso(pContext);

        VkCommandBuffer cmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);

        VkImageView texView;
        RpsResult   result = rpsVKGetCmdArgImageView(pContext, 1, &texView);
        REQUIRE(result == RPS_OK);

        RpsVariable cmdArg    = rpsCmdGetArg(pContext, 2);
        uint32_t    quadIndex = *static_cast<uint32_t*>(cmdArg);

        // quad transformation matrix.
        GeoConstantBuffer data;
        XMStoreFloat4x4(&data.offset,
                        XMMatrixAffineTransformation2D(
                            XMVectorSet(m_quadScale[0], m_quadScale[1], 1.0f, 1.0f),
                            XMVectorZero(),
                            0.0f,
                            XMVectorSet(m_quadOffsets[quadIndex][0], m_quadOffsets[quadIndex][1], 0.0f, 0.0f)));
        data.color       = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        data.aspectRatio = 1.0f;

        VkDescriptorSet ds;
        ThrowIfFailedVK(AllocFrameDescriptorSet(&m_descriptorSetLayout, 1, &ds));

        VkWriteDescriptorSet writeDescriptorSet[2] = {};

        VkDescriptorBufferInfo bufInfo = AllocAndWriteFrameConstants(&data, sizeof(data));
        AppendWriteDescriptorSetBuffers(&writeDescriptorSet[0], ds, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufInfo);

        VkDescriptorImageInfo imageInfo = {VK_NULL_HANDLE, texView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        AppendWriteDescriptorSetImages(&writeDescriptorSet[1], ds, 1, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageInfo);

        vkUpdateDescriptorSets(m_device, _countof(writeDescriptorSet), writeDescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &ds, 0, nullptr);

        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_psoDefault);
        vkCmdBindVertexBuffers(
            cmdBuf, 0 /* firstBinding */, 1 /* bindingCount */, &m_vertexBuffer, &m_quadsBufferOffset);

        vkCmdDraw(cmdBuf, 6, 1, 0, 0);
    }

    void DrawDownsample(const RpsCmdCallbackContext* pContext)
    {
        CreateGfxDownsamplePso(pContext);

        VkCommandBuffer cmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);

        VkImageView texView;
        RpsResult   result = rpsVKGetCmdArgImageView(pContext, 1, &texView);
        REQUIRE(result == RPS_OK);

        RpsVariable cmdArg  = rpsCmdGetArg(pContext, 2);
        XMFLOAT2    invSize = *static_cast<XMFLOAT2*>(cmdArg);

        VkDescriptorSet ds;
        ThrowIfFailedVK(AllocFrameDescriptorSet(&m_descriptorSetLayout, 1, &ds));

        VkWriteDescriptorSet writeDescriptorSet[2] = {};

        VkDescriptorBufferInfo bufInfo = AllocAndWriteFrameConstants(&invSize, sizeof(invSize));
        AppendWriteDescriptorSetBuffers(&writeDescriptorSet[0], ds, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufInfo);

        VkDescriptorImageInfo imageInfo = {VK_NULL_HANDLE, texView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        AppendWriteDescriptorSetImages(&writeDescriptorSet[1], ds, 1, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageInfo);

        vkUpdateDescriptorSets(m_device, _countof(writeDescriptorSet), writeDescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &ds, 0, nullptr);

        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_psoGfxDownsample);
        vkCmdBindVertexBuffers(
            cmdBuf, 0 /* firstBinding */, 1 /* bindingCount */, &m_vertexBuffer, &m_quadsBufferOffset);

        vkCmdDraw(cmdBuf, 6, 1, 0, 0);
    }

    void ComputeDownsample(const RpsCmdCallbackContext* pContext)
    {
        VkCommandBuffer cmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);

        VkImageView dstImage;
        VkImageView srcImage;

        RpsResult result = rpsVKGetCmdArgImageView(pContext, 0, &dstImage);
        REQUIRE(result == RPS_OK);

        result = rpsVKGetCmdArgImageView(pContext, 1, &srcImage);
        REQUIRE(result == RPS_OK);

        RpsVariable cmdArg  = rpsCmdGetArg(pContext, 2);
        XMFLOAT2    invSize = *static_cast<XMFLOAT2*>(cmdArg);

        cmdArg                 = rpsCmdGetArg(pContext, 3);
        XMUINT2 dispatchGroups = *static_cast<XMUINT2*>(cmdArg);

        VkDescriptorSet ds;
        ThrowIfFailedVK(AllocFrameDescriptorSet(&m_descriptorSetLayout, 1, &ds));

        VkWriteDescriptorSet writeDescriptorSet[3] = {};

        VkDescriptorImageInfo imageInfo = {VK_NULL_HANDLE, srcImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        AppendWriteDescriptorSetImages(&writeDescriptorSet[0], ds, 1, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageInfo);

        VkDescriptorImageInfo imageInfo2 = {VK_NULL_HANDLE, dstImage, VK_IMAGE_LAYOUT_GENERAL};
        AppendWriteDescriptorSetImages(&writeDescriptorSet[1], ds, 3, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageInfo2);

        VkDescriptorBufferInfo bufInfo = AllocAndWriteFrameConstants(&invSize, sizeof(invSize));
        AppendWriteDescriptorSetBuffers(&writeDescriptorSet[2], ds, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufInfo);

        vkUpdateDescriptorSets(m_device, _countof(writeDescriptorSet), writeDescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &ds, 0, nullptr);

        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_psoCompute);

        vkCmdDispatch(cmdBuf, dispatchGroups.x, dispatchGroups.y, 1);
    }

    static void DrawGeoCb(const RpsCmdCallbackContext* pContext)
    {
        static_cast<TestVkDownsample*>(pContext->pCmdCallbackContext)->DrawGeo(pContext);
    }

    static void DrawQuadsCb(const RpsCmdCallbackContext* pContext)
    {
        static_cast<TestVkDownsample*>(pContext->pCmdCallbackContext)->DrawQuads(pContext);
    }

    static void DrawDownsampleCb(const RpsCmdCallbackContext* pContext)
    {
        static_cast<TestVkDownsample*>(pContext->pCmdCallbackContext)->DrawDownsample(pContext);
    }

    static void ComputeDownsampleCb(const RpsCmdCallbackContext* pContext)
    {
        static_cast<TestVkDownsample*>(pContext->pCmdCallbackContext)->ComputeDownsample(pContext);
    }

    void CreateComputePipeline(const char* shader, const WCHAR* csEntry, VkPipeline* pPso)
    {
        const DxcDefine      defs[] = {{L"VULKAN", L"1"}};
        std::vector<uint8_t> csCode;
        DxcCompileToSpirv(shader, csEntry, L"cs_6_0", L"", defs, _countof(defs), csCode);

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

    void CreateGraphicsPipeline(
        const char* shader, const WCHAR* vsEntry, const WCHAR* psEntry, VkRenderPass renderPass, VkPipeline* pPso)
    {
        VkVertexInputBindingDescription vibd[1];
        vibd[0].binding   = 0;
        vibd[0].stride    = sizeof(Vertex);
        vibd[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription viad[3];
        viad[0].location = 0;  // POSITION
        viad[0].binding  = 0;
        viad[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
        viad[0].offset   = offsetof(Vertex, position);
        viad[1].location = 1;  // COLOR
        viad[1].binding  = 0;
        viad[1].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
        viad[1].offset   = offsetof(Vertex, color);
        viad[2].location = 2;  // TEXCOORD
        viad[2].binding  = 0;
        viad[2].format   = VK_FORMAT_R32G32_SFLOAT;
        viad[2].offset   = offsetof(Vertex, uv);

        // vertex input state
        VkPipelineVertexInputStateCreateInfo vi = {};
        vi.sType                                = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vi.vertexBindingDescriptionCount        = _countof(vibd);
        vi.pVertexBindingDescriptions           = vibd;
        vi.vertexAttributeDescriptionCount      = _countof(viad);
        vi.pVertexAttributeDescriptions         = viad;

        // input assembly state
        VkPipelineInputAssemblyStateCreateInfo ia = {};
        ia.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.topology                               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // rasterization state
        VkPipelineRasterizationStateCreateInfo rs = {};
        rs.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.polygonMode                            = VK_POLYGON_MODE_FILL;
        rs.cullMode                               = VK_CULL_MODE_BACK_BIT;
        rs.frontFace                              = VK_FRONT_FACE_CLOCKWISE;
        rs.lineWidth                              = 1.0f;

        // Color blend state
        VkPipelineColorBlendAttachmentState bs[1] = {};
        bs[0].blendEnable                         = VK_FALSE;
        bs[0].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo cb = {};
        cb.sType                               = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.attachmentCount                     = 1;
        cb.pAttachments                        = bs;

        // view port state
        VkPipelineViewportStateCreateInfo vp = {};
        vp.sType                             = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vp.viewportCount                     = 1;
        vp.scissorCount                      = 1;

        // Disable all depth testing
        VkPipelineDepthStencilStateCreateInfo ds = {};
        ds.sType                                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

        // multi sample state
        VkPipelineMultisampleStateCreateInfo ms = {};
        ms.sType                                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;

        // viewport and scissor will be dynamic state.
        std::vector<VkDynamicState>      dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState        = {};
        dynamicState.sType                                   = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.pNext                                   = NULL;
        dynamicState.pDynamicStates                          = dynamicStateEnables.data();
        dynamicState.dynamicStateCount                       = (uint32_t)dynamicStateEnables.size();

        // Load shaders
        VkShaderModule       vsModule = VK_NULL_HANDLE, psModule = VK_NULL_HANDLE;
        std::vector<uint8_t> vsCode, psCode;
        const DxcDefine      defs[] = {{L"VULKAN", L"1"}};
        DxcCompileToSpirv(shader, vsEntry, L"vs_6_0", L"", defs, _countof(defs), vsCode);
        DxcCompileToSpirv(shader, psEntry, L"ps_6_0", L"", defs, _countof(defs), psCode);

        VkShaderModuleCreateInfo smCI = {};
        smCI.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        smCI.pCode                    = reinterpret_cast<const uint32_t*>(vsCode.data());
        smCI.codeSize                 = vsCode.size();
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
        // End load shaders

        VkGraphicsPipelineCreateInfo psoCI = {};
        psoCI.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        psoCI.stageCount                   = numShaderStages;
        psoCI.pStages                      = shaderStages;
        psoCI.pVertexInputState            = &vi;
        psoCI.pInputAssemblyState          = &ia;
        psoCI.pRasterizationState          = &rs;
        psoCI.pColorBlendState             = &cb;
        psoCI.pMultisampleState            = &ms;
        psoCI.pViewportState               = &vp;
        psoCI.pDepthStencilState           = &ds;
        psoCI.pDynamicState                = &dynamicState;

        psoCI.renderPass = renderPass;
        psoCI.layout     = m_pipelineLayout;

        ThrowIfFailedVK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &psoCI, nullptr, pPso));

        vkDestroyShaderModule(m_device, vsModule, nullptr);
        vkDestroyShaderModule(m_device, psModule, nullptr);
    }

    void LoadAssets(VkCommandBuffer initCmdList, InitTempResources& tempResources)
    {
        VkSamplerCreateInfo samplerCI     = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerCI.magFilter               = VK_FILTER_LINEAR;  // VK_FILTER_NEAREST is same as D3D12 point filtering.
        samplerCI.minFilter               = VK_FILTER_LINEAR;
        samplerCI.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCI.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.mipLodBias              = 0.0f;
        samplerCI.anisotropyEnable        = VK_FALSE;
        samplerCI.compareOp               = VK_COMPARE_OP_NEVER;
        samplerCI.minLod                  = 0.0f;
        samplerCI.maxLod                  = FLT_MAX;
        samplerCI.unnormalizedCoordinates = VK_FALSE;  // UV in range [0,1]

        ThrowIfFailedVK(vkCreateSampler(m_device, &samplerCI, nullptr, &m_sampler));

        // Setup descriptor set layout.
        VkDescriptorSetLayoutBinding layoutBinding[4] = {};
        layoutBinding[0].binding                      = 2;
        layoutBinding[0].descriptorCount              = 1;
        layoutBinding[0].descriptorType               = VK_DESCRIPTOR_TYPE_SAMPLER;
        layoutBinding[0].stageFlags                   = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
        layoutBinding[0].pImmutableSamplers           = &m_sampler;
        layoutBinding[1].binding                      = 1;
        layoutBinding[1].descriptorCount              = 1;
        layoutBinding[1].descriptorType               = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        layoutBinding[1].stageFlags                   = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
        layoutBinding[2].binding                      = 3;
        layoutBinding[2].descriptorCount              = 1;
        layoutBinding[2].descriptorType               = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        layoutBinding[2].stageFlags                   = VK_SHADER_STAGE_COMPUTE_BIT;
        layoutBinding[3].binding                      = 0;
        layoutBinding[3].descriptorCount              = 1;
        layoutBinding[3].descriptorType               = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        layoutBinding[3].stageFlags =
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo setLayoutCI = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        setLayoutCI.pBindings                       = layoutBinding;
        setLayoutCI.bindingCount                    = _countof(layoutBinding);

        ThrowIfFailedVK(vkCreateDescriptorSetLayout(m_device, &setLayoutCI, nullptr, &m_descriptorSetLayout));

        VkPipelineLayoutCreateInfo pipelineCI = {};
        pipelineCI.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineCI.setLayoutCount             = 1;
        pipelineCI.pSetLayouts                = &m_descriptorSetLayout;

        ThrowIfFailedVK(vkCreatePipelineLayout(m_device, &pipelineCI, nullptr, &m_pipelineLayout));

        // create compute pipeline
        CreateComputePipeline(c_DownsampleShader, L"CSMain", &m_psoCompute);

        // Define the geometry for a triangle.
        Vertex triangleVertices[] = {
            // triangle
            {{0.0f, 0.25f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.5f, 0.0f}},
            {{0.25f, -0.25f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
            {{-0.25f, -0.25f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

            {{0.0f, 0.25f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.5f, 0.0f}},
            {{0.5f, 0.25f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
            {{0.25f, -0.25f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},

            {{0.5f, 0.25f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.5f, 0.0f}},
            {{0.75f, -0.25f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
            {{0.25f, -0.25f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},

            {{0.5f, 0.25f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.5f, 0.0f}},
            {{1.0f, 0.25f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
            {{0.75f, -0.25f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},

            // quad 0
            {{-1.0f, 1.0, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
            {{1.f, -1.f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
            {{-1.f, -1.f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
            {{-1.0f, 1.0, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
            {{1.f, 1.f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
            {{1.f, -1.f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
        };

        // setup offsets for when vkCmdBindVertexBuffers
        m_trisBufferOffset  = 0;
        m_quadsBufferOffset = sizeof(Vertex) * 12;

        const UINT vertexBufferSize = sizeof(triangleVertices);

        m_vertexBuffer = CreateAndBindStaticBuffer(
            vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

        auto         vbAlloc = AllocAndWriteFrameConstants(triangleVertices, vertexBufferSize);
        VkBufferCopy vbCopy;
        vbCopy.srcOffset = vbAlloc.offset;
        vbCopy.dstOffset = 0;
        vbCopy.size      = vertexBufferSize;
        vkCmdCopyBuffer(initCmdList, vbAlloc.buffer, m_vertexBuffer, 1, &vbCopy);

        // Create checkerboard texture
        float tintColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
        RpsTestVulkanRenderer::CreateStaticCheckerboardTexture(
            m_checkerTextureView, m_checkerTexture, initCmdList, tempResources, 256, 256, tintColor);
    }

    void UpdatePipeline(uint64_t frameIndex, uint64_t completedFrameIndex)
    {
        RpsResourceDesc                 backBufferDesc = {};
        std::vector<RpsRuntimeResource> backBuffers    = GetBackBuffers(backBufferDesc);
        TestRpsDownsample::UpdateRpsPipeline(frameIndex, completedFrameIndex, backBufferDesc, backBuffers.data());
    }

private:
    VkSampler             m_sampler             = VK_NULL_HANDLE;
    VkImage               m_checkerTexture      = VK_NULL_HANDLE;
    VkImageView           m_checkerTextureView  = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout      m_pipelineLayout      = VK_NULL_HANDLE;
    VkPipeline            m_psoDefault          = VK_NULL_HANDLE;
    VkPipeline            m_psoGfxDownsample    = VK_NULL_HANDLE;
    VkPipeline            m_psoCompute          = VK_NULL_HANDLE;
    VkBuffer              m_vertexBuffer        = VK_NULL_HANDLE;
    VkDeviceSize          m_trisBufferOffset    = 0;
    VkDeviceSize          m_quadsBufferOffset   = 0;
};

TEST_CASE(TEST_APP_NAME)
{
    TestVkDownsample renderer;

    RpsTestRunWindowInfo runInfo = {};
    runInfo.title                = TEXT(TEST_APP_NAME);
    runInfo.numFramesToRender    = g_exitAfterFrame;
    runInfo.width                = 1280;
    runInfo.height               = 720;
    runInfo.pRenderer            = &renderer;
    RpsTestRunWindowApp(&runInfo);
}