// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_DAG_SCHEDULE_HPP
#define RPS_DAG_SCHEDULE_HPP

#include "rps/runtime/common/rps_runtime.h"

#include "runtime/common/rps_render_graph.hpp"
#include "runtime/common/phases/rps_access_dag_build.hpp"

#include <algorithm>

namespace rps
{
    class DAGSchedulePass : public IRenderGraphPhase
    {
        struct ScheduleFlags
        {
            bool bUseAsync;
            bool bEliminateDeadNodes;
            bool bUseSplitBarrier;
            bool bPreferMemorySaving;
            bool bMinimizeGfxCompSwitch;
            bool bWorkloadTypePipeliningDisabled;
            bool bWorkloadTypePipeliningAggressive;
            bool bForceProgramOrder;
            bool bRandomOrder;

            explicit ScheduleFlags(uint32_t numQueues = 1, RpsScheduleFlags flags = RPS_SCHEDULE_DEFAULT)
            {
                bUseAsync           = !!(numQueues > 1);
                bEliminateDeadNodes = !(flags & RPS_SCHEDULE_DISABLE_DEAD_CODE_ELIMINATION_BIT);
                bUseSplitBarrier    = !!(flags & RPS_SCHEDULE_ALLOW_SPLIT_BARRIERS_BIT);
                bPreferMemorySaving = !!(flags & RPS_SCHEDULE_PREFER_MEMORY_SAVING_BIT);

                bWorkloadTypePipeliningDisabled   = !!(flags & RPS_SCHEDULE_WORKLOAD_TYPE_PIPELINING_DISABLE_BIT);
                bWorkloadTypePipeliningAggressive = !bWorkloadTypePipeliningDisabled &&
                                                    !!(flags & RPS_SCHEDULE_WORKLOAD_TYPE_PIPELINING_AGGRESSIVE_BIT);

                bMinimizeGfxCompSwitch = !!(flags & RPS_SCHEDULE_MINIMIZE_COMPUTE_GFX_SWITCH_BIT);
                bForceProgramOrder     = !!(flags & RPS_SCHEDULE_KEEP_PROGRAM_ORDER_BIT);
                bRandomOrder           = !bForceProgramOrder && !!(flags & RPS_SCHEDULE_RANDOM_ORDER_BIT);
            }
        };

        struct TargetInfo
        {
            const RpsQueueFlags* pQueueFlags;
            uint32_t             numQueues;
            ScheduleFlags        options;
        };

        struct ReadyNodeInfo
        {
            uint32_t nodeId;
            uint32_t depNodeId;
            uint32_t schBarrierScopeIdx;
            float    timeReady;
        };

        struct NodeSchedulingInfo
        {
            Span<uint32_t>   resourceRefs;
            Span<uint32_t>   accessFootprints;
            uint32_t         validQueueMask : 8;
            uint32_t         preferredQueueMask : 8;
            RpsNodeDeclFlags workloadTypeMask : 8;
            uint32_t         canBeEliminated : 1;
        };

        struct ResourceSchedulingInfo
        {
            uint64_t       aliasableSize;
            uint32_t       totalUserNodesCount;
            uint32_t       scheduledUserNodesCount;
            uint32_t       mostRecentRefNodeId;
            Span<uint32_t> subResourceInfoRange;
        };

        struct SubgraphSchedulingInfo
        {
            uint32_t atomicParentId;
            uint32_t atomicSubgraphId;
        };

        struct NodeQueueInfo
        {
            uint32_t queueIndex : 4;
            uint32_t batchIndex : 28;
            uint32_t scheduledIndex;
        };

        struct BatchInfo
        {
            uint32_t       queueIndex : 8;
            uint32_t       bSignal : 1;
            uint32_t       bHasCmdNode : 1;
            uint32_t       runtimeCmdBegin;
            uint32_t       runtimeCmdEnd;
            Span<uint32_t> waitForBatchIndices;

            BatchInfo(uint32_t queueIndex = RPS_MAX_QUEUES,
                      uint32_t cmdBegin   = RPS_INDEX_NONE_U32,
                      uint32_t cmdEnd     = RPS_INDEX_NONE_U32)
                : queueIndex(queueIndex)
                , bSignal(false)
                , bHasCmdNode(false)
                , runtimeCmdBegin(cmdBegin)
                , runtimeCmdEnd(cmdEnd)
            {
            }
        };

    public:
        DAGSchedulePass(RenderGraph& renderGraph)
            : m_renderGraph(renderGraph)
            , graph(m_renderGraph.GetGraph())
        {
            m_targetInfo.numQueues   = m_renderGraph.GetCreateInfo().scheduleInfo.numQueues;
            m_targetInfo.pQueueFlags = m_renderGraph.GetCreateInfo().scheduleInfo.pQueueInfos;

            if (m_targetInfo.numQueues == 0)
            {
                static const RpsQueueFlags defaultQueueInfos[] = {RPS_QUEUE_FLAG_GRAPHICS};

                m_targetInfo.numQueues   = 1;
                m_targetInfo.pQueueFlags = defaultQueueInfos;
            }
        }

