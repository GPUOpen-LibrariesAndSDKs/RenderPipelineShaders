// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#define RPS_D3D12_RUNTIME 1

#include "test_mrt_viewport_clear_shared.hpp"

#include "utils/rps_test_win32.hpp"
#include "utils/rps_test_d3d12_renderer.hpp"

using namespace DirectX;

class TestD3D12MrtViewportClear : public RpsTestD3D12Renderer, public TestRpsMrtViewportClear
{
protected:
    virtual void OnInit(ID3D12GraphicsCommandList*         pInitCmdList,
                        std::vector<ComPtr<ID3D12Object>>& tempResources) override
    {
        LoadAssets(pInitCmdList, tempResources);

        TestRpsMrtViewportClear::Init(rpsTestUtilCreateDevice(
            [this](auto pCreateInfo, auto phDevice) { return CreateRpsRuntimeDevice(*pCreateInfo, *phDevice); }));
    }

    virtual void OnPostResize() override
    {
    }

    virtual void OnCleanUp() override
    {
        TestRpsMrtViewportClear::OnDestroy();

        m_rootSignature                      = nullptr;
        m_pipelineStateRtBuffer              = nullptr;
        m_pipelineStateMrt5NoDS              = nullptr;
        m_pipelineStateMrt3DS                = nullptr;
        m_pipelineStateRtArray               = nullptr;
        m_pipelineStateRtArrayCube           = nullptr;
        m_pipelineStateBlt                   = nullptr;
        m_pipelineStateBltCube               = nullptr;
        m_pipelineStateWriteDepthStencil     = nullptr;
        m_pipelineStateReadDepthWriteStencil = nullptr;
        m_pipelineStateReadDepthStencil      = nullptr;
    }

    virtual void OnUpdate(uint32_t frameIndex) override
    {
        RpsRuntimeResource backBuffers[DXGI_MAX_SWAP_CHAIN_BUFFERS];
        RpsResourceDesc backBufferDesc;
        GetBackBuffers(backBufferDesc, backBuffers);

        const bool bBufferRTVSupported = false;

        RpsConstant               args[]         = {&backBufferDesc, &bBufferRTVSupported};
        const RpsRuntimeResource* argResources[] = {backBuffers, nullptr};

        uint64_t completedFrameIndex = CalcGuaranteedCompletedFrameIndexForRps();

        TestRpsMrtViewportClear::OnUpdate(
            frameIndex, completedFrameIndex, uint32_t(RPS_TEST_COUNTOF(args)), args, argResources);

        RpsTestD3D12Renderer::OnUpdate(frameIndex);
    }

    virtual void OnRender(uint32_t frameIndex) override
    {
        REQUIRE(RPS_SUCCEEDED(ExecuteRenderGraph(frameIndex, GetRpsRenderGraph())));
    }

protected:
    virtual void BindNodes(RpsSubprogram hRpslEntry) override final
    {
        TestRpsMrtViewportClear::BindNodes(hRpslEntry);

        RpsResult result =
            rpsProgramBindNode(hRpslEntry, "test_buffer_rtv", &TestD3D12MrtViewportClear::DrawRtbuffer, this);
        REQUIRE(result == RPS_OK);

        result =
            rpsProgramBindNode(hRpslEntry, "test_mrt_with_array", &TestD3D12MrtViewportClear::DrawMrtWithArray, this);
        REQUIRE(result == RPS_OK);

        result =
            rpsProgramBindNode(hRpslEntry, "blt_to_swapchain", &TestD3D12MrtViewportClear::DrawBlt, this);
        REQUIRE(result == RPS_OK);

        result =
            rpsProgramBindNode(hRpslEntry, "draw_cube_to_swapchain", &TestD3D12MrtViewportClear::DrawBltCube, this);
        REQUIRE(result == RPS_OK);

        result = rpsProgramBindNode(hRpslEntry,
                                    "test_bind_dsv_write_depth_stencil",
                                    &TestD3D12MrtViewportClear::BindDsvWriteDepthStencil,
                                    this);
        REQUIRE(result == RPS_OK);

        result = rpsProgramBindNode(hRpslEntry,
                                    "test_bind_dsv_read_depth_write_stencil",
                                    &TestD3D12MrtViewportClear::BindDsvReadDepthWriteStencil,
                                    this);
        REQUIRE(result == RPS_OK);

        result = rpsProgramBindNode(
            hRpslEntry, "test_bind_dsv_read_depth_stencil", &TestD3D12MrtViewportClear::BindDsvReadDepthStencil, this);
        REQUIRE(result == RPS_OK);
    }

