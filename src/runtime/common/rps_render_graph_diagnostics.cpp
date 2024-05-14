// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "runtime/common/rps_render_graph.hpp"

namespace rps
{

    void RenderGraph::GatherResourceDiagnosticInfo(RpsResourceDiagnosticInfo& dst,
                                                   const ResourceInstance&    src,
                                                   uint32_t                   resIndex)
    {
        const StrRef name = (src.resourceDeclId == RPS_INDEX_NONE_U32)
                                ? StrRef()
                                : GetBuilder().GetResourceDecls()[src.resourceDeclId].name;

        dst.name               = m_diagInfoArena.StoreStr(name).str;
        dst.temporalChildIndex = src.temporalLayerOffset;
        dst.isExternal         = src.isExternal;
        src.desc.Get(dst.desc);
        dst.allAccesses      = src.allAccesses;
        dst.initialAccess    = src.initialAccess;
        dst.lifetimeBegin    = src.lifetimeBegin;
        dst.lifetimeEnd      = src.lifetimeEnd;
        dst.allocRequirement = src.allocRequirement;
        dst.allocPlacement   = src.allocPlacement;
        dst.hRuntimeResource = src.hRuntimeResource;
    }

    void RenderGraph::GatherCmdDiagnosticInfo(RpsCmdDiagnosticInfo& dst, const RuntimeCmdInfo& src, uint32_t cmdIndex)
    {
        dst = {};

        dst.cmdIndex     = cmdIndex;
        dst.isTransition = src.isTransition;

        if (src.HasTransitionInfo())
        {
            const TransitionInfo& transInfo = m_transitions[src.cmdId];

            const auto prevAccess = CalcPreviousAccess(transInfo.prevTransition,
                                                       GetTransitions().range_all(),
                                                       GetResourceInstance(transInfo.access.resourceId));

            dst.transition.prevAccess = prevAccess;
            dst.transition.nextAccess = transInfo.access.access;
            transInfo.access.range.Get(dst.transition.range);
            dst.transition.resourceIndex = transInfo.access.resourceId;
        }
    }

    void RenderGraph::GatherHeapDiagnosticInfo(RpsHeapDiagnosticInfo& dst,
                                             const HeapInfo&          src)
    {
        dst.size            = (src.size == UINT64_MAX) ? src.maxUsedSize : src.size;
        dst.usedSize        = src.usedSize;
        dst.maxUsedSize     = src.maxUsedSize;
        dst.alignment       = src.alignment;
        dst.memoryTypeIndex = src.memTypeIndex;
        dst.hRuntimeHeap    = src.hRuntimeHeap;
    }

    RpsResult RenderGraph::UpdateDiagCache()
    {
        m_diagInfoArena.Reset();
        m_diagData.resourceInfos.reset(&m_diagInfoArena);
        m_diagData.cmdInfos.reset(&m_diagInfoArena);
        m_diagData.heapInfos.reset(&m_diagInfoArena);

        RPS_CHECK_ALLOC(m_diagData.resourceInfos.resize(m_resourceCache.size()));
        RPS_CHECK_ALLOC(m_diagData.cmdInfos.resize(m_runtimeCmdInfos.size()));
        RPS_CHECK_ALLOC(m_diagData.heapInfos.resize(m_heaps.size()));

        //Resource Infos
        const uint32_t numResources = uint32_t(m_resourceCache.size());
        for (uint32_t resIndex = 0; resIndex < numResources; ++resIndex)
        {
            RpsResourceDiagnosticInfo& writeResourceInfo = m_diagData.resourceInfos[resIndex];
            const ResourceInstance&    resInstance       = m_resourceCache[resIndex];

            GatherResourceDiagnosticInfo(writeResourceInfo, resInstance, resIndex);
        }

        //Command Infos
        const uint32_t numTotalCmds = uint32_t(m_runtimeCmdInfos.size());
        for (uint32_t rtCmdIndex = 0; rtCmdIndex < numTotalCmds; ++rtCmdIndex)
        {
            const RuntimeCmdInfo& rtCmdInfo    = m_runtimeCmdInfos[rtCmdIndex];
            RpsCmdDiagnosticInfo& writeCmdInfo = m_diagData.cmdInfos[rtCmdIndex];

            GatherCmdDiagnosticInfo(writeCmdInfo, rtCmdInfo, rtCmdIndex);
        }

        //Heap Infos
        const uint32_t numHeaps = uint32_t(m_heaps.size());
        for (uint32_t heapIndex = 0; heapIndex < numHeaps; ++heapIndex)
        {
            RpsHeapDiagnosticInfo& writeHeapInfo = m_diagData.heapInfos[heapIndex];
            const HeapInfo&        heapInfo      = m_heaps[heapIndex];

            GatherHeapDiagnosticInfo(writeHeapInfo, heapInfo);
        }

        return RPS_OK;
    }
}  // namespace rps
