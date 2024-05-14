// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "test_multi_queue_shared.hpp"

#include "rps/runtime/d3d12/rps_d3d12_runtime.h"
#include "utils/rps_test_win32.hpp"
#include "utils/rps_test_d3d12_renderer.hpp"

using namespace DirectX;

class TestD3D12MultiQueue : public RpsTestD3D12Renderer, public TestRpsMultiQueue
{
public:
    TestD3D12MultiQueue()
    {
    }

protected:
    virtual void OnInit(ID3D12GraphicsCommandList*         pInitCmdList,
                        std::vector<ComPtr<ID3D12Object>>& tempResources) override
    {
        LoadAssets(pInitCmdList, tempResources);

        TestRpsMultiQueue::Init(rpsTestUtilCreateDevice(
            [this](auto pCreateInfo, auto phDevice) { return CreateRpsRuntimeDevice(*pCreateInfo, *phDevice); }));

        auto hMainEntry = rpsRenderGraphGetMainEntry(GetRpsRenderGraph());
        REQUIRE_RPS_OK(
            rpsProgramBindNode(hMainEntry, "UpdateInstanceData", &TestD3D12MultiQueue::UpdateInstanceData, this));
        REQUIRE_RPS_OK(rpsProgramBindNode(hMainEntry, "Procedural", &TestD3D12MultiQueue::Procedural, this));
        REQUIRE_RPS_OK(rpsProgramBindNode(hMainEntry, "GenMip", &TestD3D12MultiQueue::GenMip, this));
        REQUIRE_RPS_OK(rpsProgramBindNode(hMainEntry, "ShadowMap", &TestD3D12MultiQueue::ShadowMap, this));
        REQUIRE_RPS_OK(rpsProgramBindNode(hMainEntry, "ShadingPass", &TestD3D12MultiQueue::ShadingPass, this));
    }

    virtual void OnCleanUp() override
    {
        TestRpsMultiQueue::OnDestroy();

        m_rootSigCompute          = nullptr;
        m_rootSigGfx              = nullptr;
        m_pipelineStateProcedural = nullptr;
        m_pipelineStateMipGen     = nullptr;
        m_pipelineStateShadowMap  = nullptr;
        m_pipelineStateShading    = nullptr;
    }

    virtual void OnUpdate(uint32_t frameIndex) override
    {
        Animate(XMUINT2(m_width, m_height));

        UpdatePipeline(frameIndex, CalcGuaranteedCompletedFrameIndexForRps());
    }

    virtual void OnRender(uint32_t frameIndex) override
    {
        ExecuteRenderGraph(frameIndex, GetRpsRenderGraph());
    }

private:
    void UpdateInstanceData(const RpsCmdCallbackContext* pContext,
                            ID3D12Resource*              pUploadBuffer,
                            ID3D12Resource*              pConstantBuffer)
    {
        constexpr D3D12_RANGE emptyReadRange = {0, 0};
        void*                 pData          = nullptr;

        if (SUCCEEDED(pUploadBuffer->Map(0, &emptyReadRange, &pData)))
        {
            size_t sizeToCopy =
                std::min(size_t(pUploadBuffer->GetDesc().Width), m_instanceDataGpu.size() * sizeof(InstanceDataGPU));

            memcpy(pData, m_instanceDataGpu.data(), sizeToCopy);

            pUploadBuffer->Unmap(0, nullptr);
        }

        if (SUCCEEDED(pConstantBuffer->Map(0, &emptyReadRange, &pData)))
        {
            memcpy(pData, &m_cbufferData, sizeof(m_cbufferData));

            pConstantBuffer->Unmap(0, nullptr);
        }
    }

    void Procedural(const RpsCmdCallbackContext* pContext,
                    D3D12_CPU_DESCRIPTOR_HANDLE  proceduralTextureUav,
                    ID3D12Resource*              pConstantBuffer,
                    const XMUINT2&               outputDim)
    {
        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);

        D3D12_CPU_DESCRIPTOR_HANDLE uavHdls[2] = {proceduralTextureUav, proceduralTextureUav};
        D3D12_GPU_DESCRIPTOR_HANDLE uavTable =
            AllocDynamicDescriptorsAndWrite(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, uavHdls, 2);

        BindDescriptorHeaps(pCmdList);
        pCmdList->SetComputeRootSignature(m_rootSigCompute.Get());

