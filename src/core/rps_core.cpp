// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.


#include "core/rps_core.hpp"
#include <cstdio>

// The context-less global debug printer
static RpsPrinter g_rpsGlobalDebugPrinter;

static RpsDiagLogLevel g_minLogLevel = RPS_DIAG_ERROR;

void rpsSetGlobalDebugPrinter(const RpsPrinter* pPrinter)
{
    g_rpsGlobalDebugPrinter = pPrinter ? *pPrinter : RpsPrinter{};
}

const RpsPrinter* rpsGetGlobalDebugPrinter()
{
    return &g_rpsGlobalDebugPrinter;
}

void rpsSetGlobalDebugPrinterLogLevel(RpsDiagLogLevel minLogLevel)
{
    g_minLogLevel = minLogLevel;
}

void rpsDiagLog(RpsDiagLogLevel logLevel, const char* fmt, ...)
{
    if (logLevel < g_minLogLevel)
        return;

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