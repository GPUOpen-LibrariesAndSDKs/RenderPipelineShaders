// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <stdarg.h>

#include "utils/rps_test_d3d12_renderer.hpp"
#include "utils/rps_test_common.h"
#include "utils/rps_test_win32.hpp"

RPS_DECLARE_RPSL_ENTRY(test_temporal, main);

static const char c_Shader[] = R"(
struct V2P
{
    float4 Pos : SV_Position;
    float4 Color : COLOR0;
};

cbuffer cb : register(b0)
{
    float AspectRatio;
};

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
    vsOut.Pos.x *= AspectRatio;
    vsOut.Color = float4(vId == 0 ? 1 : 0, vId == 1 ? 1 : 0, vId == 2 ? 1 : 0, 1);
    return vsOut;
}

float4 PSMain(V2P psIn) : SV_Target0
{
    return psIn.Color;
}
)";

#define TEST_APP_NAME_RAW "TestTemporal"

class TestD3D12Temporal : public RpsTestD3D12Renderer
{
protected:
    virtual void OnInit(ID3D12GraphicsCommandList*         pInitCmdList,
                        std::vector<ComPtr<ID3D12Object>>& tempResources) override
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

        m_pipelineState = nullptr;
        m_rootSignature = nullptr;
    }

    virtual void OnUpdate(uint32_t frameIndex) override
    {
        UpdateRpsPipeline(frameIndex);
    }

    virtual void OnRender(uint32_t frameIndex) override
    {
        RpsRenderGraphBatchLayout batchLayout = {};
        RpsResult                 result      = rpsRenderGraphGetBatchLayout(m_rpsRenderGraph, &batchLayout);
        REQUIRE(result == RPS_OK);

        m_fenceSignalInfos.resize(batchLayout.numFenceSignals);

        for (uint32_t iBatch = 0; iBatch < batchLayout.numCmdBatches; iBatch++)
        {
            auto& batch = batchLayout.pCmdBatches[iBatch];

            ActiveCommandList cmdList = AcquireCmdList(RPS_AFX_QUEUE_INDEX_GFX);

            RpsRenderGraphRecordCommandInfo recordInfo = {};

            recordInfo.pUserContext  = this;
            recordInfo.cmdBeginIndex = batch.cmdBegin;
            recordInfo.numCmds       = batch.numCmds;
            recordInfo.hCmdBuffer    = rpsD3D12CommandListToHandle(cmdList.cmdList.Get());

            for (uint32_t iWaitIdx = batch.waitFencesBegin; iWaitIdx < batch.waitFencesBegin + batch.numWaitFences;
                 ++iWaitIdx)
            {
                const auto& signalInfo = m_fenceSignalInfos[batchLayout.pWaitFenceIndices[iWaitIdx]];

                HRESULT hr = m_queues[batch.queueIndex]->Wait(m_fences[signalInfo.queueIndex].Get(), signalInfo.value);
                REQUIRE(SUCCEEDED(hr));
            }

            result = rpsRenderGraphRecordCommands(m_rpsRenderGraph, &recordInfo);
            REQUIRE(result == RPS_OK);

            CloseCmdList(cmdList);
            ID3D12CommandList* pCmdLists[] = {cmdList.cmdList.Get()};
            m_presentQueue->ExecuteCommandLists(1, pCmdLists);
            RecycleCmdList(cmdList);

            if (batch.signalFenceIndex != RPS_INDEX_NONE_U32)
            {
                m_fenceValue++;

                auto& signalInfo = m_fenceSignalInfos[batch.signalFenceIndex];

                signalInfo.queueIndex = batch.queueIndex;
                signalInfo.value      = m_fenceValue;

                HRESULT hr =
                    m_queues[batch.queueIndex]->Signal(m_fences[signalInfo.queueIndex].Get(), signalInfo.value);
                REQUIRE(SUCCEEDED(hr));
            }
        }
    }

private:
    void DrawTriangle(ID3D12GraphicsCommandList* pCmdList)
    {
        FLOAT aspectRatio = m_height / static_cast<float>(m_width);
        pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
        pCmdList->SetGraphicsRoot32BitConstant(0, *(const UINT*)&aspectRatio, 0);
        pCmdList->SetPipelineState(m_pipelineState.Get());
        pCmdList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->DrawInstanced(3, 1, 0, 0);
    }

    static void DrawTriangleCb(const RpsCmdCallbackContext* pContext)
    {
        auto pThis = static_cast<TestD3D12Temporal*>(pContext->pCmdCallbackContext);
        pThis->DrawTriangle(rpsD3D12CommandListFromHandle(pContext->hCommandBuffer));
    }

