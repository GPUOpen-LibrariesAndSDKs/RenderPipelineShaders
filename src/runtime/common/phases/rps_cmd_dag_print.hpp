// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_CMD_DAG_PRINT_HPP
#define RPS_CMD_DAG_PRINT_HPP

#include "runtime/common/rps_render_graph.hpp"

namespace rps
{
    class DAGPrintPhase : public IRenderGraphPhase
    {
        struct SubgraphInfo
        {
            uint32_t firstChild;
            uint32_t lastChild;
            uint32_t nextSibling;
        };

    public:
        DAGPrintPhase(RenderGraph& renderGraph)
            : m_renderGraph(renderGraph)
            , m_graph(m_renderGraph.GetGraph())
        {
        }

        virtual RpsResult Run(RenderGraphUpdateContext& context) override final
        {
            RPS_RETURN_OK_IF(!rpsAnyBitsSet(context.pUpdateInfo->diagnosticFlags, RPS_DIAGNOSTIC_ENABLE_DAG_DUMP));

            const auto cmds  = m_renderGraph.GetCmdInfos().range_all();
            const auto nodes = m_graph.GetNodes();

            ArenaCheckPoint arenaCheckpoint{context.scratchArena};

            m_subgraphInfos.reset_keep_capacity(&context.scratchArena);

            // TODO: Expose print functionality separately / allow specify printer
            PrinterRef printer(m_renderGraph.GetDevice().Printer());

            printer("\ndigraph G {\n");
            printer("    graph [ size = \"128,64\" ];\n");
            printer("    edge [ style = bold ];\n");
            printer(
                "    node [ shape = polygon, sides = 4, color = magenta, style = filled, orientation = \"45.0\" ];");

            // Transition m_nodes.
            for (uint32_t i = uint32_t(cmds.size()), iEnd = uint32_t(nodes.size()); i < iEnd; i++)
            {
                const uint32_t iTrans = i - uint32_t(cmds.size());
                if ((iTrans & 0x7) == 0)
                {
                    printer("\n       ");
                }

                printer(" ");
                m_renderGraph.PrintTransitionNodeName(printer, i);
            }

            printer(";\n");

            uint32_t printedNodes = 0;

            constexpr RpsNodeDeclFlags QueueTypeFlags =
                RPS_NODE_DECL_GRAPHICS_BIT | RPS_NODE_DECL_COMPUTE_BIT | RPS_NODE_DECL_COPY_BIT;

            printer("    node [ shape = circle, color = cyan, style = filled ];\n");
            printedNodes +=
                PrintNodes(printer, [=](auto flags) { return (flags & QueueTypeFlags) == RPS_NODE_DECL_GRAPHICS_BIT; });
            printer("    node [ shape = circle, color = orange, style = filled ];\n");
            printedNodes +=
                PrintNodes(printer, [=](auto flags) { return (flags & QueueTypeFlags) == RPS_NODE_DECL_COMPUTE_BIT; });
            printer("    node [ shape = circle, color = lime, style = filled ];\n");
            printedNodes +=
                PrintNodes(printer, [=](auto flags) { return (flags & QueueTypeFlags) == RPS_NODE_DECL_COPY_BIT; });

            printer("    node [ shape = circle, color = gray, style = filled ];\n");
            printedNodes += PrintNodes(printer, [=](auto flags) { return rpsCountBits(flags & QueueTypeFlags) != 1; });

            RPS_ASSERT(printedNodes == cmds.size());

            PrintSubgraphs(printer);

            PrintEdges(printer);

            printer("}\n");

            return RPS_OK;
        }

    private:
        template<typename TFilter>
        uint32_t PrintNodes(PrinterRef printer, TFilter filter)
        {
            const auto cmds = m_renderGraph.GetCmdInfos().range_all();

            uint32_t count = 0;

            // Command m_nodes.
            for (uint32_t i = 0; i < cmds.size(); i++)
            {
                auto& nodeDecl = *m_renderGraph.GetCmdInfo(i)->pNodeDecl;

                if (filter(nodeDecl.flags))
                {
                    count++;

                    printer(" ");
                    m_renderGraph.PrintCmdNodeName(printer, i);

                    if ((count & 0x7) == 0x7)
                    {
                        printer("\n       ");
                    }
                }
            }

            if (count > 0)
            {
                printer(";\n");
            }

            return count;
        }

