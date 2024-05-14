// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#define NOMINMAX

#include "app_framework/afx_d3d12_renderer.hpp"
#include "app_framework/afx_win32.hpp"
#include "app_framework/afx_common_helpers.hpp"

#include <directxmath.h>
#include <random>

using namespace DirectX;

#define AssertIfRpsFailed(expr) (RPS_FAILED(expr) ? assert(false) : (void)0)
#define AssertIfFailed(expr)    (FAILED(expr) ? assert(false) : (void)0)
#define AssertIfFailedEx(expr, errorBlob) \
    ((errorBlob) ? ::OutputDebugStringA((const char*)errorBlob->GetBufferPointer()) : (void)0, AssertIfFailed(expr))
#define Assert(expr) assert(expr)

RPS_DECLARE_RPSL_ENTRY(rps_multithreading, main);

static const char c_Shader[] = R"(
struct V2P
{
    float4 Pos : SV_Position;
    float4 Color : COLOR0;
};

cbuffer cb : register(b0)
{
    float2 Pos;
    float Scale;
    float OneOverAspectRatio;
    float4 Color;
};

#define PI (3.14159f)

V2P VSMain(uint vId : SV_VertexID)
{
    float2 pos[3] =
    {
        { -0.5f * OneOverAspectRatio, -0.5f * tan(PI / 6), },
        {  0.0f * OneOverAspectRatio,  0.5f / cos(PI / 6), },
        {  0.5f * OneOverAspectRatio, -0.5f * tan(PI / 6), },
    };

    V2P vsOut;
    vsOut.Pos   = float4(pos[min(vId, 2)] * Scale + Pos, 0, 1);
    vsOut.Color = Color;
    return vsOut;
}

float4 PSMain(V2P psIn) : SV_Target0
{
    return psIn.Color;
}
)";

static uint32_t DIV_CEIL(uint32_t A, uint32_t B)
{
    return (A + B - 1) / B;
}

class ScopedThreadLauncher
{
public:
    ScopedThreadLauncher(const ScopedThreadLauncher&)            = delete;
    ScopedThreadLauncher& operator=(const ScopedThreadLauncher&) = delete;

    ScopedThreadLauncher()
    {
    }

    ~ScopedThreadLauncher()
    {
        waitForAllJobs();
    }

    void waitForAllJobs()
    {
        std::lock_guard<std::mutex> lock(m_threadPoolMutex);
        for (auto& t : m_workerThreads)
        {
            if (t.joinable())
            {
                t.join();
            }
        }
        m_workerThreads.clear();
    }

    template <typename Func>
    void launchJob(Func job)
    {
        std::lock_guard<std::mutex> lock(m_threadPoolMutex);
        m_workerThreads.push_back(std::thread(job));
    }

private:
    std::vector<std::thread> m_workerThreads;
    std::mutex               m_threadPoolMutex;
};

struct CmdRangeContext
{
    //uint32_t BatchIndex;
    uint32_t              LastCmdListIndex;
    ScopedThreadLauncher* pStl;
};

struct TriangleDataCPU
{
    XMFLOAT2 Origin;
    float    Scale;
};

struct TriangleDataGPU
{
    XMFLOAT2 Pos;
    float    Scale;
    float    OneOverAspectRatio;
    XMFLOAT4 Color;
};

