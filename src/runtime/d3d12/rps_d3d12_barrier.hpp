// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_D3D12_BARRIER_HPP
#define RPS_D3D12_BARRIER_HPP

#include "runtime/d3d12/rps_d3d12_runtime_device.hpp"
#include "runtime/d3d12/rps_d3d12_util.hpp"

namespace rps
{
    struct D3D12ResolveInfo
    {
        ID3D12Resource* pSrc;
        ID3D12Resource* pDst;
        uint32_t        srcSubResource;
        uint32_t        dstSubResource;
        DXGI_FORMAT     format;

        static constexpr uint32_t RESOLVE_BATCH_SIZE = 128;
    };

    class D3D12BarrierBuilder
    {
    public:
        virtual ~D3D12BarrierBuilder()
        {
        }

        virtual bool MayNeedPlacedResourceInitState() const
        {
            return false;
        }

        virtual void EnsurePlacedResourceInitState(ResourceInstance& resInfo) const
        {
        }

        virtual void UpdateFrame(const RenderGraphUpdateContext& context) = 0;

        virtual uint32_t CreateBarrierBatch(const RenderGraphUpdateContext& context,
                                            Span<RuntimeCmdInfo>            transitionRange) = 0;

        virtual void RecordBarrierBatch(ID3D12GraphicsCommandList* pD3DCmdList, uint32_t barrierBatch) = 0;

        virtual void RecordResolveBatch(ID3D12GraphicsCommandList*      pD3DCmdList,
                                        ConstArrayRef<D3D12ResolveInfo> resolveInfos) = 0;
    };

    class D3D12ConventionalBarrierBuilder : public D3D12BarrierBuilder
    {
        struct BarrierBatch
        {
            Span<D3D12_RESOURCE_BARRIER> earlyBarriers;
            Span<ID3D12Resource*>        discardResources;
            Span<D3D12_RESOURCE_BARRIER> lateBarriers;
        };

    public:

        D3D12ConventionalBarrierBuilder(const D3D12RuntimeDevice& device)
            : m_d3dRuntimeDevice(device)
        {
        }

        static bool NeedPlacedResourceInit(const ResourceInstance& resInfo)
        {
            return rpsAnyBitsSet(resInfo.allAccesses.accessFlags,
                                 RPS_ACCESS_RENDER_TARGET_BIT | RPS_ACCESS_DEPTH_STENCIL);
        }

        static bool IsStateCompatibleForPlacedResourceInit(RpsAccessFlags accessFlags)
        {
            // TODO: Add full resource copy.
            return rpsAnyBitsSet(accessFlags, RPS_ACCESS_RENDER_TARGET_BIT | RPS_ACCESS_DEPTH_STENCIL_WRITE);
        }

        virtual bool MayNeedPlacedResourceInitState() const override final
        {
            return true;
        }

        virtual void EnsurePlacedResourceInitState(ResourceInstance& resInfo) const override final
        {
            if (NeedPlacedResourceInit(resInfo) &&
                !IsStateCompatibleForPlacedResourceInit(resInfo.initialAccess.accessFlags))
            {
                if (rpsAnyBitsSet(resInfo.allAccesses.accessFlags, RPS_ACCESS_DEPTH_STENCIL))
                {
                    resInfo.SetInitialAccess(AccessAttr{RPS_ACCESS_DEPTH_STENCIL_WRITE, RPS_SHADER_STAGE_NONE});
                }
                else if (rpsAnyBitsSet(resInfo.allAccesses.accessFlags, RPS_ACCESS_RENDER_TARGET_BIT))
                {
                    resInfo.SetInitialAccess(AccessAttr{RPS_ACCESS_RENDER_TARGET_BIT, RPS_SHADER_STAGE_NONE});
                }
            }
        }

        virtual void UpdateFrame(const RenderGraphUpdateContext& context) override final
        {
            m_barriers.reset_keep_capacity(&context.frameArena);
            m_barrierBatches.reset_keep_capacity(&context.frameArena);
            m_discardResources.reset_keep_capacity(&context.frameArena);
        }