        virtual RpsResult Run(RenderGraphUpdateContext& context) override final
        {
            RPS_RETURN_OK_IF(context.renderGraph.GetCmdInfos().empty());

            nodes     = graph.GetNodes();
            subgraphs = graph.GetSubgraphs();
            resources = m_renderGraph.GetResourceInstances().range_all();
            cmdInfos  = m_renderGraph.GetCmdInfos().range_all();

            const RpsRenderGraphCreateInfo& renderGraphCreateInfo = context.renderGraph.GetCreateInfo();

            m_targetInfo.options = ScheduleFlags(renderGraphCreateInfo.scheduleInfo.numQueues,
                                                 (context.pUpdateInfo->scheduleFlags != RPS_SCHEDULE_UNSPECIFIED)
                                                     ? context.pUpdateInfo->scheduleFlags
                                                     : renderGraphCreateInfo.scheduleInfo.scheduleFlags);

            const ScheduleFlags flags = m_targetInfo.options;

            RPS_CHECK_ARGS(flags.bRandomOrder ? (context.pUpdateInfo->pRandomNumberGenerator != nullptr) : true);

            const RpsRandomNumberGenerator randGen =
                flags.bRandomOrder ? *context.pUpdateInfo->pRandomNumberGenerator : RpsRandomNumberGenerator{};

            Arena&          scratchArena = context.scratchArena;
            ArenaCheckPoint arenaCheckpoint{scratchArena};

            readyNodes                = scratchArena.NewArray<ReadyNodeInfo>(nodes.size());
            nodeSchInfos              = scratchArena.NewArray<NodeSchedulingInfo>(nodes.size());
            resourceSchInfos          = scratchArena.NewArray<ResourceSchedulingInfo>(resources.size());
            subgraphSchInfos          = scratchArena.NewArray<SubgraphSchedulingInfo>(subgraphs.size());
            nodeReadyDeps             = scratchArena.NewArray<uint32_t>(nodes.size());
            nodeAtomicSubgraphIndices = scratchArena.NewArray<uint32_t>(nodes.size());
            scheduledNodes            = scratchArena.NewArray<uint32_t>(nodes.size());

            ArrayRef<NodeQueueInfo> nodeQueueInfos;
            if (flags.bUseAsync)
            {
                nodeQueueInfos = scratchArena.NewArray<NodeQueueInfo>(nodes.size());
            }

            nodeResourceRefs.reset(&scratchArena);

            // Node scoring:
            // [31]      : Scope bit
            // [30]      : Queue bit
            // [29]      : Barrier Batching High
            // [29 : 16] : Memory Saving
            // [16]      : Barrier Batching Low
            // [15]      : Work Type Grouping
            // [8]       : Work Type Interleave
            // [0  : 15] : Program Order
            // Note: The Work Type Grouping bit can be shifted within the Program Order bit range,
            // and the Barrier Batching High bit can be shifted within the Memory Saving bit range.
            // This allows us to interpolate between ordering preferences.
            // For example, if the schedule option prefers barrier batching over memory saving,
            // we can put the barrier batching bit on the highest bit in the memory saving score range,
            // and vice versa. In general, these are the default initial weight assignment of different node scheduling
            // factors. The actual weights of each factor are not limited to their ranges and may overlap.

            static constexpr uint32_t ScopeScoreBit         = (1u << 31);
            static constexpr uint32_t QueueScoreBit         = (1u << 30);
            static constexpr uint32_t BarrierScoreHighBit   = (1u << 29);
            static constexpr uint32_t BarrierScoreLowBit    = (1u << 16);
            static constexpr uint32_t MemoryScoreShift      = (16u);
            static constexpr uint32_t WorkTypeGroupingBit   = (1u << 15);
            static constexpr uint32_t WorkTypeInterleaveBit = (1u << 8);
            static constexpr uint32_t MemoryScoreMax        = (0x1fff);
            static constexpr uint32_t ProgramOrderScoreMax  = (0xffff);

            const uint32_t numTotalCmdNodes      = uint32_t(cmdInfos.size());
            const uint32_t maxCmdNodeId          = numTotalCmdNodes - 1;
            const uint32_t barrierBatchingBit    = flags.bPreferMemorySaving ? BarrierScoreLowBit : BarrierScoreHighBit;
            const uint32_t memoryAllocScoreShift = flags.bPreferMemorySaving ? 17 : 16;

            // Initialize node and resource scheduling info
            InitNodeResourceSchedulingInfos();

            // (Atomic) subgraph info
            const bool bHasAnyAtomicSubgraphs = InitAtomicSubgraphTopology();

            if (flags.bForceProgramOrder)
            {
                EnforceProgramOrder();
            }

            uint32_t numScheduled       = 0;
            uint32_t numEliminated      = 0;
            uint32_t numReadyCmdNodes   = 0;
            uint32_t numReadyTransNodes = 0;

            // Starting from command nodes only as transition nodes are expected to have out edges.
            for (int32_t iNode = int32_t(numTotalCmdNodes) - 1; iNode >= 0; iNode--)
            {
                const Node& node = nodes[iNode];

                if (node.outEdges.empty() || (node.outEdges.size() == nodeReadyDeps[iNode]))
                {
                    readyNodes[numReadyCmdNodes].nodeId             = iNode;
                    readyNodes[numReadyCmdNodes].depNodeId          = RPS_INDEX_NONE_U32;
                    readyNodes[numReadyCmdNodes].schBarrierScopeIdx = node.barrierScope;
                    readyNodes[numReadyCmdNodes].timeReady          = 0.0f;

                    numReadyCmdNodes++;
                }
            }

            RPS_RETURN_OK_IF(numReadyCmdNodes == 0);

            uint32_t lastNodeId    = readyNodes[0].nodeId;
            uint32_t lastCmdNodeId = lastNodeId;

            uint32_t lastBarrierScopeIdx  = nodes[lastNodeId].barrierScope;
            uint32_t lastAtomicSubgraphId = RPS_INDEX_NONE_U32;
            uint32_t atomicSGStackDepth   = 0;

            // TODO: Should be per queue
            float currentTime = 0.0f;

            bool lastNodeIsSplitCandidate = false;

            uint32_t currQueueIndex = RPS_INDEX_NONE_U32;
            uint32_t currQueueMask  = 0;

            uint32_t numBatches = 0;

            uint32_t firstUsedGfxQueue = RPS_INDEX_NONE_U32;

            while ((numReadyCmdNodes + numReadyTransNodes) > 0)
            {
                // Score and pick one node to schedule
                uint32_t highScore      = 0;
                uint32_t highScoreIndex = UINT32_MAX;

                const bool lastNodeIsTransition = IsTransitionNode(lastNodeId);

                lastCmdNodeId = lastNodeIsTransition ? lastCmdNodeId : lastNodeId;

                for (uint32_t iRN = 0; iRN < (numReadyCmdNodes + numReadyTransNodes); iRN++)
                {
                    const uint32_t currNodeId = readyNodes[iRN].nodeId;

                    const NodeSchedulingInfo& nodeInfo = nodeSchInfos[currNodeId];

                    const bool currNodeIsTransition = IsTransitionNode(currNodeId);

                    if (nodeInfo.canBeEliminated)
                    {
                        highScoreIndex = iRN;
                        break;
                    }

                    // Score nodes

                    // Barrier batching scoring
                    uint32_t barrierBatchingScore =
                        ((lastNodeIsTransition == currNodeIsTransition) ? barrierBatchingBit : 0);

                    // Program order scoring
                    const uint32_t nodeOrder = flags.bRandomOrder
                                                   ? randGen.pfnRandomUniformInt(randGen.pContext, 0, maxCmdNodeId)
                                                   : currNodeId;

                    const uint32_t programOrderScore = currNodeIsTransition ? 0 : nodeOrder;

                    // Workload type scoring
                    uint32_t pipelineWorkTypeScore = 0;

                    if (!(currNodeIsTransition || lastNodeIsTransition))
                    {
                        const bool workloadTypeEq =
                            (nodeSchInfos[lastCmdNodeId].workloadTypeMask == nodeInfo.workloadTypeMask);

                        if (flags.bMinimizeGfxCompSwitch)
                            pipelineWorkTypeScore = workloadTypeEq ? WorkTypeGroupingBit : 0;
                        else if (!flags.bWorkloadTypePipeliningDisabled)
                            pipelineWorkTypeScore = workloadTypeEq ? 0 : WorkTypeGroupingBit;
                    }
                    else if (lastNodeIsTransition && flags.bWorkloadTypePipeliningAggressive)
                    {
                        // TODO: Check the benefit / effect of this. Add a flag for control?
                        pipelineWorkTypeScore =
                            (nodeInfo.workloadTypeMask & RPS_NODE_DECL_GRAPHICS_BIT) ? WorkTypeInterleaveBit : 0;
                    }

                    // Memory allocation scoring
                    // TODO: Should be per heap
                    uint64_t allocAliasableMemSize = 0;
                    uint64_t freeAliasableMemSize  = 0;
                    uint64_t usingAliasableMemSize = 0;
                    uint64_t immediateReuseMemSize = 0;
                    for (uint32_t resIdx : nodeInfo.resourceRefs.Get(nodeResourceRefs))
                    {
                        ResourceSchedulingInfo& resSchInfo = resourceSchInfos[resIdx];

                        if (resSchInfo.scheduledUserNodesCount == 0)  // First access to a resource
                        {
                            allocAliasableMemSize += resSchInfo.aliasableSize;
                        }

                        if ((resSchInfo.scheduledUserNodesCount + 1) ==
                            resSchInfo.totalUserNodesCount)  // Last access to a resource
                        {
                            freeAliasableMemSize += resSchInfo.aliasableSize;
                        }

                        if (resSchInfo.mostRecentRefNodeId == lastCmdNodeId)
                        {
                            immediateReuseMemSize += resSchInfo.aliasableSize;  //TODO - should not limit to aliasable.
                        }

                        usingAliasableMemSize += resSchInfo.aliasableSize;
                    }

                    // Prioritize nodes with smaller new allocations
                    uint32_t memorySavingScore = uint32_t(
                        (rpsClamp<uint64_t>((maxNodeMemorySize - allocAliasableMemSize) >> 16, 0ull, MemoryScoreMax) +
                         rpsClamp<uint64_t>(freeAliasableMemSize >> 16, 0ull, MemoryScoreMax))
                        << memoryAllocScoreShift);

                    // Maximize memory saving score if the transition node is the initial access of a resource and
                    // is ready to be batched into a transition batch.
                    const bool bIsInitialTransitionNode = currNodeIsTransition && (freeAliasableMemSize != 0);
                    if (bIsInitialTransitionNode && lastNodeIsTransition)
                    {
                        memorySavingScore = MemoryScoreMax << memoryAllocScoreShift;
                    }

                    memorySavingScore = flags.bRandomOrder ? 0 : memorySavingScore;

                    uint32_t memoryReuseScore =
                        uint32_t(rpsClamp<uint64_t>((immediateReuseMemSize >> 19), 0ull, 0xFFFull)
                                 << memoryAllocScoreShift);  // TODO - use its own shift.

                    // Queue scoring:

                    uint32_t queueScore = QueueScoreBit;

                    if (flags.bUseAsync)
                    {
                        // Prioritize independent node between workload on another queue and its immediate dependent node.

                        // Penalize nodes who want a queue switch.
                        if (!rpsAnyBitsSet(currQueueMask, nodeInfo.preferredQueueMask))
                        {
                            queueScore = (QueueScoreBit >> 1);

                            // Candidate require a queue switch, and is an immediate dependent node of the previously scheduled command,
                            // raise penalty.
                            if ((!rpsAnyBitsSet(currQueueMask, nodeInfo.validQueueMask)) &&
                                (readyNodes[iRN].depNodeId == lastCmdNodeId))
                            {
                                queueScore = 0;
                            }
                        }
                    }

                    // Schedule barriers & Atomic subgraph should have highest priority.
                    uint32_t scopeScore = ScopeScoreBit;  // This is the highest bit

                    // Penalize nodes in an earlier barrier scope.
                    const uint32_t currBarrierScopeIdx = readyNodes[iRN].schBarrierScopeIdx;
                    if (currBarrierScopeIdx < lastBarrierScopeIdx)
                    {
                        scopeScore = 0;
                    }

                    // Penalize nodes in another atomic subgraph.
                    uint32_t currAtomicSubgraphId = nodeAtomicSubgraphIndices[currNodeId];
                    if (bHasAnyAtomicSubgraphs && (currAtomicSubgraphId != lastAtomicSubgraphId) &&
                        ((currAtomicSubgraphId == RPS_INDEX_NONE_U32) ||
                         (lastAtomicSubgraphId != subgraphSchInfos[currAtomicSubgraphId].atomicParentId)))
                    {
                        scopeScore = 0;
                    }

                    // Individual score categories should already be in their own bit range,
                    // OR them together as the final score.
                    const uint32_t currScore = rpsMax((scopeScore | queueScore | barrierBatchingScore |
                                                       memorySavingScore | programOrderScore | pipelineWorkTypeScore),
                                                      1u);

                    if (highScore < currScore)
                    {
                        highScore      = currScore;
                        highScoreIndex = iRN;
                    }
                }

                RPS_ASSERT(highScoreIndex != UINT32_MAX);

                // Move winner from ready list to scheduled list
                const uint32_t            scheduledNodeId   = readyNodes[highScoreIndex].nodeId;
                const Node&               scheduledNode     = nodes[scheduledNodeId];
                const NodeSchedulingInfo& scheduledNodeInfo = nodeSchInfos[scheduledNodeId];

                lastBarrierScopeIdx = readyNodes[highScoreIndex].schBarrierScopeIdx;

                const bool scheduledNodeIsTransition = IsTransitionNode(scheduledNodeId);

                lastNodeIsSplitCandidate =
                    (scheduledNodeIsTransition && (readyNodes[highScoreIndex].depNodeId != lastCmdNodeId));

                if (scheduledNodeIsTransition)
                    numReadyTransNodes--;
                else
                    numReadyCmdNodes--;

                readyNodes[highScoreIndex] = readyNodes[(numReadyCmdNodes + numReadyTransNodes)];

                // Eliminate nodes if allowed.
                // Also skip subgraph/subroutine markers.
                const bool bEliminate =
                    nodeSchInfos[scheduledNodeId].canBeEliminated ||
                    (!scheduledNodeIsTransition && cmdInfos[scheduledNode.GetCmdId()].IsNodeDeclBuiltIn());

                if (!bEliminate)
                {
                    scheduledNodes[numScheduled] = scheduledNodeId;
                    numScheduled++;

                    // Scheduled node prefer a queue switch:
                    if (flags.bUseAsync)
                    {
                        // First scheduled node, queue is not determined yet, should set bRequireAnotherQueue.
                        RPS_ASSERT((currQueueIndex != RPS_INDEX_NONE_U32) || (currQueueMask == 0));

                        const bool bRequireQueueSwitch = (0 == (scheduledNodeInfo.validQueueMask & currQueueMask));
                        const bool bPreferQueueSwitch  = (0 == (scheduledNodeInfo.preferredQueueMask & currQueueMask));
                        const bool bImmediateDependent = (readyNodes[highScoreIndex].depNodeId == lastCmdNodeId);

                        // Only actually do queue switch if:
                        // Current queue is not compatible with the candidate at all,
                        // or, candidate is not an immediate dependent.

                        const bool bSwitchQueue = bRequireQueueSwitch || (bPreferQueueSwitch && !bImmediateDependent);

                        if (bSwitchQueue)
                        {
                            numBatches++;

                            currQueueIndex = rpsFirstBitLow(scheduledNodeInfo.preferredQueueMask);
                            currQueueMask  = 1u << currQueueIndex;

                            if (m_targetInfo.pQueueFlags[currQueueIndex] & RPS_QUEUE_FLAG_GRAPHICS)
                            {
                                // Scheduling in reversed order, so last visited = first used in frame.
                                firstUsedGfxQueue = currQueueIndex;
                            }
                        }

                        nodeQueueInfos[scheduledNodeId].queueIndex     = currQueueIndex;
                        nodeQueueInfos[scheduledNodeId].scheduledIndex = (numScheduled - 1);
                    }
                }
                else
                {
                    numEliminated++;
                }

                lastAtomicSubgraphId = nodeAtomicSubgraphIndices[scheduledNodeId];

                if (bHasAnyAtomicSubgraphs && (lastAtomicSubgraphId != RPS_INDEX_NONE_U32))
                {
                    if (scheduledNodeId == subgraphs[lastAtomicSubgraphId].beginNode)
                    {
                        lastAtomicSubgraphId = subgraphSchInfos[lastAtomicSubgraphId].atomicParentId;
                    }
                }

                // Add new ready nodes to ready list
                const Node& newScheduledNode = nodes[scheduledNodeId];

                for (auto& inEdge : newScheduledNode.inEdges.Get(graph.GetEdges()))
                {
                    uint32_t    srcNodeId = inEdge.src;
                    const Node& srcNode   = nodes[srcNodeId];

                    int32_t outInputReadyCount = ++nodeReadyDeps[srcNodeId];

                    if (srcNode.outEdges.size() == outInputReadyCount)
                    {
                        ReadyNodeInfo* pNewReadyNode      = &readyNodes[(numReadyCmdNodes + numReadyTransNodes)];
                        pNewReadyNode->nodeId             = srcNodeId;
                        pNewReadyNode->depNodeId          = scheduledNodeId;
                        pNewReadyNode->schBarrierScopeIdx = srcNode.barrierScope;
                        pNewReadyNode->timeReady          = currentTime;

                        if (IsTransitionNode(srcNodeId))
                        {
                            numReadyTransNodes++;
                        }
                        else
                        {
                            numReadyCmdNodes++;
                        }
                    }
                }

                for (uint32_t resIdx : scheduledNodeInfo.resourceRefs.Get(nodeResourceRefs))
                {
                    ResourceSchedulingInfo& resSchInfo = resourceSchInfos[resIdx];
                    resSchInfo.scheduledUserNodesCount++;
                    resSchInfo.mostRecentRefNodeId = scheduledNodeId;
                    RPS_ASSERT(resSchInfo.scheduledUserNodesCount <= resSchInfo.totalUserNodesCount);
                }

                lastNodeId = scheduledNodeId;

                // TODO: Per queue, use actual time estimation.
                static const uint32_t RPS_DEFAULT_COMMAND_MAKESPAN = 1;
                currentTime += RPS_DEFAULT_COMMAND_MAKESPAN;
            }

            RPS_ASSERT((numScheduled + numEliminated) == nodes.size());
            RPS_RETURN_ERROR_IF((numScheduled + numEliminated) != nodes.size(), RPS_ERROR_INTERNAL_ERROR);

            auto& runtimeCmds = context.renderGraph.GetRuntimeCmdInfos();
            runtimeCmds.clear();
            runtimeCmds.reserve(numScheduled + 2);

            auto appendRuntimeCmdFromNodeId = [&](NodeId nodeId) {
                const auto&    node  = nodes[nodeId];
                const uint32_t cmdId = node.IsTransition() ? node.GetTransitionId() : node.GetCmdId();
                runtimeCmds.push_back({cmdId, node.IsTransition()});
            };

            auto&    cmdBatches          = context.renderGraph.GetCmdBatches();
            auto&    cmdBatchWaitIndices = context.renderGraph.GetCmdBatchWaitFenceIds();
            uint32_t numWaitFenceIdCount = 0;

            if (flags.bUseAsync && (numBatches > 1))
            {
                // Analyze cross queue synchronization

                auto batchInfos         = ArenaVector<BatchInfo>{&scratchArena};
                auto waitIndicesStorage = ArenaVector<uint32_t>{&scratchArena};
                auto batchWaitListPool  = SpanPool<uint32_t, ArenaVector<uint32_t>>{waitIndicesStorage};

                batchInfos.reserve(numBatches + 1);
                waitIndicesStorage.reserve(numBatches + 1);

                auto createNewBatch = [&](uint32_t queueIndex) {
                    const uint32_t currRuntimeCmdId = uint32_t(runtimeCmds.size());

                    if (!batchInfos.empty())
                    {
                        batchInfos.back().runtimeCmdEnd = currRuntimeCmdId;
                    }

                    batchInfos.emplace_back(BatchInfo{queueIndex, currRuntimeCmdId, currRuntimeCmdId});

                    batchWaitListPool.alloc_span(batchInfos.back().waitForBatchIndices, 0);

                    currQueueIndex = queueIndex;
                };

                // TODO: Aliasing handling assumes preamble/postamble to be graphics atm.
                // Forcing these to graphics queue for now!
                // (If no graphics queue is found, fall back to queue 0).
                firstUsedGfxQueue = (firstUsedGfxQueue != RPS_INDEX_NONE_U32) ? firstUsedGfxQueue : 0;
                createNewBatch(firstUsedGfxQueue);

                runtimeCmds.push_back(RuntimeCmdInfo{CMD_ID_PREAMBLE, true});

                int32_t perQueueLastWaitBatch[RPS_MAX_QUEUES * RPS_MAX_QUEUES];
                std::fill(std::begin(perQueueLastWaitBatch), std::end(perQueueLastWaitBatch), -1);

                for (int32_t iScheduledIdx = numScheduled - 1; iScheduledIdx >= 0; --iScheduledIdx)
                {
                    const NodeId nodeId   = scheduledNodes[iScheduledIdx];
                    const Node&  currNode = nodes[nodeId];

                    NodeQueueInfo& currNodeInfo = nodeQueueInfos[nodeId];
                    RPS_ASSERT(currNodeInfo.scheduledIndex == uint32_t(iScheduledIdx));

                    bool bAddingBatchDependency = false;

                    int32_t srcDependBatchIndices[RPS_MAX_QUEUES];
                    std::fill(std::begin(srcDependBatchIndices), std::end(srcDependBatchIndices), -1);

                    ArrayRef<int32_t> currQueueLastWaitBatches = {
                        perQueueLastWaitBatch + currNodeInfo.queueIndex * RPS_MAX_QUEUES, RPS_MAX_QUEUES};

                    for (auto& inEdge : currNode.inEdges.Get(graph.GetEdges()))
                    {
                        const NodeQueueInfo& srcNodeInfo = nodeQueueInfos[inEdge.src];

                        RPS_ASSERT(srcNodeInfo.batchIndex < batchInfos.size());
                        RPS_ASSERT(batchInfos[srcNodeInfo.batchIndex].queueIndex == srcNodeInfo.queueIndex);

                        const int32_t iSrcBatchIdx = int32_t(srcNodeInfo.batchIndex);

                        if ((currNodeInfo.queueIndex != srcNodeInfo.queueIndex) &&
                            (currQueueLastWaitBatches[srcNodeInfo.queueIndex] < iSrcBatchIdx))
                        {
                            // TODO: Recursively check if any indirect dependency already exists.

                            currQueueLastWaitBatches[srcNodeInfo.queueIndex] = iSrcBatchIdx;

                            RPS_ASSERT(srcDependBatchIndices[srcNodeInfo.queueIndex] < iSrcBatchIdx);
                            srcDependBatchIndices[srcNodeInfo.queueIndex] = iSrcBatchIdx;

                            batchInfos[srcNodeInfo.batchIndex].bSignal = true;

                            bAddingBatchDependency = true;
                        }
                    }

                    // Create new batch if:
                    // - Switch queue
                    // - Adding new dependency, and if current batch is not transition-only.
                    if ((currNodeInfo.queueIndex != currQueueIndex) ||
                        (bAddingBatchDependency && batchInfos.back().bHasCmdNode))
                    {
                        createNewBatch(currNodeInfo.queueIndex);
                    }

                    currNodeInfo.batchIndex = uint32_t(batchInfos.size() - 1);

                    batchInfos.back().bHasCmdNode |= !currNode.IsTransition();

                    appendRuntimeCmdFromNodeId(scheduledNodes[iScheduledIdx]);

                    if (bAddingBatchDependency)
                    {
                        BatchInfo& currBatch = batchInfos.back();

                        for (uint32_t iQ = 0; iQ < RPS_MAX_QUEUES; iQ++)
                        {
                            const int32_t srcDepBatchIdx = srcDependBatchIndices[iQ];
                            if (srcDepBatchIdx != -1)
                            {
                                RPS_ASSERT(iQ != currNodeInfo.queueIndex);
                                RPS_ASSERT(batchInfos[srcDepBatchIdx].bSignal);
                                RPS_ASSERT(batchInfos[srcDepBatchIdx].queueIndex == iQ);

                                batchWaitListPool.push_to_span(currBatch.waitForBatchIndices, uint32_t(srcDepBatchIdx));
                                numWaitFenceIdCount++;
                            }
                        }
                    }
                }

                if (!(m_targetInfo.pQueueFlags[currQueueIndex] & RPS_QUEUE_FLAG_GRAPHICS))
                {
                    createNewBatch(firstUsedGfxQueue);
                }

                runtimeCmds.push_back(RuntimeCmdInfo{CMD_ID_POSTAMBLE, true});

                batchInfos.back().runtimeCmdEnd = uint32_t(runtimeCmds.size());

                cmdBatches.resize(batchInfos.size());
                cmdBatchWaitIndices.reserve(numWaitFenceIdCount);

                uint32_t fenceIndex = 0;

                for (uint32_t iBatch = 0; iBatch < batchInfos.size(); iBatch++)
                {
                    const BatchInfo& batchInfo = batchInfos[iBatch];
                    RpsCommandBatch& cmdBatch  = cmdBatches[iBatch];

                    cmdBatch.cmdBegin   = batchInfo.runtimeCmdBegin;
                    cmdBatch.numCmds   = batchInfo.runtimeCmdEnd - batchInfo.runtimeCmdBegin;
                    cmdBatch.queueIndex = batchInfo.queueIndex;

                    cmdBatch.signalFenceIndex = batchInfo.bSignal ? fenceIndex : RPS_INDEX_NONE_U32;
                    fenceIndex += batchInfo.bSignal ? 1 : 0;

                    cmdBatch.waitFencesBegin = uint32_t(cmdBatchWaitIndices.size());
                    cmdBatch.numWaitFences   = batchInfo.waitForBatchIndices.size();

                    auto waitForBatchIndicesRange = batchInfo.waitForBatchIndices.Get(waitIndicesStorage);

                    for (auto waitBatchIndex : waitForBatchIndicesRange)
                    {
                        RPS_ASSERT(waitBatchIndex < iBatch);
                        RPS_ASSERT(cmdBatches[waitBatchIndex].signalFenceIndex != RPS_INDEX_NONE_U32);

                        cmdBatchWaitIndices.push_back(cmdBatches[waitBatchIndex].signalFenceIndex);
                    }
                }

                RPS_ASSERT(cmdBatchWaitIndices.size() == numWaitFenceIdCount);
            }
            else
            {
                runtimeCmds.push_back(RuntimeCmdInfo{CMD_ID_PREAMBLE, true});

                for (int32_t iScheduledIdx = numScheduled - 1; iScheduledIdx >= 0; --iScheduledIdx)
                {
                    appendRuntimeCmdFromNodeId(scheduledNodes[iScheduledIdx]);
                }

                runtimeCmds.push_back(RuntimeCmdInfo{CMD_ID_POSTAMBLE, true});

                cmdBatches.resize(1, CommandBatch{});
                cmdBatches.back().cmdBegin = 0;
                cmdBatches.back().numCmds = uint32_t(runtimeCmds.size());
            }

            return RPS_OK;
        }

