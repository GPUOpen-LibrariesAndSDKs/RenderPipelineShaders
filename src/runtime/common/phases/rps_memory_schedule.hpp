// Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the AMD INTERNAL EVALUATION LICENSE.
//
// See file LICENSE.RTF for full license details.

#ifndef _RPS_MEMORY_SCHEDULE_H_
#define _RPS_MEMORY_SCHEDULE_H_

#include "runtime/common/rps_render_graph.hpp"

#include <algorithm>
#include <numeric>

namespace rps
{
    class MemorySchedulePhase : public IRenderGraphPhase
    {
        RenderGraphUpdateContext* m_pContext = nullptr;

    public:
        MemorySchedulePhase(RenderGraph& renderGraph)
        {
        }

        virtual RpsResult Run(RenderGraphUpdateContext& context) override final
        {
            m_pContext = &context;

            CollectResourceLifeTimes(context);

            RPS_V_RETURN(CalculateResourcePlacements(context));

            RPS_V_RETURN(CalculateResourceAliasing(context));

            return RPS_OK;
        }

    private:

        void CollectResourceLifeTimes(RenderGraphUpdateContext& context)
        {
            auto&       resourceInstances = context.renderGraph.GetResourceInstances();
            const auto& transitions       = context.renderGraph.GetTransitions();
            const auto  nodes             = context.renderGraph.GetGraph().GetNodes();
            const auto  runtimeCmds       = context.renderGraph.GetRuntimeCmdInfos().range_all();

            const uint32_t lastCmdId = runtimeCmds.empty() ? 0 : uint32_t(runtimeCmds.size() - 1);

            for (auto& resInst : resourceInstances)
            {
                const bool bIsPersistent =
                    resInst.isExternal || rpsAnyBitsSet(resInst.desc.flags, RPS_RESOURCE_FLAG_PERSISTENT_BIT);

                resInst.lifetimeBegin = bIsPersistent ? 0 : UINT32_MAX;
                resInst.lifetimeEnd   = bIsPersistent ? lastCmdId : 0;
            }

            if (runtimeCmds.empty())
            {
                return;
            }

            auto fnUpdateAccessRange = [&](uint32_t resourceIndex, uint32_t runtimeCmdIdx) {
                auto& resInst         = resourceInstances[resourceIndex];
                resInst.lifetimeBegin = rpsMin(resInst.lifetimeBegin, runtimeCmdIdx);
                resInst.lifetimeEnd   = rpsMax(resInst.lifetimeEnd, runtimeCmdIdx);
            };

            for (uint32_t runtimeCmdIdx = 1; runtimeCmdIdx < (runtimeCmds.size() - 1); runtimeCmdIdx++)
            {
                const auto& runtimeCmd = runtimeCmds[runtimeCmdIdx];
                if (runtimeCmd.isTransition)
                {
                    const auto& transitionInfo = transitions[runtimeCmd.GetTransitionId()];
                    fnUpdateAccessRange(transitionInfo.access.resourceId, runtimeCmdIdx);
                }
                else
                {
                    auto paramInfos = context.renderGraph.GetCmdAccesses(runtimeCmd.GetCmdId());

                    for (auto& paramInfo : paramInfos)
                    {
                        if (paramInfo.resourceId != RPS_RESOURCE_ID_INVALID)
                        {
                            fnUpdateAccessRange(paramInfo.resourceId, runtimeCmdIdx);
                        }
                    }
                }
            }
        }

