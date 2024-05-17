// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_ACCESS_DAG_BUILD_HPP
#define RPS_ACCESS_DAG_BUILD_HPP

#include "runtime/common/rps_render_graph.hpp"

namespace rps
{
    class AccessDAGBuilderPass : public IRenderGraphPhase
    {
    public:
        AccessDAGBuilderPass(RenderGraph& renderGraph)
            : m_renderGraph(renderGraph)
            , m_pRuntimeDevice(RuntimeDevice::Get(renderGraph.GetDevice()))
            , m_transitions(renderGraph.GetTransitions())
            , m_nodeRefListPool(m_nodeRefLists)
        {
        }

        virtual RpsResult Run(RenderGraphUpdateContext& context) override final
        {
            RPS_ASSERT(&m_renderGraph == &context.renderGraph);

            const auto cmds  = m_renderGraph.GetCmdInfos().crange_all();
            auto&      graph = m_renderGraph.GetGraph();

            auto& resourceInsts = m_renderGraph.GetResourceInstances();

            ArenaCheckPoint arenaCheckpoint{context.scratchArena};

            m_resourceStates.reset_keep_capacity(&context.scratchArena);
            m_subResStates.reset_keep_capacity(&context.scratchArena);
            m_nodeRefLists.reset_keep_capacity(&context.scratchArena);
            m_nodeRefListPool.reset();

            RPS_V_RETURN(InitResourceStates());

            m_transitions.resize(1);  // 0 is reserved for INVALID_TRANSITION
            m_transitions[0] = TransitionInfo{
                {
                    RPS_RESOURCE_ID_INVALID,
                    SubresourceRangePacked(1, 0, 1, 0, 1),
                    {RPS_ACCESS_UNKNOWN, RPS_SHADER_STAGE_NONE},
                    RPS_FORMAT_UNKNOWN,
                },
                RPS_INDEX_NONE_U32,
                RPS_INDEX_NONE_U32,
            };

            // For each node, process accesses in two passes, first writeable then readonly accesses.
            // This is to allow write accesses to take precedence over read in case of overlapping R/W ranges.
            static constexpr uint32_t WriteAccessDAGPass = 0;
            static constexpr uint32_t ReadAccessDAGPass  = 1;
            static constexpr uint32_t NumAccessDAGPasses = 2;

            for (uint32_t iCmd = 0, numCmds = uint32_t(cmds.size()); iCmd < numCmds; ++iCmd)
            {
                const NodeId nodeId = iCmd;  // TODO

                auto cmdAccesses = m_renderGraph.GetCmdAccesses(iCmd);

                for (uint32_t iPass = 0; iPass < NumAccessDAGPasses; iPass++)
                {
                    // TODO: Bitmask R/W accesses per node for common cases where access count is limited.
                    for (const CmdAccessInfo& currAccess : cmdAccesses)
                    {
                        if (currAccess.resourceId == RPS_RESOURCE_ID_INVALID)
                            continue;

                        if (((iPass == WriteAccessDAGPass) &&
                             !(currAccess.access.accessFlags & RPS_ACCESS_ALL_GPU_WRITE)) ||
                            ((iPass == ReadAccessDAGPass) &&
                             (currAccess.access.accessFlags & RPS_ACCESS_ALL_GPU_WRITE)))
                        {
                            continue;
                        }

                        ResourceInstance& resInfo = resourceInsts[currAccess.resourceId];

                        const bool isSingleSubresource = (resInfo.numSubResources == 1);

                        // Previous states
                        ResourceState& resState     = m_resourceStates[currAccess.resourceId];
                        auto           subResStates = resState.subResStates.Get(m_subResStates);

                        // Set initial state
                        if ((resInfo.initialAccess.accessFlags == RPS_ACCESS_UNKNOWN) &&
                            ((!isSingleSubresource && subResStates[0].access.accessorNodes.empty()) ||
                             resState.access.accessorNodes.empty()))
                        {
                            RPS_ASSERT((isSingleSubresource && subResStates.empty()) || (subResStates.size() == 1));

                            resInfo.SetInitialAccess(currAccess.access);
                        }

                        // Buffers and single-subresource images: simple path

                        if (isSingleSubresource)
                        {
                            RPS_V_RETURN(ProcessTransition(graph,
                                                           nodeId,
                                                           resState.access,
                                                           currAccess.access,
                                                           currAccess.resourceId,
                                                           currAccess.range));
                        }
                        else
                        {
                            // Check each subresource range in previous access ranges
                            auto prevRanges = resState.subResStates.Get(m_subResStates);

                            for (SubresourceState& prevSubResState : prevRanges)
                            {
                                SubresourceRangePacked overlapRange;
                                SubresourceRangePacked remainingRanges[SubresourceRangePacked::MAX_CLIP_COMPLEMENTS];
                                uint32_t               numRemainingRanges = 0;

                                // Try to clip current access range against existing range
                                if (SubresourceRangePacked::Clip(prevSubResState.range,
                                                                 currAccess.range,
                                                                 remainingRanges,
                                                                 &numRemainingRanges,
                                                                 &overlapRange))
                                {
                                    RPS_ASSERT(resState.subResStates.size() + numRemainingRanges <=
                                               resInfo.numSubResources);

                                    const uint32_t newRangeOffset = resState.subResStates.GetEnd();
                                    resState.subResStates.SetRange(resState.subResStates.GetBegin(),
                                                                   resState.subResStates.size() + numRemainingRanges);

                                    for (uint32_t iRemaining = 0; iRemaining < numRemainingRanges; iRemaining++)
                                    {
                                        SubresourceState& newAccessState = m_subResStates[newRangeOffset + iRemaining];

                                        newAccessState.range = remainingRanges[iRemaining];

                                        RPS_V_RETURN(CloneReferenceList(prevSubResState.access.accessorNodes,
                                                                        newAccessState.access.accessorNodes));

                                        uint32_t newTransition = RenderGraph::INVALID_TRANSITION;
                                        if (prevSubResState.access.lastTransition != RenderGraph::INVALID_TRANSITION)
                                        {
                                            RPS_V_RETURN(CloneTransition(
                                                graph, prevSubResState.access.lastTransition, newTransition));
                                            m_transitions[newTransition].access.range = remainingRanges[iRemaining];
                                        }

                                        newAccessState.access.lastTransition = newTransition;
                                    }

                                    // Replace previous transition node's access range to the overlapping region.
                                    if (prevSubResState.access.lastTransition != RenderGraph::INVALID_TRANSITION)
                                    {
                                        auto& access = m_transitions[prevSubResState.access.lastTransition].access;

                                        access.range = overlapRange;
                                        FilterAccessByRange(access.access, overlapRange);
                                    }

                                    prevSubResState.range = overlapRange;

                                    auto filteredCurrentAccess = currAccess.access;
                                    FilterAccessByRange(filteredCurrentAccess, overlapRange);

                                    ProcessTransition(graph,
                                                      nodeId,
                                                      prevSubResState.access,
                                                      filteredCurrentAccess,
                                                      currAccess.resourceId,
                                                      overlapRange);
                                }
                            }
                        }
                    }
                }
            }

            auto& finalAccesses = context.renderGraph.GetResourceFinalAccesses();
            finalAccesses.resize(0);

            // Collect final states
            for (uint32_t iRes = 0, numRes = uint32_t(resourceInsts.size()); iRes < numRes; iRes++)
            {
                ResourceInstance& resInfo = resourceInsts[iRes];

                if (!resInfo.IsActive() || resInfo.IsTemporalParent())
                    continue;

                if (resInfo.numSubResources > 1)
                {
                    auto subResRangesRef = m_resourceStates[iRes].subResStates.Get(m_subResStates);

                    resInfo.finalAccesses.SetRange(uint32_t(finalAccesses.size()), subResRangesRef.size());

                    auto* pRanges = finalAccesses.grow(subResRangesRef.size());

                    for (uint32_t iRange = 0; iRange < subResRangesRef.size(); iRange++)
                    {
                        pRanges[iRange].range          = subResRangesRef[iRange].range;
                        pRanges[iRange].prevTransition = subResRangesRef[iRange].access.lastTransition;
                    }
                }
                else
                {
                    resInfo.finalAccesses.SetRange(uint32_t(finalAccesses.size()), 1);
                    auto* pRange           = finalAccesses.grow(1);
                    pRange->range          = resInfo.fullSubresourceRange;
                    pRange->prevTransition = m_resourceStates[iRes].access.lastTransition;
                }
            }

            // Save high watermark scale by 1.5 for next frame reservation size
            m_transitionCountWatermark = uint32_t(m_transitions.size());
            m_transitionCountWatermark = m_transitionCountWatermark + (m_transitionCountWatermark >> 1);

            return RPS_OK;
        }

