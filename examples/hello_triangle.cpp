// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "app_framework/afx_common_helpers.hpp"
#include "app_framework/afx_d3d12_renderer.hpp"
#include "app_framework/afx_win32.hpp"

// set this boolean to true to enable the Tutorial Part 3 extension to this tutorial.
static constexpr bool c_bBreathing = false;

#define AssertIfRpsFailed(expr) (RPS_FAILED(expr) ? assert(false) : (void)0)
#define AssertIfFailed(expr)    (FAILED(expr) ? assert(false) : (void)0)
#define AssertIfFailedEx(expr, errorBlob) \
    ((errorBlob) ? ::OutputDebugStringA((const char*)errorBlob->GetBufferPointer()) : (void)0, AssertIfFailed(expr))

RPS_DECLARE_RPSL_ENTRY(hello_triangle, main);

static const char c_Shader[] = R"(

struct V2P
{
    float4 Pos : SV_Position;
    float4 Color : COLOR0;
};

#define PI (3.14159f)

V2P VSMain(uint vId : SV_VertexID)
{
    float2 pos[3] =
    {
        { -0.5f, -0.5f * tan(PI / 6), },
        {  0.0f,  0.5f / cos(PI / 6), },
        {  0.5f, -0.5f * tan(PI / 6), },
    };

    V2P vsOut;
    vsOut.Pos = float4(pos[min(vId, 2)], 0, 1);
    vsOut.Color = float4(vId == 0 ? 1 : 0, vId == 1 ? 1 : 0, vId == 2 ? 1 : 0, 1);
    return vsOut;
}

float4 PSMain(V2P psIn) : SV_Target0
{
    return psIn.Color;
}
)";

// ---------- Only Relevant for Tutorial Part 3 (Begin) ----------
RPS_DECLARE_RPSL_ENTRY(hello_triangle, mainBreathing);

static const char c_ShaderBreathing[] = R"(

cbuffer SceneConstantBuffer : register(b0)
{
    float oneOverAspectRatio;
};

struct V2P
{
    float4 Pos : SV_Position;
    float4 Color : COLOR0;
};

#define PI (3.14159f)

V2P VSMain(uint vId : SV_VertexID)
{
    float2 pos[3] =
    {
        { -0.5f, -0.5f * tan(PI / 6), },
        {  0.0f,  0.5f / cos(PI / 6), },
        {  0.5f, -0.5f * tan(PI / 6), },
    };

    V2P vsOut;
    vsOut.Pos = float4(pos[min(vId, 2)], 0, 1);
    vsOut.Pos.x *= oneOverAspectRatio;
    vsOut.Color = float4(vId == 0 ? 1 : 0, vId == 1 ? 1 : 0, vId == 2 ? 1 : 0, 1);
    return vsOut;
}

float4 PSMain(V2P psIn) : SV_Target0
{
    return psIn.Color;
}
)";
// ---------- Only Relevant for Tutorial Part 3 (End) ------------

