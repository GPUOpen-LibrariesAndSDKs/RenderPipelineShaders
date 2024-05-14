// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#include <cstdint>

#include "afx_cmd_parser.hpp"

#include "rps/rps.h"

enum
{
    RPS_AFX_DEBUG_MSG_SEVERITY_NONE       = 0,
    RPS_AFX_DEBUG_MSG_SEVERITY_CORRUPTION = 1 << 0,
    RPS_AFX_DEBUG_MSG_SEVERITY_ERROR      = 1 << 1,
    RPS_AFX_DEBUG_MSG_SEVERITY_WARNING    = 1 << 2,
    RPS_AFX_DEBUG_MSG_SEVERITY_INFO       = 1 << 3,
};

constexpr bool DEBUG_DEVICE_DEFAULT =
#ifdef _DEBUG
    true;
#else
    false;
#endif

static rps::CmdArg<bool>     g_DebugDevice{"debug-device", DEBUG_DEVICE_DEFAULT, {"sdk-layer"}, false};
static rps::CmdArg<uint32_t> g_DebugDeviceBreakLevel{
    "debug-device-break", RPS_AFX_DEBUG_MSG_SEVERITY_ERROR | RPS_AFX_DEBUG_MSG_SEVERITY_CORRUPTION};
static rps::CmdArg<bool>     g_VSync{"vsync", false, {"vsync"}};
static rps::CmdArg<bool>     g_DebugMarkers{"debug-markers", true, {"markers"}};
static rps::CmdArg<uint32_t> g_exitAfterFrame{"exit-after-frame", 300};

enum RpsAfxQueueIndices
{
    RPS_AFX_QUEUE_INDEX_GFX,
    RPS_AFX_QUEUE_INDEX_COMPUTE,
    RPS_AFX_QUEUE_INDEX_COPY,
    RPS_AFX_QUEUE_INDEX_COUNT,
};

class RpsAfxRendererBase
{
private:
    RpsAfxRendererBase(const RpsAfxRendererBase&) = delete;
    RpsAfxRendererBase(RpsAfxRendererBase&&)      = delete;

public:
    RpsAfxRendererBase() = default;
    virtual ~RpsAfxRendererBase()
    {
    }
    virtual bool Init(void* window)
    {
        return true;
    }
    virtual void Tick()
    {
    }
    virtual void CleanUp()
    {
    }
    virtual void OnResize(uint32_t width, uint32_t height)
    {
    }
    virtual void OnKeyUp(char key)
    {
    }
    virtual void OnKeyDown(char key)
    {
    }
    virtual LRESULT WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool& bHandled)
    {
        bHandled = false;
        return 0;
    }
    virtual RpsResult CreateRpsRuntimeDevice(const RpsDeviceCreateInfo& createInfo, RpsDevice& device)
    {
        return RPS_ERROR_NOT_IMPLEMENTED;
    }
    virtual bool WaitForGpuIdle()
    {
        return false;
    }
};