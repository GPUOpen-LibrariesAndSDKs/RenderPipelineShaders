// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <stdarg.h>

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#else
#error "TODO"
#endif

#include "rps/rps.h"
#include "rps/runtime/vk/rps_vk_runtime.h"

#include "utils/rps_test_common.h"
#include "utils/rps_test_win32.hpp"
#include "utils/rps_test_vk_renderer.hpp"

RPS_DECLARE_RPSL_ENTRY(test_temporal, main);

static const char c_Shader[] = R"(
struct V2P
{
    float4 Pos : SV_Position;
    float4 Color : COLOR0;
};

[[vk::push_constant]]
struct
{
    float AspectRatio;
} cb;

#define PI (3.14159f)

V2P VSMain(uint vId : SV_VertexID)
{
    float2 pos[3] =
    {
        { -0.5f, -0.5f * tan(PI / 6), },
        {  0.0f,  0.5f / cos(PI / 6), },
        {  0.5f, -0.5f * tan(PI / 6), },
    };

    V2P vsOut;
    vsOut.Pos = float4(pos[min(vId, 2)], 0, 1);
    vsOut.Pos.x *= cb.AspectRatio;
    vsOut.Color = float4(vId == 0 ? 1 : 0, vId == 1 ? 1 : 0, vId == 2 ? 1 : 0, 1);
    return vsOut;
}

float4 PSMain(V2P psIn) : SV_Target0
{
    return psIn.Color;
}
)";

#define TEST_APP_NAME_RAW "TestTemporal"

class TestVkTemporal : public RpsTestVulkanRenderer
{
protected:
    virtual void OnInit(VkCommandBuffer initCmdBuf, InitTempResources& tempResources) override
    {
        LoadAssets();

        m_rpsDevice = rpsTestUtilCreateDevice(
            [this](auto pCreateInfo, auto phDevice) { return CreateRpsRuntimeDevice(*pCreateInfo, *phDevice); });

        LoadRpsPipeline();
    }

    virtual void OnCleanUp() override
    {
        rpsRenderGraphDestroy(m_rpsRenderGraph);

        rpsTestUtilDestroyDevice(m_rpsDevice);

        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);

        if (m_pso != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(m_device, m_pso, nullptr);
        }
    }

    virtual void OnUpdate(uint32_t frameIndex) override
    {
        UpdateRpsPipeline(frameIndex);
    }

    virtual void OnRender(uint32_t frameIndex) override
    {
        ExecuteRenderGraph(frameIndex, m_rpsRenderGraph);
    }

private:
    void DrawTriangle(VkCommandBuffer cmdBuf, VkPipeline pso)
    {
        FLOAT aspectRatio = m_height / static_cast<float>(m_width);
        vkCmdPushConstants(cmdBuf, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 4, &aspectRatio);
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pso);
        vkCmdDraw(cmdBuf, 3, 1, 0, 0);
    }

    void DrawTriangleCb(const RpsCmdCallbackContext* pContext)
    {
        if (m_pso == VK_NULL_HANDLE)
        {
            VkRenderPass renderPassFromRps = {};
            REQUIRE_RPS_OK(rpsVKGetCmdRenderPass(pContext, &renderPassFromRps));

            m_pso = CreateVkPipeline(renderPassFromRps);
        }

        DrawTriangle(rpsVKCommandBufferFromHandle(pContext->hCommandBuffer), m_pso);
    }

