// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#include "utils/rps_test_host.hpp"

RPS_DECLARE_RPSL_ENTRY(test_downsample, downsample);

static const char c_DefaultShader[] = R"(
[[vk::binding(0, 0)]]cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 offset;
    float4 color;
    float aspectRatio;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float4 uv : TEXCOORD;
};

[[vk::binding(1, 0)]] Texture2D g_texture : register(t0);
[[vk::binding(2, 0)]] SamplerState g_sampler : register(s0);

PSInput VSMain(
    [[vk::location(0)]] float4 position : POSITION,
    [[vk::location(1)]] float4 vertexColor : COLOR,
    [[vk::location(2)]] float4 uv : TEXCOORD)
{
    PSInput result;

    position.y *= aspectRatio;
    result.position = mul(offset, position);
    result.color = vertexColor * color;
    result.uv = uv;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return g_texture.Sample(g_sampler, input.uv.xy) * input.color;
}
)";

static const char c_DownsampleShader[] = R"(
[[vk::binding(0, 0)]] cbuffer DownsampleConstantBuffer : register(b0)
{
    float2 invSize;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float4 uv : TEXCOORD;
};

[[vk::binding(1, 0)]] Texture2D g_texture : register(t0);
[[vk::binding(2, 0)]] SamplerState g_sampler : register(s0);

PSInput VSMain(
    [[vk::location(0)]] float4 position : POSITION,
    [[vk::location(1)]] float4 color : COLOR,
    [[vk::location(2)]] float4 uv : TEXCOORD)
{
    PSInput result;

    result.position = position;
    position.w = 1.f;
    result.color = color;
    result.uv = uv;

    return result;
}

static float2 offsets[9] = {
    float2( 1, 1), float2( 0, 1), float2(-1, 1),
    float2( 1, 0), float2( 0, 0), float2(-1, 0),
    float2( 1,-1), float2( 0,-1), float2(-1,-1)
    };

float4 PSMain(PSInput input) : SV_Target
{
    float4 color = float4(0,0,0,0);

    for(int i=0;i<9;i++)
        color += g_texture.SampleLevel(g_sampler, input.uv.xy + (2 * invSize * offsets[i]), 0 );
    return color / 9.0f;
}

[[vk::binding(3, 0)]] RWTexture2D<float4> g_textureOut : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint2 dtId : SV_DispatchThreadID)
{
    uint2 inputSize;
    g_textureOut.GetDimensions(inputSize.x, inputSize.y);

    PSInput psInput = (PSInput)0;
    psInput.uv.xy = 1.0f - dtId.xy / float2(inputSize);

    g_textureOut[dtId] = PSMain(psInput);
}
)";

#define TEST_APP_NAME_RAW "TestDownsample"

class TestRpsDownsample : public RpsTestHost
{
public:
    static const UINT GeoLimit = 25;

    struct GeoConstantBuffer
    {
        XMFLOAT4X4 offset;
        XMFLOAT4   color;
        float      aspectRatio;
    };

    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT4 color;
        XMFLOAT2 uv;
    };

public:

    TestRpsDownsample()
        : m_translation(0.f)
        , m_rotation(0.f)
    {
    }

protected:

    void OnInit(RpsDevice hRpsDevice)
    {
        m_quadColor[0] = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
        m_quadColor[1] = XMFLOAT4(1.f, 1.f, 0.f, 1.f);
        m_quadColor[2] = XMFLOAT4(0.f, 1.f, 1.f, 1.f);
        m_quadColor[3] = XMFLOAT4(1.f, 0.f, 1.f, 1.f);

        UpdateGeometryCount(2, 2);

        RpsTestHost::OnInit(hRpsDevice, rpsTestLoadRpslEntry(test_downsample, downsample));
    }

    void UpdateRpsPipeline(uint64_t               frameIndex,
                           uint64_t               completedFrameIndex,
                           const RpsResourceDesc& backBufferDesc,
                           RpsRuntimeResource*    pBackBuffers)
    {
        const RpsBool useCompute = m_bUseAsyncCompute ? RPS_TRUE : RPS_FALSE;
        const RpsBool useArray   = m_bUseArrayMips ? RPS_TRUE : RPS_FALSE;

        const RpsRuntimeResource* argResources[] = {pBackBuffers};
        RpsConstant               argData[]      = {&backBufferDesc, &useCompute, &useArray, &m_NumGeos};

        RpsTestHost::OnUpdate(frameIndex, completedFrameIndex, _countof(argData), argData, argResources);
    }

    void OnUpdate(uint32_t frameIndex, uint32_t width, uint32_t height)
    {
        const float translationSpeed = 0.01f;
        const float rotationSpeed    = 0.02f;
        const float offsetBounds     = 1.4f;

        //m_rotation += rotationSpeed;
        m_translation += translationSpeed;
        if (m_translation > offsetBounds)
        {
            m_translation = -offsetBounds;
        }

        XMMATRIX transform = XMMatrixAffineTransformation2D(
            XMVectorSplatOne(), XMVectorZero(), m_rotation, XMVectorSet(m_translation, 0.f, 0.f, 0.f));

        for (UINT g = 0; g < 4; g++)
        {
            XMStoreFloat4x4(&m_TriangleCbData[g].offset, transform);
            m_TriangleCbData[g].color       = m_quadColor[g];
            m_TriangleCbData[g].aspectRatio = static_cast<float>(width) / height;
        }

        UpdateGeometryCount((1 + (frameIndex >> 5)) % 5, (1 + (frameIndex >> 5)) % 5);

        m_bUseArrayMips = (frameIndex / 50) & 1;
        m_bUseAsyncCompute = (frameIndex >> 5) >= 5;
    }

private:
    void UpdateGeometryCount(uint32_t rows, uint32_t cols)
    {
        assert(rows * cols <= GeoLimit);

        float cellWidth  = 2.f / cols;
        float cellHeight = 2.f / rows;

        m_NumGeos      = rows * cols;
        m_quadScale[0] = cellWidth / 2.f;
        m_quadScale[1] = cellHeight / 2.f;

        for (UINT r = 0; r < rows; r++)
        {
            for (UINT c = 0; c < cols; c++)
            {
                m_quadOffsets[r * cols + c][0] = -1.f + c * cellWidth + m_quadScale[0];
                m_quadOffsets[r * cols + c][1] = 1.f - r * cellHeight - m_quadScale[1];
            }
        }
    }

protected:
    float             m_translation;
    float             m_rotation;
    GeoConstantBuffer m_TriangleCbData[4];

    float    m_quadScale[2];
    float    m_quadOffsets[GeoLimit][2];
    XMFLOAT4 m_quadColor[4];

    bool m_bUseAsyncCompute             = false;
    bool m_bUseArrayMips                = false;
    bool m_bUseScheduler                = true;
    bool m_bUpdateRPSPipelineEveryFrame = false;
    UINT m_NumGeos                      = 0;
};