        virtual uint32_t CreateBarrierBatch(const RenderGraphUpdateContext& context,
                                            Span<RuntimeCmdInfo>            transitionRange) override final
        {
            auto& aliasingInfos     = context.renderGraph.GetResourceAliasingInfos();
            auto& resourceInstances = context.renderGraph.GetResourceInstances();
            auto& transitions       = context.renderGraph.GetTransitions();

            ArenaCheckPoint arenaCheckpoint{context.scratchArena};

            ArenaVector<uint32_t> lateCmds(&context.scratchArena);

            auto transitionRangeCmds = transitionRange.Get(context.renderGraph.GetRuntimeCmdInfos());

            BarrierBatch currBatch = {};

            currBatch.earlyBarriers.SetRange(uint32_t(m_barriers.size()), 0);
            currBatch.discardResources.SetRange(uint32_t(m_discardResources.size()), 0);

            for (uint32_t idx = 0; idx < transitionRangeCmds.size(); idx++)
            {
                auto& cmd = transitionRangeCmds[idx];
                RPS_ASSERT(cmd.isTransition);

                for (auto& aliasing : cmd.aliasingInfos.Get(aliasingInfos))
                {
                    if (aliasing.dstActivating && (aliasing.dstResourceIndex != RPS_RESOURCE_ID_INVALID))
                    {
                        auto& dstResInfo = resourceInstances[aliasing.dstResourceIndex];

                        if (NeedPlacedResourceInit(dstResInfo))
                        {
                            const bool bHasStencil = rpsFormatHasStencil(dstResInfo.desc.GetFormat());

                            // TODO: Make sure it's full resource clear.
                            if (!rpsAnyBitsSet(dstResInfo.initialAccess.accessFlags, RPS_ACCESS_CLEAR_BIT) &&
                                !rpsAllBitsSet(dstResInfo.initialAccess.accessFlags,
                                               RPS_ACCESS_COPY_DEST_BIT | RPS_ACCESS_DISCARD_DATA_BEFORE_BIT |
                                                   (bHasStencil ? RPS_ACCESS_STENCIL_DISCARD_DATA_BEFORE_BIT : 0)))
                            {
                                m_discardResources.push_back(D3D12RuntimeDevice::FromHandle(
                                    resourceInstances[aliasing.dstResourceIndex].hRuntimeResource));
                            }
                        }
                    }

                    if (aliasing.srcDeactivating && (aliasing.srcResourceIndex != RPS_RESOURCE_ID_INVALID))
                    {
                        auto& srcResInfo = resourceInstances[aliasing.srcResourceIndex];

                        // Before deactivating resource, transition it to the initial state.
                        // For placed resource that needs init, we have made sure initialAccess
                        // is compatible with the states required for init.
                        for (auto& finalAccess :
                             srcResInfo.finalAccesses.Get(context.renderGraph.GetResourceFinalAccesses()))
                        {
                            const auto prevAccess = RenderGraph::CalcPreviousAccess(
                                finalAccess.prevTransition, transitions.range_all(), srcResInfo);

                            AppendBarrier(D3D12RuntimeDevice::FromHandle(srcResInfo.hRuntimeResource),
                                          prevAccess,
                                          srcResInfo.initialAccess,
                                          srcResInfo,
                                          finalAccess.range);
                        }
                    }
                }

                if (cmd.cmdId < CMD_ID_PREAMBLE)
                {
                    const auto& currTrans   = transitions[cmd.cmdId];
                    const auto& resInstance = resourceInstances[currTrans.access.resourceId];

                    if (resInstance.isAliased && (resInstance.lifetimeBegin >= transitionRange.GetBegin()))
                    {
                        lateCmds.push_back(idx);
                    }
                    else
                    {
                        const auto prevAccess = RenderGraph::CalcPreviousAccess(
                            currTrans.prevTransition, transitions.range_all(), resInstance);

                        AppendBarrier(D3D12RuntimeDevice::FromHandle(resInstance.hRuntimeResource),
                                      prevAccess,
                                      currTrans.access.access,
                                      resInstance,
                                      currTrans.access.range);
                    }
                }
                else if (cmd.cmdId == CMD_ID_POSTAMBLE)
                {
                    // At frame end, transit non-deactivated resource states to initial states
                    for (uint32_t iRes = 0, numRes = uint32_t(resourceInstances.size()); iRes < numRes; iRes++)
                    {
                        auto& resInstance = resourceInstances[iRes];

                        RPS_ASSERT(!(resInstance.isAliased && resInstance.IsPersistent()));

                        if (resInstance.hRuntimeResource && resInstance.isAccessed && !resInstance.isAliased)
                        {
                            for (auto& finalAccess :
                                 resInstance.finalAccesses.Get(context.renderGraph.GetResourceFinalAccesses()))
                            {
                                const auto prevAccess = RenderGraph::CalcPreviousAccess(
                                    finalAccess.prevTransition, transitions.range_all(), resInstance);

                                AppendBarrier(D3D12RuntimeDevice::FromHandle(resInstance.hRuntimeResource),
                                              prevAccess,
                                              resInstance.initialAccess,
                                              resInstance,
                                              finalAccess.range);
                            }
                        }
                    }
                }
            }

            currBatch.earlyBarriers.SetEnd(uint32_t(m_barriers.size()));
            currBatch.discardResources.SetEnd(uint32_t(m_discardResources.size()));

            currBatch.lateBarriers.SetRange(uint32_t(m_barriers.size()), 0);

            for (auto lateCmdIdx : lateCmds)
            {
                auto& cmd = transitionRangeCmds[lateCmdIdx];
                RPS_ASSERT(cmd.isTransition);

                const auto& currTrans   = transitions[cmd.cmdId];
                const auto& resInstance = resourceInstances[currTrans.access.resourceId];

                const auto prevAccess =
                    RenderGraph::CalcPreviousAccess(currTrans.prevTransition, transitions.range_all(), resInstance);

                AppendBarrier(D3D12RuntimeDevice::FromHandle(resInstance.hRuntimeResource),
                              prevAccess,
                              currTrans.access.access,
                              resInstance,
                              currTrans.access.range);
            }

            currBatch.lateBarriers.SetEnd(uint32_t(m_barriers.size()));

            uint32_t batchId = RPS_INDEX_NONE_U32;

            if (!currBatch.earlyBarriers.empty() || !currBatch.discardResources.empty() ||
                !currBatch.lateBarriers.empty())
            {
                batchId = uint32_t(m_barrierBatches.size());
                m_barrierBatches.push_back(currBatch);
            }

            return batchId;
        }

