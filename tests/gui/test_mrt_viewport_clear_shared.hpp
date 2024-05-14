// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#include "utils/rps_test_host.hpp"

RPS_DECLARE_RPSL_ENTRY(test_mrt_viewport_clear, rps_main);

static const char c_Shader[] = R"(
struct V2P
{
    float4 Pos : SV_Position;
    float2 UV : TEXCOORD0;
};

V2P VSSimple(uint vertexId : SV_VertexID)
{
    V2P result;

    // Cover top left of the viewport, leaning Z 0 to 1 from left to right.
    result.Pos = float4(
        (vertexId & 1) * 2.0f - 1.0f,
        (vertexId & 2) * -1.0f + 1.0f,
        (vertexId & 1) * 1.0f,
        1);
    result.UV = float2((vertexId & 1) * 1.0f, (vertexId & 2) * 0.5f);

    return result;
}

struct PSOutMrt5
{
    float4 RT0 : SV_Target0;
    float4 RT1 : SV_Target1;
    float4 RT2 : SV_Target2;
    float4 RT3 : SV_Target3;
    float4 RT4 : SV_Target4;
};

PSOutMrt5 PSMrt5(V2P psIn)
{
    PSOutMrt5 result;
    result.RT0 = float4(psIn.UV, 0, 1);
    result.RT1 = float4(psIn.UV, 1.0f / 4, 1);
    result.RT2 = float4(psIn.UV, 2.0f / 4, 1);
    result.RT3 = float4(psIn.UV, 3.0f / 4, 1);
    result.RT4 = float4(psIn.UV, 4.0f / 4, 1);
    return result;
}

struct PSOutMrt3
{
    float4 RT0 : SV_Target0;
    float4 RT1 : SV_Target1;
    float4 RT2 : SV_Target2;
};

PSOutMrt3 PSMrt3(V2P psIn)
{
    PSOutMrt3 result;
    result.RT0 = float4(psIn.UV.x, 0, psIn.UV.y, 1);
    result.RT1 = float4(psIn.UV.x, 1.0f / 2, psIn.UV.y, 1);
    result.RT2 = float4(psIn.UV.x, 2.0f / 2, psIn.UV.y, 1);
    return result;
}

struct GSInput
{
    uint vertexId : DUMMYVERTEXID;
};

GSInput VSRtArray(uint vertexId : SV_VertexID)
{
    GSInput vsOut;
    vsOut.vertexId = vertexId;
    return vsOut;
}

struct G2P
{
    float4 Pos : SV_Position;
    float2 UV : TEXCOORD0;
    uint RtIndex : SV_RenderTargetArrayIndex;
};

[maxvertexcount(6)]
void GSRtArray(triangle GSInput Input[3], inout TriangleStream<G2P> gsOutStream)
{
    G2P gsOutVert;

    for(uint32_t iRT = 0; iRT < 2; iRT++)
    {
        gsOutVert.RtIndex = iRT;

        gsOutVert.Pos = float4(-1, 1, 0, 1);
        gsOutVert.UV = float2(0, 0);
        gsOutStream.Append(gsOutVert);

        gsOutVert.Pos = float4( 1, 1, 0, 1);
        gsOutVert.UV = float2(1, 0);
        gsOutStream.Append(gsOutVert);

        gsOutVert.Pos = float4(-1,-1, 0, 1);
        gsOutVert.UV = float2(0, 1);
        gsOutStream.Append(gsOutVert);

        gsOutStream.RestartStrip();
    }
}

float4 PSRtArray(G2P psIn) : SV_Target0
{
    return float4(psIn.UV, psIn.RtIndex * 1.0f, 1.0f);
}

V2P VSBlt(uint vertexId : SV_VertexID)
{
    V2P result;
    result.Pos = float4(
        (vertexId & 1) * 4.0f - 1.0f,
        (vertexId & 2) * -2.0f + 1.0f,
        0, 1);
    result.UV = float2((vertexId & 1) * 2.0f, (vertexId & 2) * 1.0f);

    return result;
}

[[vk::binding(1, 0)]]
Texture2D g_tex : register(t0);

[[vk::binding(0, 0)]]
SamplerState g_sampler : register(s0);

float4 PSBlt(V2P psIn) : SV_Target0
{
    return g_tex.SampleLevel(g_sampler, psIn.UV, 0);
}

[maxvertexcount(18)]
void GSRtArrayToCube(triangle GSInput Input[3], inout TriangleStream<G2P> gsOutStream)
{
    G2P gsOutVert;

    for(uint32_t iRT = 0; iRT < 6; iRT++)
    {
        gsOutVert.RtIndex = iRT;

        gsOutVert.Pos = float4(-1, 1, 0, 1);
        gsOutVert.UV = float2(0, 0);
        gsOutStream.Append(gsOutVert);

        gsOutVert.Pos = float4( 3, 1, 0, 1);
        gsOutVert.UV = float2(2, 0);
        gsOutStream.Append(gsOutVert);

        gsOutVert.Pos = float4(-1,-3, 0, 1);
        gsOutVert.UV = float2(0, 2);
        gsOutStream.Append(gsOutVert);

        gsOutStream.RestartStrip();
    }
}

[[vk::binding(2, 0)]]
Texture2D g_texArr[12] : register(t0);

