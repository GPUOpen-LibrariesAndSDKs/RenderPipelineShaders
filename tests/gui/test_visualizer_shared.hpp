// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <stdarg.h>
#include <DirectXMath.h>

#include "rps/rps.h"

#include "rps_visualizer.h"

#include "utils/rps_test_common.h"
#include "core/rps_util.hpp"

RPS_DECLARE_RPSL_ENTRY(test_visualizer, main);

static const char c_Shader[] = R"(
#ifdef __hlsl_dx_compiler
[[vk::binding(0, 0)]]
#endif
cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 offset;
    float uvDistort;
    float aspectRatio;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float4 uv : TEXCOORD;
};

#ifdef __hlsl_dx_compiler
[[vk::binding(1, 0)]]
#endif
Texture2D g_texture : register(t0);
#ifdef __hlsl_dx_compiler
[[vk::binding(2, 0)]]
#endif
SamplerState g_sampler : register(s0);

PSInput VSMain(float4 position : POSITION, float4 color : COLOR, float4 uv : TEXCOORD)
{
    PSInput result;

    position.y *= aspectRatio;
    result.position = mul(offset, position);
    result.color = color;
    result.uv = uv;
    result.uv.z = uvDistort;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    input.uv.y += sin(input.uv.x * 10.f) * input.uv.z;
    return g_texture.Sample(g_sampler, input.uv.xy) * input.color;
}
)";

#define TEST_APP_NAME_RAW "TestVisualizer"

using namespace DirectX;

class TestRpsRenderVisualizer
{
public:
    struct SceneConstantBuffer
    {
        XMFLOAT4X4 offset;
        float      uvDistort;
        float      aspectRatio;
    };

    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT4 color;
        XMFLOAT2 uv;
    };

public:
    TestRpsRenderVisualizer()
        : m_translation(0.f)
        , m_rotation(0.f)
    {
        XMStoreFloat4x4(&m_quadConstantData.offset, XMMatrixIdentity());
        m_quadConstantData.uvDistort   = 0.1f;
        m_quadConstantData.aspectRatio = 1.0f;
    }

protected:
    RpsRenderGraph GetRpsRenderGraph() const
    {
        return m_rpsRenderGraph;
    }

    void OnInit()
    {
        if (s_UseNullRuntime)
        {
            CreateRpsNullRuntimeDevice(m_rpsDevice);
        }
        else
        {
            CreateRpsDevice(m_rpsDevice);
        }

        LoadRpsPipeline();
    }

    virtual void UpdateRpsPipeline(uint64_t               frameIndex,
                                   uint64_t               completedFrameIndex,
                                   const RpsResourceDesc& backBufferDesc,
                                   RpsRuntimeResource*    pBackBuffers)
    {
        if (m_rpsRenderGraph != RPS_NULL_HANDLE)
        {
            const RpsRuntimeResource* argResources[]           = {pBackBuffers};
            RpsConstant               argDataRenderToTexture[] = {&backBufferDesc, &m_useMSAA};
            RpsConstant               argDataGeneral[]         = {&backBufferDesc};

            RpsRenderGraphUpdateInfo updateInfo = {};
            updateInfo.frameIndex               = frameIndex;
            updateInfo.gpuCompletedFrameIndex   = completedFrameIndex;
            updateInfo.numArgs = uint32_t(s_UseRenderToTextureImplementation ? RPS_COUNTOF(argDataRenderToTexture)
                                                                             : RPS_COUNTOF(argDataGeneral));
            updateInfo.ppArgs  = s_UseRenderToTextureImplementation ? argDataRenderToTexture : argDataGeneral;
            updateInfo.ppArgResources = argResources;

            updateInfo.diagnosticFlags = RPS_DIAGNOSTIC_ENABLE_RUNTIME_DEBUG_NAMES;
            if (completedFrameIndex == RPS_GPU_COMPLETED_FRAME_INDEX_NONE)
            {
                updateInfo.diagnosticFlags = RPS_DIAGNOSTIC_ENABLE_ALL;
            }
            //Disable for null runtime
            REQUIRE_RPS_OK(rpsRenderGraphUpdate(m_rpsRenderGraph, &updateInfo));

            if (m_HasResized)
            {
                RpsVisualizerUpdateInfo updateInfo = {m_rpsRenderGraph};
                REQUIRE_RPS_OK(rpsVisualizerUpdate(m_rpsVisualizer, &updateInfo));
            }
        }
    }

    void OnCleanUp()
    {
        rpsVisualizerDestroy(m_rpsVisualizer);
        rpsRenderGraphDestroy(m_rpsRenderGraph);
        rpsTestUtilDestroyDevice(m_rpsDevice);
    }

    void OnUpdate(uint32_t frameIndex, uint32_t width, uint32_t height)
    {
        const float translationSpeed = 0.01f;
        const float offsetBounds     = 1.4f;

        m_translation += translationSpeed;
        if (m_translation > offsetBounds)
        {
            m_translation = -offsetBounds;
        }

        XMMATRIX transform = XMMatrixAffineTransformation2D(
            XMVectorSplatOne(), XMVectorZero(), m_rotation, XMVectorSet(m_translation, 0.f, 0.f, 0.f));

        XMStoreFloat4x4(&m_triangleAnimationData.offset, transform);
        m_triangleAnimationData.uvDistort   = 0.1f;
        m_triangleAnimationData.aspectRatio = static_cast<float>(width) / height;
    }

    void OnRender(uint64_t frameIndex, RpsRuntimeCommandBuffer cmdBuf, uint32_t cmdBegin, uint32_t cmdCount)
    {
        RpsRenderGraphRecordCommandInfo recordInfo = {};

        recordInfo.hCmdBuffer    = cmdBuf;
        recordInfo.pUserContext  = this;
        recordInfo.frameIndex    = frameIndex;
        recordInfo.cmdBeginIndex = cmdBegin;
        recordInfo.numCmds       = cmdCount;

        RpsResult result = rpsRenderGraphRecordCommands(m_rpsRenderGraph, &recordInfo);
        REQUIRE(result == RPS_OK);

        REQUIRE_RPS_OK(rpsVisualizerDrawImGui(m_rpsVisualizer));

        m_HasResized = false;
    }

