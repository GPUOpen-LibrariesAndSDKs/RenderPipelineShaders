// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#define RPS_D3D12_RUNTIME 1

#include "test_built_in_nodes_shared.hpp"

#include "utils/rps_test_win32.hpp"
#include "utils/rps_test_d3d12_renderer.hpp"

using namespace DirectX;

class TestD3D12BuiltInNodes : public RpsTestD3D12Renderer, public TestRpsBuiltInNodes
{
protected:
    virtual void OnInit(ID3D12GraphicsCommandList*         pInitCmdList,
                        std::vector<ComPtr<ID3D12Object>>& tempResources) override
    {
        LoadAssets(pInitCmdList, tempResources);

        TestRpsBuiltInNodes::Init(rpsTestUtilCreateDevice(
            [this](auto pCreateInfo, auto phDevice) { return CreateRpsRuntimeDevice(*pCreateInfo, *phDevice); }));
    }

    virtual void OnPostResize() override
    {
    }

    virtual void OnCleanUp() override
    {
        TestRpsBuiltInNodes::OnDestroy();

        m_rootSignature         = nullptr;
        m_pipelineStateFillUV   = nullptr;
        m_pipelineStateMSAAQuad = nullptr;
        m_pipelineStateBlt      = nullptr;
    }

    virtual void OnUpdate(uint32_t frameIndex) override
    {
        RpsRuntimeResource backBuffers[DXGI_MAX_SWAP_CHAIN_BUFFERS];
        RpsResourceDesc    backBufferDesc;
        GetBackBuffers(backBufferDesc, backBuffers);

        RpsBool bTestMinMax = RPS_TRUE;

        RpsConstant               args[]         = {&backBufferDesc, &bTestMinMax};
        const RpsRuntimeResource* argResources[] = {backBuffers};

        uint64_t completedFrameIndex = CalcGuaranteedCompletedFrameIndexForRps();

        TestRpsBuiltInNodes::OnUpdate(frameIndex, completedFrameIndex, uint32_t(RPS_TEST_COUNTOF(args)), args, argResources);

        RpsTestD3D12Renderer::OnUpdate(frameIndex);
    }

    virtual void OnRender(uint32_t frameIndex) override
    {
        REQUIRE(RPS_SUCCEEDED(ExecuteRenderGraph(frameIndex, GetRpsRenderGraph())));
    }

protected:
    virtual void BindNodes(RpsSubprogram hRpslEntry) override final
    {
        TestRpsBuiltInNodes::BindNodes(hRpslEntry);

        RpsResult result =
            rpsProgramBindNode(hRpslEntry, "blt_to_swapchain", &TestD3D12BuiltInNodes::DrawBlt, this);
        REQUIRE(result == RPS_OK);

        result = rpsProgramBindNode(hRpslEntry, "fill_uv", &TestD3D12BuiltInNodes::DrawFillUV, this);
        REQUIRE(result == RPS_OK);

        result = rpsProgramBindNode(hRpslEntry, "msaa_quad", &TestD3D12BuiltInNodes::DrawMSAAQuad, this);
        REQUIRE(result == RPS_OK);
    }

    void CreateFillUV(const RpsCmdCallbackContext* pContext)
    {
        if (!m_pipelineStateFillUV)
        {
            CreateComputePSO(L"CSFillUV", &m_pipelineStateFillUV);
        }
    }

    void DrawFillUV(const RpsCmdCallbackContext* pContext, D3D12_CPU_DESCRIPTOR_HANDLE dst, float cbData)
    {
        CreateFillUV(pContext);

        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);

        D3D12_GPU_DESCRIPTOR_HANDLE uavTable =
            AllocDynamicDescriptorsAndWrite(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, &dst, 1);
        BindDescriptorHeaps(pCmdList);

        ID3D12Resource* pD3DResource = {};
        auto            pViewInfo    = static_cast<const RpsImageView*>(pContext->ppArgs[0]);
        RpsResult       result       = rpsD3D12GetCmdArgResource(pContext, 0, &pD3DResource);
        REQUIRE(result == RPS_OK);

