// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#define RPS_D3D12_RUNTIME 1

#include "test_render_to_texture_shared.hpp"

#include "utils/rps_test_win32.hpp"
#include "utils/rps_test_d3d12_renderer.hpp"

using namespace DirectX;

class TestD3D12RpsRenderToTexture : public RpsTestD3D12Renderer, public TestRpsRenderToTexture
{
    struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) SceneConstantBufferD3D12
        : public TestRpsRenderToTexture::SceneConstantBuffer
    {
    };

protected:
    virtual void OnInit(ID3D12GraphicsCommandList*         pInitCmdList,
                        std::vector<ComPtr<ID3D12Object>>& tempResources) override
    {
        LoadAssets(pInitCmdList, tempResources);

        TestRpsRenderToTexture::OnInit();

        RpsResult result =
            rpsProgramBindNode(rpsRenderGraphGetMainEntry(GetRpsRenderGraph()), "Quads", &DrawQuadsCb, this);
        REQUIRE(result == RPS_OK);
    }

    virtual void OnPostResize() override
    {
    }

    virtual void OnCleanUp() override
    {
        TestRpsRenderToTexture::OnCleanUp();

        m_rootSignature     = nullptr;
        m_pipelineState     = nullptr;
        m_pipelineStateMSAA = nullptr;
        m_vertexBuffer      = nullptr;
        m_constantBuffer    = nullptr;
        m_texture           = nullptr;
    }

    virtual void OnUpdate(uint32_t frameIndex) override
    {
        TestRpsRenderToTexture::OnUpdate(frameIndex, m_width, m_height);
        UpdatePipeline(frameIndex, CalcGuaranteedCompletedFrameIndexForRps());
    }

    virtual void OnRender(uint32_t frameIndex) override
    {
        memcpy(&m_constantBufferCpuVA[m_backBufferIndex], &m_triangleAnimationData, sizeof(m_triangleAnimationData));

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
        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);

        BindDescriptorHeaps(pCmdList);
        pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
        pCmdList->SetPipelineState(isMSAA ? m_pipelineStateMSAA.Get() : m_pipelineState.Get());

        pCmdList->SetGraphicsRootDescriptorTable(0, m_triangleConstantBufferViews.GetGPU(m_backBufferIndex));
        pCmdList->SetGraphicsRootDescriptorTable(1, m_checkerboardTextureDescriptor.GetGPU(0));

        pCmdList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->IASetVertexBuffers(0, 1, &m_triangleBufferView);
        pCmdList->DrawInstanced(3, 1, 0, 0);
    }

private:
    void DrawQuads(const RpsCmdCallbackContext* pContext,
                   rps::UnusedArg               backBuffer,
                   D3D12_CPU_DESCRIPTOR_HANDLE  offscreenRTSrv)
    {
        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);

        D3D12_GPU_DESCRIPTOR_HANDLE srvTable =
            AllocDynamicDescriptorsAndWrite(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, &offscreenRTSrv, 1);

        pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
        pCmdList->SetPipelineState(m_pipelineState.Get());

        BindDescriptorHeaps(pCmdList);
        pCmdList->SetGraphicsRootDescriptorTable(0, m_quadsConstantBufferView.GetGPU(0));
        pCmdList->SetGraphicsRootDescriptorTable(1, srvTable);
        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->IASetVertexBuffers(0, 1, &m_quadsBufferView);
        pCmdList->DrawInstanced(24, 1, 0, 0);
    }

    static void DrawQuadsCb(const RpsCmdCallbackContext* pContext)
    {
        auto pThis = static_cast<TestD3D12RpsRenderToTexture*>(pContext->pCmdCallbackContext);

        D3D12_CPU_DESCRIPTOR_HANDLE srvHdl;
        REQUIRE(RPS_OK == rpsD3D12GetCmdArgDescriptor(pContext, 1, &srvHdl));

        pThis->DrawQuads(pContext, {}, srvHdl);
    }

