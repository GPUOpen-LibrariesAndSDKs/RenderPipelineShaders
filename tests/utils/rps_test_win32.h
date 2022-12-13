// Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the AMD INTERNAL EVALUATION LICENSE.
//
// See file LICENSE.RTF for full license details.

#ifndef _RPS_TEST_WINDOWS_H_
#define _RPS_TEST_WINDOWS_H_

#include "app_framework/afx_win32.h"

typedef RpsAfxRunWindowInfo RpsTestRunWindowInfo;

static inline int RpsTestRunWindowApp(const RpsAfxRunWindowInfo* pRunInfo)
{
    return RpsAfxRunWindowApp(pRunInfo);
}

RpsRpslEntry rpsTestLoadRpslEntryFromDLL(const std::string& moduleName, const std::string& entryName)
{
    HMODULE hDLL = ::LoadLibraryA(moduleName.c_str());

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

#endif  //_RPS_TEST_WINDOWS_H_
