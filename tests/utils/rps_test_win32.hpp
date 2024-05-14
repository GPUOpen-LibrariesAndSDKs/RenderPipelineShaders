// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#include "app_framework/afx_win32.hpp"

typedef RpsAfxRunWindowInfo RpsTestRunWindowInfo;

static inline int RpsTestRunWindowApp(const RpsAfxRunWindowInfo* pRunInfo)
{
    return RpsAfxRunWindowApp(pRunInfo);
}

RpsRpslEntry rpsTestLoadRpslEntryFromDLL(const std::string& moduleName, const std::string& entryName)
{
    std::string moduleNameWithEnd = moduleName + ".dll";

    HMODULE hDLL = ::LoadLibraryA(moduleNameWithEnd.c_str());

    if (hDLL)
    {
        auto      pfn_dynLibInit = reinterpret_cast<PFN_rpslDynLibInit>(GetProcAddress(hDLL, "___rps_dyn_lib_init"));
        RpsResult result         = rpsRpslDynamicLibraryInit(pfn_dynLibInit);
        if (RPS_FAILED(result))
        {
            return nullptr;
        }

        auto pEntry = reinterpret_cast<RpsRpslEntry*>(GetProcAddress(hDLL, entryName.c_str()));
        if (pEntry)
        {
            return *pEntry;
        }
    }

    return nullptr;
}

void rpsTestPrintDebugString(const char* str)
{
    ::OutputDebugStringA(str);
}

