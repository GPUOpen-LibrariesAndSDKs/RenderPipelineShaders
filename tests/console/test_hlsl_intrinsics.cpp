// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <stdarg.h>
#include <math.h>

#include "rps/rps.h"
#include "utils/rps_test_common.h"

#include "core/rps_util.hpp"

// TODO: Reuse code:

struct float4
{
    float x, y, z, w;
};

struct int2
{
    int32_t x, y;
};

struct uint3
{
    uint32_t x, y, z;
};

uint32_t LzcntS32(int32_t a)
{
    if (a >= 0)
        return rpsFirstBitHigh(uint32_t(a));
    else
        return rpsFirstBitHigh(~(uint32_t)a);
}

void EvalOnCpu(uint32_t ia[10], float fa[16], int32_t i1, uint32_t u1, const int2& i2, const uint3& u3, float f1, const float4& f4)
{
    ia[0] = (uint32_t)abs(i1);

    ia[1] = (i1 >> 3) & 0xff;

    ia[2] = *(const uint32_t*)(&f4.w);

    ia[3] = u3.x * u3.y + u3.z;

    ia[4] = rpsCountBits(u3.y);

    ia[5] = u3.x / 17;
    ia[6] = u3.x % 17;

    uint64_t xx = (uint64_t(u1) << 32) | u3.x;
    xx          = xx / ((uint64_t)u3.y + 1);

    ia[7] = uint32_t(xx & 0xffffffff);

    ia[8] = rpsFirstBitHigh(u3.y);

    ia[9] = LzcntS32(-(int)u3.y - 4096);

    fa[0] = atan2(f4.x + f1, f4.y);

    fa[1] = *(const float*)(&u1);

    fa[2] = (((f4.x > 0) && (f4.y > 0) && (f4.z > 0) && (f4.w > 0)) && ((u3.x < 1000) || (u3.y < 1000) || (u3.z < 1000))) ? float(u1) : float(i1);

    fa[3] = i1 ? f4.x : f4.y;

    fa[4] = fmaxf(f4.x, f4.y);

    fa[5] = (f4.y - f4.x) * f4.z + f4.x;

    fa[6] = floor(f4.x);
    fa[7] = ceil(f4.y);
    fa[8] = round(f4.z * 0.5f) * 2.0f;
    fa[9] = trunc(f4.w);

    fa[10] = (isinf(f4.x) || isnan(f4.y) || !isfinite(f4.z)) ? 1.0f : 2.0f;

    fa[11] = f4.y * f4.z + f4.x;

    fa[12] = cosf(f4.x);

    fa[13] = 1.0f / sqrtf(fabs(f4.y) + 0.0001f);

    fa[14] = (f4.z < 0 ? 0 : (f4.z > 1 ? 1 : f4.z));

    fa[15] = exp2f(fmin(f4.z, 1.0f) * float(1.442695e+00));
}

RPS_DECLARE_RPSL_ENTRY(test_hlsl_intrinsics, rps_main);

struct RpslCalculationResults
{
    const uint32_t* iaRps;
    const float*    faRps;
} rpslResults;

void FooCb(const RpsCmdCallbackContext* pContext)
{
    RpslCalculationResults* pResults = static_cast<RpslCalculationResults*>(pContext->pUserRecordContext);

    pResults->iaRps = static_cast<const uint32_t*>(pContext->ppArgs[0]);
    pResults->faRps = static_cast<const float*>(pContext->ppArgs[1]);
}

