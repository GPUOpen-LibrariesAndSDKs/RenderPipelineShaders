// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_D3D12_ENHANCED_BARRIER_HPP
#define RPS_D3D12_ENHANCED_BARRIER_HPP

#if RPS_D3D12_ENHANCED_BARRIER_SUPPORT

#include "core/rps_util.hpp"
#include "runtime/d3d12/rps_d3d12_barrier.hpp"

namespace rps
{
    class D3D12EnhancedBarrierBuilder : public D3D12BarrierBuilder
    {
    public:
        D3D12EnhancedBarrierBuilder(const RuntimeDevice& runtimeDevice)
            : m_runtimeDevice(runtimeDevice)
        {
        }

        virtual void UpdateFrame(const RenderGraphUpdateContext& context) override final
        {
            m_barrierBatches.reset_keep_capacity(&context.frameArena);
            m_textureBarriers.reset_keep_capacity(&context.frameArena);
            m_bufferBarriers.reset_keep_capacity(&context.frameArena);
            m_globalBarriers.reset_keep_capacity(&context.frameArena);
        }

        virtual uint32_t CreateBarrierBatch(const RenderGraphUpdateContext& context,
                                            Span<RuntimeCmdInfo>            transitionRange) override final
        {
            RPS_ASSERT(!transitionRange.empty());

            auto& aliasingInfos     = context.renderGraph.GetResourceAliasingInfos();
            auto& resourceInstances = context.renderGraph.GetResourceInstances();
            auto  transitions       = context.renderGraph.GetTransitions().crange_all();

            auto transitionRangeCmds = transitionRange.Get(context.renderGraph.GetRuntimeCmdInfos());

            BarrierBatch currBatch                        = {};
            currBatch.offsets[D3D12_BARRIER_TYPE_GLOBAL]  = uint32_t(m_globalBarriers.size());
            currBatch.offsets[D3D12_BARRIER_TYPE_TEXTURE] = uint32_t(m_textureBarriers.size());
            currBatch.offsets[D3D12_BARRIER_TYPE_BUFFER]  = uint32_t(m_bufferBarriers.size());

            constexpr RpsAccessAttr noAccess = {RPS_ACCESS_UNKNOWN, RPS_SHADER_STAGE_NONE};

            for (uint32_t idx = 0; idx < transitionRangeCmds.size(); idx++)
            {
                auto& cmd = transitionRangeCmds[idx];
                RPS_ASSERT(cmd.isTransition);

                // Process aliasing:
                for (auto& aliasing : cmd.aliasingInfos.Get(aliasingInfos))
                {
                    // The src shouldn't be accessed by current transiton batch.
                    RPS_ASSERT((aliasing.srcResourceIndex == RPS_INDEX_NONE_U32) ||
                               (resourceInstances[aliasing.srcResourceIndex].lifetimeEnd <
                                (transitionRange.GetBegin() + idx)));

                    if (aliasing.srcDeactivating && (aliasing.srcResourceIndex != RPS_INDEX_NONE_U32))
                    {
                        const auto& srcResInfo = resourceInstances[aliasing.srcResourceIndex];

                        auto srcFinalAccesses =
                            srcResInfo.finalAccesses.Get(context.renderGraph.GetResourceFinalAccesses());

                        // TODO: Early out / conservative sync if too many final accesses
                        for (auto& srcFinalAccess : srcFinalAccesses)
                        {
                            const auto srcAccess =
                                RenderGraph::CalcPreviousAccess(srcFinalAccess.prevTransition, transitions, srcResInfo);

                            AppendBarrier(srcResInfo, srcAccess, noAccess, false, srcFinalAccess.range);
                        }
                    }

                    if (aliasing.dstActivating && (aliasing.dstResourceIndex != RPS_INDEX_NONE_U32))
                    {
                        const auto& dstResInfo         = resourceInstances[aliasing.dstResourceIndex];
                        const auto  aliasDstAccessInfo = CalcD3D12AccessInfo(dstResInfo.initialAccess);

                        AppendBarrier(
                            dstResInfo, noAccess, dstResInfo.initialAccess, true, dstResInfo.fullSubresourceRange);

                        // TODO: Whole resource already in initial layout, can skip the first access barrier.
                    }
                }

                if (cmd.cmdId < CMD_ID_PREAMBLE)
                {
                    // Process transitons:

                    const auto& currTrans   = transitions[cmd.cmdId];
                    const auto& resInstance = resourceInstances[currTrans.access.resourceId];

                    const auto prevAccess =
                        RenderGraph::CalcPreviousAccess(currTrans.prevTransition, transitions, resInstance);

                    AppendBarrier(resInstance, prevAccess, currTrans.access.access, false, currTrans.access.range);
                }
                else if (cmd.cmdId == CMD_ID_POSTAMBLE)
                {
                    // At frame end, transit non-aliased resource states to initial states
                    // All aliased resources are expected to be deactivated above from cmd.aliasingInfos.
                    // TODO: Extract non-aliased resource list ahead of time.
                    for (uint32_t iRes = 0, numRes = uint32_t(resourceInstances.size()); iRes < numRes; iRes++)
                    {
                        auto& resInstance = resourceInstances[iRes];

                        if (resInstance.isAccessed && !resInstance.isAliased && resInstance.hRuntimeResource &&
                            (resInstance.initialAccess.accessFlags != RPS_ACCESS_UNKNOWN))
                        {
                            for (auto& finalAccess :
                                 resInstance.finalAccesses.Get(context.renderGraph.GetResourceFinalAccesses()))
                            {
                                const auto srcAccess = RenderGraph::CalcPreviousAccess(
                                    finalAccess.prevTransition, transitions, resInstance);

                                AppendBarrier(resInstance,
                                              srcAccess,
                                              // TODO: For non-external resource, set no access/sync + initial layout?
                                              resInstance.initialAccess,
                                              false,
                                              transitions[finalAccess.prevTransition].access.range);
                            }
                        }
                    }
                }
            }

            currBatch.count[D3D12_BARRIER_TYPE_GLOBAL] =
                uint32_t(m_globalBarriers.size()) - currBatch.offsets[D3D12_BARRIER_TYPE_GLOBAL];
            currBatch.count[D3D12_BARRIER_TYPE_TEXTURE] =
                uint32_t(m_textureBarriers.size()) - currBatch.offsets[D3D12_BARRIER_TYPE_TEXTURE];
            currBatch.count[D3D12_BARRIER_TYPE_BUFFER] =
                uint32_t(m_bufferBarriers.size()) - currBatch.offsets[D3D12_BARRIER_TYPE_BUFFER];

            uint32_t batchId = RPS_INDEX_NONE_U32;

            if ((currBatch.count[D3D12_BARRIER_TYPE_GLOBAL] > 0) || (currBatch.count[D3D12_BARRIER_TYPE_TEXTURE] > 0) ||
                (currBatch.count[D3D12_BARRIER_TYPE_BUFFER] > 0))
            {
                batchId = uint32_t(m_barrierBatches.size());
                m_barrierBatches.push_back(currBatch);
            }

            return batchId;
        }

