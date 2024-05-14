// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#include "utils/rps_test_host.hpp"
#include "app_framework/afx_threadpool.hpp"

RPS_DECLARE_RPSL_ENTRY(test_multithreading, mt_main);

#include <random>
#include <atomic>

static const char c_Shader[] = R"(
struct V2P
{
    float4 Pos : SV_Position;
    float4 Color : COLOR0;
};

struct CBData
{
    float2 Pos;
    float Scale;
    float AspectRatio;
    float3 Color;
};

#ifndef VULKAN
ConstantBuffer<CBData> cb : register(b0);
#else
[[vk::push_constant]] CBData cb;
#endif

#define PI (3.14159f)

V2P VSMain(uint vId : SV_VertexID)
{
    float2 pos[3] =
    {
        { -0.5f * cb.AspectRatio, -0.5f * tan(PI / 6), },
        {  0.0f * cb.AspectRatio,  0.5f / cos(PI / 6), },
        {  0.5f * cb.AspectRatio, -0.5f * tan(PI / 6), },
    };

    V2P vsOut;
    vsOut.Pos = float4(pos[min(vId, 2)] * cb.Scale + cb.Pos, 0, 1);
    vsOut.Color = float4(cb.Color, 1.0f);
    return vsOut;
}

float4 PSMain(V2P psIn) : SV_Target0
{
    return psIn.Color;
}
)";

#define TEST_APP_NAME_RAW "TestMultithreading"

using namespace DirectX;

#define THREAD_SAFE_REQUIRE(EXPR)                        \
    do                                                   \
    {                                                    \
        std::lock_guard<std::mutex> lock(m_catch2Mutex); \
        REQUIRE(EXPR);                                   \
    } while (false);

class TestRpsMultithreading : public RpsTestHost
{
public:
    static constexpr uint32_t MAX_THREADS = 8;
    static constexpr uint32_t MIN_THREADS = 4;

    struct TriangleDataCPU
    {
        XMFLOAT2 Offset;
        float    Scale;
        float    Speed;
        XMFLOAT3 Color;
    };

    struct TriangleDataGPU
    {
        XMFLOAT2 Pos;
        float    Scale;
        float    AspectRatio;
        XMFLOAT3 Color;
    };

    struct CmdRangeContext
    {
        uint32_t BatchIndex;
        uint32_t LastCmdListIndex;
    };

public:
    TestRpsMultithreading()
    {
        const uint32_t hwConcurrency = std::thread::hardware_concurrency();
        const uint32_t numThreads    = std::min(MAX_THREADS, std::max(MIN_THREADS, (hwConcurrency > 0) ? (hwConcurrency - 1) : hwConcurrency));
        m_threadPool.Init(numThreads);
    }

protected:
    void Init(RpsDevice hRpsDevice)
    {
        uint32_t baseTriangles = 4096;
#ifndef _DEBUG
        m_speedMultiplier = IsSoftwareAdapter() ? 1 : 64;
#endif
        m_triangleData.resize(baseTriangles * m_speedMultiplier);
        m_runwayLength *= m_speedMultiplier;

        std::random_device                    rd;
        std::mt19937                          gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (uint32_t i = 0; i < m_triangleData.size(); i++)
        {
            m_triangleData[i].Offset = XMFLOAT2((dist(gen) - 0.5f) * m_runwayLength, dist(gen) * 2.0f - 1.0f);
            m_triangleData[i].Color  = XMFLOAT3(dist(gen), dist(gen), dist(gen));
            m_triangleData[i].Scale  = dist(gen) * 0.1f + 0.1f;
            m_triangleData[i].Speed  = (dist(gen) + 0.5f) * 0.01f * m_speedMultiplier;
        }

        RpsTestHost::OnInit(hRpsDevice, rpsTestLoadRpslEntry(test_multithreading, mt_main));
    }

    virtual void UpdateRpsPipeline(uint64_t               frameIndex,
                                   uint64_t               completedFrameIndex,
                                   const RpsResourceDesc& backBufferDesc,
                                   RpsRuntimeResource*    pBackBuffers)
    {
        const RpsRuntimeResource* argResources[] = {pBackBuffers};
        RpsConstant               argData[]      = {&backBufferDesc, &m_numPasses};

        RpsTestHost::OnUpdate(frameIndex, completedFrameIndex, _countof(argData), argData, argResources);
    }