        virtual void RecordBarrierBatch(ID3D12GraphicsCommandList* pD3DCmdList, uint32_t barrierBatch) override final
        {
            const auto& batch = m_barrierBatches[barrierBatch];

            if (!batch.earlyBarriers.empty())
            {
                auto barriers = batch.earlyBarriers.GetConstRef(m_barriers);
                pD3DCmdList->ResourceBarrier(barriers.size(), barriers.data());
            }

            auto discardResources = batch.discardResources.Get(m_discardResources);

            for (auto pRes : discardResources)
            {
                pD3DCmdList->DiscardResource(pRes, nullptr);
            }

            if (!batch.lateBarriers.empty())
            {
                auto barriers = batch.lateBarriers.GetConstRef(m_barriers);
                pD3DCmdList->ResourceBarrier(barriers.size(), barriers.data());
            }
        }

        virtual void RecordResolveBatch(ID3D12GraphicsCommandList*      pD3DCmdList,
                                        ConstArrayRef<D3D12ResolveInfo> resolveInfos) override
        {
            const uint32_t numResolves = uint32_t(resolveInfos.size());

            RPS_ASSERT(numResolves <= D3D12ResolveInfo::RESOLVE_BATCH_SIZE);

            D3D12_RESOURCE_BARRIER barriers[D3D12ResolveInfo::RESOLVE_BATCH_SIZE];

            for (uint32_t i = 0; i < numResolves; i++)
            {
                auto& barrier                  = barriers[i];
                barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                barrier.Transition.pResource   = resolveInfos[i].pSrc;
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
                barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
                barrier.Transition.Subresource = resolveInfos[i].srcSubResource;
            }

            pD3DCmdList->ResourceBarrier(numResolves, barriers);

            for (uint32_t i = 0; i < resolveInfos.size(); i++)
            {
                pD3DCmdList->ResolveSubresource(resolveInfos[i].pDst,
                                                resolveInfos[i].dstSubResource,
                                                resolveInfos[i].pSrc,
                                                resolveInfos[i].srcSubResource,
                                                resolveInfos[i].format);

                std::swap(barriers[i].Transition.StateBefore, barriers[i].Transition.StateAfter);
            }

            pD3DCmdList->ResourceBarrier(numResolves, barriers);
        }

