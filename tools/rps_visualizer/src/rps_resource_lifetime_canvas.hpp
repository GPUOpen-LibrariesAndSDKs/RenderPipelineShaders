// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_RESOURCE_LIFETIME_CANVAS_HPP
#define RPS_RESOURCE_LIFETIME_CANVAS_HPP

#include "core/rps_util.hpp"
#include "runtime/common/rps_render_graph_resource.hpp"

#include "imgui.h"

namespace rps
{
    struct DrawingState;
    struct VisualizerUpdateContext;
    struct RenderGraphVisualizationData;
    struct SubResourceAccessInfo;
    struct CmdVisualizationInfo;

    class ResourceLifetimesCanvas
    {
    private:
        enum class ResourceInspectionLevel : uint32_t
        {
            Resource,
            TemporalSlice,
            ArrayLayer,
            MipLevel,
        };

        enum class ResourceField : int
        {
            Name,
            Size,
            LifetimeStart,
            LifetimeEnd,
            LifetimeLength,
        };

        struct ResourceTableRowInfo
        {
            uint32_t                resourceIndex;
            uint32_t                childIndex : 30;
            ResourceInspectionLevel inspectionLevel : 2;
            SubresourceRangePacked  subResRange;

            bool operator==(const ResourceTableRowInfo& other) const
            {
                return ((resourceIndex == other.resourceIndex) && (inspectionLevel == other.inspectionLevel) &&
                        (childIndex == other.childIndex)) &&
                       (subResRange == other.subResRange);
            }
        };

        struct AccessVisInfo
        {
            AccessAttr access;
            uint32_t   timelinePos;
        };

        struct TransitionAccessVisInfo
        {
            AccessAttr             beforeAccess;
            AccessAttr             afterAccess;
            SubresourceRangePacked range;
        };

        struct TransitionVisInfo
        {
            Span<TransitionAccessVisInfo> transitions;
            uint32_t                      timelinePos;
        };

        struct ResourceTimelineCacheEntry
        {
            ResourceTableRowInfo    rowInfo;
            ImVec2                  clipYRange;
            ImVec2                  rowYRange;
            Span<AccessVisInfo>     accessInfos;
            Span<TransitionVisInfo> transitionInfos;

            bool Update(ResourceTableRowInfo inRowInfo, ImVec2 inClipYRange, ImVec2 inRowYRange)
            {
                bool bCacheDirty = !(rowInfo == inRowInfo);

                rowInfo    = inRowInfo;
                clipYRange = inClipYRange;
                rowYRange  = inRowYRange;

                return bCacheDirty;
            }
        };

        struct ToolTipInfo
        {
            const CmdVisualizationInfo* pCmdVisInfo;
            const ResourceTableRowInfo* pRowInfo;
            const AccessVisInfo*        pAccessVisInfo;
            const TransitionVisInfo*    pTransitionVisInfo;

            void SetCmdAccess(const ResourceTableRowInfo* pInRowInfo, const AccessVisInfo* pInAccessVisInfo)
            {
                pRowInfo           = pInRowInfo;
                pAccessVisInfo     = pInAccessVisInfo;
                pTransitionVisInfo = nullptr;
            }

            void SetTransition(const ResourceTableRowInfo* pInRowInfo, const TransitionVisInfo* pInTransitionVisInfo)
            {
                pRowInfo           = pInRowInfo;
                pAccessVisInfo     = nullptr;
                pTransitionVisInfo = pInTransitionVisInfo;
            }
        };

    public:
        ResourceLifetimesCanvas()
            : m_resourceAccessListPool(m_resourceAccessLists)
            , m_transitionAccessListPool(m_transitionAccessLists)
            , m_transitionListPool(m_transitionLists)
        {
        }

        void Update(const VisualizerUpdateContext& context);

        void Draw(DrawingState& state);

    private:
        void ReorderResources(const RenderGraphVisualizationData& visData);
        void FilterResource(const RenderGraphVisualizationData& visData);

        void UpdateResourceTimelineCache(DrawingState& state);

        bool DrawResourceTableHeader(DrawingState& state);
        void DrawResourceTable(DrawingState& state);
        void DrawResourceTableRow(DrawingState& state, const ResourceTableRowInfo& rowInfo);
        void DrawResourceTimelines(DrawingState& state);

        void DrawAccessDiscardMarkers(const DrawingState&  state,
                                      ImDrawList*          pDrawList,
                                      const UIRect&        accessRect,
                                      const AccessVisInfo& accessInfo);

        void DrawToolTip(DrawingState& state, const ToolTipInfo& toolTipInfo) const;

        static bool ResourceRowIntersectsAccess(const DrawingState&          state,
                                                const ResourceTableRowInfo&  rowInfo,
                                                const SubResourceAccessInfo& access);

        static uint32_t CalculateChildExpansion(const RpsResourceDiagnosticInfo& resourceInfo,
                                                ResourceInspectionLevel          currLevel,
                                                ResourceInspectionLevel&         outChildLevel);

    private:
        ArenaVector<uint32_t> m_sortedResourceIndices;
        ArenaVector<uint32_t> m_filteredResourceIndices;

        ArenaVector<ResourceTimelineCacheEntry> m_resourceTimelineCache;
        ResourceTableRowInfo                    m_selectedRowInfo = {RPS_RESOURCE_ID_INVALID};
        ResourceTableRowInfo                    m_hoveringRowInfo = {RPS_RESOURCE_ID_INVALID};
        struct SelectedAccessInfo
        {
            ResourceTableRowInfo rowInfo;
            uint32_t             timelinePos;
        } m_selectedAccess = {{RPS_RESOURCE_ID_INVALID}};

        ArenaVector<AccessVisInfo>                          m_resourceAccessLists;
        SpanPool<AccessVisInfo, ArenaVector<AccessVisInfo>> m_resourceAccessListPool;

        ArenaVector<TransitionAccessVisInfo>                                    m_transitionAccessLists;
        SpanPool<TransitionAccessVisInfo, ArenaVector<TransitionAccessVisInfo>> m_transitionAccessListPool;

        ArenaVector<TransitionVisInfo>                              m_transitionLists;
        SpanPool<TransitionVisInfo, ArenaVector<TransitionVisInfo>> m_transitionListPool;

        struct ResourceOrderingSpec
        {
            ResourceField fieldId;
            bool          bDescending;
        };

        ResourceOrderingSpec m_resourceSortMode   = {ResourceField::Name, false};
        std::string          m_resourceFilterText = "";

        struct ResourceTableUpdateState
        {
            uint32_t numVisibleRows;
            bool     bCacheDirty;
            bool     bPendingResourceReorder;
            bool     bPendingResourceFilter;
        };

        ResourceTableUpdateState m_tableUpdateState = {};

        float m_viewHeight = -1.0f;
    };

}  // namespace rps

#endif  //RPS_RESOURCE_LIFETIME_CANVAS_HPP