        void InsertToSortedAllocationList(ArenaVector<uint32_t>&          allocatedIndices,
                                          uint32_t                        resIndex,
                                          ConstArrayRef<ResourceInstance> resources)
        {
            auto upperBound =
                std::upper_bound(allocatedIndices.begin(), allocatedIndices.end(), resIndex, [&](auto a, auto b) {
                    auto& resA = resources[a];
                    auto& resB = resources[b];

                    if (resA.allocPlacement.heapId < resB.allocPlacement.heapId)
                        return true;
                    else if (resA.allocPlacement.heapId > resB.allocPlacement.heapId)
                        return false;
                    else if (resA.allocPlacement.offset < resB.allocPlacement.offset)
                        return true;
                    else if (resA.allocPlacement.offset > resB.allocPlacement.offset)
                        return false;
                    else if (resA.allocPlacement.offset + resA.allocRequirement.size <
                             resB.allocPlacement.offset + resB.allocRequirement.size)
                        return true;
                    else
                        return false;
                });

            allocatedIndices.insert(upperBound - allocatedIndices.begin(), resIndex);
        }

        RpsResult CalculateResourcePlacements(RenderGraphUpdateContext& context)
        {
            ArenaCheckPoint arenaCheckpoint{context.scratchArena};

            auto& heaps = context.renderGraph.GetHeapInfos();

            const bool bUseAliasing = !rpsAnyBitsSet(context.renderGraph.GetCreateInfo().renderGraphFlags,
                                                     RPS_RENDER_GRAPH_NO_GPU_MEMORY_ALIASING);

            auto resourceInstances = context.renderGraph.GetResourceInstances().range_all();

            ArenaVector<uint32_t> sortedResourceIndices(&context.scratchArena);
            sortedResourceIndices.reserve(resourceInstances.size());

            for (uint32_t iRes = 0; iRes < resourceInstances.size(); iRes++)
            {
                auto& resInst = resourceInstances[iRes];

                if ((!resInst.isExternal) && (resInst.allocRequirement.size > 0) &&
                    (resInst.lifetimeBegin <= resInst.lifetimeEnd))
                {
                    sortedResourceIndices.push_back(iRes);
                }
            }

            // Reset currently used size mark.
            // TODO: Allow external allocations to occuply spaces.
            for (auto& heap : heaps)
            {
                heap.usedSize = 0;
            }

            // TODO: Precompute comparison key
            std::sort(sortedResourceIndices.begin(), sortedResourceIndices.end(), [&](uint32_t a, uint32_t b) {
                const auto& resA = resourceInstances[a];
                const auto& resB = resourceInstances[b];

                // memory type
                uint32_t memTypeA = resA.allocRequirement.memoryTypeIndex;
                uint32_t memTypeB = resB.allocRequirement.memoryTypeIndex;

                if (memTypeA < memTypeB)
                    return true;
                else if (memTypeA > memTypeB)
                    return false;

                // For the same heap type, put pre-allocated (reused) resources ahead,
                // so they occupy the existing allocation before we try allocate anything else.
                if (!resA.isPendingCreate && resB.isPendingCreate)
                    return true;
                else if (resA.isPendingCreate && !resB.isPendingCreate)
                    return false;

                uint64_t sizeA =
                    rpsAlignUp(resA.allocRequirement.size, uint64_t(rpsMax(1u, resA.allocRequirement.alignment)));
                uint64_t sizeB =
                    rpsAlignUp(resB.allocRequirement.size, uint64_t(rpsMax(1u, resB.allocRequirement.alignment)));

                // Sort size in decreasing order
                if (sizeA > sizeB)
                    return true;
                else if (sizeA < sizeB)
                    return false;

                uint32_t cmdIndexA = resA.lifetimeBegin;
                uint32_t cmdIndexB = resB.lifetimeBegin;

                return (cmdIndexA < cmdIndexB);
            });

            // Ordered index list by (heap, offset, end), cleared when switching heap type
            ArenaVector<uint32_t> allocatedIndices(&context.scratchArena);
            allocatedIndices.reserve(sortedResourceIndices.size());

            bool bLastResPreallocated = false;

            // For each resource in sorted list, try allocate in a 2d rectangle ( width = cmd index span, height = size )
            uint32_t currHeapMemType = UINT32_MAX;
            for (size_t iIndex = 0, endIndex = sortedResourceIndices.size(); iIndex < endIndex; iIndex++)
            {
                const uint32_t iRes    = sortedResourceIndices[iIndex];
                auto&          currRes = resourceInstances[iRes];

                RPS_ASSERT(currRes.allocRequirement.size > 0);

                // Switch heap if heap type changes
                if (currHeapMemType != currRes.allocRequirement.memoryTypeIndex)
                {
                    currHeapMemType = currRes.allocRequirement.memoryTypeIndex;
                    allocatedIndices.clear();
                }

                // Existing allocation should have a valid resource handle and heap placement.
                RPS_ASSERT((!currRes.isPendingCreate) ==
                           (currRes.hRuntimeResource && (currRes.allocPlacement.heapId != RPS_INDEX_NONE_U32)));

                // Insert existing allocations into the allocatedIndices list and update the heap infos accordingly,
                // in order to hold their placements.
                if (!currRes.isPendingCreate)
                {
                    HeapInfo* pHeap = &heaps[currRes.allocPlacement.heapId];

                    RPS_ASSERT(pHeap->memTypeIndex == currRes.allocRequirement.memoryTypeIndex);
                    RPS_ASSERT(pHeap->alignment >= currRes.allocRequirement.alignment);
                    RPS_ASSERT(pHeap->size >= (currRes.allocPlacement.offset + currRes.allocRequirement.size));
                    RPS_ASSERT(bLastResPreallocated || allocatedIndices.empty());

                    InsertToSortedAllocationList(allocatedIndices, iRes, resourceInstances);

                    pHeap->usedSize =
                        rpsMax(pHeap->usedSize, currRes.allocPlacement.offset + currRes.allocRequirement.size);
                    pHeap->maxUsedSize = rpsMax(pHeap->maxUsedSize, pHeap->usedSize);

                    bLastResPreallocated = true;

                    continue;
                }

                bLastResPreallocated = false;

                // Search for a valid range, for each existing resource allocated with current heap type:
                uint32_t currHeapIndex = UINT32_MAX;

                uint64_t         prevRangeEndAligned = 0;
                uint64_t         fitness             = UINT64_MAX;  // Smaller is better
                RpsHeapPlacement rangeCandidate;

                // TODO: Allow choose first fit / best fit

                if (bUseAliasing)
                {
                    // Filter allocated resources by command index range
                    for (uint32_t iAllocated = 0, numAllocated = uint32_t(allocatedIndices.size());
                         iAllocated < numAllocated;
                         iAllocated++)
                    {
                        const uint32_t iResAllocated = allocatedIndices[iAllocated];
                        const auto&    allocatedRes  = resourceInstances[iResAllocated];

                        RPS_ASSERT(allocatedRes.allocRequirement.memoryTypeIndex == currHeapMemType);

                        // Before moving on to new heap, check any space left in current heap.
                        if (allocatedRes.allocPlacement.heapId != currHeapIndex)
                        {
                            if (currHeapIndex != UINT32_MAX)
                            {
                                const auto& currHeap = heaps[currHeapIndex];

                                // Check any space left in previous heap from last allocation to its top
                                CheckReusableSpaceInHeap(prevRangeEndAligned,
                                                         currHeap.size,
                                                         currRes.allocRequirement,
                                                         currHeapIndex,
                                                         &fitness,
                                                         &rangeCandidate);

                                // Size fits perfectly, no more search
                                if (fitness == 0)
                                    break;
                            }

                            // Switch to next heap, reset states
                            prevRangeEndAligned = 0;
                            currHeapIndex       = allocatedRes.allocPlacement.heapId;
                        }

                        // If lifetimes overlap:
                        if ((allocatedRes.lifetimeBegin <= currRes.lifetimeEnd) &&
                            (currRes.lifetimeBegin <= allocatedRes.lifetimeEnd))
                        {
                            // Only check if there is a gap between previous range end and current allocated resource start
                            if (prevRangeEndAligned < allocatedRes.allocPlacement.offset)
                            {
                                const auto& currHeap = heaps[currHeapIndex];

                                CheckReusableSpaceInHeap(prevRangeEndAligned,
                                                         allocatedRes.allocPlacement.offset,
                                                         currRes.allocRequirement,
                                                         currHeapIndex,
                                                         &fitness,
                                                         &rangeCandidate);

                                // Size fit perfectly, no more search
                                if (fitness == 0)
                                    break;
                            }

                            const uint64_t allocatedEnd =
                                allocatedRes.allocPlacement.offset + allocatedRes.allocRequirement.size;

                            prevRangeEndAligned =
                                rpsMax(prevRangeEndAligned,
                                       rpsAlignUp(allocatedEnd, uint64_t(currRes.allocRequirement.alignment)));
                        }
                    }
                }
                else
                {
                    // Not using aliasing. Check last allocation for current heap
                    if (!allocatedIndices.empty())
                    {
                        // Only check last allocation for now. Can probably look through all allocated heaps with same type and scrape any space from top to limit
                        const auto& allocatedRes = resourceInstances[allocatedIndices.back()];

                        RPS_ASSERT(allocatedRes.allocRequirement.memoryTypeIndex == currHeapMemType);

                        // Before moving on to new heap, check any space left in current heap.
                        currHeapIndex = allocatedRes.allocPlacement.heapId;
                        RPS_ASSERT(prevRangeEndAligned == 0);
                    }

                    if (currHeapIndex != UINT32_MAX)
                    {
                        prevRangeEndAligned = rpsMax(
                            prevRangeEndAligned,
                            rpsAlignUp(heaps[currHeapIndex].usedSize, uint64_t(currRes.allocRequirement.alignment)));
                    }
                }

                // Did not find valid space, try top of heap
                if ((fitness == UINT64_MAX) && (currHeapIndex != UINT32_MAX))
                {
                    // Check any space left in previous heap from last allocation to its top
                    const auto& currHeap = heaps[currHeapIndex];

                    CheckReusableSpaceInHeap(prevRangeEndAligned,
                                             currHeap.size,
                                             currRes.allocRequirement,
                                             currHeapIndex,
                                             &fitness,
                                             &rangeCandidate);
                }

                // Did not find valid space, try grab an unused existing heap / create a new heap
                if (fitness == UINT64_MAX)
                {
                    const uint32_t newHeapIdx = FindOrCreateFreeHeap(
                        currHeapMemType, currRes.allocRequirement.size, currRes.allocRequirement.alignment);

                    RPS_ASSERT(newHeapIdx != UINT32_MAX);  // TODO

                    prevRangeEndAligned = 0;
                    currHeapIndex       = newHeapIdx;

                    const auto& currHeap = heaps[currHeapIndex];

                    CheckReusableSpaceInHeap(prevRangeEndAligned,
                                             currHeap.size,
                                             currRes.allocRequirement,
                                             currHeapIndex,
                                             &fitness,
                                             &rangeCandidate);
                }

                RPS_RETURN_ERROR_IF(fitness == UINT64_MAX, RPS_ERROR_OUT_OF_MEMORY);

                auto& selectedHeap = heaps[rangeCandidate.heapId];

                // Adjust alignment if RtHeap is not created yet.
                if (!selectedHeap.hRuntimeHeap)
                {
                    selectedHeap.alignment = rpsMax(currRes.allocRequirement.alignment, selectedHeap.alignment);
                }

                RPS_ASSERT(selectedHeap.alignment >= currRes.allocRequirement.alignment);

                currRes.allocPlacement = rangeCandidate;

                // Increase heap top if needed
                selectedHeap.usedSize =
                    rpsMax(selectedHeap.usedSize, rangeCandidate.offset + currRes.allocRequirement.size);
                selectedHeap.maxUsedSize = rpsMax(selectedHeap.maxUsedSize, selectedHeap.usedSize);

                // Insert current range to allocatedIndices sorted
                InsertToSortedAllocationList(allocatedIndices, iRes, resourceInstances);
            }

            return RPS_OK;
        }

