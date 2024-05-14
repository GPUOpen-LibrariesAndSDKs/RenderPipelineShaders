// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#include "rpsl_explorer.hpp"
#include "app_framework/afx_d3d12_renderer.hpp"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx12.h"

class ToolDX12Renderer : public RpsAfxD3D12Renderer
{

public:
    ToolDX12Renderer(RpslExplorer* pRpslExplorer)
        : m_pRpslExplorer(pRpslExplorer)
    {
    }

    virtual void OnInit(ID3D12GraphicsCommandList*         pInitCmdList,
                        std::vector<ComPtr<ID3D12Object>>& tempResources) override final
    {
#if D3D12_SDK_VERSION >= 606
        // NOTE: SDkv>=606 needed for D3D12_MESSAGE_ID_NON_OPTIMAL_BARRIER_ONLY_EXECUTE_COMMAND_LISTS.
        if (g_DebugDevice)
        {
            ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
            if (SUCCEEDED(m_device->QueryInterface(__uuidof(ID3D12InfoQueue), (void**)&infoQueue)))
            {
                // Drawing barrier-only frames is a main use case of this tool, disable related warning:
                D3D12_MESSAGE_ID disabledD3DMsgIds[] = {
                    D3D12_MESSAGE_ID_NON_OPTIMAL_BARRIER_ONLY_EXECUTE_COMMAND_LISTS,
                };

                D3D12_INFO_QUEUE_FILTER filter = {};
                filter.DenyList.NumIDs         = _countof(disabledD3DMsgIds);
                filter.DenyList.pIDList        = disabledD3DMsgIds;

                infoQueue->AddStorageFilterEntries(&filter);
            }
        }
#endif //D3D12_SDK_VERSION >= 606

#if RPS_D3D12_FEATURE_D3D12_OPTIONS12_DEFINED
        D3D12_FEATURE_DATA_D3D12_OPTIONS12 featureOptionsData12 = {};

        if (SUCCEEDED(m_device->CheckFeatureSupport(
                D3D12_FEATURE_D3D12_OPTIONS12, &featureOptionsData12, sizeof(featureOptionsData12))))
        {
            auto pPrinter = rpsGetGlobalDebugPrinter();
            if (pPrinter)
            {
                pPrinter->pfnPrintf(pPrinter->pContext,
                                    "D3D12 EnhancedBarriersSupported : %s",
                                    featureOptionsData12.EnhancedBarriersSupported ? "true" : "false");
            }
        }
#endif //RPS_D3D12_FEATURE_D3D12_OPTIONS12_DEFINED

        m_pRpslExplorer->Init(m_hWnd);

        auto imguiDT = AllocStaticDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);

        ImGui_ImplDX12_Init(m_device.Get(),
                            m_backBufferCount,
                            m_swapChain.GetFormat(),
                            imguiDT.pHeap->pHeap.Get(),
                            imguiDT.GetCPU(0),
                            imguiDT.GetGPU(0));
    }

    virtual void OnCleanUp() override
    {
        ImGui_ImplDX12_Shutdown();

        m_pRpslExplorer->CleanUp();
    }

    virtual void OnPreResize() override
    {
    }

    virtual void OnPostResize() override
    {
        m_pRpslExplorer->OnResize(m_width, m_height);
    }

    virtual void OnUpdate(uint32_t frameIndex) override
    {
        ImGui_ImplDX12_NewFrame();
        m_pRpslExplorer->RenderImGuiFrame();

        RpsRuntimeResource backBuffers[DXGI_MAX_SWAP_CHAIN_BUFFERS];
        RpsResourceDesc    backBufferDesc;
        GetBackBuffers(backBufferDesc, backBuffers);

        RpsConstant               args[]         = {&backBufferDesc};
        const RpsRuntimeResource* argResources[] = {backBuffers};

        uint64_t completedFrameIndex = CalcGuaranteedCompletedFrameIndexForRps();

        m_pRpslExplorer->UpdateRpsPipeline(
            frameIndex, completedFrameIndex, uint32_t(std::size(args)), args, argResources);

        m_pRpslExplorer->Tick();
    }

    virtual void OnRender(uint32_t frameIndex) override
    {
        RpsRenderGraph hRenderGraph = m_pRpslExplorer->GetRenderGraph();
        if (hRenderGraph != RPS_NULL_HANDLE)
        {
            ExecuteRenderGraph(frameIndex, hRenderGraph);
        }

        ActiveCommandList cmdList = AcquireCmdList(RPS_AFX_QUEUE_INDEX_GFX);

        BindDescriptorHeaps(cmdList.cmdList.Get());

        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            GetBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList->ResourceBarrier(1, &barrier);

        float clearZero[4] = {};
        auto  backBufferRTV = GetBackBufferRTV();
        cmdList->ClearRenderTargetView(backBufferRTV, clearZero, 0, nullptr);
        cmdList->OMSetRenderTargets(1, &backBufferRTV, TRUE, nullptr);

        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList.cmdList.Get());

        barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            GetBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        cmdList->ResourceBarrier(1, &barrier);

        CloseCmdList(cmdList);
        ID3D12CommandList* pCmdLists[] = {cmdList.cmdList.Get()};
        m_presentQueue->ExecuteCommandLists(1, pCmdLists);
        RecycleCmdList(cmdList);
    }

    virtual LRESULT WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool& bHandled) override final
    {
        return m_pRpslExplorer->WindowProc(hWnd, message, wParam, lParam, bHandled);
    }

private:
    RpslExplorer* m_pRpslExplorer = {};
};
