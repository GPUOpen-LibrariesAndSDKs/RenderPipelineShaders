// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_DAG_BUILD_HPP
#define RPS_DAG_BUILD_HPP

#include "core/rps_graph.hpp"
#include "runtime/common/rps_cmd_buf.hpp"
#include "runtime/common/rps_render_graph.hpp"

namespace rps
{
    class DAGBuilderPass final : public IRenderGraphPhase
    {
    public:
        virtual RpsResult Run(RenderGraphUpdateContext& context) override final
        {
            auto&      renderGraph = context.renderGraph;
            auto&      graph       = renderGraph.GetGraph();
            const auto cmds        = renderGraph.GetCmdInfos().crange_all();

            ArenaCheckPoint arenaCheckpoint{context.scratchArena};

            ArenaVector<uint32_t> subgraphStack(&context.scratchArena);

            uint32_t numSequentialSubgraphOnStack = 0;
            uint32_t schBarrierount               = 0;

            for (uint32_t iCmd = 0, numCmds = uint32_t(cmds.size()); iCmd < numCmds; ++iCmd)
            {
                auto& cmd = cmds[iCmd];

                NodeId newNodeId = graph.AddNode(iCmd);
                RPS_ASSERT(iCmd == newNodeId);  // TODO - Assuming all cmd nodes are added to graph first of all.

                Node* pNode = graph.GetNode(newNodeId);

                // Handle subgraphs
                // If we are currently in a subgraph:
                if (!subgraphStack.empty())
                {
                    pNode->subgraph = subgraphStack.back();

                    if (numSequentialSubgraphOnStack > 0)
                    {
                        RPS_ASSERT(newNodeId > 0);
                        graph.AddEdge(newNodeId - 1, newNodeId);
                    }
                }

                if (cmd.nodeDeclIndex == RPS_BUILTIN_NODE_SUBGRAPH_BEGIN)
                {
                    const uint32_t parent = subgraphStack.empty() ? RPS_INDEX_NONE_U32 : subgraphStack.back();

                    const SubgraphId newSubgraphId = graph.AddSubgraph(parent, cmd.subgraphFlags, newNodeId);
                    Subgraph*        pSubgraph     = graph.GetSubgraph(newSubgraphId);

                    if (pSubgraph->IsSequential())
                    {
                        numSequentialSubgraphOnStack++;
                    }

                    subgraphStack.push_back(newSubgraphId);
                }
                else if (cmd.nodeDeclIndex == RPS_BUILTIN_NODE_SUBGRAPH_END)
                {
                    RPS_RETURN_ERROR_IF(subgraphStack.empty(), RPS_ERROR_INDEX_OUT_OF_BOUNDS);

                    Subgraph* pSubgraph = graph.GetSubgraph(subgraphStack.back());

                    pSubgraph->endNode = newNodeId;

                    if (pSubgraph->IsSequential())
                    {
                        RPS_ASSERT(numSequentialSubgraphOnStack > 0);
                        numSequentialSubgraphOnStack--;
                    }

                    subgraphStack.pop_back();
                }
                else if (cmd.nodeDeclIndex == RPS_BUILTIN_NODE_SCHEDULER_BARRIER)
                {
                    schBarrierount++;
                }

                pNode->barrierScope = schBarrierount;
            }

            // Apply explicit dependencies
            auto explicitDeps = renderGraph.GetBuilder().GetExplicitDependencies();
            for (auto dep : explicitDeps)
            {
                RPS_ASSERT(dep.before < dep.after);
                graph.AddEdge(dep.before, dep.after);
            }

            return RPS_OK;
        }
    };
}  // namespace rps

#endif  //RPS_DAG_BUILD_HPP