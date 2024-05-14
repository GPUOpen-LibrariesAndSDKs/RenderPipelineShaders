// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#include "rps/rps.h"

#include <dlfcn.h>

RpsRpslEntry rpsTestLoadRpslEntryFromDLL(const std::string& moduleName, const std::string& entryName)
{
    std::string moduleNameAsLib = std::string("lib") + moduleName + ".so";

    int   flags = RTLD_NOW;
    void* dll   = dlopen(moduleNameAsLib.c_str(), flags);

    if (dll)
    {
        auto      pfn_dynLibInit = reinterpret_cast<PFN_rpslDynLibInit>(dlsym(dll, "___rps_dyn_lib_init"));
        RpsResult result         = rpsRpslDynamicLibraryInit(pfn_dynLibInit);
        if (RPS_FAILED(result))
        {
            return nullptr;
        }

        auto pEntry = reinterpret_cast<RpsRpslEntry*>(dlsym(dll, entryName.c_str()));
        if (pEntry)
        {
            return *pEntry;
        }
    }

    return nullptr;
}