        void CheckReusableSpaceInHeap(uint64_t                      spaceBegin,
                                      uint64_t                      spaceEnd,
                                      const RpsGpuMemoryRequirement memRequirement,
                                      uint32_t                      heapIndex,
                                      uint64_t*                     pFitness,
                                      RpsHeapPlacement*             pCandidate)
        {
            auto& heaps = m_pContext->renderGraph.GetHeapInfos();

            auto& heapInfo = heaps[heapIndex];

            if ((heapInfo.hRuntimeHeap) && (heapInfo.alignment < memRequirement.alignment))
            {
                // Fail if runtime heap already exists but heap alignment is smaller than resource required alignment.
                return;
            }

            uint64_t newRangeEnd = spaceBegin + memRequirement.size;

            // Check if requiredSize can fit the space:
            if (newRangeEnd <= spaceEnd)
            {
                uint64_t newFitness = spaceEnd - newRangeEnd;

                if (newFitness < *pFitness)
                {
                    pCandidate->heapId = heapIndex;
                    pCandidate->offset    = spaceBegin;

                    *pFitness = newFitness;
                }
            }
        }

        uint32_t FindOrCreateFreeHeap(uint32_t memoryTypeIndex, uint64_t minSize, uint32_t minAlignment)
        {
            auto& heaps = m_pContext->renderGraph.GetHeapInfos();

            for (size_t heapIdx = 0, numHeaps = heaps.size(); heapIdx < numHeaps; heapIdx++)
            {
                const auto& heap = heaps[heapIdx];
                // TODO: If allocation minSize > default heap size, we allocate a heap just fit the size.
                // Need to make sure this heap is not grabbed by other smaller allocations, or check for a better solution.
                if ((heap.memTypeIndex == memoryTypeIndex) && (heap.usedSize == 0) && (minSize <= heap.size) &&
                    (minAlignment <= heap.alignment))
                {
                    RPS_ASSERT(heap.hRuntimeHeap);
                    return uint32_t(heapIdx);
                }
            }

            return AddNewHeap(memoryTypeIndex, minSize, minAlignment);
        }

