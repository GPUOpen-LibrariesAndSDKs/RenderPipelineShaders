// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_GRAPH_CANVAS_HPP
#define RPS_GRAPH_CANVAS_HPP

#include "core/rps_util.hpp"
#include "runtime/common/rps_render_graph_resource.hpp"

#include "rps_visualizer_common.hpp"
#include "rps_zoom_state.hpp"

#include "imgui.h"

namespace rps
{
    struct DrawingState;

    struct GraphCacheTransitionInfo
    {
        SubResourceAccessInfo prevAccess;
        SubResourceAccessInfo nextAccess;
        uint32_t              transitionId;
        uint32_t              graphNodeId;
        uint32_t              rtCmdId;

        bool IsActive() const
        {
            return rtCmdId != RPS_INDEX_NONE_U32;
        }
    };

    struct GraphCacheNodeInfo
    {
        Span<SubResourceAccessInfo> accessInfos;
        StrRef                      name;
        uint32_t                    cmdId;
        uint32_t                    rtCmdId;
        ImU32                       nodeColor;

        bool IsActive() const
        {
            return rtCmdId != RPS_INDEX_NONE_U32;
        }

        uint32_t GetNodeId() const
        {
            return cmdId;
        }
    };

    struct GraphCacheDependencyInfo
    {
        uint32_t srcTimelinePos;
        uint32_t srcRowIndex : 31;
        uint32_t srcIsTransition : 1;
        uint32_t srcNodeId;
        uint32_t dstTimelinePos;
        uint32_t dstRowIndex : 31;
        uint32_t dstIsTransition : 1;
        uint32_t dstNodeId;
        uint32_t laneIndex;
    };

    struct GraphCacheEntry
    {
        Span<GraphCacheNodeInfo>       nodes;
        Span<GraphCacheTransitionInfo> transitions;
        Span<GraphCacheDependencyInfo> nodeToTransDeps;
        Span<GraphCacheDependencyInfo> transToNodeDeps;
        Span<GraphCacheDependencyInfo> nodeToNodeDeps;
    };

    using GraphStrBuilder = StrBuilder<4096>;

    class GraphCanvas
    {
    public:
        GraphCanvas()
            : m_accessesPool(m_accesses)
            , m_nodesPool(m_nodes)
            , m_transitionsPool(m_transitions)
            , m_nodeDependenciesPool(m_nodeDependencies)
        {
        }

        RpsResult Draw(DrawingState& drawState);
        RpsResult Update(const VisualizerUpdateContext& context);

        static float GetDepCurveFraction(const DrawingState& drawState, const GraphCacheDependencyInfo& dep);
        static float GetRowHeight();
        static float GetRowHeightPadded();

    private:
        static ImU32    GetNodeColor(const NodeDeclInfo& nodeDecl, bool bActive = true);
        static uint64_t CalcKeyMoveHeightStep(uint64_t heightRange);

        void Select(uint32_t graphNodeId);
        void Unselect();

        void GetTransitionTooltip(const DrawingState&             state,
                                  GraphStrBuilder&                sb,
                                  const GraphCacheTransitionInfo& transitionInfo) const;
        void GetNodeTooltip(const DrawingState& state, GraphStrBuilder& sb, const GraphCacheNodeInfo& nodeInfo) const;
        uint32_t GetNodeRuntimeId(const VisualizerUpdateContext& context, const Graph& graph, const Node& node) const;
        uint32_t GetCacheEntryRowIndex(const VisualizerUpdateContext& context,
                                       const GraphCacheEntry&         cacheEntry,
                                       const Node&                    node) const;
        uint32_t GetNodeTimelinePos(const VisualizerUpdateContext& context, const Graph& graph, const Node& node) const;

    public:
        static constexpr float ConnectionThickness = 1.5f;

    private:
        static constexpr ImU32 GraphicsNodeColor                 = IM_COL32(0, 255, 255, 255);
        static constexpr ImU32 ComputeNodeColor                  = IM_COL32(255, 127, 0, 255);
        static constexpr ImU32 CopyNodeColor                     = IM_COL32(204, 255, 0, 255);
        static constexpr ImU32 MultiNodeColor                    = IM_COL32(255, 127, 0, 255);
        static constexpr ImU32 TransitionQuadColor               = IM_COL32(255, 0, 255, 255);
        static constexpr ImU32 ConnectionColor                   = IM_COL32(63, 63, 63, 255);
        static constexpr ImU32 HighlightColor                    = IM_COL32(255, 0, 0, 255);
        static constexpr ImU32 DefaultNodeColor                  = IM_COL32(127, 127, 127, 255);

        static constexpr float    BorderThickness     = 2.f;
        static constexpr float    CurveFraction       = 0.2f;
        static constexpr float    MinQuadBorderWidth  = 10.f;
        static constexpr uint64_t MinRowCount         = 10;
        static constexpr float    NodeAspectRatio     = 3.f / 1.f;

        float    m_viewHeight = -1.f;
        uint32_t m_numLanes   = 0;

        uint32_t m_highlightGraphNodeId = RPS_INDEX_NONE_U32;

        ZoomState m_graphHeightZoom;

        ArenaVector<GraphCacheEntry> m_graphCache;

        ArenaVector<SubResourceAccessInfo>                                  m_accesses;
        SpanPool<SubResourceAccessInfo, ArenaVector<SubResourceAccessInfo>> m_accessesPool;

        ArenaVector<GraphCacheNodeInfo>                               m_nodes;
        SpanPool<GraphCacheNodeInfo, ArenaVector<GraphCacheNodeInfo>> m_nodesPool;

        ArenaVector<GraphCacheTransitionInfo>                                     m_transitions;
        SpanPool<GraphCacheTransitionInfo, ArenaVector<GraphCacheTransitionInfo>> m_transitionsPool;

        ArenaVector<GraphCacheDependencyInfo>                                     m_nodeDependencies;
        SpanPool<GraphCacheDependencyInfo, ArenaVector<GraphCacheDependencyInfo>> m_nodeDependenciesPool;

        ArenaVector<uint32_t> m_highlightDependencyIndices;
    };

}  // namespace rps

#endif  //RPS_GRAPH_CANVAS_HPP