    private:
        void InitNodeResourceSchedulingInfos()
        {
            const bool bEliminateDeadNodes = m_targetInfo.options.bEliminateDeadNodes;

            std::fill(nodeReadyDeps.begin(), nodeReadyDeps.end(), 0);

            // Initialize per-resource scheduling info
            for (uint32_t iRes = 0, numRes = uint32_t(resources.size()); iRes < numRes; iRes++)
            {
                auto& resSchInfo = resourceSchInfos[iRes];

                resSchInfo.aliasableSize           = resources[iRes].allocRequirement.size;
                resSchInfo.scheduledUserNodesCount = 0;
                resSchInfo.totalUserNodesCount     = 0;
            }

            // Initialize per-node scheduling info
            maxNodeMemorySize = 0;

            constexpr RpsQueueFlags defaultQueueFlags[] = {RPS_QUEUE_FLAG_GRAPHICS};

            // Initialize queue info
            // TODO: If no need to change queue layout per schedule, this can be done statically.

            uint32_t gfxQueueMask       = 0;
            uint32_t validCompQueueMask = 0;
            uint32_t validCopyQueueMask = (1u << m_targetInfo.numQueues) - 1;
            uint32_t allQueueMask       = validCopyQueueMask;
            uint32_t asyncCompQueueMask = 0;
            uint32_t asyncCopyQueueMask = 0;

            if (m_targetInfo.numQueues > 0)
            {
                for (uint32_t iQueue = 0; iQueue < m_targetInfo.numQueues; iQueue++)
                {
                    if (m_targetInfo.pQueueFlags[iQueue] & RPS_QUEUE_FLAG_GRAPHICS)
                        gfxQueueMask |= (1u << iQueue);

                    if (m_targetInfo.pQueueFlags[iQueue] & (RPS_QUEUE_FLAG_COMPUTE | RPS_QUEUE_FLAG_GRAPHICS))
                        validCompQueueMask |= (1u << iQueue);
                    else
                        asyncCopyQueueMask |= (1u << iQueue);

                    if ((m_targetInfo.pQueueFlags[iQueue] & RPS_QUEUE_FLAG_COMPUTE) &&
                        !(m_targetInfo.pQueueFlags[iQueue] & RPS_QUEUE_FLAG_GRAPHICS))
                        asyncCompQueueMask |= (1u << iQueue);
                }
            }
            else
            {
                gfxQueueMask = 1;
            }

            asyncCompQueueMask = (asyncCompQueueMask == 0) ? validCompQueueMask : asyncCompQueueMask;
            asyncCopyQueueMask = (asyncCopyQueueMask == 0) ? asyncCompQueueMask : asyncCopyQueueMask;

            constexpr RpsNodeDeclFlags AllNodeDeclWorkloadTypeMask =
                RPS_NODE_DECL_GRAPHICS_BIT | RPS_NODE_DECL_COMPUTE_BIT | RPS_NODE_DECL_COPY_BIT;

            // Starting from command nodes only as transition nodes are expected to have out edges.
            for (uint32_t iNode = 0; iNode < cmdInfos.size(); iNode++)
            {
                auto                cmdAcceses  = m_renderGraph.GetCmdAccesses(iNode);
                NodeSchedulingInfo& nodeResInfo = nodeSchInfos[iNode];

                // TODO: Can we infer queue requirement only based on resource accesses & remove graphics/compute/copy specifiers?
                uint32_t queueMask          = gfxQueueMask;
                uint32_t preferredQueueMask = gfxQueueMask;

                const CmdInfo* const pCmdInfo  = m_renderGraph.GetCmdInfo(iNode);
                const NodeDeclInfo*  pNodeDecl = pCmdInfo->pNodeDecl;

                if (!pCmdInfo->IsNodeDeclBuiltIn())
                {
                    if (pNodeDecl->flags & RPS_NODE_DECL_COMPUTE_BIT)
                    {
                        queueMask |= validCompQueueMask;
                        preferredQueueMask = pCmdInfo->bPreferAsync ? asyncCompQueueMask : preferredQueueMask;
                    }

                    if (pNodeDecl->flags & RPS_NODE_DECL_COPY_BIT)
                    {
                        queueMask |= validCopyQueueMask;
                        preferredQueueMask = pCmdInfo->bPreferAsync ? asyncCopyQueueMask : preferredQueueMask;
                    }

                    nodeResInfo.validQueueMask     = queueMask;
                    nodeResInfo.preferredQueueMask = preferredQueueMask;
                    nodeResInfo.workloadTypeMask   = (pNodeDecl->flags & AllNodeDeclWorkloadTypeMask);
                }
                else
                {
                    // Built-in nodes such as subgraph/subprogram begin/end.
                    // TODO: Anything to do here? Deduce from contained nodes?
                    nodeResInfo.validQueueMask     = allQueueMask;
                    nodeResInfo.preferredQueueMask = allQueueMask;
                    nodeResInfo.workloadTypeMask   = AllNodeDeclWorkloadTypeMask;
                }

                RPS_ASSERT(nodeResInfo.preferredQueueMask != 0);
                RPS_ASSERT(nodeResInfo.validQueueMask != 0);
                RPS_ASSERT(nodeResInfo.workloadTypeMask != 0);
                RPS_ASSERT((nodeResInfo.preferredQueueMask & nodeResInfo.validQueueMask) ==
                           nodeResInfo.preferredQueueMask);

                nodeResInfo.resourceRefs.SetRange(uint32_t(nodeResourceRefs.size()), 0);
                nodeResourceRefs.reserve(nodeResourceRefs.size() + cmdAcceses.size());

                uint64_t nodeAliasableMemSize = 0;

                bool bMustKeep = false;

                for (uint32_t iAccess = 0, numAccesses = cmdAcceses.size(); iAccess < numAccesses; iAccess++)
                {
                    const CmdAccessInfo& access = cmdAcceses[iAccess];

                    if (access.resourceId == RPS_RESOURCE_ID_INVALID)
                        continue;

                    if (std::find(nodeResourceRefs.begin() + nodeResInfo.resourceRefs.GetBegin(),
                                  nodeResourceRefs.end(),
                                  access.resourceId) == nodeResourceRefs.end())
                    {
                        nodeResourceRefs.push_back(access.resourceId);

                        resourceSchInfos[access.resourceId].totalUserNodesCount++;
                        nodeAliasableMemSize += resourceSchInfos[access.resourceId].aliasableSize;
                    }

                    auto& resInfo = resources[access.resourceId];

                    const bool bHasGPUWrite     = rpsAnyBitsSet(access.access.accessFlags, RPS_ACCESS_ALL_GPU_WRITE);
                    const bool bIsPersistentRes = rpsAnyBitsSet(resInfo.desc.flags, RPS_RESOURCE_FLAG_PERSISTENT_BIT);
                    const bool bIsExternalRes   = resInfo.isExternal;
                    const bool bHasCPUAccess    = rpsAnyBitsSet(access.access.accessFlags, RPS_ACCESS_ALL_CPU);

                    if ((bHasGPUWrite && (bIsPersistentRes || bIsExternalRes)) || bHasCPUAccess)
                    {
                        bMustKeep = true;
                    }
                }

                nodeResInfo.resourceRefs.SetCount(uint32_t(nodeResourceRefs.size()) -
                                                  nodeResInfo.resourceRefs.GetBegin());

                maxNodeMemorySize = rpsMax(maxNodeMemorySize, nodeAliasableMemSize);

                nodeResInfo.canBeEliminated = bEliminateDeadNodes && nodes[iNode].outEdges.empty() && !bMustKeep;
            }

            // Transition nodes:

            auto transitions = m_renderGraph.GetTransitions().range_all();
            nodeResourceRefs.reserve_additional(transitions.size());

            ArrayRef<NodeSchedulingInfo> transNodeSchInfos = nodeSchInfos.range(uint32_t(cmdInfos.size()));

            for (uint32_t iTrans = 1, numTrans = uint32_t(transitions.size()); iTrans < numTrans; iTrans++)
            {
                NodeSchedulingInfo& nodeResInfo    = transNodeSchInfos[iTrans - 1];
                const auto&         transitionInfo = transitions[iTrans];

                nodeResInfo.resourceRefs.SetRange(uint32_t(nodeResourceRefs.size()), 1);

                if (m_targetInfo.options.bUseAsync)
                {
                    RpsNodeDeclFlags nodeQueueFlag =
                        GetRequiredQueueFlagsFromAccessAttr(RPS_NODE_DECL_FLAG_NONE, transitionInfo.access.access);

                    if (transitionInfo.prevTransition != RenderGraph::INVALID_TRANSITION)
                    {
                        static_assert((RPS_NODE_DECL_GRAPHICS_BIT < RPS_NODE_DECL_COMPUTE_BIT) &&
                                          (RPS_NODE_DECL_COMPUTE_BIT < RPS_NODE_DECL_COPY_BIT),
                                      "Unexpected Node Decl flag ordering");

                        nodeQueueFlag = rpsMin(
                            nodeQueueFlag,
                            GetRequiredQueueFlagsFromAccessAttr(
                                RPS_NODE_DECL_FLAG_NONE, transitions[transitionInfo.prevTransition].access.access));
                    }

                    uint32_t queueMask = gfxQueueMask;

                    if (nodeQueueFlag & RPS_NODE_DECL_COMPUTE_BIT)
                    {
                        queueMask |= validCompQueueMask;
                    }

                    if (nodeQueueFlag & RPS_NODE_DECL_COPY_BIT)
                    {
                        queueMask |= validCopyQueueMask;
                    }

                    nodeResInfo.workloadTypeMask   = nodeQueueFlag;
                    nodeResInfo.validQueueMask     = queueMask;
                    nodeResInfo.preferredQueueMask = queueMask;
                }

                RPS_ASSERT(transitionInfo.access.resourceId != RPS_RESOURCE_ID_INVALID);

                resourceSchInfos[transitionInfo.access.resourceId].totalUserNodesCount++;

                nodeResourceRefs.push_back(transitionInfo.access.resourceId);

                RPS_ASSERT(maxNodeMemorySize >= resourceSchInfos[transitionInfo.access.resourceId].aliasableSize);
            }
        }

