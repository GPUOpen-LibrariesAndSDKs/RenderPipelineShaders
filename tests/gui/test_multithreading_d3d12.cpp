// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "test_multithreading_shared.hpp"

#include "rps/runtime/d3d12/rps_d3d12_runtime.h"
#include "utils/rps_test_win32.hpp"
#include "utils/rps_test_d3d12_renderer.hpp"

using namespace DirectX;

class TestD3D12Multithreading : public RpsTestD3D12Renderer, public TestRpsMultithreading
{
protected:
    virtual void OnInit(ID3D12GraphicsCommandList* pInitCmdList, std::vector<ComPtr<ID3D12Object>>& tempResources) override
    {
        LoadAssets(pInitCmdList, tempResources);

        TestRpsMultithreading::Init(rpsTestUtilCreateDevice(
            [this](auto pCreateInfo, auto phDevice) { return CreateRpsRuntimeDevice(*pCreateInfo, *phDevice); }));
    }

    virtual void OnCleanUp() override
    {
        TestRpsMultithreading::OnDestroy();

        m_rootSignature     = nullptr;
        m_pipelineState     = nullptr;
    }

    virtual void OnUpdate(uint32_t frameIndex) override
    {
        UpdatePipeline(frameIndex, CalcGuaranteedCompletedFrameIndexForRps());

        if (frameIndex < 50 * MAX_THREADS)
        {
            SetRenderJobCount(frameIndex / 50 + 1);
        }
    }

    virtual RpsRuntimeCommandBuffer AcquireNewCommandBuffer(uint32_t* pInsertAfter) override final
    {
        std::lock_guard<std::mutex> lock(m_cmdListsMutex);

        uint32_t newIdx = uint32_t(m_activeCmdLists.size());

        m_activeCmdLists.push_back({
            AcquireCmdList(RPS_AFX_QUEUE_INDEX_GFX),
            UINT32_MAX,
        });

        if (pInsertAfter)
        {
            if (*pInsertAfter != UINT32_MAX)
            {
                uint32_t next                        = m_activeCmdLists[*pInsertAfter].Next;
                m_activeCmdLists[*pInsertAfter].Next = newIdx;
                m_activeCmdLists.back().Next         = next;
            }

            *pInsertAfter = newIdx;
        }

        return rpsD3D12CommandListToHandle(m_activeCmdLists.back().CmdList.cmdList.Get());
    }

    virtual void OnRender(uint32_t frameIndex) override
    {
        m_activeCmdLists.clear();

        TestRpsMultithreading::OnRender(frameIndex, m_numPasses);

        for (auto& cl : m_activeCmdLists)
        {
            CloseCmdList(cl.CmdList);
        }

        m_cmdListsToSubmit.clear();
        m_cmdListsToSubmit.reserve(m_activeCmdLists.size());

        for (uint32_t i = 0; i < m_activeCmdLists.size(); i = m_activeCmdLists[i].Next)
        {
            m_cmdListsToSubmit.push_back(m_activeCmdLists[i].CmdList.cmdList.Get());
        }

        REQUIRE(m_cmdListsToSubmit.size() == m_activeCmdLists.size());

        if (m_cmdListsToSubmit.size() > 0)
        {
            m_presentQueue->ExecuteCommandLists(uint32_t(m_cmdListsToSubmit.size()), m_cmdListsToSubmit.data());
        }

        for (uint32_t i = 0; i < m_activeCmdLists.size(); i++)
        {
            RecycleCmdList(m_activeCmdLists[i].CmdList);
        }
    }

    virtual void OnKeyUp(char key) override
    {
        if (key >= '1' && key <= '8')
            SetRenderJobCount(key - '1');
    }

    void SetRenderJobCount(uint32_t count)
    {
        m_renderJobs = std::max(1u, count);

        char buf[256];
        sprintf_s(buf, "TestD3D12Multithreading - %d workers on %d threads", m_renderJobs, m_threadPool.GetNumThreads());
        SetWindowText(m_hWnd, buf);
    }

protected:

