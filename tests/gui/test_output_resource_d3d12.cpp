// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#define RPS_D3D12_RUNTIME 1

#include "test_output_resource_shared.hpp"

#include "utils/rps_test_win32.hpp"
#include "utils/rps_test_d3d12_renderer.hpp"

using namespace DirectX;

class TestD3D12OutputResource : public RpsTestD3D12Renderer, public TestRpsOutputResource
{
protected:
    virtual void OnInit(ID3D12GraphicsCommandList*         pInitCmdList,
                        std::vector<ComPtr<ID3D12Object>>& tempResources) override
    {
        LoadAssets(pInitCmdList, tempResources);

        TestRpsOutputResource::OnInit();

        auto coloredTriangelEntry = rpsRenderGraphGetMainEntry(GetRpsRenderGraphColoredTriangle());

        RpsResult result =
            rpsProgramBindNode(coloredTriangelEntry, "DrawTriangle", &TestD3D12OutputResource::DrawTriangle, this);
        REQUIRE(result == RPS_OK);

        result = rpsProgramBindNode(coloredTriangelEntry, "Blt", &TestD3D12OutputResource::DrawBlt, this);
        REQUIRE(result == RPS_OK);

        auto tintedQuadEntry = rpsRenderGraphGetMainEntry(GetRpsRenderGraphTintedQuads());

        result = rpsProgramBindNode(tintedQuadEntry, "Blt", &TestD3D12OutputResource::DrawBlt, this);
        REQUIRE(result == RPS_OK);
    }

    virtual void OnCleanUp() override
    {
        TestRpsOutputResource::OnCleanUp();

        m_rootSignature             = nullptr;
        m_pipelineStateBlt          = nullptr;
        m_pipelineStateDrawTriangle = nullptr;
    }

    virtual void OnUpdate(uint32_t frameIndex) override
    {
        TestRpsOutputResource::OnUpdate(frameIndex, m_width, m_height);
        UpdatePipeline(frameIndex, CalcGuaranteedCompletedFrameIndexForRps());
    }

    virtual void OnRender(uint32_t frameIndex) override
    {
        if (m_triangleActive)
        {
            REQUIRE(RPS_SUCCEEDED(ExecuteRenderGraph(frameIndex, GetRpsRenderGraphColoredTriangle())));
        }

        REQUIRE(RPS_SUCCEEDED(ExecuteRenderGraph(frameIndex, GetRpsRenderGraphTintedQuads())));
    }

protected:
    virtual void CreateRpsDevice(RpsDevice& rpsDeviceOut) override final
    {
        rpsDeviceOut = rpsTestUtilCreateDevice(
            [this](auto pCreateInfo, auto phDevice) { return CreateRpsRuntimeDevice(*pCreateInfo, *phDevice); });
    }

    void DrawTriangle(const RpsCmdCallbackContext* pContext)
    {
        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);

        BindDescriptorHeaps(pCmdList);
        pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
        pCmdList->SetPipelineState(m_pipelineStateDrawTriangle.Get());

        ConstantData cbData = {
            {},
            float(m_width) / m_height,
            float(std::chrono::duration_cast<std::chrono::duration<double>>(m_triangleAnimationTime).count())};

        pCmdList->SetGraphicsRoot32BitConstants(0, sizeof(cbData) / sizeof(UINT), &cbData, 0);
        pCmdList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->DrawInstanced(3, 1, 0, 0);
    }

private:
    void DrawBlt(const RpsCmdCallbackContext* pContext, const XMFLOAT3& tint, D3D12_CPU_DESCRIPTOR_HANDLE srcImage)
    {
        auto pThis = static_cast<TestD3D12OutputResource*>(pContext->pCmdCallbackContext);

        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);

        D3D12_GPU_DESCRIPTOR_HANDLE srvTable =
            AllocDynamicDescriptorsAndWrite(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, &srcImage, 1);

        pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
        pCmdList->SetPipelineState(m_pipelineStateBlt.Get());

        BindDescriptorHeaps(pCmdList);
        ConstantData cbData = {
            tint,
            float(m_width) / m_height,
            (tint.x + tint.y + tint.z) > 2.5f ? 0 : float(RpsAfxCpuTimer::SecondsSinceEpoch().count())};
        pCmdList->SetGraphicsRoot32BitConstants(0, sizeof(cbData) / sizeof(UINT), &cbData, 0);
        pCmdList->SetGraphicsRootDescriptorTable(1, srvTable);
        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->DrawInstanced(3, 1, 0, 0);
    }

