// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#define RPS_D3D12_RUNTIME 1

#include "test_subprogram_shared.hpp"

#include "utils/rps_test_win32.hpp"
#include "utils/rps_test_d3d12_renderer.hpp"

using namespace DirectX;

class TestD3D12Subprogram : public RpsTestD3D12Renderer, public TestRpsSubprogram
{
protected:
    virtual void OnInit(ID3D12GraphicsCommandList*         pInitCmdList,
                        std::vector<ComPtr<ID3D12Object>>& tempResources) override
    {
        LoadAssets(pInitCmdList, tempResources);

        RpsDevice hRpsDevice = rpsTestUtilCreateDevice(
            [this](auto pCreateInfo, auto phDevice) { return CreateRpsRuntimeDevice(*pCreateInfo, *phDevice); });

        TestRpsSubprogram::Init(hRpsDevice);
    }

    virtual void OnPostResize() override
    {
    }

    virtual void OnCleanUp() override
    {
        TestRpsSubprogram::OnDestroy();

        m_rootSignature          = nullptr;
        m_pipelineStateDrawScene = nullptr;
        m_pipelineStateDrawGUI   = nullptr;
    }

    virtual void OnUpdate(uint32_t frameIndex) override
    {
        BindNodes(m_mainRpslProgram, !!((frameIndex >> 6) & 1), !!((frameIndex >> 5) & 1));

        RpsRuntimeResource backBuffers[DXGI_MAX_SWAP_CHAIN_BUFFERS];
        RpsResourceDesc    backBufferDesc;
        GetBackBuffers(backBufferDesc, backBuffers);

        RpsViewport viewport = {
            0.0f, 0.0f, float(backBufferDesc.image.width), float(backBufferDesc.image.height), 0.0f, 1.0f};

        RpsConstant               args[]         = {&backBufferDesc, &viewport};
        const RpsRuntimeResource* argResources[] = {backBuffers};

        uint64_t completedFrameIndex = CalcGuaranteedCompletedFrameIndexForRps();

        TestRpsSubprogram::OnUpdate(frameIndex, completedFrameIndex, uint32_t(RPS_TEST_COUNTOF(args)), args, argResources);

        RpsTestD3D12Renderer::OnUpdate(frameIndex);
    }

    virtual void OnRender(uint32_t frameIndex) override
    {
        REQUIRE(RPS_SUCCEEDED(ExecuteRenderGraph(frameIndex, GetRpsRenderGraph())));
    }

protected:
    virtual void BindNodes(RpsSubprogram hRpslEntry) override final
    {
        BindNodes(hRpslEntry, false, false);
    }

    void BindNodes(RpsSubprogram hRpslEntry, bool bUseSceneSubprogram, bool bUseGUISubprogram)
    {
        RpsResult result =
            bUseSceneSubprogram
                ? rpsProgramBindNodeSubprogram(hRpslEntry, "DrawScene", m_drawSceneSubprogram)
                : rpsProgramBindNode(hRpslEntry, "DrawScene", &TestD3D12Subprogram::DrawScene, this);
        REQUIRE(result == RPS_OK);

        result = bUseGUISubprogram
                     ? rpsProgramBindNodeSubprogram(hRpslEntry, "DrawGUI", m_drawGUISubprogram)
                     : rpsProgramBindNode(hRpslEntry, "DrawGUI", &TestD3D12Subprogram::DrawGUI, this);
        REQUIRE(result == RPS_OK);
    }

    void CreateScene(const RpsCmdCallbackContext* pContext)
    {
        if (!m_pipelineStateDrawScene)
        {
            RpsCmdRenderTargetInfo rtInfo;
            RpsResult              result = rpsCmdGetRenderTargetsInfo(pContext, &rtInfo);
            REQUIRE(result == RPS_OK);

            CreatePSO(L"VS", L"PSScene", false, false, &rtInfo, &m_pipelineStateDrawScene);
        }
    }

    void DrawScene(const RpsCmdCallbackContext* pContext,
                   rps::UnusedArg               rt,
                   const float                  color[4],
                   const RpsViewport&           viewport)
    {
        CreateScene(pContext);

        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);
        pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
        pCmdList->SetPipelineState(m_pipelineStateDrawScene.Get());
        pCmdList->SetGraphicsRoot32BitConstants(0, 4, color, 0);
        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->DrawInstanced(3, 1, 0, 0);
    }

    void CreateGUI(const RpsCmdCallbackContext* pContext)
    {
        if (!m_pipelineStateDrawGUI)
        {
            RpsCmdRenderTargetInfo rtInfo;
            RpsResult              result = rpsCmdGetRenderTargetsInfo(pContext, &rtInfo);
            REQUIRE(result == RPS_OK);

            CreatePSO(L"VS", L"PSGUI", false, true, &rtInfo, &m_pipelineStateDrawGUI);
        }
    }

    void DrawGUI(const RpsCmdCallbackContext* pContext,
                 rps::UnusedArg               rt,
                 const RpsViewport&           viewport,
                 const float                  color[4])
    {
        CreateGUI(pContext);

        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);
        pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
        pCmdList->SetPipelineState(m_pipelineStateDrawGUI.Get());
        pCmdList->SetGraphicsRoot32BitConstants(0, 4, color, 0);
        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->DrawInstanced(3, 1, 0, 0);
    }

private:
    void LoadAssets(ID3D12GraphicsCommandList* pInitCmdList, std::vector<ComPtr<ID3D12Object>>& tempResources)
    {
        CD3DX12_ROOT_PARAMETER   rootParameters[1] = {};
        rootParameters[0].InitAsConstants(4, 0);

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
                   bool                          bBlendEnable,
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

            if (bBlendEnable)
            {
                psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
                psoDesc.BlendState.RenderTarget[0].SrcBlend    = D3D12_BLEND_SRC_ALPHA;
                psoDesc.BlendState.RenderTarget[0].DestBlend   = D3D12_BLEND_INV_SRC_ALPHA;
                psoDesc.BlendState.RenderTarget[0].BlendOp     = D3D12_BLEND_OP_ADD;
            }

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
    ComPtr<ID3D12PipelineState> m_pipelineStateDrawScene;
    ComPtr<ID3D12PipelineState> m_pipelineStateDrawGUI;
};

TEST_CASE(TEST_APP_NAME)
{
#if _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    TestD3D12Subprogram renderer;

    RpsTestRunWindowInfo runInfo = {};
    runInfo.title                = TEXT(TEST_APP_NAME);
    runInfo.numFramesToRender    = g_exitAfterFrame;
    runInfo.width                = 1280;
    runInfo.height               = 720;
    runInfo.pRenderer            = &renderer;
    RpsTestRunWindowApp(&runInfo);
}
