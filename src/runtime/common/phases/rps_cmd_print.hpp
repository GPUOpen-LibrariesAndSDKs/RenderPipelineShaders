// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_CMD_PRINT_HPP
#define RPS_CMD_PRINT_HPP

#include "runtime/common/rps_render_graph.hpp"

namespace rps
{
    class CmdDebugPrintPhase : public IRenderGraphPhase
    {
    public:
        virtual RpsResult Run(RenderGraphUpdateContext& context) override final
        {
            RPS_RETURN_OK_IF(
                !rpsAnyBitsSet(context.pUpdateInfo->diagnosticFlags, RPS_DIAGNOSTIC_ENABLE_PRE_SCHEDULE_DUMP));

            RenderGraph& renderGraph = context.renderGraph;

            const auto cmds     = renderGraph.GetCmdInfos().crange_all();
            const auto resDecls = renderGraph.GetBuilder().GetResourceDecls();

            PrinterRef printer(context.renderGraph.GetDevice().Printer());

            printer("\nResource Declarations:");

            for (uint32_t iRes = 0; iRes < resDecls.size(); iRes++)
            {
                printer("\n%4d : ", iRes);

                PrintResourceDecl(printer, resDecls[iRes]);
            }

            printer("\nCommands:");

            for (uint32_t iCmd = 0; iCmd < cmds.size(); iCmd++)
            {
                printer("\n%4d : ", iCmd);

                PrintCmdInfo(context, printer, iCmd);
            }

            printer("\n");

            return RPS_OK;
        }

        static void PrintResourceDecl(PrinterRef printer, const ResourceDecl& resourceDecl)
        {
            printer("%.*s", resourceDecl.name.len, resourceDecl.name.str);

            const auto* pResDesc = static_cast<const ResourceDesc*>(resourceDecl.desc);

            if (pResDesc)
            {
                const ResourceDescPacked resDesc{*pResDesc};

                printer("\n        ");
                resDesc.Print(printer);
            }
            else
            {
                printer("<unused>");
            }
        }

        static void PrintCmdInfo(RenderGraphUpdateContext& context, PrinterRef printer, NodeId nodeId)
        {
            const auto& renderGraph = context.renderGraph;

            renderGraph.PrintCmdNodeName(printer, nodeId);

            const CmdInfo&      cmdInfo      = *renderGraph.GetCmdInfo(nodeId);
            const NodeDeclInfo& nodeDeclInfo = *cmdInfo.pNodeDecl;
            const auto          cmdAccesses  = renderGraph.GetCmdAccesses(nodeId);

            printer("( ");

            bool bFirstArg = true;
            for (uint32_t iParam = 0, numParams = uint32_t(nodeDeclInfo.params.size()); iParam < numParams; iParam++)
            {
                auto& paramDecl = nodeDeclInfo.params[iParam];

                if (paramDecl.access.accessFlags == RPS_ACCESS_UNKNOWN)
                    continue;

                const auto cmdAccessRange = cmdAccesses.range(paramDecl.accessOffset, paramDecl.numElements);

                printer("%s\n           ", (bFirstArg) ? "" : ",");  // Can't use iParam == 0 since it can be skipped
                bFirstArg = false;

                if (paramDecl.name)
                {
                    printer("%.*s [", paramDecl.name.len, paramDecl.name.str);
                }
                else
                {
                    printer("param_%u [", iParam);
                }

                bool bFirstAccess = true;

                for (auto& access : cmdAccessRange)
                {
                    if (access.resourceId == RPS_RESOURCE_ID_INVALID)
                        continue;

                    printer(bFirstAccess ? "" : ", ");
                    bFirstAccess = false;

                    PrintResourceReference(context, printer, access.resourceId, access.range);

                    printer(" : (");
                    access.access.Print(printer);
                    printer(")");
                }

                printer("]");

                SemanticAttr(paramDecl.semantic, paramDecl.baseSemanticIndex).Print(printer);
            }
            printer(" )");
        }

        static void PrintResourceReference(RenderGraphUpdateContext&     context,
                                           PrinterRef                    printer,
                                           uint32_t                      resourceId,
                                           const SubresourceRangePacked& range)
        {
            const auto& resInstance = context.renderGraph.GetResourceInstance(resourceId);
            const auto  resName = context.renderGraph.GetBuilder().GetResourceDecls()[resInstance.resourceDeclId].name;

            if (resName)
            {
                printer(resName);
            }
            else
            {
                printer("resource(%u)", resInstance.resourceDeclId);
            }

            if (resInstance.isTemporalSlice)
            {
                auto& temporalParentRes = context.renderGraph.GetResourceInstance(resInstance.resourceDeclId);
                RPS_ASSERT(temporalParentRes.IsTemporalParent());
                RPS_ASSERT(
                    (resourceId >= temporalParentRes.temporalLayerOffset) &&
                    (resourceId <= temporalParentRes.temporalLayerOffset + temporalParentRes.desc.temporalLayers));

                printer(", temporal_layer %u", resourceId - temporalParentRes.temporalLayerOffset);
            }

            if (resInstance.numSubResources > 1)
            {
                printer(", ");
                range.Print(printer);
            }
        }
    };

}  // namespace rps

#endif  //RPS_CMD_PRINT_HPP