class RpsMultithreading : public RpsAfxD3D12Renderer
{
public:
    static constexpr int32_t MAX_THREADS = 8;
    static constexpr int32_t MIN_THREADS = 4;

protected:
    virtual void OnInit(ID3D12GraphicsCommandList*         pInitCmdList,
                        std::vector<ComPtr<ID3D12Object>>& tempResources) override
    {
        // Create a root signature consisting of a descriptor table with a single CBV.
        {
            CD3DX12_ROOT_PARAMETER rootParameters[1];
            rootParameters[0].InitAsConstants(
                sizeof(TriangleDataGPU) / sizeof(uint32_t), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

            CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
            rootSignatureDesc.Init_1_0(
                _countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

            ComPtr<ID3DBlob> signature;
            ComPtr<ID3DBlob> error;
            AssertIfFailed(D3DX12SerializeVersionedRootSignature(
                &rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signature, &error));
            AssertIfFailed(m_device->CreateRootSignature(
                0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
        }

        // Create the pipeline state, which includes compiling and loading shaders.
        {
            ComPtr<ID3DBlob> vertexShader, pixelShader, err;

            UINT compileFlags = 0;
            AssertIfFailedEx(D3DCompile(
                c_Shader, sizeof(c_Shader), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &err), err);
            AssertIfFailedEx(D3DCompile(
                c_Shader, sizeof(c_Shader), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &err), err);

            // Describe and create the graphics pipeline state object (PSO).
            D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.InputLayout                        = {nullptr, 0};
            psoDesc.pRootSignature                     = m_rootSignature.Get();
            psoDesc.VS                                 = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
            psoDesc.PS                                 = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
            psoDesc.RasterizerState                    = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
            psoDesc.BlendState                         = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
            psoDesc.DepthStencilState.DepthEnable      = FALSE;
            psoDesc.DepthStencilState.StencilEnable    = FALSE;
            psoDesc.SampleMask                         = UINT_MAX;
            psoDesc.PrimitiveTopologyType              = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            psoDesc.NumRenderTargets                   = 1;
            psoDesc.RTVFormats[0]                      = DXGI_FORMAT_R8G8B8A8_UNORM;
            psoDesc.SampleDesc.Count                   = 1;

            AssertIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
        }

        // Create RPS runtime device.
        RpsD3D12RuntimeDeviceCreateInfo runtimeDeviceCreateInfo = {};
        runtimeDeviceCreateInfo.pD3D12Device                    = m_device.Get();
        AssertIfRpsFailed(rpsD3D12RuntimeDeviceCreate(&runtimeDeviceCreateInfo, &m_rpsDevice));

        // Create RPS render graph.
        RpsRenderGraphCreateInfo renderGraphInfo = {};
        RpsQueueFlags            queueFlags[]  = {RPS_QUEUE_FLAG_GRAPHICS, RPS_QUEUE_FLAG_COMPUTE, RPS_QUEUE_FLAG_COPY};
        renderGraphInfo.scheduleInfo.numQueues = 3;
        renderGraphInfo.scheduleInfo.pQueueInfos            = queueFlags;
        renderGraphInfo.mainEntryCreateInfo.hRpslEntryPoint = RPS_ENTRY_REF(rps_multithreading, main);
        AssertIfRpsFailed(rpsRenderGraphCreate(m_rpsDevice, &renderGraphInfo, &m_rpsRenderGraph));

        // Bind nodes.
        AssertIfRpsFailed(rpsProgramBindNode(rpsRenderGraphGetMainEntry(m_rpsRenderGraph),
                                             "GeometryPass",
                                             &RpsMultithreading::GeometryPass,
                                             this,
                                             RPS_CMD_CALLBACK_CUSTOM_ALL));

        // Init triangle CPU data.
        {
            constexpr static uint32_t baseTriangles = 1024;  // tris per viewport block.

            m_triangleData.resize(baseTriangles);
            std::random_device                    rd;
            std::mt19937                          gen(rd());
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            uint32_t                              triStride = uint32_t(std::sqrtf(float(baseTriangles)));

            for (uint32_t i = 0; i < m_triangleData.size(); i++)
            {
                float sX = 2.f / float(triStride);
                float sY = 2.f / float(triStride);
                float x  = -1.f + (i % triStride) * sX;
                float y  = -1.f + (i / triStride) * sY;

                m_triangleData[i].Origin = XMFLOAT2(x, y);
                m_triangleData[i].Scale  = dist(gen) * 0.075f + 0.075f;
            }
        }

        const int32_t hwConcurrency = std::thread::hardware_concurrency();
        m_clampedHwThreads          = std::min(MAX_THREADS, std::max(MIN_THREADS, hwConcurrency - 1));
    }

    virtual void OnCleanUp() override
    {
        rpsRenderGraphDestroy(m_rpsRenderGraph);
        rpsDeviceDestroy(m_rpsDevice);

        m_pipelineState = nullptr;
        m_rootSignature = nullptr;
    }

    virtual void OnUpdate(uint32_t frameIndex) override
    {
        if (m_rpsRenderGraph != RPS_NULL_HANDLE)
        {
            RpsRuntimeResource backBufferResources[DXGI_MAX_SWAP_CHAIN_BUFFERS];

            for (uint32_t i = 0; i < m_backBuffers.size(); i++)
            {
                // RpsRuntimeResource is a RPS_OPAQUE_HANDLE.
                // this type has a single elem "void* ptr".
                backBufferResources[i].ptr = m_backBuffers[i].Get();
            }

            RpsResourceDesc backBufferDesc   = {};
            backBufferDesc.type              = RPS_RESOURCE_TYPE_IMAGE_2D;
            backBufferDesc.temporalLayers    = uint32_t(m_backBuffers.size());
            backBufferDesc.image.width       = m_width;
            backBufferDesc.image.height      = m_height;
            backBufferDesc.image.arrayLayers = 1;
            backBufferDesc.image.mipLevels   = 1;
            backBufferDesc.image.format      = RPS_FORMAT_R8G8B8A8_UNORM;
            backBufferDesc.image.sampleCount = 1;

            float time = float(RpsAfxCpuTimer::SecondsSinceEpoch().count());

            RpsConstant               argData[]      = {&backBufferDesc, &time};
            const RpsRuntimeResource* argResources[] = {backBufferResources, nullptr};

            // RpsAfx always waits for presentation before rendering to a swapchain image again,
            // therefore the guaranteed last completed frame by the GPU is m_backBufferCount frames ago.
            //
            // RPS_GPU_COMPLETED_FRAME_INDEX_NONE means no frames are known to have completed yet;
            // we use this during the initial frames.
            const uint64_t completedFrameIndex =
                (frameIndex > m_backBufferCount) ? frameIndex - m_backBufferCount : RPS_GPU_COMPLETED_FRAME_INDEX_NONE;

            RpsRenderGraphUpdateInfo updateInfo = {};
            updateInfo.frameIndex               = frameIndex;
            updateInfo.gpuCompletedFrameIndex   = completedFrameIndex;
            updateInfo.numArgs                  = _countof(argData);
            updateInfo.ppArgs                   = argData;
            updateInfo.ppArgResources           = argResources;

            assert(_countof(argData) == _countof(argResources));

            AssertIfRpsFailed(rpsRenderGraphUpdate(m_rpsRenderGraph, &updateInfo));
        }
    }

    virtual void OnRender(uint32_t frameIndex) override
    {
        RpsRenderGraphBatchLayout batchLayout = {};
        AssertIfRpsFailed(rpsRenderGraphGetBatchLayout(m_rpsRenderGraph, &batchLayout));

        // NOTE: In a single-queue app, we expect there to be a single cmd batch.
        Assert(batchLayout.numCmdBatches == 1);

        const RpsCommandBatch& batch = batchLayout.pCmdBatches[0];

        // We will split the scheduled cmdNodes into separate batches, giving each to
        // unique worker threads.
        const uint32_t numCmds          = batch.numCmds;
        const uint32_t batchCmdEnd      = batch.cmdBegin + batch.numCmds;
        const uint32_t cmdsPerThread    = DIV_CEIL(numCmds, m_graphThreadsToLaunch);
        const uint32_t numThreadsActual = DIV_CEIL(numCmds, cmdsPerThread);
        m_graphThreadsLaunched          = numThreadsActual;

        static constexpr uint32_t                    MAX_BATCHES = 32;
        std::pair<RpsRuntimeCommandBuffer, uint32_t> buffers[MAX_BATCHES];

        Assert((numThreadsActual <= MAX_BATCHES) && (numThreadsActual > 0));

        uint32_t lastCmdListId = UINT32_MAX;
        for (uint32_t iBatch = 0; iBatch < numThreadsActual; iBatch++)
        {
            buffers[iBatch].first  = AcquireNewCommandBuffer(lastCmdListId, &lastCmdListId);
            buffers[iBatch].second = lastCmdListId;
        }

        ID3D12CommandQueue* pQueue = GetCmdQueue(RpsAfxQueueIndices(batch.queueIndex));

        uint32_t cmdBegin = 0;

        ScopedThreadLauncher stl;

        for (uint32_t iBatch = 0; iBatch < numThreadsActual; iBatch++)
        {
            const uint32_t cmdEnd = std::min(batchCmdEnd, cmdBegin + cmdsPerThread);

            auto job = [=]() {
                ScopedThreadLauncher jobStl;

                auto                   buffer     = buffers[iBatch];
                struct CmdRangeContext cmdContext = {};
                cmdContext.LastCmdListIndex       = buffer.second;
                cmdContext.pStl                   = &jobStl;

                RpsRenderGraphRecordCommandInfo recordInfo = {};
                recordInfo.hCmdBuffer                      = buffer.first;
                recordInfo.pUserContext                    = &cmdContext;
                recordInfo.frameIndex                      = frameIndex;
                recordInfo.cmdBeginIndex                   = cmdBegin;
                recordInfo.numCmds                         = cmdEnd - cmdBegin;

                AssertIfRpsFailed(rpsRenderGraphRecordCommands(m_rpsRenderGraph, &recordInfo));
            };

            stl.launchJob(job);

            cmdBegin = cmdEnd;
        }

        stl.waitForAllJobs();

        // launch command lists on GPU (from main thread).
        {
            // NOTE: In a multi-queue app there will be fences to wait on and fences to signal.
            //       The proper queue must also be selected when calling ExecuteCommandLists.
            //       Since the render graph for this app uses just one queue, there is guaranteed 1 batch
            //       in the batch layout.

            for (auto& cl : m_activeCmdLists)
            {
                CloseCmdList(cl.CmdList);
            }

            // launch command lists.
            static std::vector<ID3D12CommandList*> s_cmdListsToSubmit;
            s_cmdListsToSubmit.clear();
            s_cmdListsToSubmit.reserve(m_activeCmdLists.size());

            for (uint32_t i = 0; i < m_activeCmdLists.size(); i = m_activeCmdLists[i].Next)
            {
                s_cmdListsToSubmit.push_back(m_activeCmdLists[i].CmdList.cmdList.Get());
            }

            Assert(s_cmdListsToSubmit.size() == m_activeCmdLists.size());

            if (s_cmdListsToSubmit.size() > 0)
            {
                pQueue->ExecuteCommandLists(uint32_t(s_cmdListsToSubmit.size()), s_cmdListsToSubmit.data());
            }

            for (auto& cl : m_activeCmdLists)
            {
                RecycleCmdList(cl.CmdList);
            }
        }

        m_activeCmdLists.clear();
    }

private:
    RpsRuntimeCommandBuffer AcquireNewCommandBuffer(const uint32_t     insertAfterIdx = UINT32_MAX,
                                                    uint32_t*          idxOut         = nullptr,
                                                    RpsAfxQueueIndices queueIdx       = RPS_AFX_QUEUE_INDEX_GFX)
    {
        std::lock_guard<std::mutex> lock(m_cmdListsMutex);

        uint32_t newIdx = uint32_t(m_activeCmdLists.size());

        m_activeCmdLists.push_back({
            AcquireCmdList(queueIdx),
            UINT32_MAX,
        });

        if (insertAfterIdx != UINT32_MAX)
        {
            uint32_t next                         = m_activeCmdLists[insertAfterIdx].Next;
            m_activeCmdLists[insertAfterIdx].Next = newIdx;
            m_activeCmdLists.back().Next          = next;
        }
        if (idxOut)
        {
            *idxOut = newIdx;
        }

        return rpsD3D12CommandListToHandle(m_activeCmdLists.back().CmdList.cmdList.Get());
    }

    void GeometryPass(const RpsCmdCallbackContext* pContext,
                      rps::UnusedArg               u0,
                      float                        oneOverAspectRatio,
                      float                        timeInSeconds)
    {
        const uint32_t renderJobs =
            std::min(uint32_t(MAX_THREADS), std::max((m_clampedHwThreads / m_graphThreadsLaunched), 1u));

        const uint32_t numTrianglesPerThread = DIV_CEIL(uint32_t(m_triangleData.size()), renderJobs);

        CmdRangeContext* pRangeContext = static_cast<CmdRangeContext*>(pContext->pUserRecordContext);

        for (uint32_t i = 0; i < renderJobs; i++)
        {
            // NOTE: We use a linked-list structure to ensure that at GPU submission time, cmdLists are submitted in-order.
            auto hNewCmdBuf =
                AcquireNewCommandBuffer(pRangeContext->LastCmdListIndex, &pRangeContext->LastCmdListIndex);

            const RpsCmdCallbackContext* pLocalContext = {};
            {
                // We must use a lock when calling rpsCmdCloneContext as the cloned context
                // is a data structure managed by the RPS SDK. An alloc happens here.
                //
                // Undefined behavior _will_ occur if we are trying to alloc from two threads at once.
                std::lock_guard<std::mutex> lock(m_cmdListsMutex);

                // Each thread must have its own command list.
                // Since an RpsCmdCallbackContext has some associated command buffer,
                // we must create a new context with the new command list.
                AssertIfRpsFailed(rpsCmdCloneContext(pContext, hNewCmdBuf, &pLocalContext));
            }

            auto pCmdList = rpsD3D12CommandListFromHandle(pLocalContext->hCommandBuffer);

            const uint32_t beginIndex = numTrianglesPerThread * i;
            const uint32_t endIndex   = std::min(uint32_t(m_triangleData.size()), beginIndex + numTrianglesPerThread);

            auto job = [=]() {
                // For dx12 MT use-case, RPS expects pairing suspending + resuming passes across cmdList boundaries.
                // This is to ensure (among other things) that only the first RP (non-resuming) performs RTV clear.
                RpsCmdRenderPassBeginInfo beginInfo = {};
                RpsRuntimeRenderPassFlags rpFlags   = RPS_RUNTIME_RENDER_PASS_FLAG_NONE;
                rpFlags |= (i != 0) ? RPS_RUNTIME_RENDER_PASS_RESUMING : 0;
                rpFlags |= (i != (renderJobs - 1)) ? RPS_RUNTIME_RENDER_PASS_SUSPENDING : 0;
                beginInfo.flags = rpFlags;
                AssertIfRpsFailed(rpsCmdBeginRenderPass(pLocalContext, &beginInfo));

                // Render.
                {
                    pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
                    pCmdList->SetPipelineState(m_pipelineState.Get());
                    pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                    std::random_device                    rd;
                    std::mt19937                          gen(rd());
                    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

                    for (uint32_t triangleIdx = beginIndex; triangleIdx < endIndex; triangleIdx++)
                    {
                        TriangleDataGPU  cbData = {};
                        TriangleDataCPU& tri    = m_triangleData[triangleIdx];

                        cbData.Pos = XMFLOAT2(tri.Origin.x, tri.Origin.y);

                        cbData.OneOverAspectRatio = oneOverAspectRatio;
                        cbData.Scale              = tri.Scale * abs(sin(cbData.Pos.x + timeInSeconds));
                        cbData.Color = XMFLOAT4(i == 0 ? 0.5f : 0.f, i == 1 ? 0.5f : 0.f, i == 2 ? 0.5f : 0.f, 1.f);

                        pCmdList->SetGraphicsRoot32BitConstants(
                            0, sizeof(TriangleDataGPU) / sizeof(uint32_t), &cbData, 0);
                        pCmdList->DrawInstanced(3, 1, 0, 0);
                    }
                }
                AssertIfRpsFailed(rpsCmdEndRenderPass(pLocalContext));
            };

            pRangeContext->pStl->launchJob(job);
        }

        // One of the main roles of RPS is to insert barriers. This occurs throughout the cmdList on the context.
        //
        // In this cmdNode callback we have launched worker threads, each with their own cmdList. The very first
        // worker thread is set to be submitted to GPU after the main cmdList for this node.
        //
        // As such, we must override the command buffer to ensure that subsequent cmdNode barrier insertion occurs _after_
        // the work of the last worker for this cmdNode.
        AssertIfRpsFailed(rpsCmdSetCommandBuffer(
            pContext, AcquireNewCommandBuffer(pRangeContext->LastCmdListIndex, &pRangeContext->LastCmdListIndex)));
    }

private:
    ComPtr<ID3D12RootSignature>  m_rootSignature;
    ComPtr<ID3D12PipelineState>  m_pipelineState;
    std::vector<FenceSignalInfo> m_fenceSignalInfos;
    std::vector<TriangleDataCPU> m_triangleData;

    RpsDevice      m_rpsDevice      = RPS_NULL_HANDLE;
    RpsRenderGraph m_rpsRenderGraph = RPS_NULL_HANDLE;

    struct ActiveCommandListEx
    {
        ActiveCommandList CmdList;
        uint32_t          Next;
    };

    std::vector<ActiveCommandListEx> m_activeCmdLists;
    std::mutex                       m_cmdListsMutex;

    const uint32_t m_graphThreadsToLaunch = 4;

    uint32_t m_graphThreadsLaunched;
    uint32_t m_clampedHwThreads;
};

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
{
    rps::CLI::Parse(__argc, __argv);

    RpsMultithreading renderer;

    RpsAfxRunWindowInfo runInfo = {};
    runInfo.title               = TEXT("RpsMultithreading");
    runInfo.width               = 1280;
    runInfo.height              = 720;
    runInfo.pRenderer           = &renderer;

    RpsAfxRunWindowApp(&runInfo);

    return 0;
}