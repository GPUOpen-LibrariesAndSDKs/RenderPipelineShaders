// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <stdarg.h>
#include <DirectXMath.h>

#include "rps/rps.h"

#include "utils/rps_test_common.h"

RPS_DECLARE_RPSL_ENTRY(test_dynamic_graph, dynamic_graph);

#define TEST_APP_NAME_RAW "TestDynamicGraph"

using namespace DirectX;

class TestRpsDynamicGraph
{
protected:
    RpsRenderGraph GetRpsRenderGraph() const
    {
        return m_rpsRenderGraph;
    }

    void OnInit()
    {
        CreateRpsDevice(m_rpsDevice);
        LoadRpsPipeline();
    }

    virtual void UpdateRpsPipeline(uint64_t               frameIndex,
                                   uint64_t               completedFrameIndex,
                                   const RpsResourceDesc& backBufferDesc,
                                   RpsRuntimeResource*    pBackBuffers)
    {
        if (m_rpsRenderGraph != RPS_NULL_HANDLE)
        {
            uint32_t frameIndex32 = uint32_t(frameIndex);

            const RpsRuntimeResource* argResources[] = {pBackBuffers};
            RpsConstant               argData[]      = {&backBufferDesc, &frameIndex32};

            RpsRenderGraphUpdateInfo updateInfo = {};
            updateInfo.frameIndex               = frameIndex;
            updateInfo.gpuCompletedFrameIndex   = completedFrameIndex;
            updateInfo.numArgs                  = uint32_t(RPS_TEST_COUNTOF(argData));
            updateInfo.ppArgs                   = argData;
            updateInfo.ppArgResources           = argResources;

            updateInfo.diagnosticFlags = RPS_DIAGNOSTIC_ENABLE_RUNTIME_DEBUG_NAMES;
            if (completedFrameIndex == RPS_GPU_COMPLETED_FRAME_INDEX_NONE)
            {
                updateInfo.diagnosticFlags = RPS_DIAGNOSTIC_ENABLE_ALL;
            }

            REQUIRE_RPS_OK(rpsRenderGraphUpdate(m_rpsRenderGraph, &updateInfo));
        }
    }

    void OnCleanUp()
    {
        rpsRenderGraphDestroy(m_rpsRenderGraph);
        rpsTestUtilDestroyDevice(m_rpsDevice);
    }

    void OnUpdate(uint32_t frameIndex, uint32_t width, uint32_t height)
    {
    }

    void OnRender(uint64_t frameIndex, RpsRuntimeCommandBuffer cmdBuf, uint32_t cmdBegin, uint32_t cmdCount)
    {
        RpsRenderGraphRecordCommandInfo recordInfo = {};

        recordInfo.hCmdBuffer    = cmdBuf;
        recordInfo.pUserContext  = this;
        recordInfo.frameIndex    = frameIndex;
        recordInfo.cmdBeginIndex = cmdBegin;
        recordInfo.numCmds       = cmdCount;

        RpsResult result = rpsRenderGraphRecordCommands(m_rpsRenderGraph, &recordInfo);
        REQUIRE(result == RPS_OK);
    }

protected:
    virtual void CreateRpsDevice(RpsDevice& rpsDeviceOut) = 0;
    virtual void BindNodes(RpsSubprogram hRpslEntry) = 0;

private:
    void LoadRpsPipeline()
    {
        RpsRenderGraphCreateInfo renderGraphCreateInfo = {};
        renderGraphCreateInfo.mainEntryCreateInfo.hRpslEntryPoint =
            rpsTestLoadRpslEntry(test_dynamic_graph, dynamic_graph);
        renderGraphCreateInfo.scheduleInfo.scheduleFlags =
            RPS_SCHEDULE_KEEP_PROGRAM_ORDER_BIT | RPS_SCHEDULE_DISABLE_DEAD_CODE_ELIMINATION_BIT;

        RpsResult result = rpsRenderGraphCreate(m_rpsDevice, &renderGraphCreateInfo, &m_rpsRenderGraph);
        REQUIRE(result == RPS_OK);

        auto hRpslEntry = rpsRenderGraphGetMainEntry(m_rpsRenderGraph);
        BindNodes(hRpslEntry);
    }

private:
    RpsDevice      m_rpsDevice      = RPS_NULL_HANDLE;
    RpsRenderGraph m_rpsRenderGraph = {};
};
