// Modifications Copyright (C) 2024 Advanced Micro Devices, Inc.

// Copyright (c) Microsoft. All rights reserved.
//
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.

#pragma once

#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <mutex>
#include <vector>
#include <cassert>

#include <dxgidebug.h>
#include <pix.h>

#include "d3dx12.h"

#ifndef RPS_D3D12_RUNTIME
#define RPS_D3D12_RUNTIME 1
#endif  //RPS_D3D12_RUNTIME

#include "rps/rps.h"

#include "afx_renderer.hpp"
#include "afx_d3d_helper.hpp"
#include "afx_shader_compiler.hpp"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

static rps::CmdArg<bool> g_WarpDevice{"warp-device", false, {"warp"}, false};
static rps::CmdArg<bool> g_Dx12PreferEnhancedBarriers{"dx12-enhanced-barriers", false, {"dx12-eb"}};
static rps::CmdArg<bool> g_Dx12ForceHeapTier1{"dx12-force-heap-tier1", false};

class RpsAfxD3D12Renderer : public RpsAfxRendererBase
{
public:
    static const uint32_t NUM_SHADER_VISIBLE_DESCRIPTOR_HEAPS = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER + 1;

    struct DescriptorHeapSizeRequirement
    {
        uint32_t staticCount;
        uint32_t dynamicCountPerFrame;
    };