class HelloTriangle : public RpsAfxD3D12Renderer
{
protected:
    virtual void OnInit(ID3D12GraphicsCommandList*         pInitCmdList,
                        std::vector<ComPtr<ID3D12Object>>& tempResources) override
    {
        // Create root signature.
        {
            CD3DX12_ROOT_PARAMETER   rootParameters[1] = {};
            rootParameters[0].InitAsConstants(sizeof(float), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

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

            auto shader     = c_bBreathing ? c_ShaderBreathing : c_Shader;
            auto shaderSize = c_bBreathing ? sizeof(c_ShaderBreathing) : sizeof(c_Shader);

            AssertIfFailedEx(D3DCompile(
                shader, shaderSize, nullptr, nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &err), err);

            AssertIfFailedEx(D3DCompile(
                shader, shaderSize, nullptr, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &err), err);

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
        RpsRpslEntry entry =
            c_bBreathing ? RPS_ENTRY_REF(hello_triangle, mainBreathing) : RPS_ENTRY_REF(hello_triangle, main);

        RpsRenderGraphCreateInfo renderGraphInfo = {};
        RpsQueueFlags            queueFlags[]  = {RPS_QUEUE_FLAG_GRAPHICS, RPS_QUEUE_FLAG_COMPUTE, RPS_QUEUE_FLAG_COPY};
        renderGraphInfo.scheduleInfo.numQueues = 3;
        renderGraphInfo.scheduleInfo.pQueueInfos            = queueFlags;
        renderGraphInfo.mainEntryCreateInfo.hRpslEntryPoint = entry;
        AssertIfRpsFailed(rpsRenderGraphCreate(m_rpsDevice, &renderGraphInfo, &m_rpsRenderGraph));

        // Bind nodes.
        AssertIfRpsFailed(
            rpsProgramBindNode(rpsRenderGraphGetMainEntry(m_rpsRenderGraph), "Triangle", &DrawTriangleCb, this));

        // ---------- Only Relevant for Tutorial Part 3 (Begin) ----------
        AssertIfRpsFailed(rpsProgramBindNode(rpsRenderGraphGetMainEntry(m_rpsRenderGraph),
                                             "TriangleBreathing",
                                             &HelloTriangle::DrawTriangleBreathingCb,
                                             this));
        // ---------- Only Relevant for Tutorial Part 3 (End) ------------
    }

    virtual void OnPostResize() override
    {
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

            RpsConstant               argData[2]      = {&backBufferDesc};
            const RpsRuntimeResource* argResources[2] = {backBufferResources};

            uint32_t argDataCount = 1;

            float time = 0.f;

            if (c_bBreathing)
            {
                argDataCount = 2;
                time         = float(RpsAfxCpuTimer::SecondsSinceEpoch().count());
                argData[1]   = &time;
            }

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
            updateInfo.numArgs                  = argDataCount;
            updateInfo.ppArgs                   = argData;
            updateInfo.ppArgResources           = argResources;

            assert(_countof(argData) == _countof(argResources));

            AssertIfRpsFailed(rpsRenderGraphUpdate(m_rpsRenderGraph, &updateInfo));
        }
    }

    virtual void OnRender(uint32_t frameIndex) override
    {
        if (m_useRps)
        {
            RpsRenderGraphBatchLayout batchLayout = {};
            AssertIfRpsFailed(rpsRenderGraphGetBatchLayout(m_rpsRenderGraph, &batchLayout));

            m_fenceSignalInfos.resize(batchLayout.numFenceSignals);

            for (uint32_t ib = 0; ib < batchLayout.numCmdBatches; ib++)
            {
                const RpsCommandBatch& batch = batchLayout.pCmdBatches[ib];

                for (uint32_t iw = batch.waitFencesBegin; iw < batch.waitFencesBegin + batch.numWaitFences; ++iw)
                {
                    const FenceSignalInfo& sInfo = m_fenceSignalInfos[batchLayout.pWaitFenceIndices[iw]];
                    AssertIfFailed(m_queues[batch.queueIndex]->Wait(m_fences[sInfo.queueIndex].Get(), sInfo.value));
                }

                ID3D12CommandQueue* pQueue  = GetCmdQueue(RpsAfxQueueIndices(batch.queueIndex));
                ActiveCommandList   cmdList = AcquireCmdList(RpsAfxQueueIndices(batch.queueIndex));

                RpsRenderGraphRecordCommandInfo recordInfo = {};
                recordInfo.hCmdBuffer                      = rpsD3D12CommandListToHandle(cmdList.cmdList.Get());
                recordInfo.pUserContext                    = this;
                recordInfo.frameIndex                      = frameIndex;
                recordInfo.cmdBeginIndex                   = batch.cmdBegin;
                recordInfo.numCmds                         = batch.numCmds;

                AssertIfRpsFailed(rpsRenderGraphRecordCommands(m_rpsRenderGraph, &recordInfo));

                CloseCmdList(cmdList);

                ID3D12CommandList* pCmdLists[] = {cmdList.cmdList.Get()};
                pQueue->ExecuteCommandLists(1, pCmdLists);

                RecycleCmdList(cmdList);

                if (batch.signalFenceIndex != RPS_INDEX_NONE_U32)
                {
                    m_fenceValue++;
                    FenceSignalInfo& sInfo = m_fenceSignalInfos[batch.signalFenceIndex];
                    sInfo.queueIndex       = batch.queueIndex;
                    sInfo.value            = m_fenceValue;
                    AssertIfFailed(m_queues[batch.queueIndex]->Signal(m_fences[sInfo.queueIndex].Get(), sInfo.value));
                }
            }
        }
        else
        {
            ActiveCommandList          cmdList  = AcquireCmdList(RPS_AFX_QUEUE_INDEX_GFX);
            ID3D12GraphicsCommandList* pCmdList = cmdList.cmdList.Get();

            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                GetBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            pCmdList->ResourceBarrier(1, &barrier);

            auto rtv = GetBackBufferRTV();

            FLOAT clearColor[4] = {0.0f, 0.2f, 0.4f, 1.0f};
            auto  viewport    = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height));
            auto  scissorRect = CD3DX12_RECT(0, 0, m_width, m_height);
            pCmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
            pCmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
            pCmdList->RSSetViewports(1, &viewport);
            pCmdList->RSSetScissorRects(1, &scissorRect);

            DrawTriangle(pCmdList);

            barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                GetBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
            pCmdList->ResourceBarrier(1, &barrier);

            CloseCmdList(cmdList);
            ID3D12CommandList* pCmdLists[] = {cmdList.cmdList.Get()};
            m_presentQueue->ExecuteCommandLists(1, pCmdLists);

            RecycleCmdList(cmdList);
        }
    }