    virtual bool IsSoftwareAdapter() const override final
    {
        return m_useWarpDevice;
    }

    virtual void DrawGeometryPass(const RpsCmdCallbackContext* pContext) override final
    {
        const uint32_t numThreads = std::max(1u, std::min(MAX_THREADS, m_renderJobs));

        CmdRangeContext* pRangeContext = static_cast<CmdRangeContext*>(pContext->pUserRecordContext);

        if (m_pipelineState == nullptr)
        {
            std::lock_guard<std::mutex> lock(m_cmdListsMutex);

            if (m_pipelineState == nullptr)
            {
                RpsCmdRenderTargetInfo rtInfo;
                RpsResult              result = rpsCmdGetRenderTargetsInfo(pContext, &rtInfo);
                THREAD_SAFE_REQUIRE(result == RPS_OK);

                CreatePSO(c_Shader, &rtInfo, &m_pipelineState);
            }
        }

        for (uint32_t i = 0; i < numThreads; i++)
        {
            auto hNewCmdBuf = AcquireNewCommandBuffer(&pRangeContext->LastCmdListIndex);

            const RpsCmdCallbackContext* pLocalContext = {};
            {
                std::lock_guard<std::mutex> lock(m_cmdListsMutex);
                THREAD_SAFE_REQUIRE(rpsCmdCloneContext(pContext, hNewCmdBuf, &pLocalContext) == RPS_OK);
            }

            RpsAfxThreadPool::WaitHandle waitHdl = m_threadPool.EnqueueJob(
                [this, pLocalContext, i, numThreads, hNewCmdBuf, batchId = pRangeContext->BatchIndex]() {
                uint32_t numTrianglesPerThread = uint32_t(m_triangleData.size() + numThreads - 1) / numThreads;

                uint32_t beginIndex = numTrianglesPerThread * i;
                uint32_t endIndex   = std::min(uint32_t(m_triangleData.size()), beginIndex + numTrianglesPerThread);

                auto pCmdList = rpsD3D12CommandListFromHandle(pLocalContext->hCommandBuffer);

                if (hNewCmdBuf != pLocalContext->hCommandBuffer)
                {
                    m_failCount++;
                }

                RpsCmdRenderPassBeginInfo rpBeginInfo = {};

                if (i != 0)
                    rpBeginInfo.flags |= RPS_RUNTIME_RENDER_PASS_RESUMING;
                if (i != (numThreads - 1))
                    rpBeginInfo.flags |= RPS_RUNTIME_RENDER_PASS_SUSPENDING;

                RpsResult threadResult = rpsCmdBeginRenderPass(pLocalContext, &rpBeginInfo);
                if (threadResult != RPS_OK)
                    m_failCount++;

                const float aspectRatio = m_height / static_cast<float>(m_width);

                pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
                pCmdList->SetPipelineState(m_pipelineState.Get());
                pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                auto tid = batchId * numThreads + i;

                const XMVECTOR threadColorTint = XMVectorSet(float((tid / 7) & 1), float((tid / 13) & 1), float((tid / 25) & 1), 1.0f);

                for (uint32_t triangleIdx = beginIndex; triangleIdx < endIndex; triangleIdx++)
                {
                    TriangleDataGPU cbData = {};

                    TriangleDataCPU* pTriangle = &m_triangleData[triangleIdx];
                    pTriangle->Offset.x        = fmod(pTriangle->Offset.x + pTriangle->Speed + m_runwayLength * 0.5f, m_runwayLength) - m_runwayLength * 0.5f;

                    cbData.Pos         = pTriangle->Offset;
                    cbData.AspectRatio = aspectRatio;
                    cbData.Scale       = pTriangle->Scale;
                    XMStoreFloat3(&cbData.Color, XMVectorLerp(XMLoadFloat3(&pTriangle->Color), threadColorTint, 0.7f));

                    pCmdList->SetGraphicsRoot32BitConstants(0, sizeof(TriangleDataGPU) / sizeof(uint32_t), &cbData, 0);
                    pCmdList->DrawInstanced(3, 1, 0, 0);
                }

                threadResult = rpsCmdEndRenderPass(pLocalContext);
                if (threadResult != RPS_OK)
                    m_failCount++;

                m_executeCount++;
            });

            {
                std::lock_guard<std::mutex> lock(m_cmdListsMutex);
                m_waitHandles.emplace_back(std::move(waitHdl));
            }
        }

        RpsRuntimeCommandBuffer newBuffer = AcquireNewCommandBuffer(&pRangeContext->LastCmdListIndex);
        RpsResult               result    = rpsCmdSetCommandBuffer(pContext, newBuffer);
        THREAD_SAFE_REQUIRE(result == RPS_OK);
    }

private:
    void LoadAssets(ID3D12GraphicsCommandList* pInitCmdList, std::vector<ComPtr<ID3D12Object>>& tempResources)
    {
        OnPostResize();

        {
            CD3DX12_ROOT_PARAMETER rootParameters[1] = {};

            rootParameters[0].InitAsConstants(sizeof(TriangleDataGPU) / sizeof(uint32_t), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

            D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

            CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
            rootSignatureDesc.Init_1_0(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

            ComPtr<ID3DBlob> signature;
            ComPtr<ID3DBlob> error;
            ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signature, &error));
            ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
        }
    }