        virtual void RecordBarrierBatch(ID3D12GraphicsCommandList* pD3DCmdList, uint32_t barrierBatch) override final
        {
            const auto& batch = m_barrierBatches[barrierBatch];

            if (batch.empty())
                return;

            D3D12_BARRIER_GROUP barrierGroups[D3D12BarrierTypeCount];
            uint32_t            numGroups = 0;

            if (batch.count[D3D12_BARRIER_TYPE_GLOBAL] > 0)
            {
                barrierGroups[numGroups] = {D3D12_BARRIER_TYPE_GLOBAL, batch.count[D3D12_BARRIER_TYPE_GLOBAL]};
                barrierGroups[numGroups].pGlobalBarriers = &m_globalBarriers[batch.offsets[D3D12_BARRIER_TYPE_GLOBAL]];
                numGroups++;
            }

            if (batch.count[D3D12_BARRIER_TYPE_TEXTURE] > 0)
            {
                barrierGroups[numGroups] = {D3D12_BARRIER_TYPE_TEXTURE, batch.count[D3D12_BARRIER_TYPE_TEXTURE]};
                barrierGroups[numGroups].pTextureBarriers =
                    &m_textureBarriers[batch.offsets[D3D12_BARRIER_TYPE_TEXTURE]];
                numGroups++;
            }

            if (batch.count[D3D12_BARRIER_TYPE_BUFFER] > 0)
            {
                barrierGroups[numGroups] = {D3D12_BARRIER_TYPE_BUFFER, batch.count[D3D12_BARRIER_TYPE_BUFFER]};
                barrierGroups[numGroups].pBufferBarriers = &m_bufferBarriers[batch.offsets[D3D12_BARRIER_TYPE_BUFFER]];
                numGroups++;
            }

#if RPS_DX12_ENHANCED_BARRIER_DEBUG_DUMP
            static const StrRef barrierGroupTypeNames[] = {"Global", "Texture", "Buffer"};

            static const NameValuePair<D3D12_BARRIER_LAYOUT> layoutNames[] = {
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_UNDEFINED),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_COMMON),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_PRESENT),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_GENERIC_READ),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_RENDER_TARGET),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_UNORDERED_ACCESS),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_DEPTH_STENCIL_WRITE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_DEPTH_STENCIL_READ),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_SHADER_RESOURCE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_COPY_SOURCE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_COPY_DEST),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_RESOLVE_SOURCE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_RESOLVE_DEST),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_SHADING_RATE_SOURCE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_VIDEO_DECODE_READ),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_VIDEO_DECODE_WRITE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_VIDEO_PROCESS_READ),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_VIDEO_PROCESS_WRITE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_VIDEO_ENCODE_READ),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_VIDEO_ENCODE_WRITE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_DIRECT_QUEUE_COMMON),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_DIRECT_QUEUE_GENERIC_READ),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_DIRECT_QUEUE_COPY_SOURCE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_DIRECT_QUEUE_COPY_DEST),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_COMPUTE_QUEUE_COMMON),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_COMPUTE_QUEUE_GENERIC_READ),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_COMPUTE_QUEUE_UNORDERED_ACCESS),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_COMPUTE_QUEUE_SHADER_RESOURCE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_COMPUTE_QUEUE_COPY_SOURCE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_COMPUTE_QUEUE_COPY_DEST),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, LAYOUT_VIDEO_QUEUE_COMMON),
            };

            static const NameValuePair<D3D12_BARRIER_SYNC> syncNames[] = {
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, SYNC_NONE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, SYNC_ALL),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, SYNC_DRAW),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, SYNC_INDEX_INPUT),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, SYNC_VERTEX_SHADING),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, SYNC_PIXEL_SHADING),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, SYNC_DEPTH_STENCIL),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, SYNC_RENDER_TARGET),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, SYNC_COMPUTE_SHADING),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, SYNC_RAYTRACING),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, SYNC_COPY),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, SYNC_RESOLVE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, SYNC_EXECUTE_INDIRECT),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, SYNC_PREDICATION),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, SYNC_ALL_SHADING),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, SYNC_NON_PIXEL_SHADING),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_,
                                                  SYNC_EMIT_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, SYNC_VIDEO_DECODE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, SYNC_VIDEO_PROCESS),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, SYNC_VIDEO_ENCODE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, SYNC_COPY_RAYTRACING_ACCELERATION_STRUCTURE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, SYNC_SPLIT),
            };

            static const NameValuePair<D3D12_BARRIER_ACCESS> accessNames[] = {
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_COMMON),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_VERTEX_BUFFER),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_CONSTANT_BUFFER),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_INDEX_BUFFER),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_RENDER_TARGET),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_UNORDERED_ACCESS),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_DEPTH_STENCIL_WRITE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_DEPTH_STENCIL_READ),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_SHADER_RESOURCE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_STREAM_OUTPUT),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_INDIRECT_ARGUMENT),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_PREDICATION),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_COPY_DEST),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_COPY_SOURCE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_RESOLVE_DEST),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_RESOLVE_SOURCE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_SHADING_RATE_SOURCE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_VIDEO_DECODE_READ),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_VIDEO_DECODE_WRITE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_VIDEO_PROCESS_READ),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_VIDEO_PROCESS_WRITE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_VIDEO_ENCODE_READ),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_VIDEO_ENCODE_WRITE),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_BARRIER_, ACCESS_NO_ACCESS),
            };

            PrinterRef printer = m_runtimeDevice.GetDevice().Printer();

            printer("\n\nBarrierBatch %u", barrierBatch);

            for (uint32_t iGroup = 0; iGroup < numGroups; iGroup++)
            {
                const auto& group = barrierGroups[iGroup];

                printer("\n  Group %u : ", iGroup);
                printer(barrierGroupTypeNames[group.Type]);

                if (group.Type == D3D12_BARRIER_TYPE_TEXTURE)
                {
                    for (uint32_t iBarrier = 0; iBarrier < group.NumBarriers; iBarrier++)
                    {
                        auto& barrier = group.pTextureBarriers[iBarrier];
                        printer("\n    [%u] :", iBarrier);
                        printer("\n      SyncBefore : ");
                        printer.PrintFlags(barrier.SyncBefore, syncNames);
                        printer("\n      SyncAfter : ");
                        printer.PrintFlags(barrier.SyncAfter, syncNames);
                        printer("\n      AccessBefore : ");
                        printer.PrintFlags(barrier.AccessBefore, accessNames);
                        printer("\n      AccessAfter : ");
                        printer.PrintFlags(barrier.AccessAfter, accessNames);
                        printer("\n      LayoutBefore : ");
                        printer.PrintValueName(barrier.LayoutBefore, layoutNames);
                        printer("\n      LayoutAfter : ");
                        printer.PrintValueName(barrier.LayoutAfter, layoutNames);
                        printer("\n      pResource : %p", barrier.pResource);
                        printer("\n      Subresources : Mips [ %u, %u ), Array [ %u, %u ), Plane[ %u, %u )",
                                barrier.Subresources.IndexOrFirstMipLevel,
                                barrier.Subresources.IndexOrFirstMipLevel + barrier.Subresources.NumMipLevels,
                                barrier.Subresources.FirstArraySlice,
                                barrier.Subresources.FirstArraySlice + barrier.Subresources.NumArraySlices,
                                barrier.Subresources.FirstPlane,
                                barrier.Subresources.FirstPlane + barrier.Subresources.NumPlanes);

                        if (barrier.Flags & D3D12_TEXTURE_BARRIER_FLAG_DISCARD)
                            printer("\n      Discard: true");
                    }
                }
            }
