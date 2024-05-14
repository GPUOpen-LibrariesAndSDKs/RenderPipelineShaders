// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#define RPS_D3D12_RUNTIME 1

#include "test_downsample_shared.hpp"

#include "utils/rps_test_win32.hpp"
#include "utils/rps_test_d3d12_renderer.hpp"

using namespace DirectX;

class TestD3D12Downsample : public RpsTestD3D12Renderer, public TestRpsDownsample
{
    static const UINT TextureWidth            = 256;
    static const UINT TextureHeight           = 256;
    static const UINT MaxConstantSizePerFrame = 65536;

    struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) GeoConstantBufferD3D12
        : public TestRpsDownsample::GeoConstantBuffer
    {
    };

protected:
    virtual void OnInit(ID3D12GraphicsCommandList*         pInitCmdList,
                        std::vector<ComPtr<ID3D12Object>>& tempResources) override
    {
        LoadAssets(pInitCmdList, tempResources);

        TestRpsDownsample::OnInit(rpsTestUtilCreateDevice(
            [this](auto pCreateInfo, auto phDevice) { return CreateRpsRuntimeDevice(*pCreateInfo, *phDevice); }));
    }

    virtual void BindNodes(RpsSubprogram hRpslEntry) override
    {
        RpsResult result =
            rpsProgramBindNode(rpsRenderGraphGetMainEntry(GetRpsRenderGraph()), "Quads", &DrawQuadsCb, this);
        REQUIRE(result == RPS_OK);

        result = rpsProgramBindNode(rpsRenderGraphGetMainEntry(GetRpsRenderGraph()), "Geo", &DrawGeoCb, this);
        REQUIRE(result == RPS_OK);

        result =
            rpsProgramBindNode(rpsRenderGraphGetMainEntry(GetRpsRenderGraph()), "Downsample", &DrawDownsampleCb, this);
        REQUIRE(result == RPS_OK);

        result = rpsProgramBindNode(
            rpsRenderGraphGetMainEntry(GetRpsRenderGraph()), "DownsampleCompute", &ComputeDownsampleCb, this);
        REQUIRE(result == RPS_OK);
    }

    virtual void OnPostResize() override
    {
    }

    virtual void OnCleanUp() override
    {
        TestRpsDownsample::OnDestroy();

        m_rootSignature                  = nullptr;
        m_rootSignatureCompute           = nullptr;
        m_defaultPipelineState           = nullptr;
        m_downsamplePipelineState        = nullptr;
        m_downsampleComputePipelineState = nullptr;
        m_vertexBuffer                   = nullptr;
        m_constantBuffer                 = nullptr;
        m_texture                        = nullptr;
    }

    virtual void OnUpdate(uint32_t frameIndex) override
    {
        TestRpsDownsample::OnUpdate(frameIndex, m_width, m_height);

        UpdatePipeline(frameIndex, CalcGuaranteedCompletedFrameIndexForRps());
    }

    virtual void OnRender(uint32_t frameIndex) override
    {
        m_frameConstantUsage = 0;

        ExecuteRenderGraph(frameIndex, GetRpsRenderGraph());
    }

