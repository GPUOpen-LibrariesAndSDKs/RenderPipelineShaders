// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <stdarg.h>
#include <direct.h>

#define USE_RPSL_JIT 1

#include "rps/rps.h"
#include "utils/rps_test_common.h"

// TODO: make JIT test crossplatform
#include "utils/rps_test_win32.hpp"

static const char c_RpslCode[] = R"(
node Foo(uint2 ua, float fa[3]);
export void main(uint a, float3 b)
{
    uint2 ua = uint2( a + 1, 42 );
    float fa[3] = { b.z, b.y, b.x };
    Foo(ua, fa);
}
)";

void Foo(const RpsCmdCallbackContext* pContext, const uint32_t ua[2], const float fa[3])
{
    REQUIRE(ua);
    REQUIRE(fa);
    REQUIRE(ua[0] == 3 + 1);
    REQUIRE(fa[0] == 0.0f);
    REQUIRE(fa[1] == 2.718f);
    REQUIRE(fa[2] == 3.142f);
}

TEST_CASE("TestRPSJit")
{
    RpsResult result = RPS_OK;
    RpsDevice device = rpsTestUtilCreateDevice([](const RpsDeviceCreateInfo* pCreateInfo, RpsDevice* phDevice) {
        RpsNullRuntimeDeviceCreateInfo nullDeviceCreateInfo = {};
        nullDeviceCreateInfo.pDeviceCreateInfo              = pCreateInfo;
        return rpsNullRuntimeDeviceCreate(&nullDeviceCreateInfo, phDevice);
    });

    RPS_TEST_MALLOC_CHECKPOINT(PostCreateDevice);

    char workingDir[MAX_PATH];
    REQUIRE(_getcwd(workingDir, _countof(workingDir)) == workingDir);

    // TODO: Make dxcompiler.dll to compile RPSL directly
    // Write to temp file
    std::string tmpRpslPath = std::string(workingDir) + "\\test_rpsl_jit.rpsl";
    REQUIRE(WriteToFile(tmpRpslPath, c_RpslCode, sizeof(c_RpslCode) - 1));

    // Call rps-hlslc, compile string to bitcode
    std::stringstream rpsHlslcCmdLine;
    rpsHlslcCmdLine << "rps_hlslc/rps-hlslc.exe \"" << tmpRpslPath << "\" -od \"" << workingDir
                    << "\" -m test_rpsl_jit -O3 -rps-target-dll -rps-bc -cbe=0";
    REQUIRE(LaunchProcess(rpsHlslcCmdLine.str().c_str()));

    // JIT the bitcode
    do
    {
        const char*      argv[] = {""};
        RpsAfxJITHelper  jit(_countof(argv), argv);

        int32_t jitStartupResult = jit.pfnRpsJITStartup(1, argv);
        REQUIRE(jitStartupResult == 0);

        RpsJITModule hJITModule = jit.LoadBitcode((std::string(workingDir) + "\\test_rpsl_jit.llvm.bc").c_str());
        REQUIRE(hJITModule);

        auto moduleName = jit.GetModuleName(hJITModule);
        REQUIRE(moduleName);
        REQUIRE(moduleName == std::string("test_rpsl_jit"));

        auto entryNameTable = jit.GetEntryNameTable(hJITModule);
        REQUIRE(entryNameTable);
        REQUIRE(entryNameTable[0] == std::string("main"));
        REQUIRE(entryNameTable[1] == nullptr);

        char         nameBuf[256];
        RpsRpslEntry hRpslEntry = jit.GetEntryPoint(
            hJITModule, rpsMakeRpslEntryName(nameBuf, RPS_TEST_COUNTOF(nameBuf), moduleName, entryNameTable[0]));

        REQUIRE(hRpslEntry != nullptr);

        RpsRenderGraphCreateInfo renderGraphCreateInfo            = {};
        renderGraphCreateInfo.scheduleInfo.scheduleFlags          = RPS_SCHEDULE_DISABLE_DEAD_CODE_ELIMINATION_BIT;
        renderGraphCreateInfo.mainEntryCreateInfo.hRpslEntryPoint = hRpslEntry;

        RpsRenderGraph renderGraph = {};
        result                     = rpsRenderGraphCreate(device, &renderGraphCreateInfo, &renderGraph);
        REQUIRE(result == RPS_OK);

        auto hEntryInstance = rpsRenderGraphGetMainEntry(renderGraph);
        rpsProgramBindNode(hEntryInstance, "Foo", &Foo);

        uint32_t a    = 3;
        float    b[3] = {3.142f, 2.718f, 0.0f};

        RpsConstant args[] = {&a, b};

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
        recordInfo.cmdBeginIndex                   = batchLayout.pCmdBatches[0].cmdBegin;
        recordInfo.numCmds                         = batchLayout.pCmdBatches[0].numCmds;
        rpsRenderGraphRecordCommands(renderGraph, &recordInfo);

        rpsRenderGraphDestroy(renderGraph);

        jit.pfnRpsJITUnload(hJITModule);

    } while (false);

    RPS_TEST_MALLOC_COUNTER_EQUAL_CURRENT(PostCreateDevice);

    // Clean up
    rpsTestUtilDestroyDevice(device);
}