        void PrintSubgraphs(PrinterRef printer)
        {
            const auto subgraphs = m_graph.GetSubgraphs();

            if (subgraphs.empty())
            {
                return;
            }

            m_subgraphInfos.resize(subgraphs.size());

            for (uint32_t i = 0; i < subgraphs.size(); i++)
            {
                auto& subgraphInfo = m_subgraphInfos[i];

                subgraphInfo.firstChild  = RPS_INDEX_NONE_U32;
                subgraphInfo.lastChild   = RPS_INDEX_NONE_U32;
                subgraphInfo.nextSibling = RPS_INDEX_NONE_U32;

                const Subgraph& subgraph = subgraphs[i];

                if (subgraph.parentSubgraph != RPS_INDEX_NONE_U32)
                {
                    RPS_ASSERT(subgraph.parentSubgraph < i);

                    SubgraphInfo& parentInfo = m_subgraphInfos[subgraph.parentSubgraph];

                    if (parentInfo.firstChild == RPS_INDEX_NONE_U32)
                    {
                        parentInfo.firstChild = parentInfo.lastChild = i;
                    }
                    else
                    {
                        m_subgraphInfos[parentInfo.lastChild].nextSibling = i;
                        parentInfo.lastChild                              = i;
                    }
                }
            }

            for (uint32_t i = 0; i < subgraphs.size(); i++)
            {
                if (subgraphs[i].parentSubgraph != RPS_INDEX_NONE_U32)
                {
                    continue;  // Handled by parent
                }

                PrintSubgraphRecursive(printer, i, 1);
            }
        }

        void PrintSubgraphRecursive(PrinterRef printer, uint32_t sgIdx, uint32_t depth)
        {
            const Subgraph&     subgraph     = m_graph.GetSubgraphs()[sgIdx];
            const SubgraphInfo& subgraphInfo = m_subgraphInfos[sgIdx];
            const auto          transitions  = m_renderGraph.GetTransitions().crange_all();

            printer("%*csubgraph cluster_%d { style=\"rounded%s\"",
                    depth * 4,
                    ' ',
                    sgIdx,
                    subgraph.IsAtomic() ? "" : ",dashed");

            for (uint32_t i = subgraph.beginNode; i <= subgraph.endNode; i++)
            {
                printer(" ");
                PrintNodeName(printer, i);
            }

            // TODO: Don't always iterate all transition m_nodes.
            // Skip transitons[0] since it's reserved for invalid transition.
            RPS_ASSERT(transitions.empty() || (transitions[0].nodeId == RPS_NODEDECL_ID_INVALID));

            for (uint32_t transIdx = 1; transIdx < transitions.size(); transIdx++)
            {
                const Node* pN = m_graph.GetNode(transitions[transIdx].nodeId);
                if (pN->subgraph == sgIdx)
                {
                    printer(" ");
                    m_renderGraph.PrintTransitionNodeName(printer, transitions[transIdx].nodeId);
                }
            }

            RpsBool bNeedNewLine = (subgraphInfo.firstChild != RPS_INDEX_NONE_U32);

            if (bNeedNewLine)
            {
                printer("\n");
            }

            for (uint32_t childIdx = subgraphInfo.firstChild; childIdx != RPS_INDEX_NONE_U32;
                 childIdx          = m_subgraphInfos[childIdx].nextSibling)
            {
                PrintSubgraphRecursive(printer, childIdx, depth + 1);
            }

            if (bNeedNewLine)
            {
                printer("%*c};\n", depth * 4, ' ');
            }
            else
            {
                printer(" };\n");
            }
        }

        void PrintEdges(PrinterRef printer)
        {
            const auto nodes = m_graph.GetNodes();
            const auto edges = m_graph.GetEdges();

            for (uint32_t iNode = 0, numNodes = uint32_t(nodes.size()); iNode < numNodes; iNode++)
            {
                auto inEdges = nodes[iNode].inEdges.GetConstRef(edges);
                for (auto e : inEdges)
                {
                    printer("    ");
                    PrintNodeName(printer, e.src);
                    printer(" -> ");
                    PrintNodeName(printer, e.dst);
                    printer(";\n");
                }
            }
        }

        void PrintNodeName(PrinterRef printer, uint32_t nodeId)
        {
            if (m_graph.GetNode(nodeId)->IsTransition())
            {
                m_renderGraph.PrintTransitionNodeName(printer, nodeId);
            }
            else
            {
                m_renderGraph.PrintCmdNodeName(printer, nodeId);
            }
        }

    private:
        const RenderGraph&        m_renderGraph;
        const Graph&              m_graph;
        ArenaVector<SubgraphInfo> m_subgraphInfos;
    };

}  // namespace rps

#endif  //RPS_CMD_DAG_PRINT_HPP
