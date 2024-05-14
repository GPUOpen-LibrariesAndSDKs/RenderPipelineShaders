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

#include "utils/rps_test_common.h"

RPS_DECLARE_RPSL_ENTRY(test_render_to_texture, render_to_texture);

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

#define TEST_APP_NAME_RAW "TestRenderToTexture"

using namespace DirectX;

class TestRpsRenderToTexture
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

    TestRpsRenderToTexture()
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

    void OnInit(RpsRenderGraphFlags rgFlags = RPS_RENDER_GRAPH_FLAG_NONE)
    {
        CreateRpsDevice(m_rpsDevice);
        LoadRpsPipeline(rgFlags);
    }

    virtual void UpdateRpsPipeline(uint64_t               frameIndex,
                                   uint64_t               completedFrameIndex,
                                   const RpsResourceDesc& backBufferDesc,
                                   RpsRuntimeResource*    pBackBuffers)
    {
        if (m_rpsRenderGraph != RPS_NULL_HANDLE)
        {
            const RpsRuntimeResource* argResources[] = {pBackBuffers};
            RpsConstant               argData[]      = {&backBufferDesc, &m_useMSAA};

            RpsRenderGraphUpdateInfo updateInfo = {};
            updateInfo.frameIndex               = frameIndex;
            updateInfo.gpuCompletedFrameIndex   = completedFrameIndex;
            updateInfo.numArgs                  = uint32_t(RPS_TEST_COUNTOF(argData));
            updateInfo.ppArgs                   = argData;
            updateInfo.ppArgResources           = argResources;

            updateInfo.diagnosticFlags = RPS_DIAGNOSTIC_ENABLE_RUNTIME_DEBUG_NAMES;
            if (completedFrameIndex == RPS_GPU_COMPLETED_FRAME_INDEX_NONE)
            {
                updateInfo.diagnosticFlags = RPS_DIAGNOSTIC_ENABLE_ALL;
            }

            REQUIRE_RPS_OK(rpsRenderGraphUpdate(m_rpsRenderGraph, &updateInfo));
        }
    }

    void OnCleanUp()
    {
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

        XMMATRIX transform = XMMatrixAffineTransformation2D(XMVectorSplatOne(), XMVectorZero(), m_rotation, XMVectorSet(m_translation, 0.f, 0.f, 0.f));

        XMStoreFloat4x4(&m_triangleAnimationData.offset, transform);
        m_triangleAnimationData.uvDistort = 0.1f;
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
    }

protected:
    virtual void CreateRpsDevice(RpsDevice& rpsDeviceOut) = 0;
    virtual void DrawTriangle(const RpsCmdCallbackContext* pContext, bool bMSAA) = 0;

private:
    static void DrawTriangleCb(const RpsCmdCallbackContext* pContext)
    {
        auto       pThis = static_cast<TestRpsRenderToTexture*>(pContext->pCmdCallbackContext);
        const bool bMSAA = *rpsCmdGetArg<bool, 1>(pContext);
        pThis->DrawTriangle(pContext, bMSAA);
    }

    void LoadRpsPipeline(RpsRenderGraphFlags rgFlags)
    {
        RpsRenderGraphCreateInfo renderGraphCreateInfo = {};
        renderGraphCreateInfo.mainEntryCreateInfo.hRpslEntryPoint =
            rpsTestLoadRpslEntry(test_render_to_texture, render_to_texture);
        renderGraphCreateInfo.renderGraphFlags = rgFlags;

        RpsResult result = rpsRenderGraphCreate(m_rpsDevice, &renderGraphCreateInfo, &m_rpsRenderGraph);
        REQUIRE(result == RPS_OK);

        auto hRpslEntry = rpsRenderGraphGetMainEntry(m_rpsRenderGraph);

        result = rpsProgramBindNode(hRpslEntry, "Geometry", &DrawTriangleCb, this);
        REQUIRE(result == RPS_OK);

        result = rpsProgramBindNode(hRpslEntry, "GeometryMSAA", &DrawTriangleCb, this);
        REQUIRE(result == RPS_OK);
    }

private:
    RpsDevice      m_rpsDevice      = RPS_NULL_HANDLE;
    RpsRenderGraph m_rpsRenderGraph = {};

    float m_translation = 0.0f;
    float m_rotation = 0.0f;
    bool  m_useMSAA = true;

protected:

    SceneConstantBuffer m_triangleAnimationData;
    SceneConstantBuffer m_quadConstantData;
};