        // Returns if there are any atomic subgraphs
        bool InitAtomicSubgraphTopology()
        {
            uint32_t atomicSubgraphCount = 0;

            for (uint32_t iSG = 0; iSG < subgraphs.size(); iSG++)
            {
                auto& sg     = subgraphs[iSG];
                auto& sgInfo = subgraphSchInfos[iSG];

                sgInfo.atomicParentId = RPS_INDEX_NONE_U32;

                atomicSubgraphCount += (sg.IsAtomic() ? 1 : 0);

                uint32_t parentId = sg.parentSubgraph;
                while (parentId != RPS_INDEX_NONE_U32)
                {
                    if (subgraphs[parentId].IsAtomic())
                    {
                        sgInfo.atomicParentId = parentId;
                        break;
                    }
                    parentId = subgraphs[parentId].parentSubgraph;
                }

                const uint32_t enclosingAtomicSGId = sg.IsAtomic() ? iSG : sgInfo.atomicParentId;

                sgInfo.atomicSubgraphId = enclosingAtomicSGId;

                if (sg.IsAtomic() && sgInfo.atomicParentId != RPS_INDEX_NONE_U32)
                {
                    const Subgraph& parentAtomicSG = subgraphs[sgInfo.atomicParentId];
                    graph.AddEdge(parentAtomicSG.beginNode, sg.beginNode);
                    graph.AddEdge(sg.beginNode, parentAtomicSG.endNode);
                }
            }

            if (atomicSubgraphCount == 0)
            {
                return false;
            }

            std::fill(nodeAtomicSubgraphIndices.begin(), nodeAtomicSubgraphIndices.end(), RPS_INDEX_NONE_U32);

            for (uint32_t iSG = 0; iSG < subgraphs.size(); iSG++)
            {
                const Subgraph& sg = subgraphs[iSG];

                if (sg.IsAtomic())
                {
                    nodeAtomicSubgraphIndices[sg.beginNode] = iSG;

                    for (uint32_t iNode = sg.beginNode + 1; iNode < sg.endNode; iNode++)
                    {
                        const Node& node = nodes[iNode];

                        const uint32_t currAtomicSGId = subgraphSchInfos[node.subgraph].atomicSubgraphId;

                        if (currAtomicSGId != iSG)
                        {
                            // Current node is in a nested atomic subgraph, skip the nested subgraph range
                            RPS_ASSERT((currAtomicSGId != RPS_INDEX_NONE_U32) && (currAtomicSGId > iSG));
                            iNode = subgraphs[currAtomicSGId].endNode;
                            RPS_ASSERT(iNode < sg.endNode);
                            continue;
                        }

                        nodeAtomicSubgraphIndices[iNode] = currAtomicSGId;

                        RPS_ASSERT(currAtomicSGId == iSG);

                        AddAtomicSubgraphEdges(currAtomicSGId, sg, node, iNode);
                    }

                    nodeAtomicSubgraphIndices[sg.endNode] = iSG;
                }
            }

            auto transitions = m_renderGraph.GetTransitions().range_all();

            for (uint32_t iTrans = 1, numTrans = uint32_t(transitions.size()); iTrans < numTrans; iTrans++)
            {
                const auto& transitionInfo = transitions[iTrans];

                const Node& node = nodes[transitionInfo.nodeId];

                if (node.subgraph != RPS_INDEX_NONE_U32)
                {
                    uint32_t currAtomicSGId = subgraphSchInfos[node.subgraph].atomicSubgraphId;

                    if (currAtomicSGId != RPS_INDEX_NONE_U32)
                    {
                        nodeAtomicSubgraphIndices[transitionInfo.nodeId] = currAtomicSGId;

                        AddAtomicSubgraphEdges(currAtomicSGId, subgraphs[currAtomicSGId], node, transitionInfo.nodeId);
                    }
                }
            }

            return true;
        }

