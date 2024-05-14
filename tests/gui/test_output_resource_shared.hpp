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

RPS_DECLARE_RPSL_ENTRY(test_output_resource, render_colored);
RPS_DECLARE_RPSL_ENTRY(test_output_resource, render_tinted);

static const char c_Shader[] = R"(
struct CBData
{
    float3 tint;
    float  aspectRatio;
    float  time;
};

#if VULKAN
[[vk::push_constant]] CBData cb;
#else
ConstantBuffer<CBData> cb : register(b0);
#endif

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

[[vk::binding(0, 0)]]
Texture2D g_texture : register(t0);

[[vk::binding(1, 0)]]
SamplerState g_sampler : register(s0);

PSInput VSTriangle(uint vId : SV_VertexID)
{
    PSInput result;

    const float2 verts[] = { { 0.0f, 2.0f }, { 1.732f, -1.0f }, { -1.732f, -1.0f } };
    const float3 colors[] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };

    float2x2 rotMat = { {cos(cb.time), -sin(cb.time)}, {sin(cb.time), cos(cb.time)} };
    float2 vertPos = mul(verts[vId], rotMat);
    vertPos = vertPos * float2(0.2f, 0.2f * cb.aspectRatio);

    result.position = float4(vertPos, 0, 1);
    result.color = float4(colors[vId], 1.0f);
    result.uv = float2(0, 0);

    return result;
}

float4 PSTriangle(PSInput input) : SV_Target0
{
    return input.color;
}

PSInput VSBlt(uint vId : SV_VertexID)
{
    PSInput result;

    float2 uv = float2(float((vId & 1) << 1), float(vId & 2));

    result.position = float4(uv.x * 2.0f - 1.0f, uv.y * -2.0f + 1.0f, 0, 1);
    result.color = float4(cb.tint, 1);
    result.uv = uv + float2(sin(cb.time) * 0.2f, 0);

    return result;
}

float4 PSBlt(PSInput input) : SV_Target0
{
    return g_texture.Sample(g_sampler, input.uv) * input.color;
}
)";

#define TEST_APP_NAME_RAW "TestOutputResource"

using namespace DirectX;

class TestRpsOutputResource
{
public:
    struct ConstantData
    {
        XMFLOAT3 tint;
        float    aspectRatio;
        float    time;
    };

    TestRpsOutputResource()
    {
    }

protected:
    RpsRenderGraph GetRpsRenderGraphColoredTriangle() const
    {
        return m_rpsRenderGraphColoredTriangle;
    }

    RpsRenderGraph GetRpsRenderGraphTintedQuads() const
    {
        return m_rpsRenderGraphTintedQuads;
    }

    void OnInit()
    {
        CreateRpsDevice(m_rpsDevice);
        LoadRpsPipeline();
    }

    virtual void UpdateRpsPipeline(uint64_t               frameIndex,
                                   uint64_t               completedFrameIndex,
                                   const RpsResourceDesc& backBufferDesc,
                                   RpsRuntimeResource*    pBackBuffers)
    {
        RpsRuntimeResourceInfo offscreenTextureInfo = {};

        if (m_rpsRenderGraphColoredTriangle != RPS_NULL_HANDLE)
        {
            const RpsRuntimeResource* argResources[] = {pBackBuffers};
            RpsConstant               argData[]      = {&backBufferDesc, nullptr};

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

            if (m_triangleActive)
            {
                REQUIRE_RPS_OK(rpsRenderGraphUpdate(m_rpsRenderGraphColoredTriangle, &updateInfo));
            }

            constexpr uint32_t OffscreenTextureParamId = 1;

            REQUIRE_RPS_OK(rpsRenderGraphGetOutputParameterResourceInfos(
                m_rpsRenderGraphColoredTriangle, OffscreenTextureParamId, 0, 1, &offscreenTextureInfo));
        }

        if (m_rpsRenderGraphTintedQuads != RPS_NULL_HANDLE)
        {
            const RpsRuntimeResource* argResources[] = {pBackBuffers, &offscreenTextureInfo.hResource};
            RpsConstant               argData[]      = {&backBufferDesc, &offscreenTextureInfo.resourceDesc};

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

            REQUIRE_RPS_OK(rpsRenderGraphUpdate(m_rpsRenderGraphTintedQuads, &updateInfo));
        }
    }

    void OnCleanUp()
    {
        rpsRenderGraphDestroy(m_rpsRenderGraphColoredTriangle);
        rpsRenderGraphDestroy(m_rpsRenderGraphTintedQuads);
        rpsTestUtilDestroyDevice(m_rpsDevice);
    }

    void OnUpdate(uint32_t frameIndex, uint32_t width, uint32_t height)
    {
        using namespace std::chrono;

        const auto currTime        = RpsAfxCpuTimer::Now();
        const auto currTimeInMilli = duration_cast<milliseconds>(currTime.time_since_epoch());

        // Pause triangle animation every half seconds.
        m_triangleActive = (frameIndex < 16) || (((currTimeInMilli.count() / 500) & 1) == 0);

        if (m_triangleActive)
        {
            m_triangleAnimationTime += (currTime.time_since_epoch() - m_lastUpdateTime.time_since_epoch());
        }

        m_lastUpdateTime = currTime;
    }

protected:
    virtual void CreateRpsDevice(RpsDevice& rpsDeviceOut) = 0;

private:
    void LoadRpsPipeline()
    {
        RpsRenderGraphCreateInfo renderGraphCreateInfo = {};
        renderGraphCreateInfo.mainEntryCreateInfo.hRpslEntryPoint =
            rpsTestLoadRpslEntry(test_output_resource, render_colored);

        RpsResult result = rpsRenderGraphCreate(m_rpsDevice, &renderGraphCreateInfo, &m_rpsRenderGraphColoredTriangle);
        REQUIRE(result == RPS_OK);

        renderGraphCreateInfo.mainEntryCreateInfo.hRpslEntryPoint =
            rpsTestLoadRpslEntry(test_output_resource, render_tinted);

        result = rpsRenderGraphCreate(m_rpsDevice, &renderGraphCreateInfo, &m_rpsRenderGraphTintedQuads);
        REQUIRE(result == RPS_OK);
    }

protected:
    bool m_triangleActive = true;

    std::chrono::steady_clock::duration m_triangleAnimationTime = {};
    RpsAfxCpuTimer::time_point          m_lastUpdateTime;

private:
    RpsDevice      m_rpsDevice                     = RPS_NULL_HANDLE;
    RpsRenderGraph m_rpsRenderGraphColoredTriangle = {};
    RpsRenderGraph m_rpsRenderGraphTintedQuads     = {};
};