private:
    void LoadAssets(ID3D12GraphicsCommandList* pInitCmdList, std::vector<ComPtr<ID3D12Object>>& tempResources)
    {
        // Create a root signature consisting of a descriptor table with a single CBV.
        {
            CD3DX12_DESCRIPTOR_RANGE ranges[2]         = {};
            CD3DX12_ROOT_PARAMETER   rootParameters[2] = {};

            ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
            ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
            rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);
            rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);

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

            D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
                D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

            CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
            rootSignatureDesc.Init_1_0(_countof(rootParameters), rootParameters, 1, &sampler, rootSignatureFlags);

            ComPtr<ID3DBlob> signature;
            ComPtr<ID3DBlob> error;
            ThrowIfFailed(D3DX12SerializeVersionedRootSignature(
                &rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signature, &error));
            ThrowIfFailed(m_device->CreateRootSignature(
                0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
        }

        // Create the pipeline state, which includes compiling and loading shaders.
        {
            D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
                {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

            std::vector<uint8_t> vsCode, psCode;
            DxcCompile(c_Shader, L"VSMain", L"vs_6_0", L"", nullptr, 0, vsCode);
            DxcCompile(c_Shader, L"PSMain", L"ps_6_0", L"", nullptr, 0, psCode);

            // Describe and create the graphics pipeline state object (PSO).
            D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.InputLayout                        = {inputElementDescs, _countof(inputElementDescs)};
            psoDesc.pRootSignature                     = m_rootSignature.Get();
            psoDesc.VS                                 = CD3DX12_SHADER_BYTECODE(vsCode.data(), vsCode.size());
            psoDesc.PS                                 = CD3DX12_SHADER_BYTECODE(psCode.data(), psCode.size());
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

            psoDesc.SampleDesc.Count = 4;

            ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineStateMSAA)));
        }

        auto uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

        // Create constant buffers
        {
            auto uploadBufDesc = CD3DX12_RESOURCE_DESC::Buffer(
                sizeof(SceneConstantBufferD3D12) * (m_backBufferCount + 1), D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);

            ThrowIfFailed(m_device->CreateCommittedResource(&uploadHeapProps,
                                                            D3D12_HEAP_FLAG_NONE,
                                                            &uploadBufDesc,
                                                            D3D12_RESOURCE_STATE_GENERIC_READ,
                                                            nullptr,
                                                            IID_PPV_ARGS(&m_constantBuffer)));

            auto emptyRange = CD3DX12_RANGE(0, 0);
            ThrowIfFailed(m_constantBuffer->Map(0, &emptyRange, reinterpret_cast<void**>(&m_constantBufferCpuVA)));

            m_triangleConstantBufferViews =
                AllocStaticDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_backBufferCount);
            for (uint32_t i = 0; i < m_backBufferCount; i++)
            {
                D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
                cbvDesc.BufferLocation =
                    m_constantBuffer->GetGPUVirtualAddress() + sizeof(SceneConstantBufferD3D12) * i;
                cbvDesc.SizeInBytes = sizeof(SceneConstantBufferD3D12);
                m_device->CreateConstantBufferView(&cbvDesc, m_triangleConstantBufferViews.GetCPU(i));
            }

            m_quadsConstantBufferView               = AllocStaticDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation =
                m_constantBuffer->GetGPUVirtualAddress() + sizeof(SceneConstantBufferD3D12) * m_backBufferCount;
            cbvDesc.SizeInBytes = sizeof(SceneConstantBufferD3D12);
            m_device->CreateConstantBufferView(&cbvDesc, m_quadsConstantBufferView.GetCPU(0));

            memcpy(&m_constantBufferCpuVA[m_backBufferCount], &m_quadConstantData, sizeof(m_quadConstantData));
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

            auto vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

            ThrowIfFailed(m_device->CreateCommittedResource(&uploadHeapProps,
                                                            D3D12_HEAP_FLAG_NONE,
                                                            &vbDesc,
                                                            D3D12_RESOURCE_STATE_GENERIC_READ,
                                                            nullptr,
                                                            IID_PPV_ARGS(&m_vertexBuffer)));

            // Copy the triangle data to the vertex buffer.
            UINT8*        pVertexDataBegin;
            CD3DX12_RANGE readRange(0, 0);  // We do not intend to read from this resource on the CPU.
            ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
            memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
            m_vertexBuffer->Unmap(0, nullptr);

            // Initialize the vertex buffer view.
            m_triangleBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
            m_triangleBufferView.StrideInBytes  = sizeof(Vertex);
            m_triangleBufferView.SizeInBytes    = sizeof(Vertex) * 3;

            m_quadsBufferView.BufferLocation = m_triangleBufferView.BufferLocation + m_triangleBufferView.SizeInBytes;
            m_quadsBufferView.StrideInBytes  = sizeof(Vertex);
            m_quadsBufferView.SizeInBytes    = sizeof(Vertex) * 24;
        }

        // Create checkerboard texture
        ComPtr<ID3D12Resource> textureUploadHeap;

        // Create the texture.
        {
            static const UINT TextureWidth     = 256;
            static const UINT TextureHeight    = 256;
            static const UINT TexturePixelSize = 4;

            // Describe and create a Texture2D.
            D3D12_RESOURCE_DESC textureDesc = {};
            textureDesc.MipLevels           = 1;
            textureDesc.Format              = DXGI_FORMAT_R8G8B8A8_UNORM;
            textureDesc.Width               = TextureWidth;
            textureDesc.Height              = TextureHeight;
            textureDesc.Flags               = D3D12_RESOURCE_FLAG_NONE;
            textureDesc.DepthOrArraySize    = 1;
            textureDesc.SampleDesc.Count    = 1;
            textureDesc.SampleDesc.Quality  = 0;
            textureDesc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

            auto defaultHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            ThrowIfFailed(m_device->CreateCommittedResource(&defaultHeapProps,
                                                            D3D12_HEAP_FLAG_NONE,
                                                            &textureDesc,
                                                            D3D12_RESOURCE_STATE_COPY_DEST,
                                                            nullptr,
                                                            IID_PPV_ARGS(&m_texture)));

            const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);

            // Create the GPU upload buffer.
            auto uploadBufDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
            ThrowIfFailed(m_device->CreateCommittedResource(&uploadHeapProps,
                                                            D3D12_HEAP_FLAG_NONE,
                                                            &uploadBufDesc,
                                                            D3D12_RESOURCE_STATE_GENERIC_READ,
                                                            nullptr,
                                                            IID_PPV_ARGS(&textureUploadHeap)));
            textureUploadHeap->SetName(L"textureUploadHeap");

            // Copy data to the intermediate upload heap and then schedule a copy
            // from the upload heap to the Texture2D.
            const UINT rowPitch    = TextureWidth * TexturePixelSize;
            const UINT cellPitch   = rowPitch >> 3;      // The width of a cell in the checkboard texture.
            const UINT cellHeight  = TextureWidth >> 3;  // The height of a cell in the checkerboard texture.
            const UINT textureSize = rowPitch * TextureHeight;

            std::vector<UINT8> data(textureSize);
            UINT8*             pData = &data[0];

            for (UINT n = 0; n < textureSize; n += TexturePixelSize)
            {
                UINT x = n % rowPitch;
                UINT y = n / rowPitch;
                UINT i = x / cellPitch;
                UINT j = y / cellHeight;

                if (i % 2 == j % 2)
                {
                    pData[n]     = 0xa0;  // R
                    pData[n + 1] = 0xa0;  // G
                    pData[n + 2] = 0xa0;  // B
                    pData[n + 3] = 0xff;  // A
                }
                else
                {
                    pData[n]     = 0xff;  // R
                    pData[n + 1] = 0xff;  // G
                    pData[n + 2] = 0xff;  // B
                    pData[n + 3] = 0xff;  // A
                }
            }

            D3D12_SUBRESOURCE_DATA textureData = {};
            textureData.pData                  = &data[0];
            textureData.RowPitch               = TextureWidth * TexturePixelSize;
            textureData.SlicePitch             = textureData.RowPitch * TextureHeight;

            auto uploadBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            UpdateSubresources(pInitCmdList, m_texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
            pInitCmdList->ResourceBarrier(1, &uploadBarrier);

            // Keep it around until upload cmdlist finishes executing.
            tempResources.push_back(textureUploadHeap);

            m_checkerboardTextureDescriptor = AllocStaticCBV_SRV_UAVs(1);
            m_device->CreateShaderResourceView(m_texture.Get(), NULL, m_checkerboardTextureDescriptor.GetCPU(0));
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

        TestRpsRenderToTexture::UpdateRpsPipeline(frameIndex, completedFrameIndex, backBufferDesc, backBuffers);
    }

private:
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12PipelineState> m_pipelineStateMSAA;

    ComPtr<ID3D12Resource>    m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW  m_triangleBufferView;
    D3D12_VERTEX_BUFFER_VIEW  m_quadsBufferView;
    ComPtr<ID3D12Resource>    m_constantBuffer;
    SceneConstantBufferD3D12* m_constantBufferCpuVA;
    DescriptorTable           m_triangleConstantBufferViews;
    DescriptorTable           m_quadsConstantBufferView;
    ComPtr<ID3D12Resource>    m_texture;
    DescriptorTable           m_checkerboardTextureDescriptor;
};

TEST_CASE(TEST_APP_NAME)
{
#if _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    TestD3D12RpsRenderToTexture renderer;

    RpsTestRunWindowInfo runInfo = {};
    runInfo.title                = TEXT(TEST_APP_NAME);
    runInfo.numFramesToRender    = g_exitAfterFrame;
    runInfo.width                = 1280;
    runInfo.height               = 720;
    runInfo.pRenderer            = &renderer;
    RpsTestRunWindowApp(&runInfo);
}