        uint32_t AddNewHeap(uint32_t memoryTypeIndex, uint64_t minSize, uint32_t alignment)
        {
            auto& heaps = m_pContext->renderGraph.GetHeapInfos();

            uint32_t newHeapIdx = 0;

            for (const size_t numHeaps = heaps.size(); newHeapIdx < numHeaps; newHeapIdx++)
            {
                if (heaps[newHeapIdx].memTypeIndex == UINT32_MAX)
                {
                    break;
                }
            }

            if (newHeapIdx == heaps.size())
            {
                heaps.emplace_back();
            }

            const auto& memTypeInfo = m_pContext->renderGraph.GetMemoryTypes()[memoryTypeIndex];

            auto& newHeap = heaps[newHeapIdx];
            newHeap       = {};

            newHeap.memTypeIndex = memoryTypeIndex;
            newHeap.index        = newHeapIdx;
            newHeap.size         = UINT64_MAX;
            newHeap.alignment    = rpsMax(alignment, memTypeInfo.minAlignment);

            if (memTypeInfo.defaultHeapSize > 0)
            {
                newHeap.size = rpsMax(minSize, memTypeInfo.defaultHeapSize);
            }

            return newHeapIdx;
        }

        struct HeapRangeUsage
        {
            uint64_t size;
            uint64_t heapOffset;
            uint32_t heapIndex;
            uint32_t resourceIndex;
        };