    void DrawTriangle(ID3D12GraphicsCommandList* pCmdList, ID3D12PipelineState *pipelineState) const
    {
        pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
        pCmdList->SetPipelineState(pipelineState);
        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->DrawInstanced(3, 1, 0, 0);
    } 

    void DrawRtbuffer(const RpsCmdCallbackContext* pContext)
    {
        CreateRtBuffer(pContext);
        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);
        DrawTriangle(pCmdList, m_pipelineStateRtBuffer.Get());
    }

    virtual void Draw5MrtNoDS(const RpsCmdCallbackContext* pContext) override final
    {
        Create5MrtNoDS(pContext);
        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);
        DrawTriangle(pCmdList, m_pipelineStateMrt5NoDS.Get());
    }

    virtual void Draw3MrtDS(const RpsCmdCallbackContext* pContext) override final
    {
        Create3MrtDS(pContext);
        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);
        DrawTriangle(pCmdList, m_pipelineStateMrt3DS.Get());
    }

    virtual void DrawRtArray(const RpsCmdCallbackContext* pContext) override final
    {
        CreateRtArray(pContext);
        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);
        DrawTriangle(pCmdList, m_pipelineStateRtArray.Get());
    }

    virtual void DrawMrtWithArray(const RpsCmdCallbackContext* pContext) override final
    {
        CreateMrtWithArray(pContext);

        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);

        RpsParameterDesc paramDesc;
        RpsResult        result = rpsCmdGetParamDesc(pContext, 2, &paramDesc);
        REQUIRE(result == RPS_OK);
        REQUIRE(paramDesc.arraySize == 12);

        DescriptorTable dt = AllocDynamicDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, paramDesc.arraySize);
        D3D12_CPU_DESCRIPTOR_HANDLE dstHdl = dt.GetCPU(0);
        result = rpsD3D12CopyCmdArgDescriptors(pContext, 2, 0, paramDesc.arraySize, RPS_TRUE, &dstHdl);
        REQUIRE(result == RPS_OK);

        pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
        BindDescriptorHeaps(pCmdList);
        pCmdList->SetGraphicsRootDescriptorTable(1, dt.GetGPU(0));
        pCmdList->SetPipelineState(m_pipelineStateRtArrayCube.Get());
        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->DrawInstanced(3, 1, 0, 0);
    }

    virtual void DrawLargeArray(const RpsCmdCallbackContext* pContext) override final
    {
    }

    void DrawBlt(const RpsCmdCallbackContext* pContext,
                 RpsRuntimeResource           dstRuntimeResource,
                 D3D12_CPU_DESCRIPTOR_HANDLE  src,
                 const ViewportData&          dstViewport)
    {
        CreateBlt(pContext);

        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);

        RpsCmdViewportInfo viewportScissorInfo = {};
        RpsResult result = rpsCmdGetViewportInfo(pContext, &viewportScissorInfo);
        REQUIRE(result == RPS_OK);
        REQUIRE(viewportScissorInfo.numViewports == 1);
        REQUIRE(dstViewport.data.x == viewportScissorInfo.pViewports[0].x);
        REQUIRE(dstViewport.data.y == viewportScissorInfo.pViewports[0].y);
        REQUIRE(dstViewport.data.z == viewportScissorInfo.pViewports[0].width);
        REQUIRE(dstViewport.data.w == viewportScissorInfo.pViewports[0].height);

        if (m_frameCounter < (m_backBufferCount * 2))
        {
            RpsResourceDesc resourceDesc = {};
            RpsRuntimeResource runtimeResource;
            REQUIRE(RPS_SUCCEEDED(rpsCmdGetArgResourceDesc(pContext, 0, &resourceDesc)));
            REQUIRE(RPS_SUCCEEDED(rpsCmdGetArgResourceDesc(pContext, 1, &resourceDesc)));
            REQUIRE(RPS_ERROR_INDEX_OUT_OF_BOUNDS == rpsCmdGetArgResourceDescArray(pContext, 0, 1, &resourceDesc, 1));
            REQUIRE(RPS_ERROR_INDEX_OUT_OF_BOUNDS == rpsCmdGetArgResourceDescArray(pContext, 1, 1, &resourceDesc, 1));
            REQUIRE(RPS_ERROR_TYPE_MISMATCH ==
                    rpsCmdGetArgResourceDesc(pContext, 2, &resourceDesc));  // Not a resource
            REQUIRE(RPS_ERROR_INDEX_OUT_OF_BOUNDS == rpsCmdGetArgResourceDescArray(pContext, 4, 1, &resourceDesc, 1));

            REQUIRE(RPS_SUCCEEDED(rpsCmdGetArgRuntimeResource(pContext, 0, &runtimeResource)));
            REQUIRE(runtimeResource == dstRuntimeResource);

            ID3D12Resource*             pResource     = nullptr;
            D3D12_CPU_DESCRIPTOR_HANDLE descriptorHdl = {};
            REQUIRE((RPS_SUCCEEDED(rpsD3D12GetCmdArgResource(pContext, 0, &pResource)) && (pResource != nullptr)));
            pResource = nullptr;
            REQUIRE((RPS_SUCCEEDED(rpsD3D12GetCmdArgResourceArray(pContext, 0, 0, &pResource, 1)) &&
                     (pResource != nullptr)));
            REQUIRE(RPS_ERROR_INDEX_OUT_OF_BOUNDS == rpsD3D12GetCmdArgResourceArray(pContext, 0, 1, &pResource, 1));
            REQUIRE(RPS_ERROR_INDEX_OUT_OF_BOUNDS == rpsD3D12GetCmdArgResourceArray(pContext, 0, 0, &pResource, 2));

            REQUIRE(RPS_SUCCEEDED(rpsD3D12GetCmdArgDescriptor(pContext, 0, &descriptorHdl)));
            REQUIRE(RPS_SUCCEEDED(rpsD3D12GetCmdArgDescriptorArray(pContext, 0, 0, &descriptorHdl, 1)));
            REQUIRE(RPS_ERROR_INDEX_OUT_OF_BOUNDS ==
                    rpsD3D12GetCmdArgDescriptorArray(pContext, 0, 1, &descriptorHdl, 1));
            REQUIRE(RPS_ERROR_INDEX_OUT_OF_BOUNDS ==
                    rpsD3D12GetCmdArgDescriptorArray(pContext, 0, 0, &descriptorHdl, 2));

            pResource     = nullptr;
            descriptorHdl = {};
            REQUIRE((RPS_SUCCEEDED(rpsD3D12GetCmdArgResource(pContext, 1, &pResource)) && (pResource != nullptr)));
            REQUIRE(RPS_ERROR_TYPE_MISMATCH == rpsD3D12GetCmdArgResource(pContext, 2, &pResource));
        }

        D3D12_GPU_DESCRIPTOR_HANDLE srvTable = AllocDynamicDescriptorsAndWrite(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, &src, 1);

        pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
        BindDescriptorHeaps(pCmdList);

        pCmdList->SetPipelineState(m_pipelineStateBlt.Get());
        pCmdList->SetGraphicsRootDescriptorTable(1, srvTable);
        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->DrawInstanced(3, 1, 0, 0);
    }

    void DrawBltCube(const RpsCmdCallbackContext* pContext,
                     rps::UnusedArg               dst,
                     D3D12_CPU_DESCRIPTOR_HANDLE  src,
                     const ViewportData&          dstViewport)
    {
        CreateBltCube(pContext);

        ID3D12GraphicsCommandList*  pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);
        D3D12_GPU_DESCRIPTOR_HANDLE srvTable =
            AllocDynamicDescriptorsAndWrite(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, &src, 1);

        pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
        BindDescriptorHeaps(pCmdList);

        pCmdList->SetPipelineState(m_pipelineStateBltCube.Get());
        pCmdList->SetGraphicsRootDescriptorTable(1, srvTable);
        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->DrawInstanced(3, 1, 0, 0);
    }

    void BindDsvWriteDepthStencil(const RpsCmdCallbackContext* pContext)
    {
        if (!m_pipelineStateWriteDepthStencil)
        {
            RpsCmdRenderTargetInfo rtInfo;
            RpsResult              result = rpsCmdGetRenderTargetsInfo(pContext, &rtInfo);
            REQUIRE(result == RPS_OK);

            auto depthStencilState                    = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
            depthStencilState.StencilEnable           = TRUE;
            depthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
            depthStencilState.FrontFace.StencilFunc   = D3D12_COMPARISON_FUNC_ALWAYS;

            CreatePSO(L"VSSimple",
                      nullptr,
                      L"PSWriteDepthStencil",
                      true,
                      &rtInfo,
                      &m_pipelineStateWriteDepthStencil,
                      &depthStencilState);
        }

        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);

        pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
        pCmdList->SetPipelineState(m_pipelineStateWriteDepthStencil.Get());
        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->OMSetStencilRef(0x1);
        pCmdList->SetGraphicsRoot32BitConstant(0, 0, 0);
        pCmdList->DrawInstanced(3, 1, 0, 0);
        pCmdList->OMSetStencilRef(0x2);
        pCmdList->SetGraphicsRoot32BitConstant(0, 1, 0);
        pCmdList->DrawInstanced(3, 1, 0, 0);
    }

    void BindDsvReadDepthWriteStencil(const RpsCmdCallbackContext* pContext, D3D12_CPU_DESCRIPTOR_HANDLE depthSrv)
    {
        if (!m_pipelineStateReadDepthWriteStencil)
        {
            RpsCmdRenderTargetInfo rtInfo;
            RpsResult              result = rpsCmdGetRenderTargetsInfo(pContext, &rtInfo);
            REQUIRE(result == RPS_OK);

            auto depthStencilState                         = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
            depthStencilState.DepthWriteMask               = D3D12_DEPTH_WRITE_MASK_ZERO;
            depthStencilState.StencilEnable                = TRUE;
            depthStencilState.FrontFace.StencilPassOp      = D3D12_STENCIL_OP_INCR;
            depthStencilState.FrontFace.StencilFailOp      = D3D12_STENCIL_OP_KEEP;
            depthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
            depthStencilState.FrontFace.StencilFunc        = D3D12_COMPARISON_FUNC_EQUAL;

            CreatePSO(L"VSSimpleFlatDepth",
                      nullptr,
                      L"PSReadDepthWriteStencil",
                      true,
                      &rtInfo,
                      &m_pipelineStateReadDepthWriteStencil,
                      &depthStencilState);
        }

        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);

        D3D12_GPU_DESCRIPTOR_HANDLE depthSrvGpu = AllocDynamicDescriptorsAndWrite(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, &depthSrv, 1);

        BindDescriptorHeaps(pCmdList);

        pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
        pCmdList->SetPipelineState(m_pipelineStateReadDepthWriteStencil.Get());
        pCmdList->SetGraphicsRootDescriptorTable(1, depthSrvGpu);
        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->OMSetStencilRef(0x2);
        pCmdList->SetGraphicsRoot32BitConstant(0, AsUInt(0.25f), 1);
        pCmdList->DrawInstanced(3, 1, 0, 0);
    }

    void BindDsvReadDepthStencil(const RpsCmdCallbackContext* pContext,
                                 D3D12_CPU_DESCRIPTOR_HANDLE  depthSrv,
                                 D3D12_CPU_DESCRIPTOR_HANDLE  stencilSrv)
    {
        if (!m_pipelineStateReadDepthStencil)
        {
            RpsCmdRenderTargetInfo rtInfo;
            RpsResult              result = rpsCmdGetRenderTargetsInfo(pContext, &rtInfo);
            REQUIRE(result == RPS_OK);

            auto depthStencilState                         = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
            depthStencilState.DepthWriteMask               = D3D12_DEPTH_WRITE_MASK_ZERO;
            depthStencilState.StencilEnable                = TRUE;
            depthStencilState.StencilReadMask              = 0x3;
            depthStencilState.FrontFace.StencilPassOp      = D3D12_STENCIL_OP_KEEP;
            depthStencilState.FrontFace.StencilFailOp      = D3D12_STENCIL_OP_KEEP;
            depthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
            depthStencilState.FrontFace.StencilFunc        = D3D12_COMPARISON_FUNC_EQUAL;

            CreatePSO(L"VSSimpleFlatDepth",
                      nullptr,
                      L"PSReadDepthStencil",
                      true,
                      &rtInfo,
                      &m_pipelineStateReadDepthStencil,
                      &depthStencilState);
        }

        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);

        D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHdls[] = {depthSrv, stencilSrv};

        D3D12_GPU_DESCRIPTOR_HANDLE dsSrvsGpu =
            AllocDynamicDescriptorsAndWrite(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srvCpuHdls, RPS_TEST_COUNTOF(srvCpuHdls));

        BindDescriptorHeaps(pCmdList);

        pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
        pCmdList->SetPipelineState(m_pipelineStateReadDepthStencil.Get());
        pCmdList->SetGraphicsRootDescriptorTable(1, dsSrvsGpu);
        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->OMSetStencilRef(0x3);
        pCmdList->SetGraphicsRoot32BitConstant(0, AsUInt(0.5f), 1);
        pCmdList->DrawInstanced(3, 1, 0, 0);
    }

    void CreateRtBuffer(const RpsCmdCallbackContext* pContext)
    {
        if (!m_pipelineStateRtBuffer)
        {
            RpsCmdRenderTargetInfo rtInfo;
            RpsResult              result = rpsCmdGetRenderTargetsInfo(pContext, &rtInfo);
            REQUIRE(result == RPS_OK);

            CreatePSO(L"VSSimple", nullptr, L"PSMrt5", false, &rtInfo, &m_pipelineStateRtBuffer);
        }
    }

    void Create5MrtNoDS(const RpsCmdCallbackContext* pContext)
    {
        if (!m_pipelineStateMrt5NoDS)
        {
            RpsCmdRenderTargetInfo rtInfo;
            RpsResult              result = rpsCmdGetRenderTargetsInfo(pContext, &rtInfo);
            REQUIRE(result == RPS_OK);

            CreatePSO(L"VSSimple", nullptr, L"PSMrt5", false, &rtInfo, &m_pipelineStateMrt5NoDS);
        }
    }

    void Create3MrtDS(const RpsCmdCallbackContext* pContext)
    {
        if (!m_pipelineStateMrt3DS)
        {
            RpsCmdRenderTargetInfo rtInfo;
            RpsResult              result = rpsCmdGetRenderTargetsInfo(pContext, &rtInfo);
            REQUIRE(result == RPS_OK);

            CreatePSO(L"VSSimple", nullptr, L"PSMrt3", true, &rtInfo, &m_pipelineStateMrt3DS);
        }
    }

    void CreateRtArray(const RpsCmdCallbackContext* pContext)
    {
        if (!m_pipelineStateRtArray)
        {
            RpsCmdRenderTargetInfo rtInfo;
            RpsResult              result = rpsCmdGetRenderTargetsInfo(pContext, &rtInfo);
            REQUIRE(result == RPS_OK);

            CreatePSO(L"VSRtArray", L"GSRtArray", L"PSRtArray", false, &rtInfo, &m_pipelineStateRtArray);
        }
    }

    void CreateMrtWithArray(const RpsCmdCallbackContext* pContext)
    {
        if (!m_pipelineStateRtArrayCube)
        {
            RpsCmdRenderTargetInfo rtInfo;
            RpsResult              result = rpsCmdGetRenderTargetsInfo(pContext, &rtInfo);
            REQUIRE(result == RPS_OK);

            CreatePSO(
                L"VSRtArray", L"GSRtArrayToCube", L"PSRtArrayToCubeMRT", false, &rtInfo, &m_pipelineStateRtArrayCube);
        }
    }

    void CreateLargeArray(const RpsCmdCallbackContext* pContext)
    {
    }

    void CreateBlt(const RpsCmdCallbackContext* pContext)
    {
        if (!m_pipelineStateBlt)
        {
            RpsCmdRenderTargetInfo rtInfo;
            RpsResult              result = rpsCmdGetRenderTargetsInfo(pContext, &rtInfo);
            REQUIRE(result == RPS_OK);

            CreatePSO(L"VSBlt", nullptr, L"PSBlt", false, &rtInfo, &m_pipelineStateBlt);
        }
    }

    void CreateBltCube(const RpsCmdCallbackContext* pContext)
    {
        if (!m_pipelineStateBltCube)
        {
            RpsCmdRenderTargetInfo rtInfo;
            RpsResult              result = rpsCmdGetRenderTargetsInfo(pContext, &rtInfo);
            REQUIRE(result == RPS_OK);

            CreatePSO(L"VSBlt", nullptr, L"PSBltCube", false, &rtInfo, &m_pipelineStateBltCube);
        }
    }

