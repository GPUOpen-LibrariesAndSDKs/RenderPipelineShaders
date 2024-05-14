// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#include "utils/rps_test_host.hpp"
#include "app_framework/afx_threadpool.hpp"

RPS_DECLARE_RPSL_ENTRY(test_multi_queue, main);

#include <random>

static const char c_Shader[] = R"--(

#define COMP_RS "CBV(b0), DescriptorTable( UAV(u0, numDescriptors = 2 ) )"
#define GFX_RS "RootFlags( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT ), CBV(b0)," \
               "DescriptorTable( SRV(t0), visibility = SHADER_VISIBILITY_VERTEX )," \
               "DescriptorTable( SRV(t1, numDescriptors = 2), visibility = SHADER_VISIBILITY_PIXEL ),"  \
               "StaticSampler( s0, filter = FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, addressU = TEXTURE_ADDRESS_CLAMP, addressV = TEXTURE_ADDRESS_CLAMP, addressW = TEXTURE_ADDRESS_CLAMP, comparisonFunc = COMPARISON_LESS_EQUAL )," \
               "StaticSampler( s1, filter = FILTER_ANISOTROPIC )"

struct V2P
{
    float4 Pos : SV_Position;
    float4 Color : COLOR0;
    float2 UV : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float3 WorldPos : TEXCOORD2;
};

struct Vertex
{
    float4 Pos;
    float3 Nrm;
    float2 UV;
};

struct InstanceData
{
    float4x3 worldMat;
    float4 color;
};

struct CBufferData
{
    float4x4 viewProjMat;
    float4x4 lightViewProjMat;
    float3 lightDir;
    float timeInSeconds;
};

#ifdef VULKAN
[[vk::binding(0, 0)]]
#endif
ConstantBuffer<CBufferData> cbuf;

#ifdef VULKAN
[[vk::binding(1, 0)]]
#endif
RWTexture2D<float4> outImg : register(u0);

#ifdef VULKAN
[[vk::binding(2, 0)]]
#endif
RWTexture2D<float4> inImg : register(u1);

[RootSignature(COMP_RS)]
[numthreads(8, 8, 1)]
void CSProcedural(uint2 dtID : SV_DispatchThreadID)
{
    uint2 dim;
    outImg.GetDimensions(dim.x, dim.y);

    float sum = 0;
    for(uint i = 0; i < 16; i++)
    {
        float t = length((int2(dtID) - int2(dim / 2)) * 0.01f) * (i + 1) + cbuf.timeInSeconds;
        sum += sin(t) * pow(0.5f, i);
    }

    outImg[dtID] = sum * 0.3f + 0.7f;
}

[RootSignature(COMP_RS)]
[numthreads(8, 8, 1)]
void CSMipGen(uint2 dtID : SV_DispatchThreadID)
{
    uint2 inCoord = dtID << 1;
    outImg[dtID] = (inImg[inCoord] + inImg[inCoord + uint2(1, 0)] + inImg[inCoord + uint2(0, 1)] + inImg[inCoord + uint2(1, 1)]) * 0.25f;
}

Vertex GetCubeVertex(uint vId)
{
    uint posIdxMap[] = {
        7, 6, 3, 3, 6, 2,   // X+
        4, 5, 6, 4, 6, 7,   // Y+
        0, 7, 3, 0, 4, 7,   // Z+
        0, 5, 4, 0, 1, 5,   // X-
        3, 2, 0, 0, 2, 1,   // Y-
        1, 6, 5, 1, 2, 6,   // Z-
    };

    uint idx = posIdxMap[vId];

    Vertex vert;
    vert.Pos = float4(
        (idx & 2) ? 1 : -1,
        (idx & 4) ? 1 : -1,
        (((idx & 3) == 0) || ((idx & 3) == 3)) ? 1 : -1,
        1.0f);

    uint faceId = vId / 6;
    vert.Nrm = float3(
        (faceId == 0) ? 1.0f : ((faceId == 3) ? -1.0f : 0),
        (faceId == 1) ? 1.0f : ((faceId == 4) ? -1.0f : 0),
        (faceId == 2) ? 1.0f : ((faceId == 5) ? -1.0f : 0));

    vert.UV =
        (((faceId == 0) || (faceId == 3)) ? vert.Pos.yz :
         (((faceId == 1) || (faceId == 4)) ? vert.Pos.xz : vert.Pos.xy)) * 0.5f + 0.5f;

    return vert;
}