private:
    void DrawGeo(const RpsCmdCallbackContext* pContext)
    {
        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);

        RpsVariable cmdArg = rpsCmdGetArg(pContext, 1);
        uint32_t triangleIndex = *static_cast<uint32_t*>(cmdArg);

        uint32_t triangleDataIndex = triangleIndex % _countof(m_TriangleCbData);

        BindDescriptorHeaps(pCmdList);
        pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
        pCmdList->SetPipelineState(m_defaultPipelineState.Get());

        D3D12_GPU_VIRTUAL_ADDRESS cbGpuVa = AllocAndWriteFrameConstants(&m_TriangleCbData[triangleDataIndex], sizeof(GeoConstantBuffer));
        pCmdList->SetGraphicsRootConstantBufferView(0, cbGpuVa);
        pCmdList->SetGraphicsRootDescriptorTable(1, m_checkerboardTextureDescriptor.GetGPU(0));

        pCmdList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->IASetVertexBuffers(0, 1, &m_triangleBufferView);
        pCmdList->DrawInstanced(3 * (triangleDataIndex + 1), 1, 0, 0);
    }

    void DrawQuads(const RpsCmdCallbackContext* pContext)
    {
        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);

        D3D12_CPU_DESCRIPTOR_HANDLE srcSrv;
        RpsResult result = rpsD3D12GetCmdArgDescriptor(pContext, 1, &srcSrv);
        REQUIRE(result == RPS_OK);
        D3D12_GPU_DESCRIPTOR_HANDLE srvTable =
            AllocDynamicDescriptorsAndWrite(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, &srcSrv, 1);

        RpsVariable cmdArg = rpsCmdGetArg(pContext, 2);
        uint32_t quadIndex = *static_cast<uint32_t*>(cmdArg);

        GeoConstantBuffer data;
        XMStoreFloat4x4(&data.offset,
                        XMMatrixAffineTransformation2D(
                            XMVectorSet(m_quadScale[0], m_quadScale[1], 1.f, 1.f),
                            XMVectorZero(),
                            0.f,
                            XMVectorSet(m_quadOffsets[quadIndex][0], m_quadOffsets[quadIndex][1], 0.f, 0.f)));
        data.color                        = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
        data.aspectRatio                  = 1.0f;
        D3D12_GPU_VIRTUAL_ADDRESS cbGpuVa = AllocAndWriteFrameConstants(&data, sizeof(data));

        pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
        pCmdList->SetPipelineState(m_defaultPipelineState.Get());

        BindDescriptorHeaps(pCmdList);

        pCmdList->SetGraphicsRootConstantBufferView(0, cbGpuVa);
        pCmdList->SetGraphicsRootDescriptorTable(1, srvTable);
        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->IASetVertexBuffers(0, 1, &m_quadsBufferView);
        pCmdList->DrawInstanced(6, 1, 0, 0);
    }

    void DrawDownsample(const RpsCmdCallbackContext* pContext)
    {
        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);

        D3D12_CPU_DESCRIPTOR_HANDLE srcSrv;
        RpsResult result = rpsD3D12GetCmdArgDescriptor(pContext, 1, &srcSrv);
        REQUIRE(result == RPS_OK);

        D3D12_GPU_DESCRIPTOR_HANDLE srvTable =
            AllocDynamicDescriptorsAndWrite(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, &srcSrv, 1);

        RpsVariable cmdArg = rpsCmdGetArg(pContext, 2);
        XMFLOAT2 invSize = *static_cast<XMFLOAT2*>(cmdArg);

        D3D12_GPU_VIRTUAL_ADDRESS cbGpuVa = AllocAndWriteFrameConstants(&invSize, sizeof(invSize));

        // Set necessary state.
        pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
        pCmdList->SetPipelineState(m_downsamplePipelineState.Get());

        BindDescriptorHeaps(pCmdList);
        pCmdList->SetGraphicsRootConstantBufferView(0, cbGpuVa);
        pCmdList->SetGraphicsRootDescriptorTable(1, srvTable);

        // Record commands.
        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->IASetVertexBuffers(0, 1, &m_quadsBufferView);
        pCmdList->DrawInstanced(6, 1, 0, 0);
    }

    void ComputeDownsample(const RpsCmdCallbackContext* pContext)
    {
        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);

        D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptors[2];
        RpsResult result = rpsD3D12GetCmdArgDescriptor(pContext, 1, &cpuDescriptors[0]);
        REQUIRE(result == RPS_OK);

        result = rpsD3D12GetCmdArgDescriptor(pContext, 0, &cpuDescriptors[1]);
        REQUIRE(result == RPS_OK);

        D3D12_GPU_DESCRIPTOR_HANDLE srvUavTable = AllocDynamicDescriptorsAndWrite(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, cpuDescriptors, _countof(cpuDescriptors));

        RpsVariable cmdArg = rpsCmdGetArg(pContext, 2);
        XMFLOAT2 invSize = *static_cast<XMFLOAT2*>(cmdArg);

        cmdArg = rpsCmdGetArg(pContext, 3);
        XMUINT2 dispatchGroups = *static_cast<XMUINT2*>(cmdArg);

        // Set necessary state.
        pCmdList->SetComputeRootSignature(m_rootSignatureCompute.Get());
        pCmdList->SetPipelineState(m_downsampleComputePipelineState.Get());

        BindDescriptorHeaps(pCmdList);
        pCmdList->SetComputeRoot32BitConstants(0, 2, &invSize, 0);
        pCmdList->SetComputeRootDescriptorTable(1, srvUavTable);
        pCmdList->Dispatch(dispatchGroups.x, dispatchGroups.y, 1);
    }

    static void DrawGeoCb(const RpsCmdCallbackContext* pContext)
    {
        static_cast<TestD3D12Downsample*>(pContext->pCmdCallbackContext)->DrawGeo(pContext);
    }

    static void DrawQuadsCb(const RpsCmdCallbackContext* pContext)
    {
        static_cast<TestD3D12Downsample*>(pContext->pCmdCallbackContext)->DrawQuads(pContext);
    }

    static void DrawDownsampleCb(const RpsCmdCallbackContext* pContext)
    {
        static_cast<TestD3D12Downsample*>(pContext->pCmdCallbackContext)->DrawDownsample(pContext);
    }

    static void ComputeDownsampleCb(const RpsCmdCallbackContext* pContext)
    {
        static_cast<TestD3D12Downsample*>(pContext->pCmdCallbackContext)->ComputeDownsample(pContext);
    }