    void OnRender(uint64_t frameIndex, uint32_t numThreads)
    {
        m_waitHandles.clear();
        m_executeCount = 0;
        m_failCount    = 0;

        RpsRenderGraphBatchLayout batchLayout = {};
        REQUIRE_RPS_OK(rpsRenderGraphGetBatchLayout(GetRpsRenderGraph(), &batchLayout));

        assert(batchLayout.numCmdBatches == 1);

        const uint32_t numCmds     = batchLayout.pCmdBatches[0].numCmds;
        const uint32_t batchCmdEnd = batchLayout.pCmdBatches[0].cmdBegin + batchLayout.pCmdBatches[0].numCmds;

        uint32_t cmdBegin = 0;
        uint32_t cmdsPerThread = (numCmds + numThreads - 1) / numThreads;
        uint32_t numThreadsActual = (numCmds + cmdsPerThread - 1) / cmdsPerThread;

        static constexpr uint32_t                    MAX_BATCHES = 32;
        std::pair<RpsRuntimeCommandBuffer, uint32_t> buffers[MAX_BATCHES];

        uint32_t lastCmdListId = UINT32_MAX;
        for (uint32_t iBatch = 0; iBatch < numThreadsActual; iBatch++)
        {
            buffers[iBatch].first  = AcquireNewCommandBuffer(&lastCmdListId);
            buffers[iBatch].second  = lastCmdListId;
        }

        for (uint32_t iBatch = 0; iBatch < numThreadsActual; iBatch++)
        {
            const uint32_t cmdEnd = std::min(batchCmdEnd, cmdBegin + cmdsPerThread);

            if (cmdEnd == cmdBegin)
            {
                break;
            }

            RpsAfxThreadPool::WaitHandle waitHdl =
                m_threadPool.EnqueueJob([this, iBatch, buffer = buffers[iBatch], frameIndex, cmdBegin, cmdEnd]() {
                
                CmdRangeContext rangeContext = {iBatch, buffer.second};

                RpsRenderGraphRecordCommandInfo recordInfo = {};
                recordInfo.hCmdBuffer                      = buffer.first;
                recordInfo.pUserContext                    = &rangeContext;
                recordInfo.frameIndex                      = frameIndex;
                recordInfo.cmdBeginIndex                   = cmdBegin;
                recordInfo.numCmds                         = cmdEnd - cmdBegin;

                RpsResult threadResult = rpsRenderGraphRecordCommands(GetRpsRenderGraph(), &recordInfo);
                THREAD_SAFE_REQUIRE(threadResult == RPS_OK);
            });

            {
                std::lock_guard<std::mutex> lock(m_cmdListsMutex);
                m_waitHandles.emplace_back(std::move(waitHdl));
            }

            cmdBegin = cmdEnd;
        }

        m_threadPool.WaitIdle();

        REQUIRE(m_failCount == 0);
        REQUIRE((m_executeCount + numThreadsActual) == m_waitHandles.size());
    }

protected:
    virtual bool IsSoftwareAdapter() const
    {
        return false;
    }

    virtual RpsRuntimeCommandBuffer AcquireNewCommandBuffer(uint32_t* pInsertAfter) = 0;

    virtual void DrawGeometryPass(const RpsCmdCallbackContext* pContext) = 0;

    virtual void BindNodes(RpsSubprogram hRpslEntry) override
    {
        RpsResult result = RPS_OK;

        result = rpsProgramBindNode(hRpslEntry,
                                    "GeometryPass",
                                    &TestRpsMultithreading::DrawGeometryPass,
                                    this,
                                    RPS_CMD_CALLBACK_CUSTOM_ALL);
        REQUIRE(result == RPS_OK);
    }

protected:
    uint32_t                     m_numPasses = 4;
    std::vector<TriangleDataCPU> m_triangleData;
    float                        m_runwayLength    = 15.0f;
    int32_t                      m_speedMultiplier = 1;
    RpsAfxThreadPool             m_threadPool;

    uint32_t m_renderJobs = 8;

    std::vector<RpsAfxThreadPool::WaitHandle> m_waitHandles;

    std::atomic_int32_t m_failCount    = {};
    std::atomic_int32_t m_executeCount = {};

    std::mutex m_cmdListsMutex;
    std::mutex m_catch2Mutex;
};