TEST_CASE("TestHLSLIntrinsics")
{
    RpsDevice device = rpsTestUtilCreateDevice([](auto pCreateInfo, auto phDevice) {
        RpsNullRuntimeDeviceCreateInfo nullDeviceCreateInfo = {};
        nullDeviceCreateInfo.pDeviceCreateInfo              = pCreateInfo;
        return rpsNullRuntimeDeviceCreate(&nullDeviceCreateInfo, phDevice);
    });

    RPS_TEST_MALLOC_CHECKPOINT(PostCreateDevice);

    RpsRenderGraphCreateInfo renderGraphCreateInfo = {};
    renderGraphCreateInfo.scheduleInfo.scheduleFlags          = RPS_SCHEDULE_DISABLE_DEAD_CODE_ELIMINATION_BIT;
    renderGraphCreateInfo.mainEntryCreateInfo.hRpslEntryPoint = rpsTestLoadRpslEntry(test_hlsl_intrinsics, rps_main);

    RpsRenderGraph renderGraph = {};
    RpsResult      result      = rpsRenderGraphCreate(device, &renderGraphCreateInfo, &renderGraph);
    REQUIRE(result == RPS_OK);

    auto hEntryInstance = rpsRenderGraphGetMainEntry(renderGraph);
    rpsProgramBindNode(hEntryInstance, "Foo", &FooCb, nullptr);

    // void rps_main(int i1, uint u1, int2 i2, uint3 u3, float f1, float4 f4)

    srand(uint32_t(time(NULL)));

    for (uint32_t r = 0; r < 100; r++)
    {
        int32_t  i1 = rand();
        uint32_t u1 = uint32_t(rand());
        int2     i2 = {rand(), rand()};
        uint3    u3 = {uint32_t(rand()), uint32_t(rand()), uint32_t(rand())};
        float    f1 = rand() / float(RAND_MAX) + rand();
        float4   f4 = {rand() / float(RAND_MAX) + rand(),
                     rand() / float(RAND_MAX) + rand(),
                     rand() / float(RAND_MAX) + rand(),
                     rand() / float(RAND_MAX) + rand()};

        RpsConstant args[] = {&i1, &u1, &i2, &u3, &f1, &f4};

        RpsRenderGraphUpdateInfo updateInfo = {};
        updateInfo.frameIndex               = 0;
        updateInfo.gpuCompletedFrameIndex   = RPS_GPU_COMPLETED_FRAME_INDEX_NONE;
        updateInfo.numArgs                  = uint32_t(RPS_TEST_COUNTOF(args));
        updateInfo.ppArgs                   = args;
        updateInfo.ppArgResources           = nullptr;
        updateInfo.diagnosticFlags          = RPS_DIAGNOSTIC_ENABLE_ALL;

        REQUIRE_RPS_OK(rpsRenderGraphUpdate(renderGraph, &updateInfo));
        REQUIRE(result == RPS_OK);

        RpsRenderGraphBatchLayout batchLayout = {};
        REQUIRE_RPS_OK(rpsRenderGraphGetBatchLayout(renderGraph, &batchLayout));
        REQUIRE(batchLayout.numCmdBatches == 1);

        RpsRenderGraphRecordCommandInfo recordInfo = {};
        recordInfo.frameIndex                      = 0;
        recordInfo.pUserContext                    = &rpslResults;
        recordInfo.cmdBeginIndex                   = batchLayout.pCmdBatches[0].cmdBegin;
        recordInfo.numCmds                         = batchLayout.pCmdBatches[0].numCmds;
        REQUIRE_RPS_OK(rpsRenderGraphRecordCommands(renderGraph, &recordInfo));

        uint32_t ia[10];
        float    fa[16];

        EvalOnCpu(ia, fa, i1, u1, i2, u3, f1, f4);

        printf("\n");

        REQUIRE(rpslResults.iaRps != nullptr);
        REQUIRE(rpslResults.faRps != nullptr);

        for (uint32_t i = 0; i < RPS_TEST_COUNTOF(ia); i++)
        {
            REQUIRE(ia[i] == rpslResults.iaRps[i]);
        }

        for (uint32_t i = 0; i < RPS_TEST_COUNTOF(fa); i++)
        {
            printf("%25.10f : %25.10f\n", fa[i], rpslResults.faRps[i]);

            // Atan2 impl is slightly different between DXIL and emulation.
            const float errorTolerance = (i < 1) ? 1E-5f : FLT_EPSILON;

            REQUIRE(((isinf(fa[i]) && isinf(rpslResults.faRps[i])) ||
                     (isnan(fa[i]) && isnan(rpslResults.faRps[i])) ||
                     (fabs(fa[i] - rpslResults.faRps[i]) < errorTolerance)));
        }
    }

    rpsRenderGraphDestroy(renderGraph);

    RPS_TEST_MALLOC_COUNTER_EQUAL_CURRENT(PostCreateDevice);

    // Clean up
    rpsTestUtilDestroyDevice(device);
}
