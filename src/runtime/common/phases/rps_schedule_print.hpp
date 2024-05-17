// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_SCHEDULE_PRINT_HPP
#define RPS_SCHEDULE_PRINT_HPP

#include "rps/runtime/common/rps_runtime.h"

#include "runtime/common/rps_render_graph.hpp"
#include "runtime/common/phases/rps_cmd_print.hpp"

namespace rps
{
    class ScheduleDebugPrintPhase : public IRenderGraphPhase
    {
    public:
        virtual RpsResult Run(RenderGraphUpdateContext& context) override final
        {
            RPS_RETURN_OK_IF(
                !rpsAnyBitsSet(context.pUpdateInfo->diagnosticFlags, RPS_DIAGNOSTIC_ENABLE_POST_SCHEDULE_DUMP));

            RenderGraph& renderGraph = context.renderGraph;

            auto runtimeCmds = renderGraph.GetRuntimeCmdInfos().crange_all();
            auto cmdBatches  = renderGraph.GetCmdBatches().crange_all();
            auto cmds        = renderGraph.GetCmdInfos().crange_all();
            auto resInfos    = renderGraph.GetResourceInstances().crange_all();
            auto resDecls    = renderGraph.GetBuilder().GetResourceDecls();

            PrinterRef printer(context.renderGraph.GetDevice().Printer());

            printer("\nScheduled resources:");

            for (uint32_t iRes = 0; iRes < resInfos.size(); iRes++)
            {
                const auto& resInfo = resInfos[iRes];

                printer("\n  %u : [%u]", iRes, resInfo.resourceDeclId);

                if (resInfo.resourceDeclId < resDecls.size())
                {
                    printer(" '");
                    resDecls[resInfo.resourceDeclId].name.Print(printer);
                    printer("'");

                    if (resInfo.isAliased)
                    {
                        printer(", aliased");
                    }

                    printer("\n    accesses : ");
                    resInfo.allAccesses.Print(printer);

                    printer("\n    lifetime : [%u - %u]", resInfo.lifetimeBegin, resInfo.lifetimeEnd);
                }
                else
                {
                    RPS_ASSERT(!resInfo.IsActive());
                    printer(" (inactive)");
                }
            }

            printer("\nSchedule:");

            for (uint32_t iBatch = 0; iBatch < cmdBatches.size(); iBatch++)
            {
                const RpsCommandBatch& batchInfo = cmdBatches[iBatch];

                printer("\n  Batch %u Queue %u:", iBatch, batchInfo.queueIndex);

                if (batchInfo.signalFenceIndex != RPS_INDEX_NONE_U32)
                {
                    printer("\n    Signal : %u", batchInfo.signalFenceIndex);
                }

                if (batchInfo.numWaitFences > 0)
                {
                    printer("\n    Wait : [ ");

                    auto waitFenceIds = renderGraph.GetCmdBatchWaitFenceIds().crange_all();

                    for (uint32_t i = 0; i < batchInfo.numWaitFences; i++)
                    {
                        printer("%s%u", (i == 0) ? "" : ", ", waitFenceIds[batchInfo.waitFencesBegin + i]);
                    }
                    printer(" ]");
                }

                for (uint32_t iCmd = batchInfo.cmdBegin, cmdEnd = batchInfo.cmdBegin + batchInfo.numCmds;
                     iCmd < cmdEnd;
                     iCmd++)
                {
                    const RuntimeCmdInfo& runtimeCmd = runtimeCmds[iCmd];

                    printer("\n    %4d : ", iCmd);

                    if (runtimeCmd.isTransition)
                    {
                        // TODO: Move transitions out
                        PrintTransitionInfo(context, printer, runtimeCmd.GetTransitionId());
                    }
                    else
                    {
                        CmdDebugPrintPhase::PrintCmdInfo(context, printer, runtimeCmd.GetCmdId());
                    }
                }
            }

            printer("\n");

            return RPS_OK;
        }

    private:
        void PrintTransitionInfo(RenderGraphUpdateContext& context, PrinterRef printer, uint32_t transitionId) const
        {
            const auto& renderGraph = context.renderGraph;

            if (PrintBuiltInCmdMarker(context, printer, transitionId))
            {
                return;
            }

            const auto& transInfo = renderGraph.GetTransitionInfo(transitionId);

            renderGraph.PrintTransitionNodeName(printer, transInfo.nodeId);

            printer(" <");
            CmdDebugPrintPhase::PrintResourceReference(context, printer, transInfo.access.resourceId, transInfo.access.range);
            printer("> : ");

            const auto prevTrans =
                RenderGraph::CalcPreviousAccess(transInfo.prevTransition,
                                                renderGraph.GetTransitions().crange_all(),
                                                renderGraph.GetResourceInstance(transInfo.access.resourceId));

            printer("(");
            if (transInfo.prevTransition == RenderGraph::INVALID_TRANSITION)
            {
                printer("*"); // Denotes previous final access.
            }
            prevTrans.Print(printer);
            printer(")");

            printer(" => (");
            transInfo.access.access.Print(printer);
            printer(")");
        }

        bool PrintBuiltInCmdMarker(RenderGraphUpdateContext& context, PrinterRef printer, uint32_t transitionId) const
        {
            switch (transitionId)
            {
            case CMD_ID_PREAMBLE:
                printer("<preamble>");
                break;
            case CMD_ID_POSTAMBLE:
                printer("<postamble>");
                break;
            default:
                return false;
            }
            return true;
        }
    };
}  // namespace rps

#endif  //RPS_SCHEDULE_PRINT_HPP