private:
    D3D12_GPU_VIRTUAL_ADDRESS AllocAndWriteFrameConstants(const void* pSrcData, uint32_t size)
    {
        uint32_t allocSize = (size + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1) &
                             ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
        uint32_t newOffset = m_frameConstantUsage + allocSize;
        if (newOffset > MaxConstantSizePerFrame)
        {
            return NULL;
        }

        const uint32_t totalOffset = MaxConstantSizePerFrame * m_backBufferIndex + m_frameConstantUsage;
        memcpy(m_constantBufferCpuVA + totalOffset, pSrcData, size);

        m_frameConstantUsage = newOffset;
        return m_constantBuffer->GetGPUVirtualAddress() + totalOffset;
    }

    void LoadAssets(ID3D12GraphicsCommandList* pInitCmdList, std::vector<ComPtr<ID3D12Object>>& tempResources)
    {
        // Create a root signature consisting of a descriptor table with a single CBV.

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter                    = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
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
        sampler.ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;

        {
            CD3DX12_DESCRIPTOR_RANGE ranges[1]         = {};
            CD3DX12_ROOT_PARAMETER   rootParameters[2] = {};

            ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
            rootParameters[0].InitAsConstantBufferView(0);
            rootParameters[1].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

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

        {
            CD3DX12_DESCRIPTOR_RANGE ranges[2]         = {};
            CD3DX12_ROOT_PARAMETER   rootParameters[2] = {};
            ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
            ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

            rootParameters[0].InitAsConstants(2, 0);
            rootParameters[1].InitAsDescriptorTable(_countof(ranges), &ranges[0], D3D12_SHADER_VISIBILITY_ALL);

            CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
            rootSignatureDesc.Init_1_0(
                _countof(rootParameters), rootParameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE);

            ComPtr<ID3DBlob> signature;
            ComPtr<ID3DBlob> error;
            ThrowIfFailed(D3DX12SerializeVersionedRootSignature(
                &rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signature, &error));
            ThrowIfFailed(m_device->CreateRootSignature(
                0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignatureCompute)));
        }

        // Create the pipeline state, which includes compiling and loading shaders.
        CreateGraphicsPipeline(c_DefaultShader, sizeof(c_DefaultShader), L"VSMain", L"PSMain", &m_defaultPipelineState);
        CreateGraphicsPipeline(
            c_DownsampleShader, sizeof(c_DownsampleShader), L"VSMain", L"PSMain", &m_downsamplePipelineState);
        CreateComputePipeline(
            c_DownsampleShader, sizeof(c_DownsampleShader), L"CSMain", &m_downsampleComputePipelineState);

        // Create constant buffers
        {
            CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            CD3DX12_RESOURCE_DESC   resourceDesc   = CD3DX12_RESOURCE_DESC::Buffer(
                MaxConstantSizePerFrame * m_backBufferCount, D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
            ThrowIfFailed(m_device->CreateCommittedResource(
                &heapProperties,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&m_constantBuffer)));

            CD3DX12_RANGE range = CD3DX12_RANGE(0, 0);
            ThrowIfFailed(m_constantBuffer->Map(0, &range, reinterpret_cast<void**>(&m_constantBufferCpuVA)));
        }

        // Create vertex buffers
        {
            // Define the geometry for a triangle.
            Vertex triangleVertices[] = {
                // triangle
                {{0.0f, 0.25f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.5f, 0.0f}},
                {{0.25f, -0.25f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                {{-0.25f, -0.25f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

                {{0.0f, 0.25f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.5f, 0.0f}},
                {{0.5f, 0.25f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
                {{0.25f, -0.25f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},

                {{0.5f, 0.25f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.5f, 0.0f}},
                {{0.75f, -0.25f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                {{0.25f, -0.25f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},

                {{0.5f, 0.25f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.5f, 0.0f}},
                {{1.0f, 0.25f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
                {{0.75f, -0.25f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},

                // quad 0
                {{-1.0f, 1.0, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
                {{1.f, -1.f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
                {{-1.f, -1.f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
                {{-1.0f, 1.0, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
                {{1.f, 1.f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
                {{1.f, -1.f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
            };

            const UINT vertexBufferSize = sizeof(triangleVertices);

            // Note: using upload heaps to transfer static data like vert buffers is not
            // recommended. Every time the GPU needs it, the upload heap will be marshalled
            // over. Please read up on Default Heap usage. An upload heap is used here for
            // code simplicity and because there are very few verts to actually transfer.
            CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            CD3DX12_RESOURCE_DESC   resourceDesc   = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
            ThrowIfFailed(m_device->CreateCommittedResource(&heapProperties,
                                                            D3D12_HEAP_FLAG_NONE,
                                                            &resourceDesc,
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
            m_triangleBufferView.SizeInBytes    = sizeof(Vertex) * 12;

            m_quadsBufferView.BufferLocation = m_triangleBufferView.BufferLocation + m_triangleBufferView.SizeInBytes;
            m_quadsBufferView.StrideInBytes  = sizeof(Vertex);
            m_quadsBufferView.SizeInBytes    = sizeof(Vertex) * 6;
        }

        // Create checkerboard texture
        float tintColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
        RpsTestD3D12Renderer::CreateStaticCheckerboardTexture(m_texture, tempResources, pInitCmdList, 256, 256, tintColor);
        m_checkerboardTextureDescriptor = AllocStaticCBV_SRV_UAVs(1);
        m_device->CreateShaderResourceView(m_texture.Get(), NULL, m_checkerboardTextureDescriptor.GetCPU(0));
    }

    void CreateGraphicsPipeline(
        const char* shader, size_t shaderLength, LPCWSTR vsEntry, LPCWSTR psEntry, ID3D12PipelineState** ppPipeline)
    {
        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

        std::vector<uint8_t> vertexShader, pixelShader, err;

        DxcCompile(shader, vsEntry, L"vs_6_0", L"", nullptr, 0, vertexShader);
        DxcCompile(shader, psEntry, L"ps_6_0", L"", nullptr, 0, pixelShader);

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout                        = {inputElementDescs, _countof(inputElementDescs)};
        psoDesc.pRootSignature                     = m_rootSignature.Get();
        psoDesc.VS                                 = CD3DX12_SHADER_BYTECODE(vertexShader.data(), vertexShader.size());
        psoDesc.PS                                 = CD3DX12_SHADER_BYTECODE(pixelShader.data(), pixelShader.size());
        psoDesc.RasterizerState                    = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState                         = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable      = FALSE;
        psoDesc.DepthStencilState.StencilEnable    = FALSE;
        psoDesc.SampleMask                         = UINT_MAX;
        psoDesc.PrimitiveTopologyType              = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets                   = 1;
        psoDesc.RTVFormats[0]                      = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count                   = 1;

        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(ppPipeline)));
    }

    void CreateComputePipeline(const char* shader, size_t length, LPCWSTR entry, ID3D12PipelineState** ppPipeline)
    {
        std::vector<uint8_t> computeShader;
        DxcCompile(shader, entry, L"cs_6_0", L"", nullptr, 0, computeShader);

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.CS                                = CD3DX12_SHADER_BYTECODE(computeShader.data(), computeShader.size());
        psoDesc.NodeMask                          = 1;
        psoDesc.pRootSignature                    = m_rootSignatureCompute.Get();

        ThrowIfFailed(m_device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(ppPipeline)));
    }

    void UpdatePipeline(uint64_t frameIndex, uint64_t completedFrameIndex)
    {
        RpsRuntimeResource backBuffers[DXGI_MAX_SWAP_CHAIN_BUFFERS];
        RpsResourceDesc backBufferDesc   = {};

        GetBackBuffers(backBufferDesc, backBuffers);

        TestRpsDownsample::UpdateRpsPipeline(frameIndex, completedFrameIndex, backBufferDesc, backBuffers);
    }

private:
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12RootSignature> m_rootSignatureCompute;
    ComPtr<ID3D12PipelineState> m_defaultPipelineState;
    ComPtr<ID3D12PipelineState> m_downsamplePipelineState;
    ComPtr<ID3D12PipelineState> m_downsampleComputePipelineState;

    ComPtr<ID3D12Resource>   m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_triangleBufferView;
    D3D12_VERTEX_BUFFER_VIEW m_quadsBufferView;
    ComPtr<ID3D12Resource>   m_constantBuffer;
    uint8_t*                 m_constantBufferCpuVA;
    uint32_t                 m_frameConstantUsage = 0;
    ComPtr<ID3D12Resource>   m_texture;
    DescriptorTable          m_checkerboardTextureDescriptor;

    std::vector<FenceSignalInfo> m_fenceSignalInfos;
};

TEST_CASE(TEST_APP_NAME)
{
    TestD3D12Downsample renderer;

    RpsTestRunWindowInfo runInfo = {};
    runInfo.title                = TEXT(TEST_APP_NAME);
    runInfo.numFramesToRender    = g_exitAfterFrame;
    runInfo.width                = 1280;
    runInfo.height               = 720;
    runInfo.pRenderer            = &renderer;
    RpsTestRunWindowApp(&runInfo);
}