    RpsAfxD3D12Renderer()
    {
        m_descriptorHeapSizes[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = {1024, 512};
        m_descriptorHeapSizes[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER]     = {128, 64};
        m_descriptorHeapSizes[D3D12_DESCRIPTOR_HEAP_TYPE_RTV]         = {64, 0};
        m_descriptorHeapSizes[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]         = {64, 0};
    }

    virtual bool Init(void* hWindow) override final
    {
        m_hWnd = (HWND)hWindow;

        RECT clientRect = {};
        ::GetClientRect(m_hWnd, &clientRect);
        m_width  = clientRect.right - clientRect.left;
        m_height = clientRect.bottom - clientRect.top;

        // Create Device
        UINT dxgiFactoryFlags = 0;

        if (g_DebugDevice)
        {
            ComPtr<ID3D12Debug> debugController;

            // Enable the debug layer (requires the Graphics Tools "optional feature").
            // NOTE: Enabling the debug layer after device creation will invalidate the active device.
            {
                if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
                {
                    debugController->EnableDebugLayer();

                    // Enable additional debug layers.
                    dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
                }
            }
        }

        ComPtr<IDXGIFactory4> factory;
        ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

        auto checkDevice = [](IDXGIAdapter1* pAdapter) {
            return SUCCEEDED(D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr));
        };

        ComPtr<IDXGIAdapter1> adapter = nullptr;
        FindAdapter(factory.Get(), checkDevice, adapter.GetAddressOf(), m_useWarpDevice);
        
        if (adapter == nullptr)
        {
            throw std::exception();
        }

        ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));

        if (g_DebugDevice && (g_DebugDeviceBreakLevel != RPS_AFX_DEBUG_MSG_SEVERITY_NONE))
        {
            ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
            if (SUCCEEDED(m_device->QueryInterface(__uuidof(ID3D12InfoQueue), (void**)&infoQueue)))
            {
                static constexpr struct
                {
                    uint32_t               bit;
                    D3D12_MESSAGE_SEVERITY d3dSeverity;
                } severityMap[] = {
                    {RPS_AFX_DEBUG_MSG_SEVERITY_CORRUPTION, D3D12_MESSAGE_SEVERITY_CORRUPTION},
                    {RPS_AFX_DEBUG_MSG_SEVERITY_ERROR, D3D12_MESSAGE_SEVERITY_ERROR},
                    {RPS_AFX_DEBUG_MSG_SEVERITY_WARNING, D3D12_MESSAGE_SEVERITY_WARNING},
                    {RPS_AFX_DEBUG_MSG_SEVERITY_INFO, D3D12_MESSAGE_SEVERITY_INFO},
                };

                for (auto i = std::begin(severityMap), e = std::end(severityMap); i != e; ++i)
                {
                    if (i->bit & g_DebugDeviceBreakLevel)
                    {
                        infoQueue->SetBreakOnSeverity(i->d3dSeverity, TRUE);
                    }
                }
            }
        }

        for (UINT i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++)
        {
            m_descriptorSizes[i] =
                m_device->GetDescriptorHandleIncrementSize(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i));

            m_descriptorHeaps[i].Init(m_device.Get(),
                                      static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i),
                                      m_descriptorHeapSizes[i].staticCount,
                                      m_descriptorHeapSizes[i].dynamicCountPerFrame,
                                      DXGI_MAX_SWAP_CHAIN_BUFFERS);
        }

        m_swapChainRtvs = AllocStaticDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, DXGI_MAX_SWAP_CHAIN_BUFFERS);

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type                     = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Priority                 = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        queueDesc.NodeMask                 = 1;

        ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_presentQueue)));

        m_queueIndexToCmdListTypeMap[RPS_AFX_QUEUE_INDEX_GFX]     = D3D12_COMMAND_LIST_TYPE_DIRECT;
        m_queueIndexToCmdListTypeMap[RPS_AFX_QUEUE_INDEX_COMPUTE] = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        m_queueIndexToCmdListTypeMap[RPS_AFX_QUEUE_INDEX_COPY]    = D3D12_COMMAND_LIST_TYPE_COPY;

        for (uint32_t i = 0; i < _countof(m_fences); i++)
        {
            ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fences[i])));
        }

        m_fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

        std::fill(m_presentFenceValues.begin(), m_presentFenceValues.end(), 0ULL);

        ThrowIfFailed(m_swapChain.Create(factory.Get(),
                                         m_backBufferCount,
                                         m_width,
                                         m_height,
                                         DXGI_FORMAT_R8G8B8A8_UNORM,
                                         m_device.Get(),
                                         m_presentQueue.Get(),
                                         m_hWnd));
        UpdateSwapChainBuffers();

        ActiveCommandList cmdList = AcquireCmdList(RPS_AFX_QUEUE_INDEX_GFX);

        // To keep alive until init cmdlist is executed.
        std::vector<ComPtr<ID3D12Object>> tempResources;

        OnInit(cmdList.cmdList.Get(), tempResources);

        CloseCmdList(cmdList);
        ID3D12CommandList* pCmdLists[] = {cmdList.cmdList.Get()};
        m_presentQueue->ExecuteCommandLists(1, pCmdLists);
        RecycleCmdList(cmdList);

        WaitForGpuIdle();

        return true;
    }

    virtual void Tick() override final
    {
        OnUpdate(m_frameCounter);

        WaitForSwapChainBuffer();

        m_pendingReleaseResources[m_backBufferIndex].clear();
        for (uint32_t i = 0; i < _countof(m_descriptorHeaps); i++)
        {
            m_descriptorHeaps[i].ResetDynamic(m_backBufferIndex);
        }

        for (uint32_t i = 0; i < RPS_AFX_QUEUE_INDEX_COUNT; i++)
        {
            if (m_backBufferIndex < m_cmdAllocators[i].size())
            {
                for (auto& cmdAllocator : m_cmdAllocators[i][m_backBufferIndex])
                {
                    ThrowIfFailed(cmdAllocator->Reset());
                }
            }
        }

        OnRender(m_frameCounter);

        BOOL bFullscreen;
        m_swapChain.GetFullscreenState(&bFullscreen, nullptr);
        m_swapChain.Present(m_bVSync ? 1 : 0, (m_bVSync || bFullscreen) ? 0 : DXGI_PRESENT_ALLOW_TEARING);

        m_fenceValue++;
        ThrowIfFailed(m_presentQueue->Signal(m_fences[0].Get(), m_fenceValue));
        m_presentFenceValues[m_backBufferIndex] = m_fenceValue;

        m_backBufferIndex = m_swapChain.GetCurrentBackBufferIndex();
        m_frameCounter++;
    }

    virtual void CleanUp() override final
    {
        WaitForGpuIdle();

        OnCleanUp();

        for (uint32_t i = 0; i < _countof(m_descriptorHeaps); i++)
        {
            m_descriptorHeaps[i].CleanUp();
        }

        for (uint32_t i = 0; i < _countof(m_fences); i++)
        {
            m_fences[i] = nullptr;
        }

        for (uint32_t i = 0; i < RPS_AFX_QUEUE_INDEX_COUNT; i++)
        {
            m_cmdAllocators[i].clear();
            m_cmdLists[i].clear();
            m_queues[i] = nullptr;
        }

        for (uint32_t i = 0; i < _countof(m_descriptorHeaps); i++)
        {
            m_descriptorHeaps[i].CleanUp();
        }

        m_pendingReleaseResources.clear();

        m_backBuffers.clear();

        m_swapChain.Destroy();
        m_presentQueue = nullptr;

#if _DEBUG
        ComPtr<ID3D12DebugDevice> pDebugDevice;
        m_device.As(&pDebugDevice);
        if (pDebugDevice)
        {
            pDebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
            pDebugDevice = nullptr;
        }
#else
        m_device = nullptr;
#endif

        ::CloseHandle(m_fenceEvent);
    }

    virtual void OnResize(uint32_t width, uint32_t height) override final
    {
        if ((width > 0 && height > 0) &&
            ((m_width != width) || (m_height != height) || (m_backBuffers.size() != m_backBufferCount)))
        {
            WaitForGpuIdle();

            OnPreResize();

            m_backBuffers.clear();

            DXGI_SWAP_CHAIN_DESC desc        = {};
            BOOL                 bFullScreen = FALSE;
            ThrowIfFailed(m_swapChain.GetFullscreenState(&bFullScreen, nullptr));
            ThrowIfFailed(m_swapChain.GetDesc(&desc));
            ThrowIfFailed(
                m_swapChain.ResizeBuffers(m_backBufferCount, width, height, desc.BufferDesc.Format, desc.Flags));
            UpdateSwapChainBuffers();

            m_width  = width;
            m_height = height;

            OnPostResize();
        }
    }

    virtual RpsResult CreateRpsRuntimeDevice(const RpsDeviceCreateInfo& createInfo, RpsDevice& device) override
    {
        RpsRuntimeDeviceCreateInfo runtimeCreateInfo     = {};
        runtimeCreateInfo.pUserContext                   = this;
        runtimeCreateInfo.callbacks.pfnRecordDebugMarker = &RecordDebugMarker;

        RpsD3D12RuntimeDeviceCreateInfo runtimeDeviceCreateInfo = {};
        runtimeDeviceCreateInfo.pDeviceCreateInfo               = &createInfo;
        runtimeDeviceCreateInfo.pD3D12Device                    = m_device.Get();
        runtimeDeviceCreateInfo.pRuntimeCreateInfo              = &runtimeCreateInfo;

        if (g_Dx12PreferEnhancedBarriers)
        {
            runtimeDeviceCreateInfo.flags |= RPS_D3D12_RUNTIME_FLAG_PREFER_ENHANCED_BARRIERS;
        }

        if (g_Dx12ForceHeapTier1)
        {
            runtimeDeviceCreateInfo.flags |= RPS_D3D12_RUNTIME_FLAG_FORCE_RESOURCE_HEAP_TIER1;
        }

        return rpsD3D12RuntimeDeviceCreate(&runtimeDeviceCreateInfo, &device);
    }

