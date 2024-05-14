// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "runtime/common/rps_render_graph.hpp"

#include "runtime/common/rps_render_graph.hpp"

namespace rps
{
    RpsResult NullRuntimeBackend::CreateHeaps(const RenderGraphUpdateContext& context, ArrayRef<HeapInfo> heaps)
    {
        for (HeapInfo& heapInfo : heaps)
        {
            if (heapInfo.hRuntimeHeap == RPS_NULL_HANDLE)
            {
                //Set dummy heap handle
                ++m_heapCounter;
                heapInfo.hRuntimeHeap = rpsNullRuntimeHeapToHandle(reinterpret_cast<void*>(m_heapCounter));
            }
        }

        return RPS_OK;
    }

    RpsResult NullRuntimeBackend::RecordCommands(const RenderGraph&                     renderGraph,
                                                 const RpsRenderGraphRecordCommandInfo& recordInfo) const
    {
        //Fallback cmd recording for null runtime and missing runtimes
        RuntimeCmdCallbackContext cmdCbCtx{this, recordInfo};

        for (uint32_t rtCmdId    = recordInfo.cmdBeginIndex,
                      rtCmdIdEnd = uint32_t(recordInfo.cmdBeginIndex + recordInfo.numCmds);
             rtCmdId < rtCmdIdEnd;
             rtCmdId++)
        {
            const RuntimeCmdInfo& runtimeCmdInfo = renderGraph.GetRuntimeCmdInfos()[rtCmdId];

            if (runtimeCmdInfo.isTransition)
                continue;

            const CmdInfo& cmdInfo = renderGraph.GetCmdInfos()[runtimeCmdInfo.cmdId];
            const Cmd&     cmd     = *cmdInfo.pCmdDecl;

            if (cmd.callback.pfnCallback)
            {
                cmdCbCtx.pCmdCallbackContext = cmd.callback.pUserContext;
                cmdCbCtx.ppArgs              = cmd.args.data();
                cmdCbCtx.numArgs             = uint32_t(cmd.args.size());
                cmdCbCtx.userTag             = cmd.tag;
                cmdCbCtx.pNodeDeclInfo       = cmdInfo.pNodeDecl;
                cmdCbCtx.pCmdInfo            = &cmdInfo;
                cmdCbCtx.pCmd                = &cmd;
                cmdCbCtx.cmdId               = runtimeCmdInfo.cmdId;

                cmd.callback.pfnCallback(&cmdCbCtx);

                RPS_V_RETURN(cmdCbCtx.result);
            }
        }

        return RPS_OK;
    }

    void NullRuntimeBackend::DestroyRuntimeResourceDeferred(ResourceInstance& resource)
    {
    }

}  // namespace rps
