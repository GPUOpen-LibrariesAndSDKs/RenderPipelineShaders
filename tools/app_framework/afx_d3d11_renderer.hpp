// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#include <dxgi1_6.h>
#include <d3d11_1.h>
#include <wrl/client.h>
#include <mutex>
#include <vector>

#include <dxgidebug.h>
#include <pix.h>

#ifndef RPS_D3D11_RUNTIME
#define RPS_D3D11_RUNTIME 1
#endif  //RPS_D3D12_RUNTIME

#include "rps/rps.h"

#include "afx_renderer.hpp"
#include "afx_d3d_helper.hpp"
#include "afx_shader_compiler.hpp"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

class RpsAfxD3D11Renderer : public RpsAfxRendererBase
{
public:
    virtual bool Init(void* hWindow) override final
    {
        m_hWnd = (HWND)hWindow;

        RECT clientRect = {};
        ::GetClientRect(m_hWnd, &clientRect);
        m_width  = clientRect.right - clientRect.left;
        m_height = clientRect.bottom - clientRect.top;

        // Create Device
        UINT dxgiFactoryFlags  = 0;
        UINT deviceCreateFlags = 0;

#if defined(_DEBUG)
        ComPtr<ID3D11Debug> debugController;

        // Enable additional debug layers.
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        deviceCreateFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        ComPtr<IDXGIFactory4> factory;
        ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

        D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_1};

        auto checkDevice = [featureLevels](IDXGIAdapter1* pAdapter) {
            return SUCCEEDED(D3D11CreateDevice(pAdapter,
                                               D3D_DRIVER_TYPE_UNKNOWN,
                                               nullptr,
                                               0,
                                               featureLevels,
                                               _countof(featureLevels),
                                               D3D11_SDK_VERSION,
                                               nullptr,
                                               nullptr,
                                               nullptr));
        };

        ComPtr<IDXGIAdapter1> adapter = nullptr;
        FindAdapter(factory.Get(), checkDevice, adapter.GetAddressOf(), m_useWarpDevice);

        if (adapter == nullptr)
        {
            throw std::exception();
        }

        ThrowIfFailed(D3D11CreateDevice(adapter.Get(),
                                        D3D_DRIVER_TYPE_UNKNOWN,
                                        nullptr,
                                        deviceCreateFlags,
                                        featureLevels,
                                        _countof(featureLevels),
                                        D3D11_SDK_VERSION,
                                        &m_device,
                                        nullptr,
                                        &m_immDC));

#define D3D_SDK_LAYER_BREAK_ON_WARNING 0
#if D3D_SDK_LAYER_BREAK_ON_WARNING
        ComPtr<ID3D11InfoQueue> infoQueue = nullptr;
        if (SUCCEEDED(m_device->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&infoQueue)))
        {
            infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
            infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, TRUE);
        }
