// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>
#include <stdarg.h>
#include <DirectXMath.h>

#include "rps/rps.h"

#include "rps_test_common.h"
#include "app_framework/afx_cmd_parser.hpp"
#include "app_framework/afx_renderer.hpp"

using namespace DirectX;

enum MultiQueueMode
{
    MULTI_QUEUE_DISABLE = 0,
    MULTI_QUEUE_GFX_COMPUTE,
    MULTI_QUEUE_GFX_COMPUTE_COPY,
};

static rps::CmdArg<MultiQueueMode> g_MultiQueueMode{"multi-queue", MULTI_QUEUE_DISABLE, {"mq"}};

class RpsTestHost
{
public:
    RpsTestHost()
    {
    }
    virtual ~RpsTestHost()
    {
    }

protected:
    RpsDevice GetRpsDevice() const
    {
        return m_rpsDevice;
    }

    RpsRenderGraph GetRpsRenderGraph() const
    {
        return m_rpsRenderGraph;
    }

    void OnInit(RpsDevice hDevice, RpsRpslEntry hRpslEntryPoint)
    {
        m_rpsDevice = hDevice ? hDevice : rpsTestUtilCreateDevice();

        RpsRenderGraphCreateInfo renderGraphCreateInfo = {};
        renderGraphCreateInfo.mainEntryCreateInfo.hRpslEntryPoint = hRpslEntryPoint;

        static_assert(RPS_AFX_QUEUE_INDEX_COUNT <= RPS_MAX_QUEUES,
                      "RpsAfxQueueIndices index count must not exceed RPS_MAX_QUEUES.");
        // order of graphics, compute, and copy is written here to be same as RpsAfxQueueIndices
        RpsQueueFlags queueFlags[] = {RPS_QUEUE_FLAG_GRAPHICS, RPS_QUEUE_FLAG_COMPUTE, RPS_QUEUE_FLAG_COPY};
        if (g_MultiQueueMode != MULTI_QUEUE_DISABLE)
        {
            renderGraphCreateInfo.scheduleInfo.numQueues   = (g_MultiQueueMode == MULTI_QUEUE_GFX_COMPUTE) ? 2 : 3;
            renderGraphCreateInfo.scheduleInfo.pQueueInfos = queueFlags;
        }

        RpsResult result = rpsRenderGraphCreate(m_rpsDevice, &renderGraphCreateInfo, &m_rpsRenderGraph);
        REQUIRE(result == RPS_OK);

        BindNodes(rpsRenderGraphGetMainEntry(m_rpsRenderGraph));
    }

    virtual void BindNodes(RpsSubprogram hRpslEntry)
    {
    }

    virtual void OnDestroy()
    {
        rpsRenderGraphDestroy(m_rpsRenderGraph);
        rpsTestUtilDestroyDevice(m_rpsDevice);
    }

    void OnUpdate(uint64_t                         frameIndex,
                  uint64_t                         completedFrameIndex,
                  uint32_t                         numArgs,
                  const RpsConstant*               argData,
                  const RpsRuntimeResource* const* argResources)
    {
        if (m_rpsRenderGraph != RPS_NULL_HANDLE)
        {
            RpsRenderGraphUpdateInfo updateInfo = {};
            updateInfo.frameIndex               = frameIndex;
            updateInfo.gpuCompletedFrameIndex   = completedFrameIndex;
            updateInfo.numArgs                  = numArgs;
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

private:
    RpsDevice      m_rpsDevice      = RPS_NULL_HANDLE;
    RpsRenderGraph m_rpsRenderGraph = RPS_NULL_HANDLE;
};

int main(int argc, char* argv[])
{
    Catch::Session session;

    rps::CLI::Parse(&argc, &argv);

    int returnCode = session.applyCommandLine(argc, argv);
    if (returnCode != 0)  // Indicates a command line error
        return returnCode;

    return session.run();
}