#ifdef VULKAN
[[vk::binding(1, 0)]]
#endif
StructuredBuffer<InstanceData> instanceDataBuf : register(t0);
#ifdef VULKAN
[[vk::binding(2, 0)]]
#endif
Texture2D<float> shadowMap : register(t1);
#ifdef VULKAN
[[vk::binding(3, 0)]]
#endif
Texture2D<float4> proceduralImg : register(t2);
#ifdef VULKAN
[[vk::binding(4, 0)]]
#endif
SamplerComparisonState shadowMapSampler : register(s0);
#ifdef VULKAN
[[vk::binding(5, 0)]]
#endif
SamplerState imgSampler : register(s1);

[RootSignature( GFX_RS )]
float4 VSShadow(uint vId : SV_VertexID, uint instId : SV_InstanceID) : SV_Position
{
    Vertex vert = GetCubeVertex( vId );
    InstanceData instanceData = instanceDataBuf[instId];

    return mul( cbuf.lightViewProjMat, float4(mul(vert.Pos, instanceData.worldMat), 1) );
}

[RootSignature( GFX_RS )]
V2P VSShading(uint vId : SV_VertexID, uint instId : SV_InstanceID)
{
    Vertex vert = GetCubeVertex( vId );
    InstanceData instanceData = instanceDataBuf[instId];

    V2P vsOut;
    float3 worldPos = mul(vert.Pos, instanceData.worldMat);
    vsOut.Pos = mul( cbuf.viewProjMat, float4(worldPos, 1) );
    vsOut.Color = instanceData.color;
    vsOut.UV = vert.UV;
    vsOut.Normal = normalize(mul(vert.Nrm, (float3x3)(instanceData.worldMat)));
    vsOut.WorldPos = worldPos;

    return vsOut;
}

float4 PSShading(V2P psIn) : SV_Target0
{
    float3 lightProjCoord = mul( cbuf.lightViewProjMat, float4(psIn.WorldPos, 1) ).xyz;
    float2 lightUV = lightProjCoord.xy * float2(0.5f, -0.5f) + 0.5f;

    float shadowValue = shadowMap.SampleCmpLevelZero( shadowMapSampler, lightUV, lightProjCoord.z - 0.001f );

    float4 texureValue = proceduralImg.Sample( imgSampler, psIn.UV );

    return psIn.Color * max(0.2f, saturate(dot(psIn.Normal, -cbuf.lightDir)) * shadowValue) * texureValue;
}
)--";

using namespace DirectX;

#define TEST_APP_NAME_RAW "TestMultiQueue"

class TestRpsMultiQueue : public RpsTestHost
{
public:
    struct InstanceData
    {
        XMFLOAT3 offset;
        float    scale;
        XMFLOAT3 color;
        float    spinSpeed;
        float    rotationSpeed;
    };

    struct InstanceDataGPU
    {
        XMFLOAT3X4 transform;
        XMFLOAT4   color;
    };