        auto     desc = pD3DResource->GetDesc();
        uint32_t w    = std::max(1u, uint32_t(desc.Width) >> pViewInfo->subresourceRange.baseMipLevel);
        uint32_t h    = std::max(1u, desc.Height >> pViewInfo->subresourceRange.baseMipLevel);

        pCmdList->SetComputeRootSignature(m_rootSignature.Get());
        pCmdList->SetPipelineState(m_pipelineStateFillUV.Get());
        pCmdList->SetComputeRoot32BitConstant(0, *(const UINT*)(&cbData), 0);
        pCmdList->SetComputeRootDescriptorTable(2, uavTable);
        pCmdList->Dispatch((w + 7) / 8, (h + 7) / 8, 1);
    }

    void CreateMSAAQuad(const RpsCmdCallbackContext* pContext)
    {
        if (!m_pipelineStateMSAAQuad)
        {
            RpsCmdRenderTargetInfo rtInfo;
            RpsResult              result = rpsCmdGetRenderTargetsInfo(pContext, &rtInfo);
            REQUIRE(result == RPS_OK);

            CreatePSO(L"VSBlt", L"PSColorSample", false, &rtInfo, &m_pipelineStateMSAAQuad);
        }
    }

    void DrawMSAAQuad(const RpsCmdCallbackContext* pContext)
    {
        CreateMSAAQuad(pContext);

        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);
        pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
        pCmdList->SetPipelineState(m_pipelineStateMSAAQuad.Get());
        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->DrawInstanced(3, 1, 0, 0);
    }

    void CreateBlt(const RpsCmdCallbackContext* pContext)
    {
        if (!m_pipelineStateBlt)
        {
            RpsCmdRenderTargetInfo rtInfo;
            RpsResult              result = rpsCmdGetRenderTargetsInfo(pContext, &rtInfo);
            REQUIRE(result == RPS_OK);

            CreatePSO(L"VSBlt", L"PSBlt", false, &rtInfo, &m_pipelineStateBlt);
        }
    }

    void DrawBlt(const RpsCmdCallbackContext* pContext,
                 rps::UnusedArg               dst,
                 D3D12_CPU_DESCRIPTOR_HANDLE  src,
                 const ViewportData&          dstViewport)
    {
        CreateBlt(pContext);

        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);

        RpsCmdViewportInfo viewportScissorInfo = {};
        RpsResult          result              = rpsCmdGetViewportInfo(pContext, &viewportScissorInfo);
        REQUIRE(result == RPS_OK);
        REQUIRE(viewportScissorInfo.numViewports == 1);
        REQUIRE(dstViewport.data.x == viewportScissorInfo.pViewports[0].x);
        REQUIRE(dstViewport.data.y == viewportScissorInfo.pViewports[0].y);
        REQUIRE(dstViewport.data.z == viewportScissorInfo.pViewports[0].width);
        REQUIRE(dstViewport.data.w == viewportScissorInfo.pViewports[0].height);

        D3D12_GPU_DESCRIPTOR_HANDLE srvTable =
            AllocDynamicDescriptorsAndWrite(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, &src, 1);

        pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
        BindDescriptorHeaps(pCmdList);

        pCmdList->SetPipelineState(m_pipelineStateBlt.Get());
        pCmdList->SetGraphicsRootDescriptorTable(1, srvTable);
        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->DrawInstanced(3, 1, 0, 0);
    }

