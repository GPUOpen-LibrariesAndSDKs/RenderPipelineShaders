// Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the AMD INTERNAL EVALUATION LICENSE.
//
// See file LICENSE.RTF for full license details.

#include "core/rps_core.hpp"
#include <cstdio>

// The context-less global debug printer
static RpsPrinter g_rpsGlobalDebugPrinter;

void rpsSetGlobalDebugPrinter(const RpsPrinter* pPrinter)
{
    g_rpsGlobalDebugPrinter = pPrinter ? *pPrinter : RpsPrinter{};
}

const RpsPrinter* rpsGetGlobalDebugPrinter()
{
    return &g_rpsGlobalDebugPrinter;
}

void rpsDiagLog(const char* fmt, ...)
{
    va_list vargs;
    va_start(vargs, fmt);

    if (g_rpsGlobalDebugPrinter.pfnVPrintf)
    {
        g_rpsGlobalDebugPrinter.pfnVPrintf(g_rpsGlobalDebugPrinter.pContext, fmt, vargs);
    }
    else
    {
        vfprintf(stderr, fmt, vargs);
    }

    va_end(vargs);
}
