// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_VISUALIZER_COMMON_HPP
#define RPS_VISUALIZER_COMMON_HPP

#include "runtime/common/rps_render_graph.hpp"

#include "rps_visualizer_util.hpp"

namespace rps
{
    struct SubResourceAccessInfo
    {
        AccessAttr             access;
        RpsResourceId          resourceId;
        SubresourceRangePacked subresourceRange;

        SubResourceAccessInfo() = default;

        SubResourceAccessInfo(const CmdAccessInfo& accessInfo)
            : access(accessInfo.access)
            , resourceId(accessInfo.resourceId)
            , subresourceRange(accessInfo.range)
        {
        }
    };

    struct CmdVisualizationInfo
    {
        StrRef                      name;
        uint32_t                    cmdId : 31;
        uint32_t                    isTransition : 1;
        uint32_t                    timelinePosition;
        Span<SubResourceAccessInfo> accesses;
    };

    struct ResourceVisualizationInfo
    {
        uint32_t lifetimeBegin;
        uint32_t lifetimeEnd;
        bool     isAliased;

        ResourceVisualizationInfo(uint32_t inLifetimeBegin = 0, uint32_t inLifetimeEnd = 0, bool inIsAliased = false)
            : lifetimeBegin(inLifetimeBegin)
            , lifetimeEnd(inLifetimeEnd)
            , isAliased(inIsAliased)
        {
        }
    };

    struct RenderGraphVisualizationData
    {
        ConstArrayRef<uint32_t>                  timelinePosToCmdIdMap;
        ConstArrayRef<uint32_t>                  cmdToRuntimeIdMap;
        ConstArrayRef<uint32_t>                  transIdToRuntimeIdMap;
        ConstArrayRef<RpsCmdDiagnosticInfo>      rtCmdInfos;
        ConstArrayRef<CmdVisualizationInfo>      cmdVisInfos;
        ConstArrayRef<SubResourceAccessInfo>     accessInfos;
        ConstArrayRef<RpsResourceDiagnosticInfo> resourceInfos;
        ConstArrayRef<ResourceVisualizationInfo> resourceVisInfos;
        ConstArrayRef<RpsHeapDiagnosticInfo>     heapInfos;
    };

    struct VisualizerUpdateContext
    {
        const RenderGraph* pRenderGraph;
        Arena&             persistentArena;
        Arena&             scratchArena;

        const RenderGraphVisualizationData& visualizationData;
    };

}  // namespace rps

#endif  //RPS_VISUALIZER_COMMON_HPP