protected:
    virtual bool WaitForGpuIdle() override final
    {
        m_fenceValue++;
        ThrowIfFailed(m_presentQueue->Signal(m_fences[0].Get(), m_fenceValue));

        // Wait until the fence has been processed.
        ThrowIfFailed(m_fences[0]->SetEventOnCompletion(m_fenceValue, m_fenceEvent));
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

        m_pendingReleaseResources[m_backBufferIndex].clear();

        return true;
    }

    void WaitForSwapChainBuffer()
    {
        // If the next frame is not ready to be rendered yet, wait until it is ready.
        while (m_fences[0]->GetCompletedValue() < m_presentFenceValues[m_backBufferIndex])
        {
            ThrowIfFailed(m_fences[0]->SetEventOnCompletion(m_presentFenceValues[m_backBufferIndex], m_fenceEvent));
            WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
        }
    }

    void UpdateSwapChainBuffers()
    {
        m_backBufferIndex = m_swapChain.GetCurrentBackBufferIndex();

        if ((m_frameCounter % m_backBufferCount) != m_backBufferIndex)
            m_frameCounter = m_backBufferIndex;

        m_backBuffers.resize(m_backBufferCount);
        m_pendingReleaseResources.resize(m_backBufferCount);
        m_presentFenceValues.clear();
        m_presentFenceValues.resize(m_backBufferCount, 0ULL);

        for (uint32_t iBuf = 0; iBuf < m_backBufferCount; iBuf++)
        {
            ThrowIfFailed(m_swapChain.GetBuffer(iBuf, IID_PPV_ARGS(&m_backBuffers[iBuf])));
            m_device->CreateRenderTargetView(m_backBuffers[iBuf].Get(), nullptr, m_swapChainRtvs.GetCPU(iBuf));
        }
    }

    ID3D12Resource* GetBackBuffer() const
    {
        return m_backBuffers[m_backBufferIndex].Get();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetBackBufferRTV() const
    {
        return m_swapChainRtvs.GetCPU(m_backBufferIndex);
    }

    void GetBackBuffers(RpsResourceDesc&   backBufferDesc,
                        RpsRuntimeResource backBuffers[DXGI_MAX_SWAP_CHAIN_BUFFERS]) const
    {
        for (uint32_t i = 0; i < m_backBufferCount; i++)
        {
            backBuffers[i] = rpsD3D12ResourceToHandle(m_backBuffers[i].Get());
        }

        backBufferDesc.type              = RPS_RESOURCE_TYPE_IMAGE_2D;
        backBufferDesc.temporalLayers    = uint32_t(m_backBuffers.size());
        backBufferDesc.flags             = 0;
        backBufferDesc.image.arrayLayers = 1;
        backBufferDesc.image.mipLevels   = 1;
        backBufferDesc.image.format      = rpsFormatFromDXGI(m_swapChain.GetFormat());
        backBufferDesc.image.width       = m_width;
        backBufferDesc.image.height      = m_height;
        backBufferDesc.image.sampleCount     = 1;
    }

    static void RecordDebugMarker(void* pUserContext, const RpsRuntimeOpRecordDebugMarkerArgs* pArgs)
    {
        auto* pCmdList = rpsD3D12CommandListFromHandle(pArgs->hCommandBuffer);
        switch (pArgs->mode)
        {
        case RPS_RUNTIME_DEBUG_MARKER_BEGIN:
            PIXBeginEvent(pCmdList, 0, pArgs->text);
            break;
        case RPS_RUNTIME_DEBUG_MARKER_END:
            PIXEndEvent(pCmdList);
            break;
        case RPS_RUNTIME_DEBUG_MARKER_LABEL:
            PIXSetMarker(pCmdList, 0, pArgs->text);
            break;
        }
    }

public:
    struct ActiveCommandList
    {
        uint32_t                          backBufferIndex;
        RpsAfxQueueIndices                queueIndex;
        ComPtr<ID3D12GraphicsCommandList> cmdList;
        ComPtr<ID3D12CommandAllocator>    cmdAllocator;

        ID3D12GraphicsCommandList* operator->() const
        {
            return cmdList.Get();
        }
    };

    struct FenceSignalInfo
    {
        uint32_t queueIndex;
        UINT64   value;

        FenceSignalInfo()
            : queueIndex(UINT32_MAX)
            , value(UINT64_MAX)
        {
        }
    };

protected:
    virtual void OnInit(ID3D12GraphicsCommandList* pInitCmdList, std::vector<ComPtr<ID3D12Object>>& tempResources)
    {
    }

    virtual void OnCleanUp()
    {
    }

    virtual void OnPreResize()
    {
    }

    virtual void OnPostResize()
    {
    }

    virtual void OnUpdate(uint32_t frameIndex)
    {
    }

    virtual void OnRender(uint32_t frameIndex)
    {
        ActiveCommandList cmdList = AcquireCmdList(RPS_AFX_QUEUE_INDEX_GFX);

        CloseCmdList(cmdList);
        ID3D12CommandList* pCmdLists[] = {cmdList.cmdList.Get()};
        m_presentQueue->ExecuteCommandLists(1, pCmdLists);
        RecycleCmdList(cmdList);
    }

    RpsResult ExecuteRenderGraph(uint32_t frameIndex, RpsRenderGraph hRenderGraph)
    {
        RpsRenderGraphBatchLayout batchLayout = {};

        RpsResult result = rpsRenderGraphGetBatchLayout(hRenderGraph, &batchLayout);
        if (RPS_FAILED(result))
        {
            return result;
        }

        m_fenceSignalInfos.resize(batchLayout.numFenceSignals);

        for (uint32_t iBatch = 0; iBatch < batchLayout.numCmdBatches; iBatch++)
        {
            auto& batch = batchLayout.pCmdBatches[iBatch];

            ID3D12CommandQueue* pCurrQueue = GetCmdQueue(RpsAfxQueueIndices(batch.queueIndex));

            ActiveCommandList cmdList = AcquireCmdList(RpsAfxQueueIndices(batch.queueIndex));

            RpsRenderGraphRecordCommandInfo recordInfo = {};

            recordInfo.pUserContext  = this;
            recordInfo.cmdBeginIndex = batch.cmdBegin;
            recordInfo.numCmds       = batch.numCmds;
            recordInfo.hCmdBuffer    = rpsD3D12CommandListToHandle(cmdList.cmdList.Get());

            if (g_DebugMarkers)
            {
                recordInfo.flags = RPS_RECORD_COMMAND_FLAG_ENABLE_COMMAND_DEBUG_MARKERS;
            }

            for (uint32_t iWaitIdx = batch.waitFencesBegin; iWaitIdx < (batch.waitFencesBegin + batch.numWaitFences);
                 iWaitIdx++)
            {
                const auto& signalInfo = m_fenceSignalInfos[batchLayout.pWaitFenceIndices[iWaitIdx]];
                HRESULT     hr         = pCurrQueue->Wait(m_fences[signalInfo.queueIndex].Get(), signalInfo.value);
                if (FAILED(hr))
                {
                    return RPS_ERROR_UNSPECIFIED;
                }
            }

            result = rpsRenderGraphRecordCommands(hRenderGraph, &recordInfo);
            if (RPS_FAILED(result))
                return result;

            CloseCmdList(cmdList);
            ID3D12CommandList* pCmdLists[] = {cmdList.cmdList.Get()};
            pCurrQueue->ExecuteCommandLists(1, pCmdLists);
            RecycleCmdList(cmdList);

            if (batch.signalFenceIndex != RPS_INDEX_NONE_U32)
            {
                m_fenceValue++;

                auto& signalInfo = m_fenceSignalInfos[batch.signalFenceIndex];

                signalInfo.queueIndex = batch.queueIndex;
                signalInfo.value      = m_fenceValue;

                HRESULT hr = pCurrQueue->Signal(m_fences[signalInfo.queueIndex].Get(), signalInfo.value);
                if (FAILED(hr))
                {
                    return RPS_ERROR_UNSPECIFIED;
                }
            }
        }

        return result;
    }

    ActiveCommandList AcquireCmdList(RpsAfxQueueIndices queueIndex)
    {
        ActiveCommandList result = {};
        result.backBufferIndex   = m_backBufferIndex;
        result.queueIndex        = queueIndex;

        const D3D12_COMMAND_LIST_TYPE cmdListType = m_queueIndexToCmdListTypeMap[queueIndex];

        std::lock_guard<std::mutex> lock(m_cmdListMutex);

        if (m_cmdAllocators[queueIndex].size() <= m_backBufferIndex)
        {
            m_cmdAllocators[queueIndex].resize(m_backBufferCount);
        }

        if (m_cmdAllocators[queueIndex][m_backBufferIndex].empty())
        {
            ThrowIfFailed(m_device->CreateCommandAllocator(cmdListType, IID_PPV_ARGS(&result.cmdAllocator)));
        }
        else
        {
            result.cmdAllocator = m_cmdAllocators[queueIndex][m_backBufferIndex].back();
            m_cmdAllocators[queueIndex][m_backBufferIndex].pop_back();
        }

        if (m_cmdLists[queueIndex].empty())
        {
            ThrowIfFailed(m_device->CreateCommandList(
                1, cmdListType, result.cmdAllocator.Get(), nullptr, IID_PPV_ARGS(&result.cmdList)));
        }
        else
        {
            result.cmdList = m_cmdLists[queueIndex].back();
            m_cmdLists[queueIndex].pop_back();
            ThrowIfFailed(result.cmdList->Reset(result.cmdAllocator.Get(), nullptr));
        }

        return result;
    }

    void CloseCmdList(ActiveCommandList& cmdList)
    {
        assert(cmdList.cmdAllocator != nullptr);
        assert(cmdList.cmdList != nullptr);
        assert(cmdList.backBufferIndex == m_backBufferIndex);

        const D3D12_COMMAND_LIST_TYPE cmdListType = cmdList.cmdList->GetType();
        const uint32_t                queueIndex  = cmdList.queueIndex;

        std::lock_guard<std::mutex> lock(m_cmdListMutex);

        m_cmdAllocators[queueIndex][m_backBufferIndex].push_back(cmdList.cmdAllocator);
        cmdList.cmdAllocator = nullptr;

        ThrowIfFailed(cmdList.cmdList->Close());
    }

    void RecycleCmdList(ActiveCommandList& cmdList)
    {
        assert(cmdList.cmdAllocator == nullptr);

        const D3D12_COMMAND_LIST_TYPE cmdListType = cmdList.cmdList->GetType();
        const uint32_t                queueIndex  = cmdList.queueIndex;

        std::lock_guard<std::mutex> lock(m_cmdListMutex);

        m_cmdLists[queueIndex].push_back(cmdList.cmdList);
        cmdList.cmdList = nullptr;
    }

    ID3D12CommandQueue* GetCmdQueue(RpsAfxQueueIndices queueIndex)
    {
        ID3D12CommandQueue* result = nullptr;

        if (queueIndex < RPS_AFX_QUEUE_INDEX_COUNT)
        {
            std::lock_guard<std::mutex> lock(m_cmdListMutex);

            result = m_queues[queueIndex].Get();

            if (!result)
            {
                if (m_queueIndexToCmdListTypeMap[queueIndex] == D3D12_COMMAND_LIST_TYPE_DIRECT)
                {
                    m_queues[queueIndex] = m_presentQueue;
                }
                else
                {
                    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
                    queueDesc.Type                     = m_queueIndexToCmdListTypeMap[queueIndex];

                    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_queues[queueIndex])));
                }

                result = m_queues[queueIndex].Get();
            }
        }

        return result;
    }

    struct DescriptorHeap;

    struct DescriptorTable
    {
        DescriptorHeap*             pHeap;
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHdl;
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHdl;

        D3D12_CPU_DESCRIPTOR_HANDLE GetCPU(uint32_t index) const
        {
            return {cpuHdl.ptr + index * pHeap->descriptorSize};
        }
        D3D12_GPU_DESCRIPTOR_HANDLE GetGPU(uint32_t index) const
        {
            return {gpuHdl.ptr + index * pHeap->descriptorSize};
        }
    };

    struct DescriptorHeap
    {
        ComPtr<ID3D12DescriptorHeap> pHeap;

        uint32_t descriptorSize             = 0;
        uint32_t capacity                   = 0;
        uint32_t staticDescriptors          = 0;
        uint32_t dynamicDescriptorsPerFrame = 0;
        uint32_t currentDynamicStart        = 0;
        uint32_t numStaticUsed              = 0;
        uint32_t numDynamicUsed             = 0;

        HRESULT AllocStatic(uint32_t count, DescriptorTable* pTable)
        {
            return AllocRange(0, staticDescriptors, &numStaticUsed, count, pTable);
        }

        HRESULT AllocDynamic(uint32_t count, DescriptorTable* pTable)
        {
            return AllocRange(
                currentDynamicStart, currentDynamicStart + dynamicDescriptorsPerFrame, &numDynamicUsed, count, pTable);
        }

        void Init(ID3D12Device*              pDevice,
                  D3D12_DESCRIPTOR_HEAP_TYPE type,
                  uint32_t                   staticCount,
                  uint32_t                   dynamicCountPerFrame,
                  uint32_t                   maxQueuedFrames)
        {
            uint32_t totalCount = staticCount + dynamicCountPerFrame * maxQueuedFrames;

            descriptorSize             = pDevice->GetDescriptorHandleIncrementSize(type);
            capacity                   = totalCount;
            staticDescriptors          = staticCount;
            dynamicDescriptorsPerFrame = dynamicCountPerFrame;
            currentDynamicStart        = 0;
            numStaticUsed              = 0;
            numDynamicUsed             = 0;

            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Type                       = type;
            desc.NodeMask                   = 1;
            desc.NumDescriptors             = capacity;
            desc.Flags                      = (type < (int32_t)NUM_SHADER_VISIBLE_DESCRIPTOR_HEAPS)
                                                  ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                                  : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

            ThrowIfFailed(pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pHeap)));

            descriptorSize = pDevice->GetDescriptorHandleIncrementSize(type);
        }

        void CleanUp()
        {
            pHeap = nullptr;
        }

        void ResetDynamic(uint32_t backBufferIndex)
        {
            numDynamicUsed      = 0;
            currentDynamicStart = staticDescriptors + dynamicDescriptorsPerFrame * backBufferIndex;
        }

        void ResetStatic(uint32_t numStaticDescriptorsToKeep)
        {
            assert(numStaticDescriptorsToKeep <= numStaticUsed);
            numStaticUsed = numStaticDescriptorsToKeep;
        }

    private:
        HRESULT AllocRange(uint32_t begin, uint32_t end, uint32_t* pUsed, uint32_t count, DescriptorTable* pTable)
        {
            if (begin + (*pUsed) + count > end)
            {
                return E_OUTOFMEMORY;
            }

            pTable->cpuHdl  = pHeap->GetCPUDescriptorHandleForHeapStart();
            pTable->gpuHdl  = (pHeap->GetDesc().Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
                                  ? pHeap->GetGPUDescriptorHandleForHeapStart()
                                  : D3D12_GPU_DESCRIPTOR_HANDLE{};
            size_t ptrDelta = (begin + (*pUsed)) * descriptorSize;
            pTable->cpuHdl.ptr += ptrDelta;
            pTable->gpuHdl.ptr += ptrDelta;
            pTable->pHeap = this;

            *pUsed += count;

            return S_OK;
        }
    };

    DescriptorTable AllocStaticDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t count)
    {
        DescriptorTable result = {};
        ThrowIfFailed(m_descriptorHeaps[type].AllocStatic(count, &result));
        return result;
    }

    DescriptorTable AllocStaticCBV_SRV_UAVs(uint32_t count)
    {
        return AllocStaticDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, count);
    }

    DescriptorTable AllocDynamicDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t count)
    {
        DescriptorTable result = {};
        ThrowIfFailed(m_descriptorHeaps[type].AllocDynamic(count, &result));
        return result;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE AllocDynamicDescriptorsAndWrite(D3D12_DESCRIPTOR_HEAP_TYPE         type,
                                                                const D3D12_CPU_DESCRIPTOR_HANDLE* pCpuOnlyHandles,
                                                                uint32_t                           numHandles,
                                                                bool                               bSingleTable = false)
    {
        DescriptorTable table = AllocDynamicDescriptors(type, numHandles);

        if (bSingleTable)
        {
            m_device->CopyDescriptorsSimple(numHandles, table.GetCPU(0), pCpuOnlyHandles[0], type);
        }
        else
        {
            for (uint32_t i = 0; i < numHandles; i++)
            {
                m_device->CopyDescriptorsSimple(1, table.GetCPU(i), pCpuOnlyHandles[i], type);
            }
        }
        return table.GetGPU(0);
    }

    D3D12_GPU_DESCRIPTOR_HANDLE AllocDynamicDescriptorAndWriteCBV(D3D12_GPU_VIRTUAL_ADDRESS gpuVa, uint32_t size)
    {
        auto table = AllocDynamicDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation                  = gpuVa;
        cbvDesc.SizeInBytes                     = size;
        m_device->CreateConstantBufferView(&cbvDesc, table.GetCPU(0));

        return table.GetGPU(0);
    }

    D3D12_GPU_DESCRIPTOR_HANDLE AllocStaticDescriptorsAndWriteCBV(D3D12_GPU_VIRTUAL_ADDRESS gpuVa, uint32_t size)
    {
        auto table = AllocStaticDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation                  = gpuVa;
        cbvDesc.SizeInBytes                     = size;
        m_device->CreateConstantBufferView(&cbvDesc, table.GetCPU(0));

        return table.GetGPU(0);
    }

    void BindDescriptorHeaps(ID3D12GraphicsCommandList* pCmdList)
    {
        ID3D12DescriptorHeap* pHeaps[2] = {};
        uint32_t              numHeaps  = 0;
        for (uint32_t i = 0; i < NUM_SHADER_VISIBLE_DESCRIPTOR_HEAPS; i++)
        {
            if (m_descriptorHeaps[i].pHeap)
            {
                pHeaps[numHeaps] = m_descriptorHeaps[i].pHeap.Get();
                numHeaps++;
            }
        }
        pCmdList->SetDescriptorHeaps(numHeaps, pHeaps);
    }

    struct SwapChain
    {
        HRESULT Create(IDXGIFactory2*      pFactory,
                       uint32_t            backBufferCount,
                       uint32_t            width,
                       uint32_t            height,
                       DXGI_FORMAT         backBufferFormat,
                       ID3D12Device*       pDevice,
                       ID3D12CommandQueue* pPresentQueue,
                       HWND                hWnd)
        {
            if (m_swapChain)
                return S_FALSE;

            m_pDevice = pDevice;
            m_hWnd    = hWnd;

            // Describe and create the swap chain.
            DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
            swapChainDesc.BufferCount           = backBufferCount;
            swapChainDesc.Width                 = width;
            swapChainDesc.Height                = height;
            swapChainDesc.Format                = backBufferFormat;
            swapChainDesc.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swapChainDesc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            swapChainDesc.SampleDesc.Count      = 1;
            swapChainDesc.Flags                 = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

            ComPtr<IDXGISwapChain1> swapChain;

            HRESULT hr =
                pFactory->CreateSwapChainForHwnd(pPresentQueue, hWnd, &swapChainDesc, nullptr, nullptr, &swapChain);
            if (hr != DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
            {
                ThrowIfFailed(hr);

                // This sample does not support fullscreen transitions.
                ThrowIfFailed(pFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

                ThrowIfFailed(swapChain.As(&m_swapChain));
            }
            else
            {
                DWORD sessionId = 0;
                ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);
                if (sessionId == 0)
                {
                    fprintf_s(stderr, "\nCreating fallback dummy swapchain for session 0 process.");
                    hr = ResizeBuffers(backBufferCount, width, height, backBufferFormat, 0);
                }
            }

            return hr;
        }

        void Destroy()
        {
            m_swapChain       = nullptr;
            m_backBufferIndex = 0;
            m_buffers.clear();
        }

        HRESULT GetFullscreenState(BOOL* pbFullscreen, IDXGIOutput** ppOutput)
        {
            if (m_swapChain)
                return m_swapChain->GetFullscreenState(pbFullscreen, ppOutput);

            if (pbFullscreen)
                *pbFullscreen = FALSE;

            if (ppOutput)
                *ppOutput = nullptr;

            return S_OK;
        }

        HRESULT GetDesc(DXGI_SWAP_CHAIN_DESC* pDesc) const
        {
            if (m_swapChain)
                return m_swapChain->GetDesc(pDesc);

            if (m_buffers.empty())
                return E_FAIL;

            const D3D12_RESOURCE_DESC bufDesc = m_buffers[0]->GetDesc();

            pDesc->BufferDesc.Width                   = (uint32_t)bufDesc.Width;
            pDesc->BufferDesc.Height                  = bufDesc.Height;
            pDesc->BufferDesc.RefreshRate.Denominator = 1;
            pDesc->BufferDesc.RefreshRate.Numerator   = 60;
            pDesc->BufferDesc.Format                  = bufDesc.Format;
            pDesc->BufferDesc.ScanlineOrdering        = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
            pDesc->BufferDesc.Scaling                 = DXGI_MODE_SCALING_UNSPECIFIED;
            pDesc->SampleDesc.Count                   = 1;
            pDesc->SampleDesc.Quality                 = 0;
            pDesc->BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            pDesc->BufferCount                        = (UINT)m_buffers.size();
            pDesc->OutputWindow                       = m_hWnd;
            pDesc->Windowed                           = TRUE;
            pDesc->SwapEffect                         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            pDesc->Flags                              = 0;

            return S_OK;
        }

        DXGI_FORMAT GetFormat() const
        {
            DXGI_SWAP_CHAIN_DESC desc;
            if (SUCCEEDED(GetDesc(&desc)))
                return desc.BufferDesc.Format;
            else
                return DXGI_FORMAT_UNKNOWN;
        }

        HRESULT ResizeBuffers(UINT backBufferCount, UINT width, UINT height, DXGI_FORMAT backBufferFormat, UINT flags)
        {
            if (m_swapChain)
                return m_swapChain->ResizeBuffers(backBufferCount, width, height, backBufferFormat, flags);

            m_buffers.clear();
            m_buffers.resize(backBufferCount);
            for (uint32_t i = 0; i < backBufferCount; i++)
            {
                auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
                auto resDesc   = CD3DX12_RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                                                     0,
                                                     width,
                                                     height,
                                                     1,
                                                     1,
                                                     backBufferFormat,
                                                     1,
                                                     0,
                                                     D3D12_TEXTURE_LAYOUT_UNKNOWN,
                                                     D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
                ThrowIfFailed(m_pDevice->CreateCommittedResource(&heapProps,
                                                                 D3D12_HEAP_FLAG_NONE,
                                                                 &resDesc,
                                                                 D3D12_RESOURCE_STATE_COMMON,
                                                                 nullptr,
                                                                 IID_PPV_ARGS(&m_buffers[i])));
                m_backBufferIndex = 0;
            }

            return S_OK;
        }

        HRESULT GetBuffer(UINT index, REFIID riid, void** ppSurface)
        {
            if (m_swapChain)
            {
                return m_swapChain->GetBuffer(index, riid, ppSurface);
            }
            else
            {
                return m_buffers[index]->QueryInterface(riid, ppSurface);
            }
        }

        HRESULT Present(UINT syncInternal, UINT flags)
        {
            HRESULT hr = S_OK;
            if (m_swapChain)
            {
                hr                = m_swapChain->Present(syncInternal, flags);
                m_backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
            }
            else
            {
                m_backBufferIndex = (m_backBufferIndex + 1) % m_buffers.size();
            }
            return hr;
        }

        UINT GetCurrentBackBufferIndex()
        {
            return m_swapChain ? m_swapChain->GetCurrentBackBufferIndex() : m_backBufferIndex;
        }

        HWND                                m_hWnd;
        ComPtr<ID3D12Device>                m_pDevice;
        ComPtr<IDXGISwapChain3>             m_swapChain;
        std::vector<ComPtr<ID3D12Resource>> m_buffers;
        uint32_t                            m_backBufferIndex;
    };

    uint64_t CalcGuaranteedCompletedFrameIndexForRps() const
    {
        // we always wait for swapchain buffer before rendering a new frame, so can guarntee at least
        // this index for gpu complete status
        return (m_frameCounter > m_backBufferCount) ? m_frameCounter - m_backBufferCount
                                                    : RPS_GPU_COMPLETED_FRAME_INDEX_NONE;
    }

    void CreateStaticCheckerboardTexture(ComPtr<ID3D12Resource>&            texture,
                                         std::vector<ComPtr<ID3D12Object>>& tempResources,
                                         ID3D12GraphicsCommandList*         pCmdList,
                                         uint32_t                           texWidth,
                                         uint32_t                           texHeight,
                                         const float                        tintColor[4]) const
    {
        ComPtr<ID3D12Resource> textureUploadHeap;

        static const UINT TexturePixelSize = 4;

        // Describe and create a Texture2D.
        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.MipLevels           = 1;
        textureDesc.Format              = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.Width               = texWidth;
        textureDesc.Height              = texHeight;
        textureDesc.Flags               = D3D12_RESOURCE_FLAG_NONE;
        textureDesc.DepthOrArraySize    = 1;
        textureDesc.SampleDesc.Count    = 1;
        textureDesc.SampleDesc.Quality  = 0;
        textureDesc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

        auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(m_device->CreateCommittedResource(&heapProps,
                                                        D3D12_HEAP_FLAG_NONE,
                                                        &textureDesc,
                                                        D3D12_RESOURCE_STATE_COPY_DEST,
                                                        nullptr,
                                                        IID_PPV_ARGS(&texture)));

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Get(), 0, 1);

        // Create the GPU upload buffer.
        auto uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bufferDesc      = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

        ThrowIfFailed(m_device->CreateCommittedResource(&uploadHeapProps,
                                                        D3D12_HEAP_FLAG_NONE,
                                                        &bufferDesc,
                                                        D3D12_RESOURCE_STATE_GENERIC_READ,
                                                        nullptr,
                                                        IID_PPV_ARGS(&textureUploadHeap)));
        textureUploadHeap->SetName(L"textureUploadHeap");

        // Copy data to the intermediate upload heap and then schedule a copy
        // from the upload heap to the Texture2D.
        const UINT rowPitch    = texWidth * TexturePixelSize;
        const UINT cellPitch   = rowPitch >> 3;  // The width of a cell in the checkboard texture.
        const UINT cellHeight  = texWidth >> 3;  // The height of a cell in the checkerboard texture.
        const UINT textureSize = rowPitch * texHeight;

        std::vector<UINT8> data(textureSize);
        UINT8*             pData = &data[0];

#define RPS_AFX_SCALE_BYTE(B, S) (std::max<uint8_t>(0, std::min<int32_t>(0xff, (int32_t((B) * (S))))))

        for (UINT n = 0; n < textureSize; n += TexturePixelSize)
        {
            UINT x = n % rowPitch;
            UINT y = n / rowPitch;
            UINT i = x / cellPitch;
            UINT j = y / cellHeight;

            if (i % 2 == j % 2)
            {
                pData[n]     = RPS_AFX_SCALE_BYTE(0xa0, tintColor[0]);  // R
                pData[n + 1] = RPS_AFX_SCALE_BYTE(0xa0, tintColor[1]);  // G
                pData[n + 2] = RPS_AFX_SCALE_BYTE(0xa0, tintColor[2]);  // B
                pData[n + 3] = RPS_AFX_SCALE_BYTE(0xff, tintColor[3]);  // A
            }
            else
            {
                pData[n]     = RPS_AFX_SCALE_BYTE(0xff, tintColor[0]);  // R
                pData[n + 1] = RPS_AFX_SCALE_BYTE(0xff, tintColor[1]);  // G
                pData[n + 2] = RPS_AFX_SCALE_BYTE(0xff, tintColor[2]);  // B
                pData[n + 3] = RPS_AFX_SCALE_BYTE(0xff, tintColor[3]);  // A
            }
        }

#undef RPS_AFX_SCALE_BYTE

        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData                  = &data[0];
        textureData.RowPitch               = texWidth * TexturePixelSize;
        textureData.SlicePitch             = textureData.RowPitch * texHeight;

        UpdateSubresources(pCmdList, texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        pCmdList->ResourceBarrier(1, &barrier);

        // Keep it around until upload cmdlist finishes executing.
        tempResources.push_back(textureUploadHeap);
    }

protected:
    bool                                m_useWarpDevice   = g_WarpDevice;
    bool                                m_bVSync          = g_VSync;
    HWND                                m_hWnd            = NULL;
    UINT                                m_width           = 0;
    UINT                                m_height          = 0;
    uint32_t                            m_backBufferCount = 3;
    ComPtr<ID3D12Device>                m_device;
    SwapChain                           m_swapChain;
    uint32_t                            m_backBufferIndex = 0;
    ComPtr<ID3D12CommandQueue>          m_presentQueue;
    ComPtr<ID3D12CommandQueue>          m_queues[RPS_AFX_QUEUE_INDEX_COUNT];
    ComPtr<ID3D12Fence>                 m_fences[RPS_AFX_QUEUE_INDEX_COUNT];
    std::vector<FenceSignalInfo>        m_fenceSignalInfos;
    std::vector<UINT64>                 m_presentFenceValues;
    std::vector<ComPtr<ID3D12Resource>> m_backBuffers;
    UINT64                              m_fenceValue = 0;
    HANDLE                              m_fenceEvent = INVALID_HANDLE_VALUE;
    uint32_t                            m_descriptorSizes[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
    DescriptorHeapSizeRequirement       m_descriptorHeapSizes[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
    DescriptorTable                     m_swapChainRtvs;

    D3D12_COMMAND_LIST_TYPE                                  m_queueIndexToCmdListTypeMap[RPS_AFX_QUEUE_INDEX_COUNT];
    std::vector<std::vector<ComPtr<ID3D12CommandAllocator>>> m_cmdAllocators[RPS_AFX_QUEUE_INDEX_COUNT];
    std::vector<ComPtr<ID3D12GraphicsCommandList>>           m_cmdLists[RPS_AFX_QUEUE_INDEX_COUNT];
    std::mutex                                               m_cmdListMutex;
    DescriptorHeap                                           m_descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

    std::vector<std::vector<ComPtr<ID3D12Object>>> m_pendingReleaseResources;

    uint32_t m_frameCounter = 0;
};

#ifdef RPS_DX12_AGILITY_SDK_VER
extern "C" __declspec(dllexport) const UINT D3D12SDKVersion = RPS_DX12_AGILITY_SDK_VER;
extern "C" __declspec(dllexport) const char* D3D12SDKPath   = u8".\\D3D12\\";
#endif  //ifdef RPS_DX12_AGILITY_SDK_VER