private:
    void LoadAssets(ID3D12GraphicsCommandList* pInitCmdList, std::vector<ComPtr<ID3D12Object>>& tempResources)
    {
        CD3DX12_DESCRIPTOR_RANGE ranges[1]         = {};
        CD3DX12_ROOT_PARAMETER   rootParameters[2] = {};

        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 12, 0);
        rootParameters[0].InitAsConstants(2, 0);
        rootParameters[1].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

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

        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_0(_countof(rootParameters), rootParameters, 1, &sampler, rootSignatureFlags);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signature, &error));
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    }

    void CreatePSO(LPCWSTR                           vsEntry,
                   LPCWSTR                           gsEntry,
                   LPCWSTR                           psEntry,
                   bool                              bDepthEnable,
                   const RpsCmdRenderTargetInfo*     pRenderTargetInfo,
                   ID3D12PipelineState**             ppPSO,
                   const CD3DX12_DEPTH_STENCIL_DESC* pCustomDepthStencilDesc = nullptr)
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

            if (pCustomDepthStencilDesc)
            {
                psoDesc.DepthStencilState = *pCustomDepthStencilDesc;
            }
            else
            {
                psoDesc.DepthStencilState             = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
                psoDesc.DepthStencilState.DepthEnable = !!bDepthEnable;
            }

            psoDesc.SampleMask                         = UINT_MAX;
            psoDesc.PrimitiveTopologyType              = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

            psoDesc.DSVFormat        = rpsFormatToDXGI(pRenderTargetInfo->depthStencilFormat);
            psoDesc.SampleDesc.Count = pRenderTargetInfo->numSamples;
            psoDesc.NumRenderTargets = pRenderTargetInfo->numRenderTargets;

            for (uint32_t iRT = 0; iRT < pRenderTargetInfo->numRenderTargets; iRT++)
            {
                psoDesc.RTVFormats[iRT] = rpsFormatToDXGI(pRenderTargetInfo->renderTargetFormats[iRT]);
            }

            DxcCompile(c_Shader, vsEntry, L"vs_6_0", L"", nullptr, 0, vsCode);
            DxcCompile(c_Shader, psEntry, L"ps_6_0", L"", nullptr, 0, psCode);
            psoDesc.VS = CD3DX12_SHADER_BYTECODE(vsCode.data(), vsCode.size());
            psoDesc.PS = CD3DX12_SHADER_BYTECODE(psCode.data(), psCode.size());

            if (gsEntry)
            {
                DxcCompile(c_Shader, gsEntry, L"gs_6_0", L"", nullptr, 0, gsCode);
                psoDesc.GS = CD3DX12_SHADER_BYTECODE(gsCode.data(), gsCode.size());
            }

            ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(ppPSO)));
        }
    }