        static D3D12_RESOURCE_STATES CalcD3D12State(const RpsAccessAttr& access)
        {
            const auto accessFlags = access.accessFlags & RPS_ACCESS_ALL_ACCESS_MASK;

            if (rpsAnyBitsSet(accessFlags, RPS_ACCESS_DEPTH_STENCIL_WRITE))
                return D3D12_RESOURCE_STATE_DEPTH_WRITE;

            switch (accessFlags)
            {
            case RPS_ACCESS_RENDER_TARGET_BIT:
                return D3D12_RESOURCE_STATE_RENDER_TARGET;
            case RPS_ACCESS_UNORDERED_ACCESS_BIT:
                return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            case RPS_ACCESS_STREAM_OUT_BIT:
                return D3D12_RESOURCE_STATE_STREAM_OUT;
            case RPS_ACCESS_COPY_DEST_BIT:
                return D3D12_RESOURCE_STATE_COPY_DEST;
            case RPS_ACCESS_RESOLVE_DEST_BIT:
            case RPS_ACCESS_RESOLVE_DEST_BIT | RPS_ACCESS_RENDER_TARGET_BIT:
                return D3D12_RESOURCE_STATE_RESOLVE_DEST;
            case RPS_ACCESS_RAYTRACING_AS_BUILD_BIT:
                return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            case RPS_ACCESS_RAYTRACING_AS_READ_BIT:
                return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            case RPS_ACCESS_CPU_READ_BIT:
                return D3D12_RESOURCE_STATE_COPY_DEST;
            case RPS_ACCESS_CPU_WRITE_BIT:
                return D3D12_RESOURCE_STATE_GENERIC_READ;
            case RPS_ACCESS_PRESENT_BIT:
                return D3D12_RESOURCE_STATE_PRESENT;
            default:
                break;
            };

            static constexpr struct
            {
                RpsAccessFlagBits     rpsFlag;
                D3D12_RESOURCE_STATES d3dState;
            } rpsToD3D12ResStates[] = {
                {RPS_ACCESS_INDIRECT_ARGS_BIT, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT},
                {RPS_ACCESS_INDEX_BUFFER_BIT, D3D12_RESOURCE_STATE_INDEX_BUFFER},
                {RPS_ACCESS_VERTEX_BUFFER_BIT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER},
                {RPS_ACCESS_CONSTANT_BUFFER_BIT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER},
                {RPS_ACCESS_SHADING_RATE_BIT, D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE},
                {RPS_ACCESS_DEPTH_READ_BIT, D3D12_RESOURCE_STATE_DEPTH_READ},
                {RPS_ACCESS_STENCIL_READ_BIT, D3D12_RESOURCE_STATE_DEPTH_READ},
                {RPS_ACCESS_COPY_SRC_BIT, D3D12_RESOURCE_STATE_COPY_SOURCE},
                {RPS_ACCESS_RESOLVE_SRC_BIT, D3D12_RESOURCE_STATE_RESOLVE_SOURCE},
            };

            D3D12_RESOURCE_STATES readStates = static_cast<D3D12_RESOURCE_STATES>(0);

            for (auto& mapEntry : rpsToD3D12ResStates)
            {
                if (rpsAnyBitsSet(accessFlags, mapEntry.rpsFlag))
                {
                    readStates |= mapEntry.d3dState;
                }
            }

            if (rpsAnyBitsSet(accessFlags, RPS_ACCESS_SHADER_RESOURCE_BIT))
            {
                if (rpsAnyBitsSet(access.accessStages, RPS_SHADER_STAGE_PS))
                    readStates |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

                if (rpsAnyBitsSet(access.accessStages, (~RPS_SHADER_STAGE_PS) & RPS_SHADER_STAGE_ALL))
                    readStates |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            }

            return readStates;
        }