        void EnforceProgramOrder()
        {
            const uint32_t numSrcNodes = (cmdInfos.size() > 1) ? uint32_t(cmdInfos.size() - 1) : 0;

            for (uint32_t iSrcNode = 0; iSrcNode < numSrcNodes; iSrcNode++)
            {
                graph.AddEdge(iSrcNode, iSrcNode + 1);
            }
        }

        void AddAtomicSubgraphEdges(uint32_t currAtomicSGId, const Subgraph& sg, const Node& node, uint32_t nodeId)
        {
            bool addEdgeFromSGBegin = (node.inEdges.empty());

            for (const Edge& inEdge : node.inEdges.Get(graph.GetEdges()))
            {
                uint32_t srcNode       = inEdge.src;
                uint32_t srcSubgraph   = nodes[srcNode].subgraph;
                uint32_t srcAtomicSGId = (srcSubgraph == RPS_INDEX_NONE_U32)
                                             ? RPS_INDEX_NONE_U32
                                             : subgraphSchInfos[srcSubgraph].atomicSubgraphId;

                if (currAtomicSGId != srcAtomicSGId)
                {
                    if (!graph.IsParentSubgraph(currAtomicSGId, srcAtomicSGId))
                    {
                        addEdgeFromSGBegin = true;

                        uint32_t outermostAtomicSGIdx = currAtomicSGId;
                        for (uint32_t parentAtomicId = currAtomicSGId;
                             (parentAtomicId != RPS_INDEX_NONE_U32) &&
                             !graph.IsParentSubgraph(parentAtomicId, srcAtomicSGId);
                             parentAtomicId = subgraphSchInfos[parentAtomicId].atomicParentId)
                        {
                            outermostAtomicSGIdx = parentAtomicId;
                        }

                        const Subgraph& outermostAtomicSG = subgraphs[outermostAtomicSGIdx];
                        if (srcNode != outermostAtomicSG.beginNode)
                        {
                            graph.AddEdge(srcNode, outermostAtomicSG.beginNode);
                        }
                    }
                }
            }

            if (addEdgeFromSGBegin)
            {
                graph.AddEdge(sg.beginNode, nodeId);
            }

            RpsBool addEdgeToSGEnd = (node.outEdges.empty());

            for (const Edge& outEdge : node.outEdges.Get(graph.GetEdges()))
            {
                uint32_t dstNode       = outEdge.dst;
                uint32_t dstSubgraph   = nodes[dstNode].subgraph;
                uint32_t dstAtomicSGId = (dstSubgraph == RPS_INDEX_NONE_U32)
                                             ? RPS_INDEX_NONE_U32
                                             : subgraphSchInfos[dstSubgraph].atomicSubgraphId;

                if (currAtomicSGId != dstAtomicSGId)
                {
                    if (!graph.IsParentSubgraph(currAtomicSGId, dstAtomicSGId))
                    {
                        addEdgeToSGEnd = RPS_TRUE;

                        uint32_t outermostAtomicSGIdx = currAtomicSGId;
                        for (uint32_t parentAtomicId = currAtomicSGId;
                             (parentAtomicId != RPS_INDEX_NONE_U32) &&
                             !graph.IsParentSubgraph(parentAtomicId, dstAtomicSGId);
                             parentAtomicId = subgraphSchInfos[parentAtomicId].atomicParentId)
                        {
                            outermostAtomicSGIdx = parentAtomicId;
                        }

                        const Subgraph& outermostAtomicSG = subgraphs[outermostAtomicSGIdx];

                        if (dstNode != outermostAtomicSG.endNode)
                        {
                            graph.AddEdge(outermostAtomicSG.endNode, dstNode);
                        }
                    }
                }
            }

            if (addEdgeToSGEnd)
            {
                graph.AddEdge(nodeId, sg.endNode);
            }
        }