    private:
        struct AccessState
        {
            Span<NodeId> accessorNodes;
            uint32_t     lastTransition;
        };

        struct SubresourceState
        {
            AccessState            access;
            SubresourceRangePacked range;
        };

        struct ResourceState
        {
            AccessState            access;
            Span<SubresourceState> subResStates;
        };

    public:
        ConstArrayRef<TransitionInfo> GetTransitionInfos() const
        {
            return m_transitions.range_all();
        }

    private:
        RpsResult InitResourceStates()
        {
            auto resourceInstances = m_renderGraph.GetResourceInstances().range_all();

            RPS_RETURN_ERROR_IF(!m_resourceStates.resize(resourceInstances.size(), {}), RPS_ERROR_OUT_OF_MEMORY);

            const uint32_t numRes = uint32_t(resourceInstances.size());

            uint32_t totalSubResources = 0;

            for (uint32_t iRes = 0; iRes < numRes; iRes++)
            {
                const auto& resInstance = resourceInstances[iRes];
                if (!resInstance.IsTemporalParent() && (resInstance.numSubResources > 1))
                {
                    SubresourceState* pSubResStates = m_subResStates.grow(resInstance.numSubResources, {});
                    RPS_RETURN_ERROR_IF(!pSubResStates, RPS_ERROR_OUT_OF_MEMORY);

                    pSubResStates[0].range = resInstance.fullSubresourceRange;

                    // Init size to 0. SubResStates grows as subresource ranges are accessed:
                    m_resourceStates[iRes].subResStates.SetRange(totalSubResources, 1);
                    totalSubResources += resInstance.numSubResources;
                }
            }

            return RPS_OK;
        }