    void CreatePSO(const char* shader, const RpsCmdRenderTargetInfo* pRtInfo, ID3D12PipelineState** ppState)
    {
        std::vector<uint8_t> vsCode, psCode;
        DxcCompile(c_Shader, L"VSMain", L"vs_6_0", L"", nullptr, 0, vsCode);
        DxcCompile(c_Shader, L"PSMain", L"ps_6_0", L"", nullptr, 0, psCode);

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature                     = m_rootSignature.Get();
        psoDesc.VS                                 = CD3DX12_SHADER_BYTECODE(vsCode.data(), vsCode.size());
        psoDesc.PS                                 = CD3DX12_SHADER_BYTECODE(psCode.data(), psCode.size());
        psoDesc.RasterizerState                    = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState                         = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable      = FALSE;
        psoDesc.DepthStencilState.StencilEnable    = FALSE;
        psoDesc.SampleMask                         = UINT_MAX;
        psoDesc.PrimitiveTopologyType              = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        psoDesc.DSVFormat        = rpsFormatToDXGI(pRtInfo->depthStencilFormat);
        psoDesc.NumRenderTargets = pRtInfo->numRenderTargets;
        for (uint32_t i = 0; i < pRtInfo->numRenderTargets; i++)
        {
            psoDesc.RTVFormats[i] = rpsFormatToDXGI(pRtInfo->renderTargetFormats[i]);
        }
        psoDesc.SampleDesc.Count = pRtInfo->numSamples;

        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
    }

    void UpdatePipeline(uint64_t frameIndex, uint64_t completedFrameIndex)
    {
        RpsRuntimeResource backBuffers[DXGI_MAX_SWAP_CHAIN_BUFFERS];
        RpsResourceDesc    backBufferDesc = {};

        GetBackBuffers(backBufferDesc, backBuffers);

        TestRpsMultithreading::UpdateRpsPipeline(frameIndex, completedFrameIndex, backBufferDesc, backBuffers);
    }

private:
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;

    struct ActiveCommandListEx
    {
        ActiveCommandList CmdList;
        uint32_t          Next;
    };

    std::vector<ActiveCommandListEx> m_activeCmdLists;

    std::vector<ID3D12CommandList*> m_cmdListsToSubmit;
};

TEST_CASE(TEST_APP_NAME)
{
#if _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    TestD3D12Multithreading renderer;

    RpsTestRunWindowInfo runInfo = {};
    runInfo.title                = TEXT(TEST_APP_NAME);
    runInfo.numFramesToRender    = g_exitAfterFrame;
    runInfo.width                = 1280;
    runInfo.height               = 720;
    runInfo.pRenderer            = &renderer;
    RpsTestRunWindowApp(&runInfo);
}