#endif

        ThrowIfFailed(m_swapChain.Create(
            factory.Get(), m_backBufferCount, m_width, m_height, DXGI_FORMAT_R8G8B8A8_UNORM, m_device.Get(), m_hWnd));
        UpdateSwapChainBuffers();

        OnInit();

        return true;
    }

    virtual void Tick() override final
    {
        OnUpdate(m_frameCounter);

        OnRender(m_frameCounter);

        m_swapChain.Present(m_bVSync ? 1 : 0, 0);

        m_backBufferIndex = m_swapChain.GetCurrentBackBufferIndex();
        m_frameCounter++;
    }

    virtual void CleanUp() override final
    {
        WaitForGpuIdle();

        OnCleanUp();

        m_deferredContexts.clear();
        m_swapChainRtv = nullptr;
        m_backBuffer   = nullptr;
        m_swapChain.Destroy();
        m_immDC = nullptr;

#if _DEBUG
        ComPtr<ID3D11Debug> pDebugDevice;
        m_device.As(&pDebugDevice);
        if (pDebugDevice)
        {
            pDebugDevice->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
            pDebugDevice = nullptr;
        }
#endif
        m_device = nullptr;
    }

    virtual void OnResize(uint32_t width, uint32_t height) override final
    {
        if ((width > 0 && height > 0) && ((m_width != width) || (m_height != height)))
        {
            WaitForGpuIdle();

            OnPreResize();

            m_swapChainRtv = nullptr;
            m_backBuffer   = nullptr;

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

protected:

    virtual bool WaitForGpuIdle() override final
    {
        D3D11_QUERY_DESC queryDesc = {};
        queryDesc.Query            = D3D11_QUERY_EVENT;

        ComPtr<ID3D11Query> pEventQuery;
        ThrowIfFailed(m_device->CreateQuery(&queryDesc, &pEventQuery));

        m_immDC->End(pEventQuery.Get());

        BOOL data;
        while (S_FALSE == m_immDC->GetData(pEventQuery.Get(), &data, sizeof(data), 0))
        {
            ::Sleep(100);
        }

        return true;
    }

    void UpdateSwapChainBuffers()
    {
        m_backBufferIndex = m_swapChain.GetCurrentBackBufferIndex();

        if ((m_frameCounter % m_backBufferCount) != m_backBufferIndex)
            m_frameCounter = m_backBufferIndex;

        ThrowIfFailed(m_swapChain.GetBuffer(0, IID_PPV_ARGS(&m_backBuffer)));
        m_device->CreateRenderTargetView(m_backBuffer.Get(), nullptr, &m_swapChainRtv);
    }

    ID3D11Resource* GetBackBuffer() const
    {
        return m_backBuffer.Get();
    }

    ID3D11RenderTargetView* GetBackBufferRTV() const
    {
        return m_swapChainRtv.Get();
    }

    void GetBackBuffers(RpsResourceDesc&   backBufferDesc,
                        RpsRuntimeResource backBuffers[DXGI_MAX_SWAP_CHAIN_BUFFERS]) const
    {
        backBuffers[0]                   = rpsD3D11ResourceToHandle(m_backBuffer.Get());
        backBufferDesc.type              = RPS_RESOURCE_TYPE_IMAGE_2D;
        backBufferDesc.temporalLayers    = 1;
        backBufferDesc.flags             = 0;
        backBufferDesc.image.arrayLayers = 1;
        backBufferDesc.image.mipLevels   = 1;
        backBufferDesc.image.format      = rpsFormatFromDXGI(m_swapChain.GetFormat());
        backBufferDesc.image.width       = m_width;
        backBufferDesc.image.height      = m_height;
        backBufferDesc.image.sampleCount = 1;
    }

public:
    struct ActiveCommandList
    {
        uint32_t                    backBufferIndex;
        ComPtr<ID3D11DeviceContext> cmdList;

        ID3D11DeviceContext* operator->() const
        {
            return cmdList.Get();
        }
    };

protected:
    virtual void OnInit()
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
    }

    virtual RpsResult CreateRpsRuntimeDevice(const RpsDeviceCreateInfo& createInfo, RpsDevice& device) override
    {
        RpsD3D11RuntimeDeviceCreateInfo runtimeDeviceCreateInfo = {};
        runtimeDeviceCreateInfo.pDeviceCreateInfo               = &createInfo;
        runtimeDeviceCreateInfo.pD3D11Device                    = m_device.Get();

        return rpsD3D11RuntimeDeviceCreate(&runtimeDeviceCreateInfo, &device);
    }

    RpsResult ExecuteRenderGraph(uint32_t frameIndex, RpsRenderGraph hRenderGraph)
    {
        RpsRenderGraphBatchLayout batchLayout = {};

        RpsResult result = rpsRenderGraphGetBatchLayout(hRenderGraph, &batchLayout);
        if (RPS_FAILED(result))
        {
            return result;
        }

        for (uint32_t iBatch = 0; iBatch < batchLayout.numCmdBatches; iBatch++)
        {
            auto& batch = batchLayout.pCmdBatches[iBatch];

            ActiveCommandList cmdList{m_backBufferIndex, m_immDC};

            RpsRenderGraphRecordCommandInfo recordInfo = {};

            recordInfo.pUserContext  = this;
            recordInfo.cmdBeginIndex = batch.cmdBegin;
            recordInfo.numCmds       = batch.numCmds;
            recordInfo.hCmdBuffer    = rpsD3D11DeviceContextToHandle(cmdList.cmdList.Get());

            result = rpsRenderGraphRecordCommands(hRenderGraph, &recordInfo);
            if (RPS_FAILED(result))
                return result;
        }

        return result;
    }

    ActiveCommandList AcquireDeferredContext()
    {
        ActiveCommandList result = {};
        result.backBufferIndex   = m_backBufferIndex;

        std::lock_guard<std::mutex> lock(m_cmdListMutex);

        if (m_deferredContexts.empty())
        {
            ThrowIfFailed(m_device->CreateDeferredContext(0, &result.cmdList));
        }
        else
        {
            result.cmdList = m_deferredContexts.back();
            m_deferredContexts.pop_back();
        }

        return result;
    }

    void RecycleCmdList(ActiveCommandList& cmdList)
    {
        if (cmdList.cmdList == m_immDC)
            return;

        std::lock_guard<std::mutex> lock(m_cmdListMutex);

        m_deferredContexts.push_back(cmdList.cmdList);
        cmdList.cmdList = nullptr;
    }

    struct SwapChain
    {
        HRESULT Create(IDXGIFactory2* pFactory,
                       uint32_t       backBufferCount,
                       uint32_t       width,
                       uint32_t       height,
                       DXGI_FORMAT    backBufferFormat,
                       ID3D11Device*  pDevice,
                       HWND           hWnd)
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

            ComPtr<IDXGISwapChain1> swapChain;

            HRESULT hr = pFactory->CreateSwapChainForHwnd(pDevice, hWnd, &swapChainDesc, nullptr, nullptr, &swapChain);
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

            *pbFullscreen = FALSE;
            *ppOutput     = nullptr;
            return S_OK;
        }

        HRESULT GetDesc(DXGI_SWAP_CHAIN_DESC* pDesc) const
        {
            if (m_swapChain)
                return m_swapChain->GetDesc(pDesc);

            if (m_buffers.empty())
                return E_FAIL;

            D3D11_TEXTURE2D_DESC backBufferDesc;
            m_buffers[0]->GetDesc(&backBufferDesc);

            pDesc->BufferDesc.Width                   = (uint32_t)backBufferDesc.Width;
            pDesc->BufferDesc.Height                  = backBufferDesc.Height;
            pDesc->BufferDesc.RefreshRate.Denominator = 1;
            pDesc->BufferDesc.RefreshRate.Numerator   = 60;
            pDesc->BufferDesc.Format                  = backBufferDesc.Format;
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
            m_buffers.clear();

            if (m_swapChain)
                return m_swapChain->ResizeBuffers(backBufferCount, width, height, backBufferFormat, flags);

            m_buffers.resize(backBufferCount);
            for (uint32_t i = 0; i < backBufferCount; i++)
            {
                auto resDesc = CD3D11_TEXTURE2D_DESC(
                    backBufferFormat,
                    width,
                    height,
                    1,
                    1,
                    D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
                    D3D11_USAGE_DEFAULT,
                    0,
                    1,
                    0,
                    0);

                ThrowIfFailed(m_pDevice->CreateTexture2D(&resDesc, nullptr, &m_buffers[i]));
            }
            m_backBufferIndex = 0;

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

        HWND                                 m_hWnd;
        ComPtr<ID3D11Device>                 m_pDevice;
        ComPtr<IDXGISwapChain3>              m_swapChain;
        std::vector<ComPtr<ID3D11Texture2D>> m_buffers;
        uint32_t                             m_backBufferIndex;
    };

    uint64_t CalcGuaranteedCompletedFrameIndexForRps() const
    {
        // we always wait for swapchain buffer before rendering a new frame, so can guarntee at least
        // this index for gpu complete status
        return (m_frameCounter > m_backBufferCount) ? m_frameCounter - m_backBufferCount
                                                    : RPS_GPU_COMPLETED_FRAME_INDEX_NONE;
    }

    void CreateStaticCheckerboardTexture(ComPtr<ID3D11Resource>& texture,
                                         uint32_t                texWidth,
                                         uint32_t                texHeight,
                                         const float             tintColor[4]) const
    {
        static const UINT TexturePixelSize = 4;

        // Describe and create a Texture2D.
        D3D11_TEXTURE2D_DESC textureDesc = {};

        textureDesc.Width            = texWidth;
        textureDesc.Height           = texHeight;
        textureDesc.MipLevels        = 1;
        textureDesc.ArraySize        = 1;
        textureDesc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.Usage            = D3D11_USAGE_IMMUTABLE;
        textureDesc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
        textureDesc.CPUAccessFlags   = 0;

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

        D3D11_SUBRESOURCE_DATA textureData = {};
        textureData.pSysMem                = &data[0];
        textureData.SysMemPitch            = texWidth * TexturePixelSize;
        textureData.SysMemSlicePitch       = textureData.SysMemPitch * texHeight;

        ComPtr<ID3D11Texture2D> tex2D;
        ThrowIfFailed(m_device->CreateTexture2D(&textureDesc, &textureData, &tex2D));

        texture = tex2D;
    }

protected:
    bool                           m_useWarpDevice   = false;
    bool                           m_bVSync          = g_VSync;
    HWND                           m_hWnd            = NULL;
    UINT                           m_width           = 0;
    UINT                           m_height          = 0;
    uint32_t                       m_backBufferCount = 3;
    ComPtr<ID3D11Device>           m_device;
    SwapChain                      m_swapChain;
    uint32_t                       m_backBufferIndex = 0;
    ComPtr<ID3D11DeviceContext>    m_immDC;
    ComPtr<ID3D11Resource>         m_backBuffer;
    ComPtr<ID3D11RenderTargetView> m_swapChainRtv;

    std::vector<ComPtr<ID3D11DeviceContext>> m_deferredContexts;
    std::mutex                               m_cmdListMutex;

    uint32_t m_frameCounter = 0;
};