        bool IsFullResource(const SubresourceRangePacked& range, const ResourceInstance& resourceDesc)
        {
            return range.GetNumSubresources() == resourceDesc.numSubResources;
        }

        RpsResult CloneReferenceList(const Span<uint32_t> src, Span<uint32_t>& dst)
        {
            m_nodeRefListPool.alloc_span(dst, src.size());

            if (!src.empty())
            {
                if (dst.GetEnd() > m_nodeRefLists.size())
                {
                    RPS_CHECK_ALLOC(m_nodeRefLists.resize(dst.GetEnd()));
                }

                auto srcRange = src.Get(m_nodeRefLists);
                auto dstRange = dst.Get(m_nodeRefLists);
                std::copy(srcRange.begin(), srcRange.end(), dstRange.begin());
            }

            return RPS_OK;
        }

        RpsResult CloneTransition(Graph& graph, uint32_t srcTransition, uint32_t& dstTransition)
        {
            const TransitionInfo& srcTrans        = m_transitions[srcTransition];
            int32_t               newTransitionId = int32_t(m_transitions.size());

            const NodeId newTransNodeId = graph.CloneNode(srcTrans.nodeId, -newTransitionId);
            RPS_RETURN_ERROR_IF(newTransNodeId == RPS_INDEX_NONE_U32, RPS_ERROR_UNSPECIFIED);

            TransitionInfo* pNewTrans = m_transitions.grow(1);
            RPS_RETURN_ERROR_IF(!pNewTrans, RPS_ERROR_OUT_OF_MEMORY);

            *pNewTrans        = srcTrans;
            pNewTrans->nodeId = newTransNodeId;

            dstTransition = newTransitionId;

            return RPS_OK;
        }

