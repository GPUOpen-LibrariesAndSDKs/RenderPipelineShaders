// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#define RPS_D3D11_RUNTIME 1

#include "test_render_to_texture_shared.hpp"

#include "utils/rps_test_win32.hpp"
#include "utils/rps_test_d3d11_renderer.hpp"

using namespace DirectX;

class TestD3D11RpsRenderToTexture : public RpsTestD3D11Renderer, public TestRpsRenderToTexture
{
    struct alignas(16) SceneConstantBufferD3D11 : public TestRpsRenderToTexture::SceneConstantBuffer
    {
    };

protected:
    virtual void OnInit() override
    {
        LoadAssets();
        //No lifetime analysis required for DX11
        TestRpsRenderToTexture::OnInit(RPS_RENDER_GRAPH_NO_LIFETIME_ANALYSIS);


        RpsResult result = rpsProgramBindNode(
            rpsRenderGraphGetMainEntry(GetRpsRenderGraph()), "Quads", &TestD3D11RpsRenderToTexture::DrawQuads, this);
        REQUIRE(result == RPS_OK);
    }

    virtual void OnPostResize() override
    {
    }

    virtual void OnCleanUp() override
    {
        m_pSampler                = nullptr;
        m_inputLayout             = nullptr;
        m_VS                      = nullptr;
        m_PS                      = nullptr;
        m_vertexBuffer            = nullptr;
        m_triangleCB              = nullptr;
        m_quadCB                  = nullptr;
        m_checkerboardTextureView = nullptr;

        TestRpsRenderToTexture::OnCleanUp();
    }

    virtual void OnUpdate(uint32_t frameIndex) override
    {
        TestRpsRenderToTexture::OnUpdate(frameIndex, m_width, m_height);
        UpdatePipeline(frameIndex, CalcGuaranteedCompletedFrameIndexForRps());
    }

    virtual void OnRender(uint32_t frameIndex) override
    {
        D3D11_MAPPED_SUBRESOURCE mappedCb;
        ThrowIfFailed(m_immDC->Map(m_triangleCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedCb));

        memcpy(mappedCb.pData, &m_triangleAnimationData, sizeof(m_triangleAnimationData));

        m_immDC->Unmap(m_triangleCB.Get(), 0);

        REQUIRE(RPS_SUCCEEDED(ExecuteRenderGraph(frameIndex, GetRpsRenderGraph())));
    }

protected:
    virtual void CreateRpsDevice(RpsDevice& rpsDeviceOut) override final
    {
        rpsDeviceOut = rpsTestUtilCreateDevice(
            [this](auto pCreateInfo, auto phDevice) { return CreateRpsRuntimeDevice(*pCreateInfo, *phDevice); });
    }

    virtual void DrawTriangle(const RpsCmdCallbackContext* pContext, bool isMSAA) override final
    {
        ID3D11DeviceContext* pCmdList = rpsD3D11DeviceContextFromHandle(pContext->hCommandBuffer);

        pCmdList->VSSetShader(m_VS.Get(), nullptr, 0);
        pCmdList->PSSetShader(m_PS.Get(), nullptr, 0);

        pCmdList->VSSetConstantBuffers(0, 1, m_triangleCB.GetAddressOf());
        pCmdList->PSSetShaderResources(0, 1, m_checkerboardTextureView.GetAddressOf());
        pCmdList->PSSetSamplers(0, 1, m_pSampler.GetAddressOf());

        pCmdList->IASetInputLayout(m_inputLayout.Get());
        pCmdList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        UINT vbStride = sizeof(Vertex);
        UINT vbOffset = 0;
        pCmdList->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &vbStride, &vbOffset);
        pCmdList->DrawInstanced(3, 1, 0, 0);
    }

private:
    void DrawQuads(const RpsCmdCallbackContext* pContext,
                   rps::UnusedArg               backBuffer,
                   ID3D11ShaderResourceView*    offscreenRTSrv)
    {
        ID3D11DeviceContext* pCmdList = rpsD3D11DeviceContextFromHandle(pContext->hCommandBuffer);

        pCmdList->VSSetShader(m_VS.Get(), nullptr, 0);

        pCmdList->VSSetConstantBuffers(0, 1, m_quadCB.GetAddressOf());
        pCmdList->PSSetShaderResources(0, 1, &offscreenRTSrv);
        pCmdList->PSSetSamplers(0, 1, m_pSampler.GetAddressOf());

        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        UINT vbStride = sizeof(Vertex);
        UINT vbOffset = sizeof(Vertex) * 3;
        pCmdList->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &vbStride, &vbOffset);
        pCmdList->DrawInstanced(24, 1, 0, 0);
    }