private:
    void LoadAssets()
    {
        // Create a root signature consisting of a descriptor table with a single CBV.
        {
            CD3DX12_ROOT_PARAMETER rootParameters[1];
            rootParameters[0].InitAsConstants(1, 0);

            D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

            CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
            rootSignatureDesc.Init_1_0(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

            ComPtr<ID3DBlob> signature;
            ComPtr<ID3DBlob> error;
            ThrowIfFailed(D3DX12SerializeVersionedRootSignature(
                &rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signature, &error));
            ThrowIfFailed(m_device->CreateRootSignature(
                0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
        }

        // Create the pipeline state, which includes compiling and loading shaders.
        {
            ComPtr<ID3DBlob> vertexShader, pixelShader, err;

#if defined(_DEBUG)
            // Enable better shader debugging with the graphics debugging tools.
            UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
            UINT compileFlags = 0;
#endif

            ThrowIfFailedEx(D3DCompile(c_Shader,
                                       sizeof(c_Shader),
                                       nullptr,
                                       nullptr,
                                       nullptr,
                                       "VSMain",
                                       "vs_5_0",
                                       compileFlags,
                                       0,
                                       &vertexShader,
                                       &err),
                            err);
            ThrowIfFailedEx(D3DCompile(c_Shader,
                                       sizeof(c_Shader),
                                       nullptr,
                                       nullptr,
                                       nullptr,
                                       "PSMain",
                                       "ps_5_0",
                                       compileFlags,
                                       0,
                                       &pixelShader,
                                       &err),
                            err);

            // Describe and create the graphics pipeline state object (PSO).
            D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.InputLayout                        = {nullptr, 0};
            psoDesc.pRootSignature                     = m_rootSignature.Get();
            psoDesc.VS                                 = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
            psoDesc.PS                                 = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
            psoDesc.RasterizerState                    = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
            psoDesc.BlendState                         = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
            psoDesc.DepthStencilState.DepthEnable      = FALSE;
            psoDesc.DepthStencilState.StencilEnable    = FALSE;
            psoDesc.SampleMask                         = UINT_MAX;
            psoDesc.PrimitiveTopologyType              = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            psoDesc.NumRenderTargets                   = 1;
            psoDesc.RTVFormats[0]                      = DXGI_FORMAT_R8G8B8A8_UNORM;
            psoDesc.SampleDesc.Count                   = 1;

            ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
        }
    }

    void LoadRpsPipeline()
    {
        RpsRenderGraphCreateInfo renderGraphInfo            = {};
        renderGraphInfo.mainEntryCreateInfo.hRpslEntryPoint = rpsTestLoadRpslEntry(test_temporal, main);

        REQUIRE_RPS_OK(rpsRenderGraphCreate(m_rpsDevice, &renderGraphInfo, &m_rpsRenderGraph));

        rpsProgramBindNode(rpsRenderGraphGetMainEntry(m_rpsRenderGraph), "Triangle", &DrawTriangleCb, this);
    }

    void UpdateRpsPipeline(uint64_t frameIndex)
    {
        if (m_rpsRenderGraph != RPS_NULL_HANDLE)
        {
            RpsRuntimeResource backBufferResources[DXGI_MAX_SWAP_CHAIN_BUFFERS] = {};
            for (uint32_t i = 0; i < m_backBuffers.size(); i++)
            {
                backBufferResources[i] = {m_backBuffers[i].Get()};
            }
            const RpsRuntimeResource* argResources[] = {backBufferResources};

            RpsResourceDesc backBufferDesc   = {};
            backBufferDesc.type              = RPS_RESOURCE_TYPE_IMAGE_2D;
            backBufferDesc.temporalLayers    = uint32_t(m_backBuffers.size());
            backBufferDesc.image.arrayLayers = 1;
            backBufferDesc.image.mipLevels   = 1;
            backBufferDesc.image.format      = RPS_FORMAT_R8G8B8A8_UNORM;
            backBufferDesc.image.width       = m_width;
            backBufferDesc.image.height      = m_height;
            backBufferDesc.image.sampleCount = 1;

            RpsConstant argData[] = {&backBufferDesc, &frameIndex};

            const uint64_t completedFrameIndex = CalcGuaranteedCompletedFrameIndexForRps();

            RpsRenderGraphUpdateInfo updateInfo = {};
            updateInfo.frameIndex               = frameIndex;
            updateInfo.gpuCompletedFrameIndex   = completedFrameIndex;
            updateInfo.numArgs                  = 2;
            updateInfo.ppArgs                   = argData;
            updateInfo.ppArgResources           = argResources;

            updateInfo.diagnosticFlags = RPS_DIAGNOSTIC_ENABLE_RUNTIME_DEBUG_NAMES;
            if (frameIndex < m_backBufferCount)
            {
                updateInfo.diagnosticFlags = RPS_DIAGNOSTIC_ENABLE_ALL;
            }

            REQUIRE_RPS_OK(rpsRenderGraphUpdate(m_rpsRenderGraph, &updateInfo));
        }
    }

private:
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;

    RpsDevice      m_rpsDevice      = {};
    RpsRenderGraph m_rpsRenderGraph = {};
};

TEST_CASE(TEST_APP_NAME)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#if defined(BREAK_AT_ALLOC_ID)
    _CrtSetBreakAlloc(BREAK_AT_ALLOC_ID);
#endif

    TestD3D12Temporal renderer;

    RpsTestRunWindowInfo runInfo = {};
    runInfo.title                = TEXT(TEST_APP_NAME);
    runInfo.numFramesToRender    = g_exitAfterFrame;
    runInfo.width                = 1280;
    runInfo.height               = 720;
    runInfo.pRenderer            = &renderer;
    RpsTestRunWindowApp(&runInfo);
}