        RpsResult AddNewTransition(Graph&                        graph,
                                   NodeId                        currNodeId,
                                   AccessState&                  accessorInfo,
                                   const RpsAccessAttr&          newAccess,
                                   uint32_t                      resourceId,
                                   const SubresourceRangePacked& range)
        {
            uint32_t newTransitionId = uint32_t(m_transitions.size());

            TransitionInfo* pNewTrans = m_transitions.grow(1);
            RPS_RETURN_ERROR_IF(!pNewTrans, RPS_ERROR_OUT_OF_MEMORY);

            const NodeId newTransNodeId = graph.AddNode(-int32_t(newTransitionId));

            pNewTrans->access.resourceId = resourceId;
            pNewTrans->access.range      = range;
            pNewTrans->access.access     = newAccess;
            pNewTrans->nodeId            = newTransNodeId;
            pNewTrans->prevTransition    = accessorInfo.lastTransition;

            Node*       pNewTransNode   = graph.GetNode(newTransNodeId);
            const Node* pCurrNode       = graph.GetNode(currNodeId);
            pNewTransNode->subgraph     = pCurrNode->subgraph;
            pNewTransNode->barrierScope = pCurrNode->barrierScope;

            // Add edges for existing accessors -> new transition node
            // TODO: If previous accessors are ordered, only add edge from the last node
            for (auto accessorNodeId : accessorInfo.accessorNodes.Get(m_nodeRefLists))
            {
                graph.AddEdge(accessorNodeId, newTransNodeId);
                // RPS_V_RETURN(result);
            }

            m_nodeRefListPool.free_span(accessorInfo.accessorNodes);
            accessorInfo.lastTransition = newTransitionId;

            return RPS_OK;
        }

        RpsResult ProcessTransition(Graph&                        graph,
                                    NodeId                        currNodeId,
                                    AccessState&                  accessState,
                                    const RpsAccessAttr&          newAccess,
                                    uint32_t                      resourceId,
                                    const SubresourceRangePacked& range)
        {
            const RpsAccessAttr beforeAccess = m_transitions[accessState.lastTransition].access.access;

            AccessTransitionInfo transitionInfo;

            const NodeId lastAccessor = accessState.accessorNodes.empty()
                                            ? RPS_INDEX_NONE_U32
                                            : accessState.accessorNodes.Get(m_nodeRefLists).back();

            const bool bAccessedByCurrentNode = (lastAccessor == currNodeId);

            if ((accessState.lastTransition == RenderGraph::INVALID_TRANSITION) ||
                NeedTransition(beforeAccess, newAccess, transitionInfo, bAccessedByCurrentNode))
            {
                // New Transition

                if (!bAccessedByCurrentNode)
                {
                    AddNewTransition(graph, currNodeId, accessState, newAccess, resourceId, range);
                }
                else
                {
                    // This normally indicates an application error where the same subresource has multiple incompatible accesses on the same node:

                    // If current node has both write and read access, write takes precedence.
                    // TODO: Add a flag to control if to allow R/W accesses to coexist at all.
                    if (newAccess.accessFlags & RPS_ACCESS_ALL_GPU_WRITE)
                    {
                        RPS_TODO("Handle before/after & overlapped access with new write access.");
                    }
                }
            }
            else
            {
                // No Transition
                if (transitionInfo.bMergedAccessStates)
                {
                    // We should have splitted (cloned) the previou transition node if any, and patched its range to the current one.
                    RPS_ASSERT(m_transitions[accessState.lastTransition].access.range == range);
                    m_transitions[accessState.lastTransition].access.access = transitionInfo.mergedAccess;
                }

                if (transitionInfo.bKeepOrdering && !accessState.accessorNodes.empty())
                {
                    NodeId lastAccessor = accessState.accessorNodes.Get(m_nodeRefLists).back();

                    // TODO: Validate all existing accessors are in order.
                    if (lastAccessor != currNodeId)
                    {
                        graph.AddEdge(lastAccessor, currNodeId);
                    }
                }
            }

            graph.AddEdge(m_transitions[accessState.lastTransition].nodeId, currNodeId);
            AddNodeReference(accessState.accessorNodes, currNodeId);

            return RPS_OK;
        }

        void AddNodeReference(Span<NodeId>& refNodes, NodeId newRef)
        {
            if (refNodes.empty() || (refNodes.Get(m_nodeRefLists).back() != newRef))
            {
                m_nodeRefListPool.push_to_span(refNodes, newRef);
            }
        }