        // Foreach command, update resource usage ranges & find alias
        RpsResult CalculateResourceAliasing(RenderGraphUpdateContext& context)
        {
            const bool bUseAliasing = !rpsAnyBitsSet(context.renderGraph.GetCreateInfo().renderGraphFlags,
                                                     RPS_RENDER_GRAPH_NO_GPU_MEMORY_ALIASING);

            auto resourceInstances = context.renderGraph.GetResourceInstances().range_all();
            auto scheduledCmds     = context.renderGraph.GetRuntimeCmdInfos().range_all();

            RPS_RETURN_OK_IF(resourceInstances.empty() || scheduledCmds.empty());

            ArenaCheckPoint arenaCheckpoint{context.scratchArena};

            ArenaVector<HeapRangeUsage> heapRangeUsages(&context.scratchArena);
            heapRangeUsages.reserve(resourceInstances.size());

            ArenaBitVector<> aliasingSrcBitMask(&context.scratchArena);
            aliasingSrcBitMask.Resize(uint32_t(resourceInstances.size()));
            aliasingSrcBitMask.Fill(false);

            ArenaVector<uint32_t> pendingAliasingSrcs(&context.scratchArena);
            pendingAliasingSrcs.reserve(resourceInstances.size());

            // TODO: Only check the resources referenced by current cmd? Do all resources starting at a cmd referenced by the cmd?
            ArenaVector<uint32_t> resourceIdxSortedByLifetimeStart(resourceInstances.size(), &context.scratchArena);
            std::iota(resourceIdxSortedByLifetimeStart.begin(), resourceIdxSortedByLifetimeStart.end(), 0);
            std::sort(resourceIdxSortedByLifetimeStart.begin(),
                      resourceIdxSortedByLifetimeStart.end(),
                      [&](uint32_t i, uint32_t j) {
                          return resourceInstances[i].lifetimeBegin < resourceInstances[j].lifetimeBegin;
                      });

            auto& aliasingInfos = context.renderGraph.GetResourceAliasingInfos();
            aliasingInfos.clear();

            uint32_t numCmdExtraAliasingInfos = 0;

            if (bUseAliasing)
            {
                uint32_t resIdxSorted = 0;

                uint32_t numAliasingRes    = 0;
                uint32_t numDeactivatedRes = 0;

                for (uint32_t iCmd = 0, numNodes = uint32_t(scheduledCmds.size()); iCmd < numNodes; iCmd++)
                {
                    RuntimeCmdInfo& runtimeCmd = scheduledCmds[iCmd];
                    runtimeCmd.aliasingInfos.SetRange(uint32_t(aliasingInfos.size()), 0);

                    // For each resource starting at cmd, find overlapping with previous ranges
                    for (; (resIdxSorted < resourceIdxSortedByLifetimeStart.size()); resIdxSorted++)
                    {
                        const uint32_t resIndex = resourceIdxSortedByLifetimeStart[resIdxSorted];
                        auto&          resInst  = resourceInstances[resIndex];

                        // Move on to the next cmd
                        if (resInst.lifetimeBegin != iCmd)
                            break;

                        if (resInst.isExternal)
                            continue;

                        RPS_ASSERT((resInst.allocRequirement.size > 0) || (resInst.HasNoAccess()));
                        RPS_ASSERT(resInst.lifetimeBegin != UINT32_MAX);

                        RPS_ASSERT(runtimeCmd.isTransition);

                        HeapRangeUsage currentResourceRange;
                        currentResourceRange.heapIndex     = resInst.allocPlacement.heapId;
                        currentResourceRange.heapOffset    = resInst.allocPlacement.offset;
                        currentResourceRange.size          = resInst.allocRequirement.size;
                        currentResourceRange.resourceIndex = resIndex;

                        HeapRangeUsage complementParts[2];

                        uint32_t initialNumActiveRanges = uint32_t(heapRangeUsages.size());

                        resInst.isAliased = false;

                        for (uint32_t iRange = 0;
                             iRange < rpsMin(initialNumActiveRanges, uint32_t(heapRangeUsages.size()));
                             iRange++)
                        {
                            if (heapRangeUsages[iRange].heapIndex != currentResourceRange.heapIndex)
                            {
                                continue;
                            }

                            uint32_t clipResultMask = 0;

                            if (HeapRangeClip(
                                    &currentResourceRange, &heapRangeUsages[iRange], complementParts, clipResultMask))
                            {
                                const uint32_t currAliasingInfoIdx = uint32_t(aliasingInfos.size());

                                const uint32_t srcResourceIdx = heapRangeUsages[iRange].resourceIndex;

                                ResourceAliasingInfo* pAliasingInfo = aliasingInfos.grow(1);

                                pAliasingInfo->srcResourceIndex = srcResourceIdx;
                                pAliasingInfo->dstResourceIndex = resIndex;
                                pAliasingInfo->srcDeactivating  = RPS_FALSE;
                                pAliasingInfo->dstActivating    = RPS_FALSE;
                                // dstActivating Will be set on the last aliasing info where current resIndex is dst.

                                // First time seen src as aliasing src, deactivate
                                bool bFirstTimeAsSrc = !aliasingSrcBitMask.ExchangeBit(srcResourceIdx, true);
                                if (bFirstTimeAsSrc)
                                {
                                    pAliasingInfo->srcDeactivating = RPS_TRUE;
                                    numDeactivatedRes++;
                                }

                                auto& srcResInfo = resourceInstances[srcResourceIdx];

                                if (!srcResInfo.isAliased)
                                {
                                    // Src resource is not aliased yet. May need to initialize it before first access.
                                    pendingAliasingSrcs.push_back(srcResourceIdx);
                                    srcResInfo.isAliased = true;

                                    numAliasingRes++;
                                }

                                resInst.isAliased = true;

                                if (clipResultMask & 0x1)
                                {
                                    heapRangeUsages[iRange] = complementParts[0];

                                    if (clipResultMask & 0x2)
                                    {
                                        heapRangeUsages.push_back(complementParts[1]);
                                    }
                                }
                                else if (clipResultMask == 0x2)
                                {
                                    heapRangeUsages[iRange] = complementParts[1];
                                }
                                else  // fully overlap
                                {
                                    heapRangeUsages[iRange] = heapRangeUsages.back();
                                    heapRangeUsages.pop_back();

                                    iRange--;
                                }
                            }
                        }

                        heapRangeUsages.push_back(currentResourceRange);

                        if (resInst.isAliased)
                        {
                            RPS_ASSERT(!aliasingInfos.empty());
                            RPS_ASSERT(aliasingInfos.back().dstResourceIndex == resIndex);

                            aliasingInfos.back().dstActivating = RPS_TRUE;
                            numAliasingRes++;
                        }
                    }

                    // Update Cmd Aliasing info
                    runtimeCmd.aliasingInfos.SetCount(
                        uint32_t(aliasingInfos.size() - runtimeCmd.aliasingInfos.GetBegin()));

                    // TODO: Only need to iterate through transitions?
                    RPS_ASSERT(runtimeCmd.isTransition || runtimeCmd.aliasingInfos.empty());
                }

                RPS_ASSERT(numAliasingRes <= resourceInstances.size());
                RPS_ASSERT(numDeactivatedRes <= numAliasingRes);


                // Preamble:
                // For aliasing source-only resources. May need to initialize them before first access.
                // Adding to the first node's aliasing info list

                uint32_t preambleAliasingInfoOffset = uint32_t(aliasingInfos.size());

                for (auto pendingAliasingSrcRes : pendingAliasingSrcs)
                {
                    ResourceAliasingInfo* pPreambleAliasingInfo = aliasingInfos.grow(1);
                    pPreambleAliasingInfo->srcResourceIndex     = RPS_RESOURCE_ID_INVALID;
                    pPreambleAliasingInfo->dstResourceIndex     = pendingAliasingSrcRes;
                    pPreambleAliasingInfo->srcDeactivating      = RPS_FALSE;
                    pPreambleAliasingInfo->dstActivating        = RPS_TRUE;
                }

                RPS_ASSERT(scheduledCmds[0].cmdId == CMD_ID_PREAMBLE);
                scheduledCmds[0].aliasingInfos.SetRange(preambleAliasingInfoOffset,
                                                        uint32_t(pendingAliasingSrcs.size()));

                // Postamble:
                // Resources that are aliased but no successor, so havn't been deactivated at the end of frame.
                // Insert aliasingInfo at the end.
                // purely for generating DX12 transition barrier to raise them to RT/DS states for discard next frame.
                // Check per resource aliasing info, mark pre-discard and post-discard transitions

                uint32_t postambleAliasingInfoOffset = uint32_t(aliasingInfos.size());

                uint32_t numAliasingResCounted = 0;
                for (uint32_t iRes = 0, numRes = uint32_t(resourceInstances.size()); iRes < numRes; iRes++)
                {
                    const auto& resInst = resourceInstances[iRes];

                    // Persistent resource shouldn't be aliased.
                    RPS_ASSERT(
                        (resInst.isExternal || rpsAnyBitsSet(resInst.desc.flags, RPS_RESOURCE_FLAG_PERSISTENT_BIT))
                            ? !resInst.isAliased
                            : true);

                    if (resInst.lifetimeBegin == UINT32_MAX)
                    {
                        continue;
                    }

                    if (resInst.isAliased && !aliasingSrcBitMask.GetBit(iRes))
                    {
                        ResourceAliasingInfo* pFrameEndAliasingInfo = aliasingInfos.grow(1);
                        pFrameEndAliasingInfo->srcResourceIndex     = iRes;
                        pFrameEndAliasingInfo->dstResourceIndex     = RPS_RESOURCE_ID_INVALID;
                        pFrameEndAliasingInfo->srcDeactivating      = RPS_TRUE;
                        pFrameEndAliasingInfo->dstActivating        = RPS_FALSE;

                        numDeactivatedRes++;
                    }

                    numAliasingResCounted += resInst.isAliased ? 1 : 0;
                }

                RPS_ASSERT(scheduledCmds.back().cmdId == CMD_ID_POSTAMBLE);
                scheduledCmds.back().aliasingInfos.SetRange(
                    postambleAliasingInfoOffset, uint32_t(aliasingInfos.size() - postambleAliasingInfoOffset));

                RPS_ASSERT(numAliasingRes == numAliasingResCounted);
            }

            return RPS_OK;
        }