        bool IsTransitionNode(NodeId id) const
        {
            return id >= cmdInfos.size();
        }

    private:
        RenderGraph&  m_renderGraph;
        TargetInfo    m_targetInfo;

        Graph&                          graph;
        ConstArrayRef<Node>             nodes;
        ConstArrayRef<Subgraph>         subgraphs;
        ConstArrayRef<ResourceInstance> resources;
        ConstArrayRef<CmdInfo>          cmdInfos;

        ArrayRef<NodeId, uint32_t>                 scheduledNodes;
        ArrayRef<ReadyNodeInfo, uint32_t>          readyNodes;
        ArrayRef<NodeSchedulingInfo, uint32_t>     nodeSchInfos;
        ArrayRef<ResourceSchedulingInfo, uint32_t> resourceSchInfos;
        ArrayRef<SubgraphSchedulingInfo, uint32_t> subgraphSchInfos;
        ArrayRef<uint32_t, uint32_t>               nodeReadyDeps;
        ArrayRef<uint32_t, uint32_t>               nodeAtomicSubgraphIndices;
        ArenaVector<uint32_t>                      nodeResourceRefs;

        uint64_t   maxNodeMemorySize = 0;
    };
}  // namespace rps

#endif  //RPS_DAG_SCHEDULE_HPP
