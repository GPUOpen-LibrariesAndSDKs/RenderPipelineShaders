// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <stdarg.h>

#include "rps/rps.h"

#include "utils/rps_test_common.h"

void* FailingMalloc(void* pContext, size_t size, size_t alignment)
{
    return NULL;
}

// Test device creation APIs
TEST_CASE("DeviceCreation")
{
    RpsDevice           device = RPS_NULL_HANDLE;
    RpsDeviceCreateInfo createInfo;

    // Invalid Input
    memset(&createInfo, 0, sizeof(createInfo));
    RpsResult result = rpsDeviceCreate(&createInfo, NULL);
    REQUIRE(result == RPS_ERROR_INVALID_ARGUMENTS);
    REQUIRE(device == RPS_NULL_HANDLE);

    // Default allocator
    result = rpsDeviceCreate(&createInfo, &device);
    REQUIRE(result == RPS_OK);
    REQUIRE(device != RPS_NULL_HANDLE);
    rpsDeviceDestroy(device);
    device = RPS_NULL_HANDLE;

    // OOM
    createInfo.allocator.pfnAlloc = FailingMalloc;
    createInfo.allocator.pfnFree  = CountedFree;
    createInfo.printer.pfnPrintf  = NULL;
    result                        = rpsDeviceCreate(&createInfo, &device);
    REQUIRE(result == RPS_ERROR_OUT_OF_MEMORY);
    REQUIRE(device == RPS_NULL_HANDLE);
    REQUIRE(g_NumMallocs == 0);

    // Success
    createInfo.allocator.pfnAlloc = CountedMalloc;
    createInfo.allocator.pfnFree  = CountedFree;
    createInfo.printer.pfnPrintf  = NULL;

    result = rpsDeviceCreate(&createInfo, &device);
    REQUIRE(result == RPS_OK);
    REQUIRE(device != RPS_NULL_HANDLE);
    REQUIRE(g_NumMallocs > 0);

    // Clean up
    rpsDeviceDestroy(device);
    REQUIRE(g_NumMallocs == 0);
}