private:
    void LoadAssets(ID3D12GraphicsCommandList* pInitCmdList, std::vector<ComPtr<ID3D12Object>>& tempResources)
    {
        // Create a root signature consisting of a descriptor table with a single CBV.
        {
            CD3DX12_DESCRIPTOR_RANGE ranges[1]         = {};
            CD3DX12_ROOT_PARAMETER   rootParameters[2] = {};

            ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
            rootParameters[0].InitAsConstants(5, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
            rootParameters[1].InitAsDescriptorTable(1, ranges, D3D12_SHADER_VISIBILITY_PIXEL);

            D3D12_STATIC_SAMPLER_DESC sampler = {};
            sampler.Filter                    = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            sampler.AddressU                  = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            sampler.AddressV                  = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            sampler.AddressW                  = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            sampler.MipLODBias                = 0;
            sampler.MaxAnisotropy             = 0;
            sampler.ComparisonFunc            = D3D12_COMPARISON_FUNC_NEVER;
            sampler.BorderColor               = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
            sampler.MinLOD                    = 0.0f;
            sampler.MaxLOD                    = D3D12_FLOAT32_MAX;
            sampler.ShaderRegister            = 0;
            sampler.RegisterSpace             = 0;
            sampler.ShaderVisibility          = D3D12_SHADER_VISIBILITY_PIXEL;

            CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
            rootSignatureDesc.Init_1_0(
                _countof(rootParameters), rootParameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE);

            ComPtr<ID3DBlob> signature;
            ComPtr<ID3DBlob> error;
            ThrowIfFailed(D3DX12SerializeVersionedRootSignature(
                &rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signature, &error));
            ThrowIfFailed(m_device->CreateRootSignature(
                0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
        }

        {
            D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.pRootSignature                     = m_rootSignature.Get();
            psoDesc.RasterizerState                    = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
            psoDesc.BlendState                         = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
            psoDesc.DepthStencilState.DepthEnable      = FALSE;
            psoDesc.DepthStencilState.StencilEnable    = FALSE;
            psoDesc.SampleMask                         = UINT_MAX;
            psoDesc.PrimitiveTopologyType              = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            psoDesc.NumRenderTargets                   = 1;
            psoDesc.RTVFormats[0]                      = m_swapChain.GetFormat();
            psoDesc.SampleDesc.Count                   = 1;

            std::vector<uint8_t> vsCode, psCode;
            DxcCompile(c_Shader, L"VSTriangle", L"vs_6_0", L"", nullptr, 0, vsCode);
            DxcCompile(c_Shader, L"PSTriangle", L"ps_6_0", L"", nullptr, 0, psCode);
            psoDesc.VS = CD3DX12_SHADER_BYTECODE(vsCode.data(), vsCode.size());
            psoDesc.PS = CD3DX12_SHADER_BYTECODE(psCode.data(), psCode.size());

            ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineStateDrawTriangle)));

            DxcCompile(c_Shader, L"VSBlt", L"vs_6_0", L"", nullptr, 0, vsCode);
            DxcCompile(c_Shader, L"PSBlt", L"ps_6_0", L"", nullptr, 0, psCode);
            psoDesc.VS = CD3DX12_SHADER_BYTECODE(vsCode.data(), vsCode.size());
            psoDesc.PS = CD3DX12_SHADER_BYTECODE(psCode.data(), psCode.size());

            ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineStateBlt)));
        }
    }

    void UpdatePipeline(uint64_t frameIndex, uint64_t completedFrameIndex)
    {
        RpsRuntimeResource backBuffers[DXGI_MAX_SWAP_CHAIN_BUFFERS];
        for (uint32_t i = 0; i < m_backBufferCount; i++)
        {
            backBuffers[i] = rpsD3D12ResourceToHandle(m_backBuffers[i].Get());
        }

        RpsResourceDesc backBufferDesc   = {};
        backBufferDesc.type              = RPS_RESOURCE_TYPE_IMAGE_2D;
        backBufferDesc.temporalLayers    = uint32_t(m_backBuffers.size());
        backBufferDesc.image.arrayLayers = 1;
        backBufferDesc.image.mipLevels   = 1;
        backBufferDesc.image.format      = rpsFormatFromDXGI(m_swapChain.GetFormat());
        backBufferDesc.image.width       = m_width;
        backBufferDesc.image.height      = m_height;
        backBufferDesc.image.sampleCount = 1;

        TestRpsOutputResource::UpdateRpsPipeline(frameIndex, completedFrameIndex, backBufferDesc, backBuffers);
    }

private:
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineStateDrawTriangle;
    ComPtr<ID3D12PipelineState> m_pipelineStateBlt;
};

TEST_CASE(TEST_APP_NAME)
{
#if _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    TestD3D12OutputResource renderer;

    RpsTestRunWindowInfo runInfo = {};
    runInfo.title                = TEXT(TEST_APP_NAME);
    runInfo.numFramesToRender    = g_exitAfterFrame;
    runInfo.width                = 1280;
    runInfo.height               = 720;
    runInfo.pRenderer            = &renderer;
    RpsTestRunWindowApp(&runInfo);
}