    struct CBufferData
    {
        XMFLOAT4X4 viewProjMat;
        XMFLOAT4X4 lightViewProjMat;
        XMFLOAT3   lightDir;
        float      timeInSeconds;
    };

public:
    TestRpsMultiQueue()
    {
        g_MultiQueueMode = MULTI_QUEUE_GFX_COMPUTE_COPY;
    }

protected:
    void Init(RpsDevice hRpsDevice)
    {
        uint32_t numInstances = 4096;
        m_instanceData.resize(numInstances);

        std::random_device                    rd;
        std::mt19937                          gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        m_instanceData[0]        = {};
        m_instanceData[0].offset = XMFLOAT3(0, -32.0f, 0);
        m_instanceData[0].scale  = 32.0f;
        m_instanceData[0].color  = XMFLOAT3(1, 1, 1);

        for (uint32_t i = 1; i < m_instanceData.size(); i++)
        {
            m_instanceData[i].offset =
                XMFLOAT3((dist(gen) - 0.5f) * 64.0f, dist(gen) * 3.0f + 4.0f, (dist(gen) - 0.5f) * 64.0f);
            m_instanceData[i].scale = dist(gen) * 0.4f + 0.4f;
            XMStoreFloat3(&m_instanceData[i].color, DirectX::XMColorHSVToRGB(XMVectorSet(dist(gen), 1, 1, 0)));
            m_instanceData[i].spinSpeed     = dist(gen);
            m_instanceData[i].rotationSpeed = dist(gen);
        }
        m_instanceDataGpu.resize(m_instanceData.size());

        m_cbufferData = {};

        XMVECTOR lightDir = XMVector3Normalize(XMVectorSet(1, -0.5f, -0.75f, 1));

        XMStoreFloat3(&m_cbufferData.lightDir, lightDir);

        XMMATRIX lightView = XMMatrixLookToRH(-lightDir * 100, lightDir, XMVectorSet(0, 1, 0, 1));
        XMMATRIX lightProj = XMMatrixOrthographicOffCenterRH(-75, 75, -75, 75, 0.5f, 1000.0f);

        XMStoreFloat4x4(&m_cbufferData.lightViewProjMat, lightView * lightProj);

        RpsTestHost::OnInit(hRpsDevice, rpsTestLoadRpslEntry(test_multi_queue, main));
    }

    void Animate(const XMUINT2& viewportSize)
    {
        float time = float(RpsAfxCpuTimer::SecondsSinceEpoch().count());

        for (uint32_t i = 0; i < m_instanceData.size(); i++)
        {
            auto&       instanceDataGpu = m_instanceDataGpu[i];
            const auto& instanceData    = m_instanceData[i];

            XMMATRIX transform =
                XMMatrixScaling(instanceData.scale, instanceData.scale, instanceData.scale) *
                XMMatrixRotationAxis(XMVectorSet(0, 1, 0, 1), instanceData.spinSpeed * time * XM_2PI) *
                XMMatrixTranslation(instanceData.offset.x, instanceData.offset.y, instanceData.offset.z) *
                XMMatrixRotationAxis(XMVectorSet(0, 1, 0, 1), instanceData.rotationSpeed * time * XM_2PI);

            XMStoreFloat3x4(&instanceDataGpu.transform, transform);
            XMStoreFloat4(&instanceDataGpu.color, XMLoadFloat3(&instanceData.color));
        }

        XMMATRIX camView = XMMatrixLookAtRH(XMVectorSet(0, 40, 80, 1), XMVectorZero(), XMVectorSet(0, 1, 0, 1));
        XMMATRIX camProj =
            XMMatrixPerspectiveFovRH(XMConvertToRadians(53), viewportSize.x / float(viewportSize.y), 0.5f, 1000.0f);

        XMStoreFloat4x4(&m_cbufferData.viewProjMat, camView * camProj);
        m_cbufferData.timeInSeconds = time;
    }

    virtual void UpdateRpsPipeline(uint64_t                  frameIndex,
                                   uint64_t                  completedFrameIndex,
                                   const RpsResourceDesc&    backBufferDesc,
                                   const RpsRuntimeResource* pBackBuffers)
    {
        const uint32_t numInstances = uint32_t(m_instanceData.size());

        const RpsRuntimeResource* argResources[] = {pBackBuffers};
        RpsConstant               argData[]      = {&backBufferDesc, &numInstances, &m_shadowMapDim, &m_proceduralTextureDim};

        RpsTestHost::OnUpdate(frameIndex, completedFrameIndex, _countof(argData), argData, argResources);
    }

private:
    void DefaultCallback(const RpsCmdCallbackContext* pContext)
    {
    }

protected:
    std::vector<InstanceData>    m_instanceData;
    std::vector<InstanceDataGPU> m_instanceDataGpu;
    CBufferData                  m_cbufferData;
    uint32_t                     m_shadowMapDim         = 8192;
    uint32_t                     m_proceduralTextureDim = 4096;
};