private:
    void LoadAssets()
    {
        {
            D3D11_SAMPLER_DESC sampler = {};

            sampler.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            sampler.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
            sampler.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
            sampler.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
            sampler.MipLODBias     = 0;
            sampler.MaxAnisotropy  = 0;
            sampler.ComparisonFunc = D3D11_COMPARISON_NEVER;
            sampler.MinLOD         = 0.0f;
            sampler.MaxLOD         = D3D11_FLOAT32_MAX;

            m_device->CreateSamplerState(&sampler, &m_pSampler);
        }

        // Create the pipeline state, which includes compiling and loading shaders.
        {
            D3D11_INPUT_ELEMENT_DESC inputElementDescs[] = {
                {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0}};

            ComPtr<ID3DBlob> dxbc, err;

            ThrowIfFailedEx(
                D3DCompile(
                    c_Shader, sizeof(c_Shader), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &dxbc, &err),
                err);
            ThrowIfFailed(
                m_device->CreateVertexShader(dxbc->GetBufferPointer(), dxbc->GetBufferSize(), nullptr, &m_VS));

            ThrowIfFailed(m_device->CreateInputLayout(inputElementDescs,
                                                      _countof(inputElementDescs),
                                                      dxbc->GetBufferPointer(),
                                                      dxbc->GetBufferSize(),
                                                      &m_inputLayout));

            ThrowIfFailedEx(
                D3DCompile(
                    c_Shader, sizeof(c_Shader), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &dxbc, &err),
                err);
            ThrowIfFailed(m_device->CreatePixelShader(dxbc->GetBufferPointer(), dxbc->GetBufferSize(), nullptr, &m_PS));
        }

        // Create constant buffers
        {
            CD3D11_BUFFER_DESC cbufDesc(sizeof(SceneConstantBufferD3D11),
                                        D3D11_BIND_CONSTANT_BUFFER,
                                        D3D11_USAGE_DYNAMIC,
                                        D3D11_CPU_ACCESS_WRITE);

            ThrowIfFailed(m_device->CreateBuffer(&cbufDesc, nullptr, &m_triangleCB));

            cbufDesc.Usage          = D3D11_USAGE_IMMUTABLE;
            cbufDesc.CPUAccessFlags = 0;

            D3D11_SUBRESOURCE_DATA quadCBInitData = {};
            quadCBInitData.pSysMem                = &m_quadConstantData;

            ThrowIfFailed(m_device->CreateBuffer(&cbufDesc, &quadCBInitData, &m_quadCB));
        }

        // Create vertex buffers
        {
            // Define the geometry for a triangle.
            Vertex triangleVertices[] = {
                // triangle
                {{0.0f, 0.25f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.5f, 0.0f}},
                {{0.25f, -0.25f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                {{-0.25f, -0.25f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

                // quad 0
                {{-1.0f, 1.0, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
                {{0.f, 0.f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
                {{-1.f, 0.f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
                {{-1.0f, 1.0, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
                {{0.f, 1.f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
                {{0.f, 0.f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},

                // quad 1
                {{-1.0f, 0.0, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
                {{0.f, -1.f, 0.0f}, {0.f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
                {{-1.f, -1.f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
                {{-1.0f, 0.0, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
                {{0.f, 0.f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
                {{0.f, -1.f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},

                // quad 2
                {{0.0f, 1.0, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
                {{1.f, 0.f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
                {{0.f, 0.f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                {{0.0f, 1.0, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
                {{1.f, 1.f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
                {{1.f, 0.f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},

                // quad 3
                {{0.0f, 0.0, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                {{1.f, -1.f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
                {{0.f, -1.f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
                {{0.0f, 0.0, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                {{1.f, 0.f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
                {{1.f, -1.f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            };

            const UINT vertexBufferSize = sizeof(triangleVertices);

            auto vbDesc = CD3D11_BUFFER_DESC(vertexBufferSize, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_IMMUTABLE);

            D3D11_SUBRESOURCE_DATA vbInitData = {};
            vbInitData.pSysMem                = triangleVertices;

            ThrowIfFailed(m_device->CreateBuffer(&vbDesc, &vbInitData, &m_vertexBuffer));
        }

        // Create checkerboard texture
        ComPtr<ID3D11Resource> checkerboardTexture;
        float                  tintColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
        CreateStaticCheckerboardTexture(checkerboardTexture, 256, 256, tintColor);

        ThrowIfFailed(m_device->CreateShaderResourceView(checkerboardTexture.Get(), NULL, &m_checkerboardTextureView));
    }

    void UpdatePipeline(uint64_t frameIndex, uint64_t completedFrameIndex)
    {
        RpsRuntimeResource backBuffers[1];
        backBuffers[0] = rpsD3D11ResourceToHandle(m_backBuffer.Get());

        RpsResourceDesc backBufferDesc   = {};
        backBufferDesc.type              = RPS_RESOURCE_TYPE_IMAGE_2D;
        backBufferDesc.temporalLayers    = 1;
        backBufferDesc.image.arrayLayers = 1;
        backBufferDesc.image.mipLevels   = 1;
        backBufferDesc.image.format      = rpsFormatFromDXGI(m_swapChain.GetFormat());
        backBufferDesc.image.width       = m_width;
        backBufferDesc.image.height      = m_height;
        backBufferDesc.image.sampleCount = 1;

        TestRpsRenderToTexture::UpdateRpsPipeline(frameIndex, completedFrameIndex, backBufferDesc, backBuffers);
    }

private:
    ComPtr<ID3D11SamplerState>       m_pSampler;
    ComPtr<ID3D11InputLayout>        m_inputLayout;
    ComPtr<ID3D11VertexShader>       m_VS;
    ComPtr<ID3D11PixelShader>        m_PS;
    ComPtr<ID3D11Buffer>             m_vertexBuffer;
    ComPtr<ID3D11Buffer>             m_triangleCB;
    ComPtr<ID3D11Buffer>             m_quadCB;
    ComPtr<ID3D11ShaderResourceView> m_checkerboardTextureView;
};

TEST_CASE(TEST_APP_NAME)
{
#if _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    TestD3D11RpsRenderToTexture renderer;

    RpsTestRunWindowInfo runInfo = {};
    runInfo.title                = TEXT(TEST_APP_NAME);
    runInfo.numFramesToRender    = g_exitAfterFrame;
    runInfo.width                = 1280;
    runInfo.height               = 720;
    runInfo.pRenderer            = &renderer;
    RpsTestRunWindowApp(&runInfo);
}