private:
    void DrawTriangle(ID3D12GraphicsCommandList* pCmdList)
    {
        pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
        pCmdList->SetPipelineState(m_pipelineState.Get());
        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->DrawInstanced(3, 1, 0, 0);
    }

    static void DrawTriangleCb(const RpsCmdCallbackContext* pContext)
    {
        HelloTriangle* pThis = static_cast<HelloTriangle*>(pContext->pCmdCallbackContext);
        pThis->DrawTriangle(rpsD3D12CommandListFromHandle(pContext->hCommandBuffer));
    }

    // ---------- Only Relevant for Tutorial Part 3 (Begin) ----------
    void DrawTriangleBreathingCb(const RpsCmdCallbackContext* pContext,
                                 rps::UnusedArg               u0,
                                 float                        oneOverAspectRatio,
                                 float                        timeInSeconds)
    {
        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);

        pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
        pCmdList->SetPipelineState(m_pipelineState.Get());

        oneOverAspectRatio *= abs(sin(timeInSeconds));

        pCmdList->SetGraphicsRoot32BitConstant(0, *(const UINT*)&oneOverAspectRatio, 0);

        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->DrawInstanced(3, 1, 0, 0);
    }
    // ---------- Only Relevant for Tutorial Part 3 (End) ------------

private:
    ComPtr<ID3D12RootSignature>  m_rootSignature;
    ComPtr<ID3D12PipelineState>  m_pipelineState;
    RpsDevice                    m_rpsDevice      = RPS_NULL_HANDLE;
    RpsRenderGraph               m_rpsRenderGraph = RPS_NULL_HANDLE;
    const bool                   m_useRps         = true;
    std::vector<FenceSignalInfo> m_fenceSignalInfos;
};

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
{
    rps::CLI::Parse(__argc, __argv);

    HelloTriangle renderer;

    RpsAfxRunWindowInfo runInfo = {};
    runInfo.title               = TEXT("HelloTriangle");
    runInfo.width               = 1280;
    runInfo.height              = 720;
    runInfo.pRenderer           = &renderer;

    RpsAfxRunWindowApp(&runInfo);

    return 0;
}