        // Clip rhs with lhs, returns if clipping (intersection) happened, and output a bit mask of complement ranges:
        // Bit 0: complements from rhs, that's less than lhs
        // Bit 1: complements from rhs, that's greater than lhs
        bool HeapRangeClip(const HeapRangeUsage* lhs,
                           const HeapRangeUsage* rhs,
                           HeapRangeUsage        rhsComplements[2],
                           uint32_t&             complementMask)
        {
            complementMask = 0;

            RPS_ASSERT(lhs->heapIndex == rhs->heapIndex);

            const uint64_t lhsEnd = (lhs->heapOffset + lhs->size);
            const uint64_t rhsEnd = (rhs->heapOffset + rhs->size);

            if ((lhsEnd > rhs->heapOffset) && (lhs->heapOffset < rhsEnd))
            {
                if (lhs->heapOffset > rhs->heapOffset)
                {
                    rhsComplements[0].heapIndex     = rhs->heapIndex;
                    rhsComplements[0].heapOffset    = rhs->heapOffset;
                    rhsComplements[0].size          = lhs->heapOffset - rhs->heapOffset;
                    rhsComplements[0].resourceIndex = rhs->resourceIndex;
                    complementMask |= 0x1;
                }

                if (lhsEnd < rhsEnd)
                {
                    rhsComplements[1].heapIndex     = lhs->heapIndex;
                    rhsComplements[1].heapOffset    = lhsEnd;
                    rhsComplements[1].size          = rhsEnd - lhsEnd;
                    rhsComplements[1].resourceIndex = rhs->resourceIndex;
                    complementMask |= 0x2;
                }

                return true;
            }

            return false;
        }
    };
}

#endif  //_RPS_MEMORY_SCHEDULE_H_