private:
    void LoadAssets(ID3D12GraphicsCommandList* pInitCmdList, std::vector<ComPtr<ID3D12Object>>& tempResources)
    {
        CD3DX12_DESCRIPTOR_RANGE ranges[2]         = {};
        CD3DX12_ROOT_PARAMETER   rootParameters[3] = {};

        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
        rootParameters[0].InitAsConstants(1, 0);
        rootParameters[1].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[2].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL);

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter                    = D3D12_FILTER_MIN_MAG_MIP_POINT;
        sampler.AddressU                  = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressV                  = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressW                  = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.MipLODBias                = 0;
        sampler.MaxAnisotropy             = 0;
        sampler.ComparisonFunc            = D3D12_COMPARISON_FUNC_NEVER;
        sampler.BorderColor               = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD                    = 0.0f;
        sampler.MaxLOD                    = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister            = 0;
        sampler.RegisterSpace             = 0;
        sampler.ShaderVisibility          = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_0(_countof(rootParameters), rootParameters, 1, &sampler, rootSignatureFlags);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailedEx(D3DX12SerializeVersionedRootSignature(
                            &rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signature, &error),
                        error);
        ThrowIfFailed(m_device->CreateRootSignature(
            0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    }

    void CreatePSO(LPCWSTR                       vsEntry,
                   LPCWSTR                       psEntry,
                   bool                          bDepthEnable,
                   const RpsCmdRenderTargetInfo* pRenderTargetInfo,
                   ID3D12PipelineState**         ppPSO)
    {
        // Create the pipeline state, which includes compiling and loading shaders.
        {
            std::vector<uint8_t> vsCode, psCode, gsCode;

            // Describe and create the graphics pipeline state object (PSO).
            D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.InputLayout                        = {nullptr, 0};
            psoDesc.pRootSignature                     = m_rootSignature.Get();
            psoDesc.RasterizerState                    = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
            psoDesc.BlendState                         = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
            psoDesc.DepthStencilState                  = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
            psoDesc.DepthStencilState.DepthEnable      = !!bDepthEnable;
            psoDesc.SampleMask                         = UINT_MAX;
            psoDesc.PrimitiveTopologyType              = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

            psoDesc.DSVFormat                         = rpsFormatToDXGI(pRenderTargetInfo->depthStencilFormat);
            psoDesc.SampleDesc.Count                  = pRenderTargetInfo->numSamples;
            psoDesc.NumRenderTargets                  = pRenderTargetInfo->numRenderTargets;
            psoDesc.RasterizerState.MultisampleEnable = pRenderTargetInfo->numSamples > 1;

            for (uint32_t iRT = 0; iRT < pRenderTargetInfo->numRenderTargets; iRT++)
            {
                psoDesc.RTVFormats[iRT] = rpsFormatToDXGI(pRenderTargetInfo->renderTargetFormats[iRT]);
            }

            DxcCompile(c_Shader, vsEntry, L"vs_6_0", L"", nullptr, 0, vsCode);
            DxcCompile(c_Shader, psEntry, L"ps_6_0", L"", nullptr, 0, psCode);
            psoDesc.VS = CD3DX12_SHADER_BYTECODE(vsCode.data(), vsCode.size());
            psoDesc.PS = CD3DX12_SHADER_BYTECODE(psCode.data(), psCode.size());

            ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(ppPSO)));
        }
    }

    void CreateComputePSO(LPCWSTR csEntry, ID3D12PipelineState** ppPSO)
    {
        std::vector<uint8_t>              csCode;
        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature                    = m_rootSignature.Get();

        DxcCompile(c_Shader, csEntry, L"cs_6_0", L"", nullptr, 0, csCode);
        psoDesc.CS = CD3DX12_SHADER_BYTECODE(csCode.data(), csCode.size());

        ThrowIfFailed(m_device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(ppPSO)));
    }

private:
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineStateFillUV;
    ComPtr<ID3D12PipelineState> m_pipelineStateBlt;
    ComPtr<ID3D12PipelineState> m_pipelineStateMSAAQuad;
};

TEST_CASE(TEST_APP_NAME)
{
#if _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    TestD3D12BuiltInNodes renderer;

    RpsTestRunWindowInfo runInfo = {};
    runInfo.title                = TEXT(TEST_APP_NAME);
    runInfo.numFramesToRender    = g_exitAfterFrame;
    runInfo.width                = 1280;
    runInfo.height               = 720;
    runInfo.pRenderer            = &renderer;
    RpsTestRunWindowApp(&runInfo);
}