private:
    void LoadAssets()
    {
        VkPushConstantRange pushConstRange = {};
        pushConstRange.offset              = 0;
        pushConstRange.size                = 4;
        pushConstRange.stageFlags          = VK_SHADER_STAGE_VERTEX_BIT;

        VkPipelineLayoutCreateInfo plCI = {};
        plCI.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plCI.setLayoutCount             = 0;
        plCI.pushConstantRangeCount     = 1;
        plCI.pPushConstantRanges        = &pushConstRange;

        ThrowIfFailedVK(vkCreatePipelineLayout(m_device, &plCI, nullptr, &m_pipelineLayout));
    }

    VkPipeline CreateVkPipeline(VkRenderPass renderPass)
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

        DxcCompileToSpirv(c_Shader, L"VSMain", L"vs_6_0", L"", nullptr, 0, vsCode);
        DxcCompileToSpirv(c_Shader, L"PSMain", L"ps_6_0", L"", nullptr, 0, psCode);

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

        VkPipeline pso = {};
        ThrowIfFailedVK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &psoCI, nullptr, &pso));

        vkDestroyShaderModule(m_device, vsModule, nullptr);
        vkDestroyShaderModule(m_device, psModule, nullptr);

        return pso;
    }

    void LoadRpsPipeline()
    {
        RpsRenderGraphCreateInfo renderGraphInfo            = {};
        renderGraphInfo.mainEntryCreateInfo.hRpslEntryPoint = rpsTestLoadRpslEntry(test_temporal, main);

        REQUIRE_RPS_OK(rpsRenderGraphCreate(m_rpsDevice, &renderGraphInfo, &m_rpsRenderGraph));

        REQUIRE_RPS_OK(rpsProgramBindNode(
            rpsRenderGraphGetMainEntry(m_rpsRenderGraph), "Triangle", &TestVkTemporal::DrawTriangleCb, this));
    }

    void UpdateRpsPipeline(uint64_t frameIndex)
    {
        if (m_rpsRenderGraph != RPS_NULL_HANDLE)
        {
            RpsRuntimeResource backBufferResources[16] = {};

            for (uint32_t i = 0; i < m_swapChainImages.size(); i++)
            {
                backBufferResources[i] = {m_swapChainImages[i].image};
            }
            const RpsRuntimeResource* argResources[] = {backBufferResources};

            RpsResourceDesc backBufferDesc   = {};
            backBufferDesc.type              = RPS_RESOURCE_TYPE_IMAGE_2D;
            backBufferDesc.temporalLayers    = uint32_t(m_swapChainImages.size());
            backBufferDesc.image.arrayLayers = 1;
            backBufferDesc.image.mipLevels   = 1;
            backBufferDesc.image.format      = rpsFormatFromVK(m_swapChainFormat.format);
            backBufferDesc.image.width       = m_width;
            backBufferDesc.image.height      = m_height;
            backBufferDesc.image.sampleCount = 1;

            RpsConstant argData[] = {&backBufferDesc, &frameIndex};

            const uint64_t gpuCompletedFrameIndex = CalcGuaranteedCompletedFrameIndexForRps();

            RpsRenderGraphUpdateInfo updateInfo = {};
            updateInfo.frameIndex               = frameIndex;
            updateInfo.gpuCompletedFrameIndex   = gpuCompletedFrameIndex;
            updateInfo.numArgs                  = 2;
            updateInfo.ppArgs                   = argData;
            updateInfo.ppArgResources           = argResources;

            updateInfo.diagnosticFlags = RPS_DIAGNOSTIC_ENABLE_RUNTIME_DEBUG_NAMES;
            if (frameIndex < m_swapChainImages.size())
            {
                updateInfo.diagnosticFlags = RPS_DIAGNOSTIC_ENABLE_ALL;
            }

            REQUIRE_RPS_OK(rpsRenderGraphUpdate(m_rpsRenderGraph, &updateInfo));
        }
    }

private:
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline       m_pso            = VK_NULL_HANDLE;

    RpsDevice      m_rpsDevice      = {};
    RpsRenderGraph m_rpsRenderGraph = {};
};

TEST_CASE(TEST_APP_NAME)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#if defined(BREAK_AT_ALLOC_ID)
    _CrtSetBreakAlloc(BREAK_AT_ALLOC_ID);
#endif

    TestVkTemporal renderer;

    RpsTestRunWindowInfo runInfo = {};
    runInfo.title                = TEXT(TEST_APP_NAME);
    runInfo.numFramesToRender    = g_exitAfterFrame;
    runInfo.width                = 1280;
    runInfo.height               = 720;
    runInfo.pRenderer            = &renderer;
    RpsTestRunWindowApp(&runInfo);
}