#endif

            ID3D12GraphicsCommandList7* pD3DCmdList7 = static_cast<ID3D12GraphicsCommandList7*>(pD3DCmdList);
            pD3DCmdList7->Barrier(numGroups, barrierGroups);
        }

        virtual void RecordResolveBatch(ID3D12GraphicsCommandList*      pD3DCmdList,
                                        ConstArrayRef<D3D12ResolveInfo> resolveInfos) override
        {
            ID3D12GraphicsCommandList7* pD3DCmdList7 = static_cast<ID3D12GraphicsCommandList7*>(pD3DCmdList);

            const uint32_t numResolves = uint32_t(resolveInfos.size());

            RPS_ASSERT(numResolves <= D3D12ResolveInfo::RESOLVE_BATCH_SIZE);

            D3D12_TEXTURE_BARRIER barriers[D3D12ResolveInfo::RESOLVE_BATCH_SIZE];

            D3D12_BARRIER_GROUP barrierGroup = {};
            barrierGroup.Type                = D3D12_BARRIER_TYPE_TEXTURE;
            barrierGroup.NumBarriers         = numResolves;
            barrierGroup.pTextureBarriers    = barriers;

            for (uint32_t i = 0; i < numResolves; i++)
            {
                auto& barrier        = barriers[i];
                barrier.SyncBefore   = D3D12_BARRIER_SYNC_RENDER_TARGET;
                barrier.SyncAfter    = D3D12_BARRIER_SYNC_RESOLVE;
                barrier.AccessBefore = D3D12_BARRIER_ACCESS_RENDER_TARGET;
                barrier.AccessAfter  = D3D12_BARRIER_ACCESS_RESOLVE_SOURCE;
                barrier.LayoutBefore = D3D12_BARRIER_LAYOUT_RENDER_TARGET;
                barrier.LayoutAfter  = D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE;
                barrier.pResource    = resolveInfos[i].pSrc;
                barrier.Flags        = D3D12_TEXTURE_BARRIER_FLAG_NONE;
                barrier.Subresources = {};
                // TODO: Can use 1 barrier per resource with subresource range for EB path.
                barrier.Subresources.IndexOrFirstMipLevel = resolveInfos[i].srcSubResource;
            }

            pD3DCmdList7->Barrier(1, &barrierGroup);

            for (uint32_t i = 0; i < resolveInfos.size(); i++)
            {
                pD3DCmdList->ResolveSubresource(resolveInfos[i].pDst,
                                                resolveInfos[i].dstSubResource,
                                                resolveInfos[i].pSrc,
                                                resolveInfos[i].srcSubResource,
                                                resolveInfos[i].format);

                auto& barrier = barriers[i];
                std::swap(barrier.SyncBefore, barrier.SyncAfter);
                std::swap(barrier.AccessBefore, barrier.AccessAfter);
                std::swap(barrier.LayoutBefore, barrier.LayoutAfter);
            }

            pD3DCmdList7->Barrier(1, &barrierGroup);
        }

    private:
        struct D3D12EnhancedBarrierAccessInfo
        {
            D3D12_BARRIER_ACCESS        access;
            D3D12_BARRIER_LAYOUT        layout;
            D3D12_BARRIER_SYNC          sync;
        };

        static D3D12_BARRIER_SYNC GetD3D12SyncFlagsForShaderStages(RpsShaderStageFlags shaderStages)
        {
            D3D12_BARRIER_SYNC result = D3D12_BARRIER_SYNC_NONE;

            constexpr RpsShaderStageFlags allVertexShadingStages = RPS_SHADER_STAGE_VS | RPS_SHADER_STAGE_HS |
                                                                   RPS_SHADER_STAGE_DS | RPS_SHADER_STAGE_GS |
                                                                   RPS_SHADER_STAGE_AS | RPS_SHADER_STAGE_MS;

            static constexpr struct
            {
                RpsShaderStageFlags rpsStages;
                D3D12_BARRIER_SYNC  d3dSync;
            } stageToSyncMap[] = {
                {allVertexShadingStages, D3D12_BARRIER_SYNC_VERTEX_SHADING},
                {RPS_SHADER_STAGE_PS, D3D12_BARRIER_SYNC_PIXEL_SHADING},
                {RPS_SHADER_STAGE_CS, D3D12_BARRIER_SYNC_COMPUTE_SHADING},
                {RPS_SHADER_STAGE_RAYTRACING, D3D12_BARRIER_SYNC_RAYTRACING},
            };

            for (auto i = std::begin(stageToSyncMap), e = std::end(stageToSyncMap); i != e; ++i)
            {
                if (shaderStages & i->rpsStages)
                {
                    result |= i->d3dSync;
                }
            }

            return result;
        }

        static D3D12EnhancedBarrierAccessInfo CalcD3D12AccessInfo(const RpsAccessAttr& access)
        {
            if (access.accessFlags == RPS_ACCESS_UNKNOWN)
            {
                return D3D12EnhancedBarrierAccessInfo{
                    D3D12_BARRIER_ACCESS_NO_ACCESS, D3D12_BARRIER_LAYOUT_UNDEFINED, D3D12_BARRIER_SYNC_NONE};
            }

            if (access.accessFlags & RPS_ACCESS_RENDER_TARGET_BIT)
            {
                if (access.accessFlags & RPS_ACCESS_RESOLVE_DEST_BIT)
                {
                    return D3D12EnhancedBarrierAccessInfo{D3D12_BARRIER_ACCESS_RESOLVE_DEST,
                                                          D3D12_BARRIER_LAYOUT_RESOLVE_DEST,
                                                          D3D12_BARRIER_SYNC_RESOLVE};
                }
                else
                {
                    return D3D12EnhancedBarrierAccessInfo{D3D12_BARRIER_ACCESS_RENDER_TARGET,
                                                          D3D12_BARRIER_LAYOUT_RENDER_TARGET,
                                                          D3D12_BARRIER_SYNC_RENDER_TARGET};
                }
            }

            if (access.accessFlags & RPS_ACCESS_DEPTH_STENCIL)
            {
                if (access.accessFlags & RPS_ACCESS_DEPTH_STENCIL_WRITE)
                    return D3D12EnhancedBarrierAccessInfo{D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE,
                                                          D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE,
                                                          D3D12_BARRIER_SYNC_DEPTH_STENCIL};
                else
                    return D3D12EnhancedBarrierAccessInfo{D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ,
                                                          D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ,
                                                          D3D12_BARRIER_SYNC_DEPTH_STENCIL};
            }

            // TODO: Queue types

            if (access.accessFlags & RPS_ACCESS_UNORDERED_ACCESS_BIT)
            {

                if (access.accessFlags & RPS_ACCESS_CLEAR_BIT)
                {
                    return D3D12EnhancedBarrierAccessInfo{D3D12_BARRIER_ACCESS_UNORDERED_ACCESS,
                                                          D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS,
                                                          D3D12_BARRIER_SYNC_CLEAR_UNORDERED_ACCESS_VIEW};
                }

                return D3D12EnhancedBarrierAccessInfo{D3D12_BARRIER_ACCESS_UNORDERED_ACCESS,
                                                      D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS,
                                                      GetD3D12SyncFlagsForShaderStages(access.accessStages)};
            }

            if (access.accessFlags & RPS_ACCESS_COPY_DEST_BIT)
            {
                // TODO: Check self copy

                return D3D12EnhancedBarrierAccessInfo{
                    D3D12_BARRIER_ACCESS_COPY_DEST, D3D12_BARRIER_LAYOUT_COPY_DEST, D3D12_BARRIER_SYNC_COPY};
            }

            if (access.accessFlags & RPS_ACCESS_RESOLVE_DEST_BIT)
            {
                return D3D12EnhancedBarrierAccessInfo{
                    D3D12_BARRIER_ACCESS_RESOLVE_DEST, D3D12_BARRIER_LAYOUT_RESOLVE_DEST, D3D12_BARRIER_SYNC_RESOLVE};
            }

            // TODO: Handle AS COPY / EmitPostBuildInfo
            if (access.accessFlags & RPS_ACCESS_RAYTRACING_AS_BUILD_BIT)
            {
                return D3D12EnhancedBarrierAccessInfo{D3D12_BARRIER_ACCESS_RESOLVE_DEST,
                                                      D3D12_BARRIER_LAYOUT_COMMON,
                                                      D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE};
            }

            if (access.accessFlags & RPS_ACCESS_STREAM_OUT_BIT)
            {
                return D3D12EnhancedBarrierAccessInfo{
                    D3D12_BARRIER_ACCESS_STREAM_OUTPUT, D3D12_BARRIER_LAYOUT_COMMON, D3D12_BARRIER_SYNC_VERTEX_SHADING};
            }

            // At this point we should have handled all GPU-write accesses.
            RPS_ASSERT(!(access.accessFlags & RPS_ACCESS_ALL_GPU_WRITE));

            D3D12EnhancedBarrierAccessInfo result = {};

            result.sync = GetD3D12SyncFlagsForShaderStages(access.accessStages);

            // clang-format off
            static constexpr struct
            {
                RpsAccessFlags       rpsAccess;
                D3D12_BARRIER_ACCESS d3dAccess;
                D3D12_BARRIER_LAYOUT d3dLayout;
                D3D12_BARRIER_SYNC   sync;
            } readAccessMap[] = {
                {RPS_ACCESS_SHADER_RESOURCE_BIT,    D3D12_BARRIER_ACCESS_SHADER_RESOURCE,     D3D12_BARRIER_LAYOUT_SHADER_RESOURCE,     D3D12_BARRIER_SYNC_NONE}, // Sync comes from ShaderStage flags
                {RPS_ACCESS_COPY_SRC_BIT,           D3D12_BARRIER_ACCESS_COPY_SOURCE,         D3D12_BARRIER_LAYOUT_COPY_SOURCE,         D3D12_BARRIER_SYNC_COPY},
                {RPS_ACCESS_RESOLVE_SRC_BIT,        D3D12_BARRIER_ACCESS_RESOLVE_SOURCE,      D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE,      D3D12_BARRIER_SYNC_RESOLVE},
                {RPS_ACCESS_SHADING_RATE_BIT,       D3D12_BARRIER_ACCESS_SHADING_RATE_SOURCE, D3D12_BARRIER_LAYOUT_SHADING_RATE_SOURCE, D3D12_BARRIER_SYNC_PIXEL_SHADING},
                {RPS_ACCESS_INDIRECT_ARGS_BIT,      D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT,   D3D12_BARRIER_LAYOUT_COMMON,              D3D12_BARRIER_SYNC_EXECUTE_INDIRECT},
                {RPS_ACCESS_INDEX_BUFFER_BIT,       D3D12_BARRIER_ACCESS_INDEX_BUFFER,        D3D12_BARRIER_LAYOUT_COMMON,              D3D12_BARRIER_SYNC_INDEX_INPUT},
                {RPS_ACCESS_VERTEX_BUFFER_BIT,      D3D12_BARRIER_ACCESS_VERTEX_BUFFER,       D3D12_BARRIER_LAYOUT_COMMON,              D3D12_BARRIER_SYNC_VERTEX_SHADING},
                {RPS_ACCESS_CONSTANT_BUFFER_BIT,    D3D12_BARRIER_ACCESS_CONSTANT_BUFFER,     D3D12_BARRIER_LAYOUT_COMMON,              D3D12_BARRIER_SYNC_NONE},
                {RPS_ACCESS_PRESENT_BIT,            D3D12_BARRIER_ACCESS_COMMON,              D3D12_BARRIER_LAYOUT_COMMON,              D3D12_BARRIER_SYNC_ALL}, //SyncBefore bits D3D12_BARRIER_SYNC_NONE are incompatible with AccessBefore bits D3D12_BARRIER_ACCESS_COMMON in texture barrier at group [0], index [2]. [ STATE_SETTING ERROR #1331: INCOMPATIBLE_BARRIER_VALUES]
                {RPS_ACCESS_RAYTRACING_AS_READ_BIT, D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ, D3D12_BARRIER_LAYOUT_COMMON, D3D12_BARRIER_SYNC_RAYTRACING}, // TODO: Does this include DXR1.1 shaders?
            };
            // clang-format on

            // TODO: Skip buffer-only accesses for images
            // TODO: Avoid / warn on generic/common images
            bool bFoundAccess = false;
            for (auto i = std::begin(readAccessMap), e = std::end(readAccessMap); i != e; ++i)
            {
                if (access.accessFlags & i->rpsAccess)
                {
                    result.access |= i->d3dAccess;
                    result.sync |= i->sync;

                    result.layout = (result.layout == D3D12_BARRIER_LAYOUT_COMMON) ? i->d3dLayout
                                                                                   : D3D12_BARRIER_LAYOUT_GENERIC_READ;

                    bFoundAccess = true;
                }
            }

            if (!bFoundAccess)
            {
                result = {D3D12_BARRIER_ACCESS_NO_ACCESS, D3D12_BARRIER_LAYOUT_COMMON, D3D12_BARRIER_SYNC_NONE};
            }

            return result;
        }

        D3D12_BARRIER_SUBRESOURCE_RANGE GetD3D12SubresourceRange(const SubresourceRangePacked& range)
        {
            D3D12_BARRIER_SUBRESOURCE_RANGE result;

            // Expecting d3d12 aspect mask to be 1,2,3 for now.
            RPS_ASSERT((range.aspectMask >= 1) && (range.aspectMask <= 3));

            result.IndexOrFirstMipLevel = range.baseMipLevel;
            result.NumMipLevels         = range.GetMipLevelCount();
            result.FirstArraySlice      = range.baseArrayLayer;
            result.NumArraySlices       = range.GetArrayLayerCount();
            result.FirstPlane           = (range.aspectMask & 1) ? 0 : 1;
            result.NumPlanes            = (range.aspectMask == 3) ? 2 : 1;

            return result;
        }

        static bool ResourceMayNeedPlacedResourceInit(const ResourceInstance& resInfo)
        {
            const bool bHasStencil = rpsFormatHasStencil(resInfo.desc.GetFormat());

            // TODO: Make sure it's full resource clear.
            return (resInfo.isAliased || resInfo.isPendingInit) &&
                   rpsAnyBitsSet(
                       resInfo.allAccesses.accessFlags,
                       (RPS_ACCESS_RENDER_TARGET_BIT | RPS_ACCESS_DEPTH_STENCIL | RPS_ACCESS_UNORDERED_ACCESS_BIT)) &&
                   !rpsAnyBitsSet(resInfo.initialAccess.accessFlags, RPS_ACCESS_CLEAR_BIT) &&
                   !rpsAllBitsSet(resInfo.initialAccess.accessFlags,
                                  RPS_ACCESS_COPY_DEST_BIT | RPS_ACCESS_DISCARD_DATA_BEFORE_BIT |
                                      (bHasStencil ? RPS_ACCESS_STENCIL_DISCARD_DATA_BEFORE_BIT : 0));
        }

        void AppendBarrier(const ResourceInstance&      resInfo,
                           const RpsAccessAttr&         prevAccess,
                           const RpsAccessAttr&         currAccess,
                           bool                         bDiscard,
                           const SubresourceRangePacked range)
        {
            // TODO: Make a texture-only version of CalcD3D12AccessInfo
            auto beforeAccessInfo = CalcD3D12AccessInfo(prevAccess);
            auto afterAccessInfo  = CalcD3D12AccessInfo(currAccess);

            beforeAccessInfo.sync   = beforeAccessInfo.sync;
            afterAccessInfo.sync    = afterAccessInfo.sync;
            beforeAccessInfo.access = beforeAccessInfo.access;
            afterAccessInfo.access  = afterAccessInfo.access;

            if ((beforeAccessInfo.access == D3D12_BARRIER_ACCESS_UNORDERED_ACCESS) &&
                (afterAccessInfo.access == D3D12_BARRIER_ACCESS_UNORDERED_ACCESS) &&
                (prevAccess.accessFlags & RPS_ACCESS_RELAXED_ORDER_BIT) &&
                (currAccess.accessFlags & RPS_ACCESS_RELAXED_ORDER_BIT))
            {
                return;
            }

            if ((beforeAccessInfo.access == afterAccessInfo.access) &&
                ((beforeAccessInfo.access == D3D12_BARRIER_ACCESS_RENDER_TARGET) ||
                 (beforeAccessInfo.access == D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE)))
            {
                return;
            }

            if (resInfo.desc.IsImage())
            {
                D3D12_TEXTURE_BARRIER* pBarrier = m_textureBarriers.grow(1);

                pBarrier->SyncBefore   = beforeAccessInfo.sync;
                pBarrier->SyncAfter    = afterAccessInfo.sync;
                pBarrier->AccessBefore = beforeAccessInfo.access;
                pBarrier->AccessAfter  = afterAccessInfo.access;
                pBarrier->LayoutBefore = beforeAccessInfo.layout;
                pBarrier->LayoutAfter  = afterAccessInfo.layout;
                pBarrier->pResource    = rpsD3D12ResourceFromHandle(resInfo.hRuntimeResource);
                pBarrier->Subresources = GetD3D12SubresourceRange(range);
                pBarrier->Flags        = D3D12_TEXTURE_BARRIER_FLAG_NONE;

                if (bDiscard && ResourceMayNeedPlacedResourceInit(resInfo))
                {
                    pBarrier->Subresources = {UINT32_MAX};
                    pBarrier->Flags        = D3D12_TEXTURE_BARRIER_FLAG_DISCARD;
                }
            }
            else if (resInfo.desc.IsBuffer())
            {
                D3D12_BUFFER_BARRIER* pBarrier = m_bufferBarriers.grow(1);

                pBarrier->SyncBefore   = beforeAccessInfo.sync;
                pBarrier->SyncAfter    = afterAccessInfo.sync;
                pBarrier->AccessBefore = beforeAccessInfo.access;
                pBarrier->AccessAfter  = afterAccessInfo.access;
                pBarrier->pResource    = rpsD3D12ResourceFromHandle(resInfo.hRuntimeResource);
                pBarrier->Offset       = 0;
                pBarrier->Size         = UINT64_MAX;
            }
            else
            {
                RPS_TODO();
            }
        }

    private:
        static constexpr uint32_t D3D12BarrierTypeCount = D3D12_BARRIER_TYPE_BUFFER + 1;

        struct BarrierBatch
        {
            uint32_t offsets[D3D12BarrierTypeCount];
            uint32_t count[D3D12BarrierTypeCount];

            bool empty() const
            {
                return (count[D3D12_BARRIER_TYPE_GLOBAL] == 0) && (count[D3D12_BARRIER_TYPE_TEXTURE] == 0) &&
                       (count[D3D12_BARRIER_TYPE_BUFFER] == 0);
            }
        };

        const RuntimeDevice& m_runtimeDevice;

        ArenaVector<BarrierBatch>              m_barrierBatches;
        ArenaVector<D3D12_TEXTURE_BARRIER>     m_textureBarriers;
        ArenaVector<D3D12_BUFFER_BARRIER>      m_bufferBarriers;
        ArenaVector<D3D12_GLOBAL_BARRIER>      m_globalBarriers;
    };
}

#endif  //RPS_D3D12_ENHANCED_BARRIER_SUPPORT

#endif  //RPS_D3D12_ENHANCED_BARRIER_HPP
