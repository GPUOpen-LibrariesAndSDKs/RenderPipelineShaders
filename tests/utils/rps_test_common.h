// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#include "rps/rps.h"

#ifdef __cplusplus

#include <catch2/catch.hpp>
#include <functional>
#include <inttypes.h>
#include <algorithm>

#include "app_framework/afx_common_helpers.hpp"

#define RPS_TEST_COUNTOF(A) (std::extent<decltype(A)>::value)

#if RPS_HAS_MAYBE_UNUSED
#define RPS_TEST_MAYBE_UNUSED [[maybe_unused]]
#else
#define RPS_TEST_MAYBE_UNUSED
#endif

#define RPS_TEST_CONCATENATE_DIRECT(A, B)   A##B
#define RPS_TEST_CONCATENATE_INDIRECT(A, B) RPS_TEST_CONCATENATE_DIRECT(A, B)

#define REQUIRE_RPS_OK(Expr)                                                 \
    do                                                                       \
    {                                                                        \
        RpsResult RPS_TEST_CONCATENATE_INDIRECT(result_, __LINE__) = Expr;   \
        REQUIRE(RPS_TEST_CONCATENATE_INDIRECT(result_, __LINE__) == RPS_OK); \
    } while (false);

#ifdef USE_RPSL_DLL
RpsRpslEntry rpsTestLoadRpslEntryFromDLL(const std::string& moduleName, const std::string& entryName);
#define rpsTestLoadRpslEntry(ModuleName, EntryFunction) \
    rpsTestLoadRpslEntryFromDLL(#ModuleName "_rpsl", RPS_ENTRY_NAME(ModuleName, EntryFunction))
#else
#define rpsTestLoadRpslEntry(ModuleName, EntryFunction) RPS_ENTRY_REF(ModuleName, EntryFunction)
#endif

static int g_NumMallocs = 0;

static void* CountedMalloc(void* pContext, size_t size, size_t alignment)
{
    g_NumMallocs++;
#if defined(_MSC_VER) || defined(__MINGW32__)
    return _aligned_malloc(size, alignment);
#else
    const size_t alignedSize = alignment ? (size + (alignment - 1)) & ~(alignment - 1) : size;
    return aligned_alloc(alignment, alignedSize);
#endif
}

static void CountedFree(void* pContext, void* ptr)
{
    g_NumMallocs--;
#if defined(_MSC_VER) || defined(__MINGW32__)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

RPS_TEST_MAYBE_UNUSED static void* CountedRealloc(
    void* pContext, void* oldBuffer, size_t oldSize, size_t newSize, size_t alignment)
{
    void* pNewBuffer = oldBuffer;

    if (newSize > oldSize)
    {
        pNewBuffer = CountedMalloc(pContext, newSize, alignment);
        if (oldBuffer)
        {
            memcpy(pNewBuffer, oldBuffer, std::min(oldSize, newSize));
            CountedFree(pContext, oldBuffer);
        }
    }

    return pNewBuffer;
}

void rpsTestPrintDebugString(const char *);

static void PrintToStdErr(void* pCtx, const char* formatString, ...)
{
    va_list args;
    va_start(args, formatString);
#if _MSC_VER
    vfprintf_s(stderr, formatString, args);
#else
    vfprintf(stderr, formatString, args);
#endif
    va_end(args);

    fflush(stderr);

    // TODO: Currently rpsTestPrintDebugString only implemented for WIN32.
#if defined(_MSC_VER) && defined(_WIN32)
    char buf[4096];

    va_start(args, formatString);
    vsprintf_s(buf, formatString, args);
    va_end(args);
    rpsTestPrintDebugString(buf);
#endif
}

RPS_TEST_MAYBE_UNUSED static uint32_t rpsTestUtilGetMallocCounter()
{
    return g_NumMallocs;
}

RPS_TEST_MAYBE_UNUSED static RpsDevice rpsTestUtilCreateDevice(
    std::function<std::remove_pointer<PFN_rpsDeviceCreate>::type> createDevice = {})
{
    RpsDevice           device     = RPS_NULL_HANDLE;
    RpsDeviceCreateInfo createInfo = {};

    createInfo.allocator.pfnAlloc = CountedMalloc;
    createInfo.allocator.pfnFree  = CountedFree;
    createInfo.printer.pfnPrintf  = PrintToStdErr;

    RpsResult result = createDevice ? createDevice(&createInfo, &device) : rpsDeviceCreate(&createInfo, &device);
    REQUIRE(result == RPS_OK);
    REQUIRE(device != RPS_NULL_HANDLE);
    REQUIRE(g_NumMallocs > 0);

    return device;
}

RPS_TEST_MAYBE_UNUSED static RpsDevice rpsTestUtilCreateNullRuntimeDevice()
{
    RpsDevice           device     = RPS_NULL_HANDLE;
    RpsDeviceCreateInfo createInfo = {};

    createInfo.allocator.pfnAlloc = CountedMalloc;
    createInfo.allocator.pfnFree  = CountedFree;
    createInfo.printer.pfnPrintf  = PrintToStdErr;

    RpsRuntimeCallbacks callbacks       = {};
    callbacks.pfnBuildRenderGraphPhases = nullptr;

    RpsRuntimeDeviceCreateInfo rtCreate = {};

    RpsNullRuntimeDeviceCreateInfo nullCreateInfo = {};
    nullCreateInfo.pDeviceCreateInfo              = &createInfo;

    RpsResult result = rpsNullRuntimeDeviceCreate(&nullCreateInfo, &device);
    REQUIRE(result == RPS_OK);
    REQUIRE(device != RPS_NULL_HANDLE);
    REQUIRE(g_NumMallocs > 0);

    return device;
}

RPS_TEST_MAYBE_UNUSED static void rpsTestUtilDestroyDevice(RpsDevice device)
{
    rpsDeviceDestroy(device);
    REQUIRE(rpsTestUtilGetMallocCounter() == 0);
}

#define RPS_TEST_MALLOC_CHECKPOINT(Id)                const uint32_t RPS_MALLOC_COUNTER_##Id = rpsTestUtilGetMallocCounter()
#define RPS_TEST_MALLOC_COUNTER_EQUAL(Id1, Id2)       REQUIRE(RPS_MALLOC_COUNTER_##Id1 == RPS_MALLOC_COUNTER_##Id2)
#define RPS_TEST_MALLOC_COUNTER_COMPARE(Id1, Op, Id2) REQUIRE(RPS_MALLOC_COUNTER_##Id1 Op RPS_MALLOC_COUNTER_##Id2)
#define RPS_TEST_MALLOC_COUNTER_EQUAL_CURRENT(Id)     REQUIRE(RPS_MALLOC_COUNTER_##Id == rpsTestUtilGetMallocCounter())

extern "C" {

void REQUIRE_PROXY(RpsBool condition, const char* expr, const char* file, int line)
{
    REQUIRE(condition);
}

void REQUIRE_OK_PROXY(RpsResult result, const char* expr, const char* file, int line)
{
    REQUIRE(result == RPS_OK);
}

}  // extern "C"

// TODO: impl other platforms.
#ifdef _WIN32
#include "rps_test_win32.hpp"
#elif defined(__linux__)
#include "rps_test_linux.hpp"
#endif

#else  //__cplusplus

extern void REQUIRE_PROXY(RpsBool condition, const char* expr, const char* file, int line);
extern void REQUIRE_OK_PROXY(RpsResult result, const char* expr, const char* file, int line);

#define REQUIRE(Cond)        REQUIRE_PROXY(Cond, #Cond, __FILE__, __LINE__)
#define REQUIRE_RPS_OK(Cond) REQUIRE_OK_PROXY(Cond, #Cond " == RPS_OK", __FILE__, __LINE__)

#define RPS_TEST_COUNTOF(A) (sizeof(A) / sizeof(A[0]))

#endif  //__cplusplus
