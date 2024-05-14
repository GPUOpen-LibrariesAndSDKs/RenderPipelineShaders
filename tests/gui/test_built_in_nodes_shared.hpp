// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#include "utils/rps_test_host.hpp"

RPS_DECLARE_RPSL_ENTRY(test_built_in_nodes, rps_main);

static const char c_Shader[] = R"(

struct V2P
{
    float4 Pos : SV_Position;
    float2 UV : TEXCOORD0;
};

struct CBData
{
    float data;
};

#ifndef VULKAN
ConstantBuffer<CBData> cb : register(b0);
#else
[[vk::push_constant]] CBData cb;
#endif

[[vk::binding(2, 0)]]
RWTexture2D<float4> g_rwTex : register(u0);

[numthreads(8, 8, 1)]
void CSFillUV(uint3 dtId : SV_DispatchThreadID)
{
    uint w, h;
    g_rwTex.GetDimensions(w, h);

    if(all(dtId.xy < uint2(w, h)))
    {
        float4 color = float4(dtId.xy / float2(w, h), (cb.data > 0.5f) ? (dtId.x & 1) : (dtId.y & 1), 1);
        g_rwTex[dtId.xy] = color;
    }
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

float4 PSColorSample(V2P psIn, uint sampId : SV_SampleIndex) : SV_Target0
{
    return float4(
        (sampId == 0) ? psIn.UV.xy : (1.0f.xx - psIn.UV.xy), 0, 1);
}
)";

#define TEST_APP_NAME_RAW "TestBuiltInNode"

using namespace DirectX;

class TestRpsBuiltInNodes : public RpsTestHost
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

    TestRpsBuiltInNodes()
    {
    }

protected:
    void Init(RpsDevice hRpsDevice)
    {
        RpsTestHost::OnInit(hRpsDevice, rpsTestLoadRpslEntry(test_built_in_nodes, rps_main));
    }

};
