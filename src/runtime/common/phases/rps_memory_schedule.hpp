// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_MEMORY_SCHEDULE_HPP
#define RPS_MEMORY_SCHEDULE_HPP

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

            const bool bUseAliasing = !rpsAnyBitsSet(context.renderGraph.GetCreateInfo().renderGraphFlags,
                                                     RPS_RENDER_GRAPH_NO_GPU_MEMORY_ALIASING);

            // TODO: Make sure LifetimeAnalysis phase has run before current phase.

            RPS_V_RETURN(CalculateResourcePlacements(context));

            // TODO: when implementing RPS_RENDER_GRAPH_NO_GPU_MEMORY_ALIASING as a dynamic flag
            // we need to remove the IF below and call CalculateResourceAliasing each frame to ensure
            // isAliased is always properly set for every resource instance.
            if (bUseAliasing)
            {
                RPS_V_RETURN(CalculateResourceAliasing(context));
            }
            else
            {
                context.renderGraph.GetResourceAliasingInfos().clear();
            }

            return RPS_OK;
        }

    private:

        struct AllocationIndexLessComparer
        {
            ConstArrayRef<ResourceInstance> resources;

            struct AllocInfo
            {
                RpsHeapPlacement        allocPlacement;
                RpsGpuMemoryRequirement allocRequirement;
            };

            template <typename TAllocInfo>
            bool operator()(const TAllocInfo& a, uint32_t idxB) const
            {
                const auto& resB = resources[idxB];

                if (a.allocPlacement.heapId < resB.allocPlacement.heapId)
                    return true;
                else if (a.allocPlacement.heapId > resB.allocPlacement.heapId)
                    return false;
                else if (a.allocPlacement.offset < resB.allocPlacement.offset)
                    return true;
                else if (a.allocPlacement.offset > resB.allocPlacement.offset)
                    return false;
                else if (a.allocRequirement.size < resB.allocRequirement.size)
                    return true;

                return false;
            }

            bool operator()(uint32_t idxA, uint32_t idxB) const
            {
                auto& resA = resources[idxA];
                return (*this)(resA, idxB);
            }
        };

        bool InsertPreAllocatedResourceToSortedAllocationList(const HeapInfo&                 currHeap,
                                                              ArenaVector<uint32_t>&          allocatedIndices,
                                                              uint32_t                        resIndex,
                                                              ConstArrayRef<ResourceInstance> resources)
        {
            const auto& currRes = resources[resIndex];

            auto& context = *m_pContext;

            const bool bUseAliasing = !rpsAnyBitsSet(context.renderGraph.GetCreateInfo().renderGraphFlags,
                                                     RPS_RENDER_GRAPH_NO_GPU_MEMORY_ALIASING);

            RPS_ASSERT(!resources[resIndex].isPendingCreate);
            RPS_ASSERT(currHeap.memTypeIndex == currRes.allocRequirement.memoryTypeIndex);
            RPS_ASSERT(currHeap.alignment >= currRes.allocRequirement.alignment);
            RPS_ASSERT(currHeap.size >= (currRes.allocPlacement.offset + currRes.allocRequirement.size));

            if (currHeap.usedSize <= currRes.allocPlacement.offset)
            {
                // Current resource offset is higher than occupied range in the heap,
                // no need to check overlap against existing allocations.
                InsertToSortedAllocationList(allocatedIndices, resIndex, resources);

                return true;
            }

            const uint64_t currResPlacementEnd = currRes.allocPlacement.offset + currRes.allocRequirement.size;

            const AllocationIndexLessComparer comparer{resources};

            AllocationIndexLessComparer::AllocInfo checkRangeBegin = {{currRes.allocPlacement.heapId, 0}, {}};
            AllocationIndexLessComparer::AllocInfo checkRangeEnd   = {
                {currRes.allocPlacement.heapId, currResPlacementEnd}, {}};

            auto checkRangeBeginIter =
                std::upper_bound(allocatedIndices.begin(), allocatedIndices.end(), checkRangeBegin, comparer);
            auto checkRangeEndIter =
                std::upper_bound(checkRangeBeginIter, allocatedIndices.end(), checkRangeEnd, comparer);

            for (auto iter = checkRangeBeginIter; iter < checkRangeEndIter; ++iter)
            {
                const auto&    allocatedRes = resources[*iter];
                const uint64_t allocatedResPlacementEnd =
                    (allocatedRes.allocPlacement.offset + allocatedRes.allocRequirement.size);

                RPS_ASSERT(allocatedRes.allocPlacement.heapId == currRes.allocPlacement.heapId);
                RPS_ASSERT(allocatedRes.allocPlacement.offset < currResPlacementEnd);

                // it is strictly not allowed for any two resources to ever overlap both in lifetime and heap
                // placement.
                //
                // dynamic render graphs can cause 2d rect overlapping. overlap can occur when:
                // - runtime cmd lifetimes change from previous graph.
                // - a resource becomes temporarily unused (still declared) and a new allocation in the interim overlaps prev heap region.
                const bool bLifetimesOverlap = bUseAliasing ? ((allocatedRes.lifetimeBegin <= currRes.lifetimeEnd) &&
                                                               (currRes.lifetimeBegin <= allocatedRes.lifetimeEnd))
                                                            : true;
                if ((currRes.allocPlacement.offset < allocatedResPlacementEnd) && bLifetimesOverlap)
                {
                    return false;
                }
            }

            auto insertAt = std::upper_bound(checkRangeBeginIter, checkRangeEndIter, resIndex, comparer);

            RPS_ASSERT((insertAt == allocatedIndices.begin()) || !comparer(resIndex, *(insertAt - 1)));
            RPS_ASSERT((insertAt == allocatedIndices.end()) || comparer(resIndex, *insertAt));

            allocatedIndices.insert(insertAt - allocatedIndices.begin(), resIndex);

            return true;
        }

        void InsertToSortedAllocationList(ArenaVector<uint32_t>&          allocatedIndices,
                                          uint32_t                        resIndex,
                                          ConstArrayRef<ResourceInstance> resources)
        {
            auto upperBound = std::upper_bound(
                allocatedIndices.begin(), allocatedIndices.end(), resIndex, AllocationIndexLessComparer{resources});

            allocatedIndices.insert(upperBound - allocatedIndices.begin(), resIndex);
        }

        RpsResult CalculateResourcePlacements(RenderGraphUpdateContext& context)
        {
            ArenaCheckPoint arenaCheckpoint{context.scratchArena};

            auto& heaps = context.renderGraph.GetHeapInfos();

            // TODO: to support a dynamic version of this flag, need to clear all allocPlacement for every res inst.
            const bool bUseAliasing = !rpsAnyBitsSet(context.renderGraph.GetCreateInfo().renderGraphFlags,
                                                     RPS_RENDER_GRAPH_NO_GPU_MEMORY_ALIASING);

            auto resourceInstances = context.renderGraph.GetResourceInstances().range_all();

            ArenaVector<uint32_t> sortedResourceIndices(&context.scratchArena);
            sortedResourceIndices.reserve(resourceInstances.size());

            for (uint32_t iRes = 0; iRes < resourceInstances.size(); iRes++)
            {
                auto& resInst = resourceInstances[iRes];

                // forfeit spot in heap to prevent potential 2D overlap of already placed persistent resources.
                // TODO(optimization): see if we can prevent the need to invalidate resource here.
                if (resInst.HasEmptyLifetime() && (resInst.allocPlacement.heapId != RPS_INDEX_NONE_U32))
                {
                    resInst.InvalidateRuntimeResource(context.renderGraph.GetRuntimeBackend());
                }
                else if ((!resInst.isExternal) && (!resInst.IsTemporalParent()) &&
                         (resInst.allocRequirement.size > 0) && (!resInst.HasEmptyLifetime()))
                {
                    sortedResourceIndices.push_back(iRes);
                }
            }

            // Reset currently used size mark.
            // TODO: Allow external allocations to occupy spaces.
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

            // For each resource in sorted list, try allocate in a 2d rectangle ( width = cmd index span, height = size )
            uint32_t currHeapMemType = UINT32_MAX;
            for (size_t iIndex = 0, endIndex = sortedResourceIndices.size(); iIndex < endIndex; iIndex++)
            {
                RPS_V_RETURN(CalculateResourcePlacementsForMemoryType(
                    currHeapMemType, sortedResourceIndices.crange_all(), iIndex, bUseAliasing));
            }

            return RPS_OK;
        }

        RpsResult CalculateResourcePlacementsForMemoryType(uint32_t&                 currHeapMemType,
                                                           ConstArrayRef<uint32_t>   sortedResourceIndices,
                                                           size_t&                   iIndex,
                                                           bool                      bUseAliasing)
        {
            auto& context = *m_pContext;

            auto resourceInstances = context.renderGraph.GetResourceInstances().range_all();
            auto pRuntimeBackend   = context.renderGraph.GetRuntimeBackend();

            auto& heaps = context.renderGraph.GetHeapInfos();

            ArenaCheckPoint arenaCheckpoint{context.scratchArena};

            // Ordered index list by (heap, offset, end), cleared when switching heap type
            ArenaVector<uint32_t> allocatedIndices(&context.scratchArena);
            allocatedIndices.reserve(sortedResourceIndices.size());

            ArenaVector<uint32_t> pendingReallocIndices(&context.scratchArena);
            pendingReallocIndices.reserve(sortedResourceIndices.size());

            auto flushPendingReallocIndices = [&]() {
                for (uint32_t pendingIdx : pendingReallocIndices)
                {
                    RPS_V_RETURN(
                        CalculateResourcePlacement(currHeapMemType, allocatedIndices, pendingIdx, bUseAliasing));
                }
                pendingReallocIndices.clear();
                return RPS_OK;
            };

            bool bLastResPreallocated = false;

            for (; iIndex < sortedResourceIndices.size(); iIndex++)
            {
                const uint32_t iRes    = sortedResourceIndices[iIndex];
                auto&          currRes = resourceInstances[iRes];

                RPS_ASSERT(currRes.allocRequirement.size > 0);

                // Switch heap if heap type changes
                if (currHeapMemType != currRes.allocRequirement.memoryTypeIndex)
                {
                    RPS_V_RETURN(flushPendingReallocIndices());

                    currHeapMemType = currRes.allocRequirement.memoryTypeIndex;
                    iIndex--;
                    break;
                }

                // Existing allocation should have a valid resource handle and heap placement.
                RPS_ASSERT((!currRes.isPendingCreate) ==
                           (currRes.hRuntimeResource && (currRes.allocPlacement.heapId != RPS_INDEX_NONE_U32)));

                // Insert existing allocations into the allocatedIndices list and update the heap infos accordingly,
                // in order to hold their placements.
                if (!currRes.isPendingCreate)
                {
                    HeapInfo& currHeap = heaps[currRes.allocPlacement.heapId];

                    RPS_ASSERT(bLastResPreallocated || allocatedIndices.empty());

                    if (InsertPreAllocatedResourceToSortedAllocationList(
                            currHeap, allocatedIndices, iRes, resourceInstances))
                    {
                        currHeap.usedSize =
                            rpsMax(currHeap.usedSize, currRes.allocPlacement.offset + currRes.allocRequirement.size);
                        currHeap.maxUsedSize = rpsMax(currHeap.maxUsedSize, currHeap.usedSize);
                    }
                    else
                    {
                        currRes.InvalidateRuntimeResource(pRuntimeBackend);

                        pendingReallocIndices.push_back(iRes);
                    }

                    bLastResPreallocated = true;

                    continue;
                }

                if (bLastResPreallocated)
                {
                    RPS_V_RETURN(flushPendingReallocIndices());

                    bLastResPreallocated = false;
                }

                RPS_V_RETURN(
                    CalculateResourcePlacement(currHeapMemType, allocatedIndices, iRes, bUseAliasing));
            }

            RPS_V_RETURN(flushPendingReallocIndices());

            return RPS_OK;
        }

        RpsResult CalculateResourcePlacement(uint32_t                  currHeapMemType,
                                             ArenaVector<uint32_t>&    allocatedIndices,
                                             uint32_t                  resIndex,
                                             bool                      bUseAliasing)
        {
            auto& context = *m_pContext;

            auto& heaps             = context.renderGraph.GetHeapInfos();
            auto  resourceInstances = context.renderGraph.GetResourceInstances().range_all();
            auto& currRes           = resourceInstances[resIndex];

            {
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
                InsertToSortedAllocationList(allocatedIndices, resIndex, resourceInstances);
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
            auto resourceInstances = context.renderGraph.GetResourceInstances().range_all();
            auto scheduledCmds     = context.renderGraph.GetRuntimeCmdInfos().range_all();

            RPS_RETURN_OK_IF(resourceInstances.empty() || scheduledCmds.empty());

            ArenaCheckPoint arenaCheckpoint{context.scratchArena};

            ArenaVector<HeapRangeUsage> heapRangeUsages(&context.scratchArena);
            heapRangeUsages.reserve(resourceInstances.size());


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


            uint32_t resIdxSorted = 0;

            uint32_t numAliasingRes    = 0;
            uint32_t numDeactivatedRes = 0;

            ArenaVector<uint32_t> pendingAliasingSrcs(&context.scratchArena);
            RPS_CHECK_ALLOC(pendingAliasingSrcs.reserve(resourceInstances.size()));

            ArenaBitVector<> aliasingSrcBitMask(&context.scratchArena);
            RPS_CHECK_ALLOC(aliasingSrcBitMask.Resize(uint32_t(resourceInstances.size()), false));

            ArenaBitVector<> prevFrameAliasedMasks{&context.scratchArena,};
            RPS_CHECK_ALLOC(prevFrameAliasedMasks.Resize(resourceInstances.size()));

            for (uint32_t iRes = 0; iRes < resourceInstances.size(); iRes++)
            {
                ResourceInstance& resInst = resourceInstances[iRes];
                prevFrameAliasedMasks.SetBit(iRes, resInst.isAliased);
                if (resInst.HasEmptyLifetime())
                {
                    resInst.isAliased = false;
                }
            }

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

                    RPS_ASSERT(resInst.IsActive());

                    if (resInst.isExternal || resInst.IsTemporalParent())
                        continue;

                    RPS_ASSERT((resInst.allocRequirement.size > 0) || (resInst.HasNoAccess()));
                    RPS_ASSERT(resInst.lifetimeBegin != ResourceInstance::LIFETIME_UNDEFINED);

                    RPS_ASSERT(runtimeCmd.isTransition);

                    HeapRangeUsage currentResourceRange;
                    currentResourceRange.heapIndex     = resInst.allocPlacement.heapId;
                    currentResourceRange.heapOffset    = resInst.allocPlacement.offset;
                    currentResourceRange.size          = resInst.allocRequirement.size;
                    currentResourceRange.resourceIndex = resIndex;

                    HeapRangeUsage complementParts[2];

                    uint32_t initialNumActiveRanges = uint32_t(heapRangeUsages.size());

                    resInst.isAliased = false;

                    for (uint32_t iRange = 0; iRange < rpsMin(initialNumActiveRanges, uint32_t(heapRangeUsages.size()));
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

                                RPS_ASSERT(!srcResInfo.IsPersistent());

                                numAliasingRes++;
                            }

                            resInst.isAliased = true;

                            RPS_ASSERT(!resInst.IsPersistent());

                            RPS_ASSERT((resInst.lifetimeBegin > srcResInfo.lifetimeEnd) ||
                                        (resInst.lifetimeEnd < srcResInfo.lifetimeBegin));

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

            const uint32_t preambleAliasingInfoOffset = uint32_t(aliasingInfos.size());

            for (const uint32_t pendingAliasingSrcResIdx : pendingAliasingSrcs)
            {
                if (prevFrameAliasedMasks.GetBit(pendingAliasingSrcResIdx) ||
                    resourceInstances[pendingAliasingSrcResIdx].isPendingCreate)
                {
                    ResourceAliasingInfo* pPreambleAliasingInfo = aliasingInfos.grow(1);
                    pPreambleAliasingInfo->srcResourceIndex     = RPS_RESOURCE_ID_INVALID;
                    pPreambleAliasingInfo->dstResourceIndex     = pendingAliasingSrcResIdx;
                    pPreambleAliasingInfo->srcDeactivating      = RPS_FALSE;
                    pPreambleAliasingInfo->dstActivating        = RPS_TRUE;
                }
            }

            RPS_ASSERT(scheduledCmds[0].cmdId == CMD_ID_PREAMBLE);
            scheduledCmds[0].aliasingInfos.SetRange(preambleAliasingInfoOffset,
                                                    uint32_t(aliasingInfos.size()) - preambleAliasingInfoOffset);

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
                RPS_ASSERT(!(resInst.IsPersistent() && resInst.isAliased));

                if (resInst.lifetimeBegin == ResourceInstance::LIFETIME_UNDEFINED)
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
            RPS_ASSERT(numDeactivatedRes == numAliasingRes);


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

#endif  //RPS_MEMORY_SCHEDULE_HPP