private:
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineStateRtBuffer;
    ComPtr<ID3D12PipelineState> m_pipelineStateMrt5NoDS;
    ComPtr<ID3D12PipelineState> m_pipelineStateMrt3DS;
    ComPtr<ID3D12PipelineState> m_pipelineStateRtArray;
    ComPtr<ID3D12PipelineState> m_pipelineStateRtArrayCube;
    ComPtr<ID3D12PipelineState> m_pipelineStateBlt;
    ComPtr<ID3D12PipelineState> m_pipelineStateBltCube;
    ComPtr<ID3D12PipelineState> m_pipelineStateWriteDepthStencil;
    ComPtr<ID3D12PipelineState> m_pipelineStateReadDepthWriteStencil;
    ComPtr<ID3D12PipelineState> m_pipelineStateReadDepthStencil;
};

TEST_CASE(TEST_APP_NAME)
{
#if _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    TestD3D12MrtViewportClear renderer;

    RpsTestRunWindowInfo runInfo = {};
    runInfo.title                = TEXT(TEST_APP_NAME);
    runInfo.numFramesToRender    = g_exitAfterFrame;
    runInfo.width                = 1280;
    runInfo.height               = 720;
    runInfo.pRenderer            = &renderer;
    RpsTestRunWindowApp(&runInfo);
}