protected:
    virtual void CreateRpsDevice(RpsDevice& rpsDeviceOut)                        = 0;
    virtual void CreateRpsNullRuntimeDevice(RpsDevice& rpsDeviceOut)             = 0;
    virtual void DrawTriangle(const RpsCmdCallbackContext* pContext, bool bMSAA) = 0;

private:
    static void DummyCb(const RpsCmdCallbackContext*)
    {
    }

    static void DrawTriangleCb(const RpsCmdCallbackContext* pContext)
    {
        auto       pThis = static_cast<TestRpsRenderVisualizer*>(pContext->pCmdCallbackContext);
        const bool bMSAA = *rpsCmdGetArg<bool, 1>(pContext);
        pThis->DrawTriangle(pContext, bMSAA);
    }

    void LoadRpsPipeline()
    {
        RpsRenderGraphCreateInfo renderGraphCreateInfo            = {};
        renderGraphCreateInfo.mainEntryCreateInfo.hRpslEntryPoint = rpsTestLoadRpslEntry(test_visualizer, main);

        RpsResult result = rpsRenderGraphCreate(m_rpsDevice, &renderGraphCreateInfo, &m_rpsRenderGraph);
        REQUIRE(result == RPS_OK);

        RpsSubprogram hRpslEntry = rpsRenderGraphGetMainEntry(m_rpsRenderGraph);

        if (s_UseRenderToTextureImplementation && !s_UseNullRuntime)
        {
            //Regular implementation
            result = rpsProgramBindNode(hRpslEntry, "Geometry", &DrawTriangleCb, this);
            REQUIRE(result == RPS_OK);

            result = rpsProgramBindNode(hRpslEntry, "GeometryMSAA", &DrawTriangleCb, this);
            REQUIRE(result == RPS_OK);
        }
        else
        {
            //Dummy Command for null runtime
            result = rpsProgramBindNode(hRpslEntry, nullptr, &DummyCb, this);
            REQUIRE(result == RPS_OK);
        }
        result = rpsVisualizerCreate(m_rpsDevice, nullptr, &m_rpsVisualizer);
        REQUIRE(result == RPS_OK);
    }

private:
    RpsDevice      m_rpsDevice      = RPS_NULL_HANDLE;
    RpsRenderGraph m_rpsRenderGraph = RPS_NULL_HANDLE;

    float m_translation = 0.0f;
    float m_rotation    = 0.0f;
    bool  m_useMSAA     = true;

protected:
    static constexpr bool s_UseNullRuntime                   = true;
    static constexpr bool s_UseRenderToTextureImplementation = false;
    static_assert(s_UseRenderToTextureImplementation || s_UseNullRuntime,
                  "If the RenderToTextureImplementation is not used, the NullRuntime is required.");

    RpsVisualizer m_rpsVisualizer = RPS_NULL_HANDLE;

    SceneConstantBuffer m_triangleAnimationData;
    SceneConstantBuffer m_quadConstantData;

    bool m_HasResized = true;
};
