// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "afx_renderer.hpp"

typedef struct RpsAfxRunWindowInfo
{
    LPCTSTR             title;
    LONG                width;
    LONG                height;
    UINT                numFramesToRender;
    RpsAfxRendererBase* pRenderer;
} RpsAfxRunWindowInfo;

LRESULT CALLBACK RpsAfxWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

static inline int RpsAfxRunWindowApp(const RpsAfxRunWindowInfo* pRunInfo)
{
    HWND    hWnd    = NULL;
    HMODULE hModule = NULL;
    ::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                        (LPCTSTR)(&RpsAfxRunWindowApp),
                        &hModule);

    // Initialize the window class.
    WNDCLASSEX windowClass    = {0};
    windowClass.cbSize        = sizeof(WNDCLASSEX);
    windowClass.style         = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc   = RpsAfxWindowProc;
    windowClass.hInstance     = (HINSTANCE)hModule;
    windowClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = TEXT("RPSAfxApp");
    RegisterClassEx(&windowClass);

    RECT windowRect = {0, 0, pRunInfo->width, pRunInfo->height};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    // Create the window and store a handle to it.
    hWnd = CreateWindow(windowClass.lpszClassName,
                        pRunInfo->title,
                        WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT,
                        CW_USEDEFAULT,
                        windowRect.right - windowRect.left,
                        windowRect.bottom - windowRect.top,
                        nullptr,  // We have no parent window.
                        nullptr,  // We aren't using menus.
                        (HINSTANCE)hModule,
                        pRunInfo->pRenderer);

    if (!pRunInfo->pRenderer->Init(hWnd))
    {
        return -1;
    }

    ::ShowWindow(hWnd, SW_SHOW);

    UINT frameCounter = 0;

    // Main sample loop.
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        // Process any messages in the queue.
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            pRunInfo->pRenderer->Tick();

            frameCounter++;
            if ((pRunInfo->numFramesToRender != 0) && (frameCounter >= pRunInfo->numFramesToRender))
            {
                ::PostMessage(hWnd, WM_CLOSE, 0, 0);
            }
        }
    }

    pRunInfo->pRenderer->CleanUp();

    // Return this part of the WM_QUIT message to Windows.
    return (INT)msg.wParam;
}

// Main message handler for the sample.
LRESULT CALLBACK RpsAfxWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    RpsAfxRendererBase* pRenderer = reinterpret_cast<RpsAfxRendererBase*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    if (pRenderer)
    {
        bool    bHandled = false;
        LRESULT result   = pRenderer->WindowProc(hWnd, message, wParam, lParam, bHandled);

        if (bHandled)
        {
            return result;
        }
    }

    switch (message)
    {
    case WM_CREATE:
    {
        // Save the DXSample* passed in to CreateWindow.
        LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
    }
        return 0;

    case WM_KEYDOWN:
        if (pRenderer)
        {
            pRenderer->OnKeyDown((UINT8)(wParam));
        }
        return 0;

    case WM_KEYUP:
        if (pRenderer)
        {
            pRenderer->OnKeyUp((UINT8)(wParam));
        }
        return 0;

    case WM_WINDOWPOSCHANGED:
        if (pRenderer)
        {
            RECT rect = {};
            if (::GetClientRect(hWnd, &rect))
            {
                pRenderer->OnResize(rect.right - rect.left, rect.bottom - rect.top);
            }
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    // Handle any messages the switch statement didn't.
    return DefWindowProc(hWnd, message, wParam, lParam);
}

bool LaunchProcess(char* commandLine)
{
    // create a pipe to get possible errors from the process
    //
    HANDLE hChildStdOutRead  = NULL;
    HANDLE hChildStdOutWrite = NULL;

    SECURITY_ATTRIBUTES saAttr = {};
    saAttr.nLength             = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle      = TRUE;
    if (!CreatePipe(&hChildStdOutRead, &hChildStdOutWrite, &saAttr, 0))
        return false;

    // launch process
    //
    PROCESS_INFORMATION pi = {};
    STARTUPINFOA        si = {};
    si.cb                  = sizeof(si);
    si.dwFlags             = STARTF_USESTDHANDLES;
    si.hStdError           = hChildStdOutWrite;
    si.hStdOutput          = hChildStdOutWrite;
    si.wShowWindow         = SW_HIDE;

    if (CreateProcessA(NULL, commandLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(hChildStdOutWrite);

        ULONG exitCode;
        if (GetExitCodeProcess(pi.hProcess, &exitCode))
        {
            HANDLE hParentStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD  dwRead, dwWritten;
            char   chBuf[4096];

            for (;;)
            {
                BOOL bSuccess = ReadFile(hChildStdOutRead, chBuf, _countof(chBuf), &dwRead, NULL);

                if (!bSuccess || dwRead == 0)
                    break;

                bSuccess = WriteFile(hParentStdOut, chBuf, dwRead, &dwWritten, NULL);
                if (!bSuccess)
                    break;
            }

            if (exitCode == 0)
            {
                return true;
            }
        }

        CloseHandle(hChildStdOutRead);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    return false;
}

bool LaunchProcess(const char* commandLine)
{
    std::string mutableCmdLine = commandLine;
    return LaunchProcess(&mutableCmdLine[0]);
}