        pCmdList->SetPipelineState(m_pipelineStateProcedural.Get());
        pCmdList->SetComputeRootConstantBufferView(0, pConstantBuffer->GetGPUVirtualAddress());
        pCmdList->SetComputeRootDescriptorTable(1, uavTable);
        pCmdList->Dispatch(DivRoundUp(outputDim.x, 8), DivRoundUp(outputDim.y, 8), 1);
    }

    void GenMip(const RpsCmdCallbackContext* pContext,
                D3D12_CPU_DESCRIPTOR_HANDLE  outMip,
                D3D12_CPU_DESCRIPTOR_HANDLE  inMip,
                const XMUINT2&               outputDim)
    {
        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);

        D3D12_CPU_DESCRIPTOR_HANDLE uavHdls[2] = {outMip, inMip};
        D3D12_GPU_DESCRIPTOR_HANDLE uavTable =
            AllocDynamicDescriptorsAndWrite(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, uavHdls, 2);

        BindDescriptorHeaps(pCmdList);
        pCmdList->SetComputeRootSignature(m_rootSigCompute.Get());

        pCmdList->SetPipelineState(m_pipelineStateMipGen.Get());
        pCmdList->SetComputeRootDescriptorTable(1, uavTable);
        pCmdList->Dispatch(DivRoundUp(outputDim.x, 8), DivRoundUp(outputDim.y, 8), 1);
    }

    void ShadowMap(const RpsCmdCallbackContext* pContext,
                   D3D12_CPU_DESCRIPTOR_HANDLE  instanceBuffer,
                   ID3D12Resource*              pConstBuffer)
    {
        if (!m_pipelineStateShadowMap)
        {
            RpsCmdRenderTargetInfo renderTargetInfo;
            REQUIRE_RPS_OK(rpsCmdGetRenderTargetsInfo(pContext, &renderTargetInfo));
            CreateGfxPSO(L"VSShadow", nullptr, &renderTargetInfo, &m_pipelineStateShadowMap);
        }

        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);

        D3D12_GPU_DESCRIPTOR_HANDLE srvTable =
            AllocDynamicDescriptorsAndWrite(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, &instanceBuffer, 1);

        BindDescriptorHeaps(pCmdList);
        pCmdList->SetGraphicsRootSignature(m_rootSigGfx.Get());

        pCmdList->SetPipelineState(m_pipelineStateShadowMap.Get());
        pCmdList->SetGraphicsRootConstantBufferView(0, pConstBuffer->GetGPUVirtualAddress());
        pCmdList->SetGraphicsRootDescriptorTable(1, srvTable);
        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->DrawInstanced(36, uint32_t(m_instanceData.size()), 0, 0);
    }

    void ShadingPass(const RpsCmdCallbackContext* pContext,
                     rps::UnusedArg               colorBuffer,
                     rps::UnusedArg               depthBuffer,
                     D3D12_CPU_DESCRIPTOR_HANDLE  instanceBuffer,
                     D3D12_CPU_DESCRIPTOR_HANDLE  shadowMap,
                     D3D12_CPU_DESCRIPTOR_HANDLE  proceduralTexture,
                     ID3D12Resource*              pConstBuffer)
    {
        if (!m_pipelineStateShading)
        {
            RpsCmdRenderTargetInfo renderTargetInfo;
            REQUIRE_RPS_OK(rpsCmdGetRenderTargetsInfo(pContext, &renderTargetInfo));
            CreateGfxPSO(L"VSShading", L"PSShading", &renderTargetInfo, &m_pipelineStateShading);
        }

        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);

        D3D12_GPU_DESCRIPTOR_HANDLE srvTable =
            AllocDynamicDescriptorsAndWrite(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, &instanceBuffer, 1);

        D3D12_CPU_DESCRIPTOR_HANDLE srcSRVs[] = {shadowMap, proceduralTexture};
        D3D12_GPU_DESCRIPTOR_HANDLE srvTablePS =
            AllocDynamicDescriptorsAndWrite(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srcSRVs, 2);

        BindDescriptorHeaps(pCmdList);
        pCmdList->SetGraphicsRootSignature(m_rootSigGfx.Get());

        pCmdList->SetPipelineState(m_pipelineStateShading.Get());
        pCmdList->SetGraphicsRootConstantBufferView(0, pConstBuffer->GetGPUVirtualAddress());
        pCmdList->SetGraphicsRootDescriptorTable(1, srvTable);
        pCmdList->SetGraphicsRootDescriptorTable(2, srvTablePS);
        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->DrawInstanced(36, uint32_t(m_instanceData.size()), 0, 0);
    }