void PSRtArrayToCubeMRT(G2P psIn,
    out float4 rt0 : SV_Target0,
    out float4 rt1 : SV_Target1,
    out float4 rt2 : SV_Target2,
    out float4 rt3 : SV_Target3,
    out float4 rt4 : SV_Target4,
    out float4 rt5 : SV_Target5)
{
    float4 colorSrc0 = g_texArr[psIn.RtIndex].SampleLevel(g_sampler, psIn.UV, 0);
    float4 colorSrc1 = g_texArr[psIn.RtIndex + 6].SampleLevel(g_sampler, psIn.UV, 0);

    rt0 = lerp(colorSrc0, colorSrc1, 1 / 7.0f); // cube 0
    rt1 = lerp(colorSrc0, colorSrc1, 2 / 7.0f); // cube 1
    rt2 = lerp(colorSrc0, colorSrc1, 3 / 7.0f); // cube 2

    rt3 = lerp(colorSrc0, colorSrc1, 5 / 7.0f); // cube 3
    rt4 = lerp(colorSrc0, colorSrc1, 6 / 7.0f); // cube 4

    rt5 = lerp(colorSrc0, colorSrc1, 4 / 7.0f); // cube 5
}

static const float PI = 3.14159265f;

[[vk::binding(1, 0)]]
TextureCube<float4> g_cubeTex : register(t0);

float4 PSBltCube(V2P psIn) : SV_Target0
{
    float phi = psIn.UV.x * PI * 2;
    float theta = psIn.UV.y * PI;

    float sinTheta = sin(theta);

    float3 coord = float3(
        sinTheta * sin(phi),
        sinTheta * cos(phi),
        cos(theta));

    return g_cubeTex.SampleLevel(g_sampler, coord, 0);
}

[[vk::binding(1, 0)]]
Texture2D<float> g_DepthSrv : register(t0);

[[vk::binding(3, 0)]]
Texture2D<uint2> g_StencilSrv : register(t1);

struct CBData
{
    uint drawId;
    float flatDepth;
};

#if VULKAN
[[vk::push_constant]] CBData cb;
#else
ConstantBuffer<CBData> cb : register(b0);
#endif

V2P VSSimpleFlatDepth(uint vertexId : SV_VertexID)
{
    V2P result;

    // Cover top left of the viewport, leaning Z 0 to 1 from left to right.
    result.Pos = float4(
        (vertexId & 1) * 2.0f - 1.0f,
        (vertexId & 2) * -1.0f + 1.0f,
        cb.flatDepth,
        1);
    result.UV = float2((vertexId & 1) * 1.0f, (vertexId & 2) * 0.5f);

    return result;
}

float4 PSWriteDepthStencil(V2P psIn) : SV_Target0
{
    uint2 tile = (uint2)(psIn.Pos.xy) / 12;

    if ((cb.drawId == 0) == ((tile.x & 1u) != (tile.y & 1u)))
        discard;

    return float4(0, 1, 0, 0);
}

float4 PSReadDepthWriteStencil(V2P psIn, uint sampleIdx : SV_SampleIndex) : SV_Target0
{
    float fDepthSrvValue = g_DepthSrv.Load(int3(psIn.Pos.xy, 0));

    return float4(0, 0, fDepthSrvValue, 0);
}

float4 PSReadDepthStencil(V2P psIn, uint sampleIdx : SV_SampleIndex) : SV_Target0
{
    float fDepthSrvValue = g_DepthSrv.Load(int3(psIn.Pos.xy, 0));

#if VULKAN
#define STENCIL_COMPONENT r
#else
#define STENCIL_COMPONENT g
#endif

    uint uStencilValue = g_StencilSrv.Load(int3(psIn.Pos.xy, 0)).STENCIL_COMPONENT;

    return float4(uStencilValue / 2.0f, fDepthSrvValue, 0, 1);
}

)";

#define TEST_APP_NAME_RAW "TestMultipleRenderTargetClear"

using namespace DirectX;

class TestRpsMrtViewportClear : public RpsTestHost
{
public:
    struct ViewportData
    {
        XMFLOAT4 data;

        ViewportData()
        {
        }
        ViewportData(const ViewportData& r)
        {
            FAIL();
            data = r.data;
        }
        ViewportData& operator=(const ViewportData& r)
        {
            FAIL();
            data = r.data;
            return *this;
        }
    };

    TestRpsMrtViewportClear()
    {
    }

protected:
    void Init(RpsDevice hRpsDevice)
    {
        RpsTestHost::OnInit(hRpsDevice, rpsTestLoadRpslEntry(test_mrt_viewport_clear, rps_main));
    }

    virtual void BindNodes(RpsSubprogram hRpslEntry) override
    {
        RpsResult result =
            rpsProgramBindNode(hRpslEntry, "test_unordered_5_mrt_no_ds", &TestRpsMrtViewportClear::Draw5MrtNoDS, this);
        REQUIRE(result == RPS_OK);

        result = rpsProgramBindNode(hRpslEntry, "test_unordered_3_mrt_ds", &TestRpsMrtViewportClear::Draw3MrtDS, this);
        REQUIRE(result == RPS_OK);

        result = rpsProgramBindNode(hRpslEntry, "test_rt_array", &TestRpsMrtViewportClear::DrawRtArray, this);
        REQUIRE(result == RPS_OK);

        result = rpsProgramBindNode(hRpslEntry, "test_large_array", &TestRpsMrtViewportClear::DrawLargeArray, this);
        REQUIRE(result == RPS_OK);
    }

protected:
    virtual void Draw5MrtNoDS(const RpsCmdCallbackContext* pContext)     = 0;
    virtual void Draw3MrtDS(const RpsCmdCallbackContext* pContext)       = 0;
    virtual void DrawRtArray(const RpsCmdCallbackContext* pContext)      = 0;
    virtual void DrawMrtWithArray(const RpsCmdCallbackContext* pContext) = 0;
    virtual void DrawLargeArray(const RpsCmdCallbackContext* pContext)   = 0;
};