        static bool IsUploadOrReadbackResource(const D3D12RuntimeDevice& device, const ResourceInstance& resInfo)
        {
            if (resInfo.isExternal)
                return false; // TODO

            auto& heapInfo = device.GetD3D12HeapTypeInfo(resInfo.allocRequirement.memoryTypeIndex);
            return (heapInfo.type == D3D12_HEAP_TYPE_READBACK) || (heapInfo.type == D3D12_HEAP_TYPE_UPLOAD);
        }

        static D3D12_RESOURCE_STATES CalcResourceInitState(const D3D12RuntimeDevice& device,
                                                           const ResourceInstance&   resInfo)
        {
            auto& heapInfo = device.GetD3D12HeapTypeInfo(resInfo.allocRequirement.memoryTypeIndex);

            if (heapInfo.type == D3D12_HEAP_TYPE_READBACK)
                return D3D12_RESOURCE_STATE_COPY_DEST;
            else if (heapInfo.type == D3D12_HEAP_TYPE_UPLOAD)
                return D3D12_RESOURCE_STATE_GENERIC_READ;

            return resInfo.desc.IsImage() ? D3D12ConventionalBarrierBuilder::CalcD3D12State(resInfo.initialAccess)
                                          : D3D12_RESOURCE_STATE_COMMON;
        }

    private:
        void AppendBarrier(ID3D12Resource*              pResource,
                           const RpsAccessAttr&         prevAccess,
                           const RpsAccessAttr&         currAccess,
                           const ResourceInstance&      resInfo,
                           const SubresourceRangePacked range)
        {
            auto stateBefore = CalcD3D12State(prevAccess);
            auto stateAfter  = CalcD3D12State(currAccess);

            if (IsUploadOrReadbackResource(m_d3dRuntimeDevice, resInfo))
            {
                return;
            }

            if (stateBefore != stateAfter)
            {
                const bool isFullRes = (resInfo.numSubResources == 1) || (resInfo.fullSubresourceRange == range);

                auto* pBarriers = m_barriers.grow(isFullRes ? 1 : range.GetNumSubresources());

                pBarriers[0].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                pBarriers[0].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                pBarriers[0].Transition.pResource   = pResource;
                pBarriers[0].Transition.StateBefore = stateBefore;
                pBarriers[0].Transition.StateAfter  = stateAfter;
                pBarriers[0].Transition.Subresource = isFullRes ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : 0;

                if (!isFullRes)
                {
                    RPS_ASSERT(resInfo.desc.IsImage());

                    uint32_t planeMask = range.aspectMask;
                    uint32_t idx       = 0;

                    for (uint32_t iPlane = 0; planeMask != 0; planeMask = planeMask >> 1, iPlane++)
                    {
                        if (planeMask & 1)
                        {
                            for (uint32_t iArray = range.baseArrayLayer; iArray < range.arrayLayerEnd; iArray++)
                            {
                                for (uint32_t iMip = range.baseMipLevel; iMip < range.mipLevelEnd; iMip++)
                                {
                                    pBarriers[idx] = pBarriers[0];
                                    pBarriers[idx].Transition.Subresource =
                                        D3D12CalcSubresource(iMip,
                                                             iArray,
                                                             iPlane,
                                                             resInfo.desc.image.mipLevels,
                                                             resInfo.desc.image.arrayLayers);

                                    idx++;
                                }
                            }
                        }
                    }
                }
            }
            else if ((stateBefore == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) &&
                     (stateAfter == D3D12_RESOURCE_STATE_UNORDERED_ACCESS))
            {
                auto* pBarrier = m_barriers.grow(1);

                pBarrier->Type          = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                pBarrier->Flags         = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                pBarrier->UAV.pResource = pResource;
            }
        }

    private:
        const D3D12RuntimeDevice&           m_d3dRuntimeDevice;
        ArenaVector<BarrierBatch>           m_barrierBatches;
        ArenaVector<D3D12_RESOURCE_BARRIER> m_barriers;
        ArenaVector<ID3D12Resource*>        m_discardResources;
    };
}  // namespace rps

#endif  //RPS_D3D12_BARRIER_HPP