private:
    void LoadAssets(ID3D12GraphicsCommandList* pInitCmdList, std::vector<ComPtr<ID3D12Object>>& tempResources)
    {
        OnPostResize();

        CreateComputePSOs();
    }

    void CreateComputePSOs()
    {
        std::vector<uint8_t> csCode;

        DxcCompile(c_Shader, L"CSProcedural", L"cs_6_0", L"", nullptr, 0, csCode);
        ThrowIfFailed(m_device->CreateRootSignature(0, csCode.data(), csCode.size(), IID_PPV_ARGS(&m_rootSigCompute)));

        D3D12_COMPUTE_PIPELINE_STATE_DESC compPsoDesc = {};

        compPsoDesc.pRootSignature     = m_rootSigCompute.Get();
        compPsoDesc.CS.pShaderBytecode = csCode.data();
        compPsoDesc.CS.BytecodeLength  = csCode.size();

        ThrowIfFailed(m_device->CreateComputePipelineState(&compPsoDesc, IID_PPV_ARGS(&m_pipelineStateProcedural)));

        DxcCompile(c_Shader, L"CSMipGen", L"cs_6_0", L"", nullptr, 0, csCode);

        compPsoDesc.CS.pShaderBytecode = csCode.data();
        compPsoDesc.CS.BytecodeLength  = csCode.size();

        ThrowIfFailed(m_device->CreateComputePipelineState(&compPsoDesc, IID_PPV_ARGS(&m_pipelineStateMipGen)));
    }

    void CreateGfxPSO(LPCWSTR                       vsEntry,
                      LPCWSTR                       psEntry,
                      const RpsCmdRenderTargetInfo* pRtInfo,
                      ID3D12PipelineState**         ppState)
    {
        std::vector<uint8_t> vsCode, psCode;

        DxcCompile(c_Shader, vsEntry, L"vs_6_0", L"", nullptr, 0, vsCode);

        if (!m_rootSigGfx)
        {
            ThrowIfFailed(m_device->CreateRootSignature(0, vsCode.data(), vsCode.size(), IID_PPV_ARGS(&m_rootSigGfx)));
        }

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature                     = m_rootSigGfx.Get();
        psoDesc.VS                                 = CD3DX12_SHADER_BYTECODE(vsCode.data(), vsCode.size());
        if (psEntry)
        {
            DxcCompile(c_Shader, psEntry, L"ps_6_0", L"", nullptr, 0, psCode);
            psoDesc.PS = CD3DX12_SHADER_BYTECODE(psCode.data(), psCode.size());
        }
        psoDesc.DepthStencilState     = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState       = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState            = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask            = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        psoDesc.DSVFormat        = rpsFormatToDXGI(pRtInfo->depthStencilFormat);
        psoDesc.NumRenderTargets = pRtInfo->numRenderTargets;
        for (uint32_t i = 0; i < pRtInfo->numRenderTargets; i++)
        {
            psoDesc.RTVFormats[i] = rpsFormatToDXGI(pRtInfo->renderTargetFormats[i]);
        }
        psoDesc.SampleDesc.Count = pRtInfo->numSamples;

        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(ppState)));
    }

    void UpdatePipeline(uint64_t frameIndex, uint64_t completedFrameIndex)
    {
        RpsRuntimeResource backBuffers[DXGI_MAX_SWAP_CHAIN_BUFFERS];
        RpsResourceDesc    backBufferDesc = {};

        GetBackBuffers(backBufferDesc, backBuffers);

        TestRpsMultiQueue::UpdateRpsPipeline(frameIndex, completedFrameIndex, backBufferDesc, backBuffers);
    }

private:
    ComPtr<ID3D12RootSignature> m_rootSigCompute;
    ComPtr<ID3D12RootSignature> m_rootSigGfx;
    ComPtr<ID3D12PipelineState> m_pipelineStateProcedural;
    ComPtr<ID3D12PipelineState> m_pipelineStateMipGen;
    ComPtr<ID3D12PipelineState> m_pipelineStateShadowMap;
    ComPtr<ID3D12PipelineState> m_pipelineStateShading;
};

TEST_CASE(TEST_APP_NAME)
{
#if _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    TestD3D12MultiQueue renderer;

    RpsTestRunWindowInfo runInfo = {};
    runInfo.title                = TEXT(TEST_APP_NAME);
    runInfo.numFramesToRender    = g_exitAfterFrame;
    runInfo.width                = 1280;
    runInfo.height               = 720;
    runInfo.pRenderer            = &renderer;
    RpsTestRunWindowApp(&runInfo);
}