        void FilterAccessByRange(RpsAccessAttr& access, const SubresourceRangePacked& range)
        {
            if (access.accessFlags & RPS_ACCESS_DEPTH_STENCIL)
            {
                const auto aspectUsage = m_pRuntimeDevice->GetImageAspectUsages(range.aspectMask);

                if (!(aspectUsage & RPS_IMAGE_ASPECT_DEPTH))
                {
                    access.accessFlags &= ~(RPS_ACCESS_DEPTH_READ_BIT | RPS_ACCESS_DEPTH_WRITE_BIT);

                    // Any SRV states should be for depth plane, removing:
                    if (access.accessFlags & RPS_ACCESS_STENCIL_WRITE_BIT)
                    {
                        access.accessFlags &= ~RPS_ACCESS_SHADER_RESOURCE_BIT;
                        access.accessStages = RPS_SHADER_STAGE_NONE;
                    }
                }

                if (!(aspectUsage & RPS_IMAGE_ASPECT_STENCIL))
                {
                    access.accessFlags &= ~(RPS_ACCESS_STENCIL_READ_BIT | RPS_ACCESS_STENCIL_WRITE_BIT);

                    // Any SRV states should be for stencil plane, removing:
                    if (access.accessFlags & RPS_ACCESS_DEPTH_WRITE_BIT)
                    {
                        access.accessFlags &= ~RPS_ACCESS_SHADER_RESOURCE_BIT;
                        access.accessStages = RPS_SHADER_STAGE_NONE;
                    }
                }
            }
        }

        static bool IsReadOnly(const RpsAccessAttr& access)
        {
            return !rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_ALL_GPU_WRITE | RPS_ACCESS_CPU_WRITE_BIT);
        }

        inline bool NeedTransition(const RpsAccessAttr&  before,
                                   const RpsAccessAttr&  after,
                                   AccessTransitionInfo& accessTransInfo,
                                   bool                  bAccessedByCurrentNode) const
        {
            // Coarse filter before calling runtime dependent CalculateAccessTransition:
            // TODO: Get this from runtime device
            constexpr RpsAccessFlags c_runtimeDependentAccessFlags = RPS_ACCESS_COPY_SRC_BIT |
                                                                     RPS_ACCESS_COPY_DEST_BIT | RPS_ACCESS_CLEAR_BIT |
                                                                     RPS_ACCESS_RENDER_PASS | RPS_ACCESS_DEPTH_STENCIL;

            accessTransInfo = {};

            if (rpsAnyBitsSet((before.accessFlags | after.accessFlags), c_runtimeDependentAccessFlags) &&
                m_pRuntimeDevice->CalculateAccessTransition(before, after, accessTransInfo))
            {
                return accessTransInfo.bTransition;
            }

            if ((IsReadOnly(before) && IsReadOnly(after)) ||
                (bAccessedByCurrentNode && rpsAllBitsSet(before.accessFlags, after.accessFlags)))
            {
                accessTransInfo.bMergedAccessStates = before != after;
                accessTransInfo.mergedAccess        = before | after;

                return false;
            }
            else
            {
                const bool relaxOrdering =
                    rpsAnyBitsSet(before.accessFlags & after.accessFlags, RPS_ACCESS_RELAXED_ORDER_BIT);

                accessTransInfo.bKeepOrdering = !relaxOrdering;

                const bool isUav = rpsAnyBitsSet(before.accessFlags, RPS_ACCESS_UNORDERED_ACCESS_BIT);

                // By default UAV to UAV access needs a sync, unless RELAXED_ORDER is specified on both access.
                accessTransInfo.bTransition = (before != after) || (isUav && !relaxOrdering);

                return accessTransInfo.bTransition;
            }
        }

    private:
        RenderGraph&   m_renderGraph;
        RuntimeDevice* m_pRuntimeDevice = nullptr;

        ArenaVector<TransitionInfo>& m_transitions;

        ArenaVector<ResourceState>    m_resourceStates;
        ArenaVector<SubresourceState> m_subResStates;

        ArenaVector<NodeId>                   m_nodeRefLists;
        SpanPool<NodeId, ArenaVector<NodeId>> m_nodeRefListPool;

        uint32_t m_transitionCountWatermark = 0;
    };
}

#endif  //RPS_ACCESS_DAG_BUILD_HPP
