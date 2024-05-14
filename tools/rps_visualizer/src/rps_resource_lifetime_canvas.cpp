// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "rps/runtime/common/rps_runtime.h"

#include "core/rps_util.hpp"

#include "rps_overlay_state.hpp"
#include "rps_resource_lifetime_canvas.hpp"
#include "rps_visualizer.hpp"

#include "rps_imgui_helpers.hpp"

#include <numeric>

IMGUI_API const char* ImStristr(const char* haystack,
                                const char* haystack_end,
                                const char* needle,
                                const char* needle_end);

namespace rps
{

    void ResourceLifetimesCanvas::Update(const VisualizerUpdateContext& context)
    {
        m_sortedResourceIndices.reset_keep_capacity(&context.persistentArena);
        m_filteredResourceIndices.reset_keep_capacity(&context.persistentArena);
        m_resourceTimelineCache.reset_keep_capacity(&context.persistentArena);
        m_resourceAccessLists.reset_keep_capacity(&context.persistentArena);
        m_transitionAccessLists.reset_keep_capacity(&context.persistentArena);
        m_transitionLists.reset_keep_capacity(&context.persistentArena);
        m_resourceAccessListPool.reset();
        m_transitionAccessListPool.reset();
        m_transitionListPool.reset();

        m_sortedResourceIndices.resize(context.visualizationData.resourceInfos.size());
        std::iota(m_sortedResourceIndices.begin(), m_sortedResourceIndices.end(), 0);

        ReorderResources(context.visualizationData);
        FilterResource(context.visualizationData);
    }

    template <typename TKeyGetter>
    void SortResource(TKeyGetter                               keyGetter,
                      ConstArrayRef<RpsResourceDiagnosticInfo> resourceInfos,
                      ArrayRef<uint32_t>                       sortedIndices,
                      bool                                     bDescending)
    {
        std::stable_sort(sortedIndices.begin(), sortedIndices.end(), [&](uint32_t leftIndex, uint32_t rightIndex) {
            const RpsResourceDiagnosticInfo& left  = resourceInfos[leftIndex];
            const RpsResourceDiagnosticInfo& right = resourceInfos[rightIndex];
            return bDescending ? (keyGetter(left) > keyGetter(right)) : (keyGetter(left) < keyGetter(right));
        });
    }

    void ResourceLifetimesCanvas::ReorderResources(const RenderGraphVisualizationData& visData)
    {
        const bool bDescending = m_resourceSortMode.bDescending;

        auto resourceInfos = visData.resourceInfos;

        if (resourceInfos.empty())
        {
            return;
        }

        RPS_ASSERT(m_sortedResourceIndices.size() == resourceInfos.size());

        auto sortByName = [&](size_t leftIndex, size_t rightIndex) {
            const RpsResourceDiagnosticInfo& left  = resourceInfos[leftIndex];
            const RpsResourceDiagnosticInfo& right = resourceInfos[rightIndex];

            const int cmpResult = strcmp(left.name ? left.name : "", right.name ? right.name : "");
            return bDescending ? (cmpResult > 0) : (cmpResult < 0);
        };

        switch (m_resourceSortMode.fieldId)
        {
        case ResourceField::Name:
            std::stable_sort(m_sortedResourceIndices.begin(), m_sortedResourceIndices.end(), sortByName);
            break;
        case ResourceField::Size:
            SortResource([&](const RpsResourceDiagnosticInfo& target) { return target.allocRequirement.size; },
                         resourceInfos,
                         m_sortedResourceIndices.range_all(),
                         bDescending);
            break;
        case ResourceField::LifetimeStart:
            SortResource([&](const RpsResourceDiagnosticInfo& target) { return target.lifetimeBegin; },
                         resourceInfos,
                         m_sortedResourceIndices.range_all(),
                         bDescending);
            break;
        case ResourceField::LifetimeEnd:
            SortResource([&](const RpsResourceDiagnosticInfo& target) { return target.lifetimeEnd; },
                         resourceInfos,
                         m_sortedResourceIndices.range_all(),
                         bDescending);
            break;
        case ResourceField::LifetimeLength:
            SortResource(
                [&](const RpsResourceDiagnosticInfo& target) { return target.lifetimeEnd - target.lifetimeBegin; },
                resourceInfos,
                m_sortedResourceIndices.range_all(),
                bDescending);
            break;
        }
    }

    void ResourceLifetimesCanvas::FilterResource(const RenderGraphVisualizationData& visData)
    {
        m_filteredResourceIndices.clear();

        auto resourceInfos = visData.resourceInfos;

        if (!m_resourceFilterText.empty())
        {
            for (uint32_t idx : m_sortedResourceIndices)
            {
                if (ImStristr(resourceInfos[idx].name,
                              nullptr,
                              m_resourceFilterText.c_str(),
                              m_resourceFilterText.c_str() + m_resourceFilterText.length()))
                {
                    m_filteredResourceIndices.push_back(idx);
                }
            }
        }
    }

    static bool IsTemporalParent(const RpsResourceDiagnosticInfo& resourceInfo)
    {
        return (resourceInfo.desc.temporalLayers > 1) && (resourceInfo.temporalChildIndex != RPS_INDEX_NONE_U32);
    }

    bool ResourceLifetimesCanvas::ResourceRowIntersectsAccess(const DrawingState&          state,
                                                              const ResourceTableRowInfo&  rowInfo,
                                                              const SubResourceAccessInfo& access)
    {
        const auto& rowResourceInfo = state.visData.resourceInfos[rowInfo.resourceIndex];

        const bool bResourceMatch =
            (rowInfo.resourceIndex == access.resourceId) ||
            (IsTemporalParent(rowResourceInfo) && (access.resourceId >= rowResourceInfo.temporalChildIndex) &&
             (access.resourceId < (rowResourceInfo.temporalChildIndex + rowResourceInfo.desc.temporalLayers)));

        return bResourceMatch && SubresourceRangePacked::Intersect(rowInfo.subResRange, access.subresourceRange);
    }

    void ResourceLifetimesCanvas::UpdateResourceTimelineCache(DrawingState& state)
    {
        m_resourceAccessLists.clear();
        m_transitionAccessLists.clear();
        m_transitionLists.clear();
        m_resourceAccessListPool.reset();
        m_transitionAccessListPool.reset();
        m_transitionListPool.reset();

        const RenderGraphVisualizationData& visData = state.visData;

        for (auto& resCacheEntry : m_resourceTimelineCache)
        {
            resCacheEntry.accessInfos     = {};
            resCacheEntry.transitionInfos = {};
        }

        for (uint32_t iCmd = 0; iCmd < visData.cmdVisInfos.size(); iCmd++)
        {
            const auto& cmdVisInfo = visData.cmdVisInfos[iCmd];
            if (cmdVisInfo.isTransition)
            {
                // Before / After Access
                RPS_ASSERT(cmdVisInfo.accesses.empty() || (cmdVisInfo.accesses.size() == 2));

                if (cmdVisInfo.accesses.size() == 2)
                {
                    auto transitionAccesses = cmdVisInfo.accesses.Get(visData.accessInfos);

                    for (auto& resCacheEntry : m_resourceTimelineCache)
                    {
                        if (ResourceRowIntersectsAccess(state, resCacheEntry.rowInfo, transitionAccesses[0]) ||
                            ResourceRowIntersectsAccess(state, resCacheEntry.rowInfo, transitionAccesses[1]))
                        {
                            auto cacheEntryTransitionList = resCacheEntry.transitionInfos.Get(m_transitionLists);

                            if (cacheEntryTransitionList.empty() ||
                                (cacheEntryTransitionList.back().timelinePos != cmdVisInfo.timelinePosition))
                            {
                                m_transitionListPool.push_to_span(resCacheEntry.transitionInfos,
                                                                  TransitionVisInfo{{}, cmdVisInfo.timelinePosition});

                                cacheEntryTransitionList = resCacheEntry.transitionInfos.Get(m_transitionLists);
                            }

                            m_transitionAccessListPool.push_to_span(
                                cacheEntryTransitionList.back().transitions,
                                TransitionAccessVisInfo{transitionAccesses[0].access,
                                                        transitionAccesses[1].access,
                                                        transitionAccesses[1].subresourceRange});
                        }
                    }
                }
            }
            else
            {
                for (auto& access : cmdVisInfo.accesses.Get(visData.accessInfos))
                {
                    for (auto& resCacheEntry : m_resourceTimelineCache)
                    {
                        if (ResourceRowIntersectsAccess(state, resCacheEntry.rowInfo, access))
                        {
                            auto cacheEntryAccessList = resCacheEntry.accessInfos.Get(m_resourceAccessLists);

                            if (!cacheEntryAccessList.empty() &&
                                (cacheEntryAccessList.back().timelinePos == cmdVisInfo.timelinePosition))
                            {
                                cacheEntryAccessList.back().access |= access.access;
                            }
                            else
                            {
                                m_resourceAccessListPool.push_to_span(
                                    resCacheEntry.accessInfos,
                                    AccessVisInfo{access.access, cmdVisInfo.timelinePosition});
                            }
                        }
                    }
                }
            }
        }
    }

    static int ImGuiInputTextCb(ImGuiInputTextCallbackData* data)
    {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
        {
            std::string* pStr = static_cast<std::string*>(data->UserData);
            RPS_ASSERT(data->Buf == pStr->c_str());
            pStr->resize(data->BufTextLen);
            data->Buf = &(*pStr)[0];
        }
        return 0;
    }

    void ResourceLifetimesCanvas::Draw(DrawingState& state)
    {
        m_tableUpdateState = {};

        if (DrawResourceTableHeader(state))
        {
            DrawResourceTable(state);

            DrawResourceTimelines(state);

            custom_imgui::DrawHorizontalSplitter("##ResTblHSplitter", m_viewHeight);
        }

        if (m_tableUpdateState.bPendingResourceReorder)
        {
            ReorderResources(state.visData);
            m_tableUpdateState.bPendingResourceFilter = true;
        }

        if (m_tableUpdateState.bPendingResourceFilter)
        {
            FilterResource(state.visData);
        }
    }

    bool ResourceLifetimesCanvas::DrawResourceTableHeader(DrawingState& state)
    {
        bool bExpanded = false;

        if (ImGui::BeginTable("Resource_Table_Header", 2, ImGuiTableFlags_Resizable))
        {
            ImGui::TableNextColumn();
            bExpanded = ImGui::CollapsingHeader("Resources");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            m_tableUpdateState.bPendingResourceFilter |= ImGui::InputText("##ResourceSearchBox",
                                                                          &m_resourceFilterText[0],
                                                                          m_resourceFilterText.capacity(),
                                                                          ImGuiInputTextFlags_CallbackResize,
                                                                          &ImGuiInputTextCb,
                                                                          &m_resourceFilterText);
            ImGui::EndTable();
        }

        const ImVec2 resourceTableTopLeft     = ImGui::GetCursorPos();
        const float  resourceViewHeaderHeight = resourceTableTopLeft.y - state.resourceHeaderTopY;

        ImGui::TableNextColumn();
        state.resourceHeaderBottomY   = resourceTableTopLeft.y;
        state.rightPaneTopLeftScr.x   = ImGui::GetCursorScreenPos().x;
        state.rightPaneWidth          = ImGui::GetColumnWidth();
        state.rightPaneRightScr       = state.rightPaneTopLeftScr.x + state.rightPaneWidth;
        state.rightPaneBodyTopY       = state.rightPaneTopLeftScr.y + resourceViewHeaderHeight;
        state.rightPaneBottomRightScr = ImGui::GetWindowDrawList()->GetClipRectMax();

        // May have 1 frame delay
        const bool bTimelineScrollVisible =
            state.timelineZoom.GetTotalRangeInPixels() > (state.rightPaneRightScr - state.rightPaneTopLeftScr.x);
        const float scrollBarSize = ImGui::GetStyle().ScrollbarSize;

        state.rightPaneBodyBottomY = state.rightPaneBottomRightScr.y - (bTimelineScrollVisible ? scrollBarSize : 0);

        ImGui::TableSetColumnIndex(0);
        ImGui::SetCursorPos(resourceTableTopLeft);

        const float resTblFrameHeight = ImGui::GetWindowHeight();
        state.defaultSubViewHeight    = resTblFrameHeight - resourceViewHeaderHeight * 2;

        m_viewHeight = (m_viewHeight < 0.0f) ? state.defaultSubViewHeight : m_viewHeight;

        return bExpanded;
    }

    void ResourceLifetimesCanvas::DrawResourceTable(DrawingState& state)
    {
        ImGui::BeginChild("Resource_Table_Frame", ImVec2(0.0f, m_viewHeight), false);

        auto resourceInfos = state.visData.resourceInfos;

        static constexpr ImGuiTableFlags ResourceTableFlags =
            ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

        if (ImGui::BeginTable("Resource_Table", 5, ResourceTableFlags, ImVec2(0.0f, 0.0f)))
        {
            ImGui::TableSetupScrollFreeze(0, 1);

            static constexpr struct
            {
                const char*   name;
                ResourceField orderMode;
            } columnInfos[] = {
                {"Name", ResourceField::Name},
                {"Size (KiB)", ResourceField::Size},
                {"Begin Cmd", ResourceField::LifetimeStart},
                {"End Cmd", ResourceField::LifetimeEnd},
                {"Lifetime", ResourceField::LifetimeLength},
            };

            for (uint32_t iCol = 0; iCol < RPS_COUNTOF(columnInfos); iCol++)
            {
                ImGui::TableSetupColumn(
                    columnInfos[iCol].name, ImGuiTableColumnFlags_None, 0.0f, ImGuiID(columnInfos[iCol].orderMode));
            }

            ImGui::TableHeadersRow();

            if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs())
            {
                if (sortSpecs->SpecsDirty && (sortSpecs->SpecsCount == 1))
                {
                    m_resourceSortMode.fieldId = ResourceField(sortSpecs->Specs[0].ColumnUserID);
                    m_resourceSortMode.bDescending =
                        (sortSpecs->Specs[0].SortDirection == ImGuiSortDirection_Descending);

                    m_tableUpdateState.bPendingResourceReorder = true;
                    sortSpecs->SpecsDirty                      = false;
                }
            }

            auto isTemporalChild = [](const RpsResourceDiagnosticInfo& resourceInfo) {
                return ((resourceInfo.desc.temporalLayers > 1) &&
                        (resourceInfo.temporalChildIndex == RPS_RESOURCE_ID_INVALID));
            };

            //Construct named rectangles for all resources with their lifetimes
            auto activeResourceIndices = m_resourceFilterText.empty() ? m_sortedResourceIndices.crange_all()
                                                                      : m_filteredResourceIndices.crange_all();

            ResourceTableRowInfo rowInfo = {};

            for (uint32_t resIdxIdx = 0; resIdxIdx < activeResourceIndices.size(); ++resIdxIdx)
            {
                const uint32_t                   resourceIndex = activeResourceIndices[resIdxIdx];
                const RpsResourceDiagnosticInfo& resourceInfo  = resourceInfos[resourceIndex];

                if (isTemporalChild(resourceInfo))
                    continue;

                rowInfo.resourceIndex = resourceIndex;
                rowInfo.subResRange   = {};

                if (resourceInfo.desc.type != RPS_RESOURCE_TYPE_BUFFER)
                {
                    rowInfo.subResRange.arrayLayerEnd = resourceInfo.desc.image.arrayLayers;
                    rowInfo.subResRange.mipLevelEnd   = resourceInfo.desc.image.mipLevels;
                }

                DrawResourceTableRow(state, rowInfo);
            }

            ImGui::EndTable();
        }

        ImGui::EndChild();
    }

    uint32_t ResourceLifetimesCanvas::CalculateChildExpansion(const RpsResourceDiagnosticInfo& resourceInfo,
                                                              ResourceInspectionLevel          currLevel,
                                                              ResourceInspectionLevel&         outChildLevel)
    {
        uint32_t childCount = 0;

        outChildLevel = ResourceInspectionLevel(int32_t(currLevel) + 1);

        const auto& resDesc = resourceInfo.desc;

        if ((currLevel == ResourceInspectionLevel::Resource) && IsTemporalParent(resourceInfo))
        {
            outChildLevel = ResourceInspectionLevel::TemporalSlice;
            childCount    = resDesc.temporalLayers;
        }

        if (resDesc.type != RPS_RESOURCE_TYPE_BUFFER)
        {
            if ((childCount == 0) && (resDesc.image.arrayLayers > 1) &&
                (int32_t(currLevel) < int32_t(ResourceInspectionLevel::ArrayLayer)))
            {
                outChildLevel = ResourceInspectionLevel::ArrayLayer;
                childCount    = resDesc.image.arrayLayers;
            }

            if ((childCount == 0) && (resDesc.image.mipLevels > 1) &&
                (int32_t(currLevel) < int32_t(ResourceInspectionLevel::MipLevel)))
            {
                outChildLevel = ResourceInspectionLevel::MipLevel;
                childCount    = resDesc.image.mipLevels;
            }
        }

        return childCount;
    }

    void ResourceLifetimesCanvas::DrawResourceTableRow(DrawingState& state, const ResourceTableRowInfo& rowInfo)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        ImDrawList*  pDrawList       = ImGui::GetWindowDrawList();
        const ImVec2 prevClipRectMin = pDrawList->GetClipRectMin();
        const ImVec2 prevClipRectMax = pDrawList->GetClipRectMax();

        const RpsResourceDiagnosticInfo& resourceInfo    = state.visData.resourceInfos[rowInfo.resourceIndex];
        const ResourceVisualizationInfo& resourceVisInfo = state.visData.resourceVisInfos[rowInfo.resourceIndex];

        StrBuilder<> sb;
        switch (rowInfo.inspectionLevel)
        {
        case ResourceInspectionLevel::Resource:
            sb.AppendFormat("%s", resourceInfo.name);
            break;
        case ResourceInspectionLevel::TemporalSlice:
            if (rowInfo.childIndex == 0)
                sb.Append("Temporal Layer: Current");
            else
                sb.AppendFormat("Temporal Layer: N - %d", rowInfo.childIndex);
            break;
        case ResourceInspectionLevel::ArrayLayer:
            sb.AppendFormat("Array [%d]", rowInfo.childIndex);
            break;
        case ResourceInspectionLevel::MipLevel:
            sb.AppendFormat("Mip [%d]", rowInfo.childIndex);
            break;
        }

        const ImVec2 rowTopLeftScr = ImGui::GetCursorScreenPos() - ImGui::GetStyle().CellPadding;

        if (ImGui::Selectable("##ResTblRowSel",
                              (m_selectedRowInfo == rowInfo),
                              ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap))
        {
            m_selectedRowInfo = rowInfo;
        }

        if (ImGui::IsItemHovered())
        {
            m_hoveringRowInfo = rowInfo;

            // Unsure why Selectable not selecting rows > 1. Manually select on click:
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                m_selectedRowInfo = rowInfo;
            }
        }

        ImGui::SameLine();

        ResourceInspectionLevel childLevel = {};
        const uint32_t          childCount = CalculateChildExpansion(resourceInfo, rowInfo.inspectionLevel, childLevel);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, ImGui::GetStyle().FramePadding.y));
        const bool bExpanded =
            ImGui::TreeNodeEx(sb.c_str(), (childCount > 0) ? ImGuiTreeNodeFlags_None : ImGuiTreeNodeFlags_Leaf);
        ImGui::PopStyleVar();

        if (ImGui::IsItemHovered() && state.settings.bDrawResourceTooltips)
        {
            StrBuilder<> sb;
            ResourceDescPacked(resourceInfo.desc).Print(sb.AsPrinter());

            ImGui::BeginTooltip();
            ImGui::TextUnformatted(sb.c_str());
            ImGui::EndTooltip();
        }

        const float rowBottomYScr = ImGui::GetCursorScreenPos().y - ImGui::GetStyle().CellPadding.y;

        if (ImGui::TableNextColumn() &&
            (int32_t(rowInfo.inspectionLevel) <= int32_t(ResourceInspectionLevel::TemporalSlice)))
        {
            ImGui::Text("0x%" PRIx64, resourceInfo.allocRequirement.size / 1024);
            ImGui::IsItemVisible();
        }

        if (ImGui::TableNextColumn())
        {
            ImGui::Text("%d", resourceVisInfo.lifetimeBegin);
        }

        if (ImGui::TableNextColumn())
        {
            ImGui::Text("%d", resourceVisInfo.lifetimeEnd);
        }

        if (ImGui::TableNextColumn())
        {
            ImGui::Text("%d", resourceVisInfo.lifetimeEnd - resourceVisInfo.lifetimeBegin);
        }

        const bool bRowVisible = (rowBottomYScr >= prevClipRectMin.y) && (rowTopLeftScr.y <= prevClipRectMax.y);

        if (bRowVisible)
        {
            const float clipRectTopY = rpsMax(prevClipRectMin.y, state.rightPaneBodyTopY);

            if (m_tableUpdateState.numVisibleRows == m_resourceTimelineCache.size())
            {
                m_resourceTimelineCache.emplace_back(ResourceTimelineCacheEntry{});
                m_tableUpdateState.bCacheDirty = true;
            }

            RPS_ASSERT(m_tableUpdateState.numVisibleRows < m_resourceTimelineCache.size());

            m_tableUpdateState.bCacheDirty |= m_resourceTimelineCache[m_tableUpdateState.numVisibleRows].Update(
                rowInfo, ImVec2(clipRectTopY, prevClipRectMax.y), ImVec2(rowTopLeftScr.y, rowBottomYScr));
            m_tableUpdateState.numVisibleRows++;
        }

        if (bExpanded)
        {
            ResourceTableRowInfo childRowInfo = rowInfo;
            childRowInfo.inspectionLevel      = childLevel;

            for (uint32_t childIndex = 0; childIndex < childCount; childIndex++)
            {
                childRowInfo.childIndex = childIndex;

                switch (childLevel)
                {
                case ResourceInspectionLevel::TemporalSlice:
                    RPS_ASSERT(resourceInfo.temporalChildIndex != RPS_INDEX_NONE_U32);
                    childRowInfo.resourceIndex = resourceInfo.temporalChildIndex + childIndex;
                    break;
                case ResourceInspectionLevel::ArrayLayer:
                    childRowInfo.subResRange.baseArrayLayer = childIndex;
                    childRowInfo.subResRange.arrayLayerEnd  = childIndex + 1;
                    break;
                case ResourceInspectionLevel::MipLevel:
                    childRowInfo.subResRange.baseMipLevel = childIndex;
                    childRowInfo.subResRange.mipLevelEnd  = childIndex + 1;
                    break;
                default:
                    RPS_ASSERT(false && "Unexpected childLevel");
                    break;
                }

                DrawResourceTableRow(state, childRowInfo);
            }

            ImGui::TreePop();
        }
    }

    void ResourceLifetimesCanvas::DrawAccessDiscardMarkers(const DrawingState&  state,
                                                           ImDrawList*          pDrawList,
                                                           const UIRect&        accessRect,
                                                           const AccessVisInfo& accessInfo)
    {
        static constexpr float OneOverSqrt3       = 0.57735f;
        static constexpr ImU32 DiscardMarkerColor = IM_COL32(0xff, 0, 0, 0xff);
        static constexpr ImU32 DiscardMarkerBorderColor = IM_COL32(0x7f, 0, 0, 0xff);

        static constexpr RpsAccessFlags DiscardBeforeFlags =
            (RPS_ACCESS_DISCARD_DATA_BEFORE_BIT | RPS_ACCESS_STENCIL_DISCARD_DATA_BEFORE_BIT);
        static constexpr RpsAccessFlags DiscardAfterFlags =
            (RPS_ACCESS_DISCARD_DATA_AFTER_BIT | RPS_ACCESS_STENCIL_DISCARD_DATA_AFTER_BIT);

        const float triHeight = (accessRect.end.y - accessRect.start.y) * 0.35f;

        if (accessInfo.access.accessFlags & DiscardBeforeFlags)
        {
            ImVec2 markerTri[3] = {
                {accessRect.start.x, accessRect.start.y},
                {accessRect.start.x + triHeight, accessRect.start.y},
                {accessRect.start.x, accessRect.start.y + triHeight},
            };
            pDrawList->PushClipRect(accessRect.start, accessRect.end + ImVec2(1, 0), true);
            pDrawList->AddTriangleFilled(markerTri[0], markerTri[1], markerTri[2], DiscardMarkerColor);
            pDrawList->AddTriangle(markerTri[0], markerTri[1], markerTri[2], DiscardMarkerBorderColor);
            pDrawList->PopClipRect();

            if (ImTriangleContainsPoint(markerTri[0], markerTri[1], markerTri[2], state.mousePosScr))
            {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted("Previous data discarded.");
                ImGui::EndTooltip();
            }
        }

        if (accessInfo.access.accessFlags & DiscardAfterFlags)
        {
            ImVec2 markerTri[3] = {
                {accessRect.end.x - triHeight, accessRect.end.y},
                {accessRect.end.x, accessRect.end.y - triHeight},
                {accessRect.end.x, accessRect.end.y},
            };

            pDrawList->PushClipRect(accessRect.start, accessRect.end + ImVec2(1, 0), true);
            pDrawList->AddTriangleFilled(markerTri[0], markerTri[1], markerTri[2], DiscardMarkerColor);
            pDrawList->AddTriangle(markerTri[0], markerTri[1], markerTri[2], DiscardMarkerBorderColor);
            pDrawList->PopClipRect();

            if (ImTriangleContainsPoint(markerTri[0], markerTri[1], markerTri[2], state.mousePosScr))
            {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted("Data discarded afterwards.");
                ImGui::EndTooltip();
            }
        }
    }

    void ResourceLifetimesCanvas::DrawResourceTimelines(DrawingState& state)
    {
        if (m_tableUpdateState.numVisibleRows < m_resourceTimelineCache.size())
        {
            m_resourceTimelineCache.resize(m_tableUpdateState.numVisibleRows);
        }

        if (m_tableUpdateState.bCacheDirty)
        {
            UpdateResourceTimelineCache(state);

            m_tableUpdateState.bCacheDirty = false;
        }

        const float timelineLeftX  = state.rightPaneTopLeftScr.x;
        const float timelineRightX = state.rightPaneRightScr;

        const uint32_t visibleTimelineBegin = uint32_t(state.timelineZoom.GetVisibleRangeBegin());
        const uint32_t visibleTimelineEnd   = uint32_t(state.timelineZoom.GetVisibleRangeEnd());
        const float    unitPixels           = state.timelineZoom.UnitsToPixels(1);

        const ImU32 timelineColor    = ImGui::GetColorU32(ImGuiCol_PlotLines);
        const ImU32 selectedRowBgCol = ImGui::GetColorU32(ImGuiCol_Header);
        const ImU32 hoveredRowBgCol  = ImGui::GetColorU32(ImGuiCol_HeaderHovered, 0.5f);

        float rowSpacing = 1.0f;

        auto* pDrawList = ImGui::GetWindowDrawList();

        const bool bLButtonDblClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

        // Clear selection on click. New selection will be determined below.
        if (bLButtonDblClicked)
        {
            m_selectedAccess = {ResourceTableRowInfo{RPS_RESOURCE_ID_INVALID}};
        }

        ImVec2 selectedAccessClipYRange = {};
        ImVec2 selectedAccessRowYRange  = {};

        const CmdVisualizationInfo* const pHoveredCmd = OverlayState::PickCmdVisInfoAtMouseCursor(state);

        ToolTipInfo toolTip = {};
        toolTip.pCmdVisInfo = pHoveredCmd;

        bool bHoveringOnAnyRow = false;

        for (uint32_t iRow = 0; iRow < m_resourceTimelineCache.size(); iRow++)
        {
            const auto& cacheEntry = m_resourceTimelineCache[iRow];

            const float rowCenterY = (cacheEntry.rowYRange.x + cacheEntry.rowYRange.y) * 0.5f;

            const UIRect rowRectNoSpacing = {ImVec2(timelineLeftX, cacheEntry.rowYRange.x),
                                             ImVec2(timelineRightX, cacheEntry.rowYRange.y)};

            const UIRect rowRect = {ImVec2(timelineLeftX, cacheEntry.rowYRange.x + rowSpacing),
                                    ImVec2(timelineRightX, cacheEntry.rowYRange.y - rowSpacing)};

            ImGui::PushClipRect(
                ImVec2(timelineLeftX, cacheEntry.clipYRange.x), ImVec2(timelineRightX, cacheEntry.clipYRange.y), false);

            const bool bHoveringRow = ImGui::IsMouseHoveringRect(rowRect.start, rowRect.end, true);
            bHoveringOnAnyRow |= bHoveringRow;

            if (bHoveringRow)
            {
                m_hoveringRowInfo = cacheEntry.rowInfo;

                // Select on click
                if (bLButtonDblClicked)
                {
                    m_selectedRowInfo = cacheEntry.rowInfo;
                }
            }

            // Draw background of hovered / selected row
            if (bHoveringRow || (m_hoveringRowInfo == cacheEntry.rowInfo))
            {
                pDrawList->AddRectFilled(rowRectNoSpacing.start, rowRectNoSpacing.end, hoveredRowBgCol);
            }
            else if (m_selectedRowInfo == cacheEntry.rowInfo)
            {
                pDrawList->AddRectFilled(rowRectNoSpacing.start, rowRectNoSpacing.end, selectedRowBgCol);
            }

            float prevAccessRight = 0.0f;

            const auto accessList = cacheEntry.accessInfos.Get(m_resourceAccessLists);

            for (uint32_t iAccess = 0; iAccess < accessList.size(); iAccess++)
            {
                const auto& access = accessList[iAccess];

                const bool bVisible =
                    (access.timelinePos >= visibleTimelineBegin) && (access.timelinePos < visibleTimelineEnd);

                const float currAccessLeft =
                    (int32_t(access.timelinePos) - int32_t(visibleTimelineBegin)) * unitPixels + timelineLeftX;

                if (state.settings.bDrawResourceConnectors && (iAccess > 0) && (prevAccessRight < currAccessLeft) &&
                    (access.timelinePos >= visibleTimelineBegin))
                {
                    pDrawList->AddLine(
                        ImVec2(prevAccessRight, rowCenterY), ImVec2(currAccessLeft, rowCenterY), timelineColor, 1.0f);
                }

                const float currAccessRight = currAccessLeft + unitPixels;

                if (bVisible)
                {
                    const UIRect rect = {ImVec2(currAccessLeft, rowRect.start.y),
                                         ImVec2(currAccessRight, rowRect.end.y)};

                    if (state.settings.bDrawResourceAccesses)
                    {
                        const auto colorScheme = GetAccessCategoryFromAccessFlags(access.access.accessFlags);

                        pDrawList->AddRectFilled(rect.start, rect.end, GetColorByAccessCategory(colorScheme));
                    }
                    else if (state.settings.bDrawResourceConnectors)
                    {
                        pDrawList->AddLine({rect.start.x, rowCenterY}, {rect.end.x, rowCenterY}, timelineColor);
                    }

                    if (state.settings.bDrawSubResourceDataLifetimeMarkers)
                    {
                        DrawAccessDiscardMarkers(state, pDrawList, rect, access);
                    }

                    if (bHoveringRow && pHoveredCmd && rect.Contains(state.mousePosScr))
                    {
                        toolTip.SetCmdAccess(&cacheEntry.rowInfo, &access);

                        if (bLButtonDblClicked)
                        {
                            m_selectedAccess = {cacheEntry.rowInfo, pHoveredCmd->timelinePosition};
                        }
                    }

                    if (m_selectedAccess.rowInfo == cacheEntry.rowInfo)
                    {
                        selectedAccessClipYRange = cacheEntry.clipYRange;
                        selectedAccessRowYRange  = cacheEntry.rowYRange;
                    }
                }

                prevAccessRight = currAccessRight;

                if (access.timelinePos > visibleTimelineEnd)
                {
                    break;
                }
            }

            const ImU32 transitionQuadColor       = IM_COL32(255, 0, 255, 255);
            const ImU32 transitionQuadBorderColor = IM_COL32(25, 25, 25, 255);

            const auto transitionList = cacheEntry.transitionInfos.Get(m_transitionLists);

            for (uint32_t iTransition = 0; iTransition < transitionList.size(); iTransition++)
            {
                const auto& transition = transitionList[iTransition];

                const bool bVisible =
                    (transition.timelinePos >= visibleTimelineBegin) && (transition.timelinePos <= visibleTimelineEnd);

                if (bVisible && state.settings.bDrawResourceTransitions)
                {
                    const float currCmdLeft =
                        (int32_t(transition.timelinePos) - int32_t(visibleTimelineBegin)) * unitPixels + timelineLeftX;
                    const float quadExtent = rowRect.GetSize().y * 0.3f;
                    const float quadTop    = rowRect.start.y + rowRect.GetSize().y * 0.2f;

                    const ImVec2 quadVertices[] = {
                        ImVec2(currCmdLeft, quadTop),
                        ImVec2(currCmdLeft + quadExtent, quadTop + quadExtent),
                        ImVec2(currCmdLeft, quadTop + quadExtent * 2),
                        ImVec2(currCmdLeft - quadExtent, quadTop + quadExtent),
                    };

                    pDrawList->AddQuadFilled(
                        quadVertices[0], quadVertices[1], quadVertices[2], quadVertices[3], transitionQuadColor);

                    pDrawList->AddQuad(
                        quadVertices[0], quadVertices[1], quadVertices[2], quadVertices[3], transitionQuadBorderColor);

                    const ImVec2 quadCenter        = {currCmdLeft, quadTop + quadExtent};
                    const ImVec2 quadCenterToMouse = (ImGui::GetMousePos() - quadCenter);

                    const ImVec2 rotatedQuadCenterToMouse = {(quadCenterToMouse.x - quadCenterToMouse.y),
                                                             (quadCenterToMouse.x + quadCenterToMouse.y)};

                    const bool bHoveringQuad = (abs(rotatedQuadCenterToMouse.x) < quadExtent) &&
                                               (abs(rotatedQuadCenterToMouse.y) < quadExtent);

                    if (bHoveringQuad && pHoveredCmd)
                    {
                        toolTip.SetTransition(&cacheEntry.rowInfo, &transition);
                    }
                }
            }

            ImGui::PopClipRect();
        }

        if (!bHoveringOnAnyRow)
        {
            m_hoveringRowInfo = {RPS_RESOURCE_ID_INVALID};
        }

        if ((m_selectedAccess.rowInfo.resourceIndex != RPS_RESOURCE_ID_INVALID) &&
            (selectedAccessClipYRange.y > selectedAccessClipYRange.x))
        {
            const float highLightAccessLeft =
                (int32_t(m_selectedAccess.timelinePos) - int32_t(visibleTimelineBegin)) * unitPixels + timelineLeftX;

            pDrawList->PushClipRect(
                ImVec2(rpsMax(highLightAccessLeft - 2, state.rightPaneTopLeftScr.x), selectedAccessClipYRange.x - 2),
                ImVec2(rpsMin(highLightAccessLeft + unitPixels + 2, state.rightPaneRightScr),
                       selectedAccessClipYRange.y + 2));

            const ImU32 selectedAccessBoxCol = ImGui::GetColorU32(ImVec4(1, 0, 0, 0.75f));

            pDrawList->AddRect(ImVec2(highLightAccessLeft, selectedAccessRowYRange.x),
                               ImVec2(highLightAccessLeft + unitPixels, selectedAccessRowYRange.y),
                               selectedAccessBoxCol,
                               0.0f,
                               ImDrawFlags_None,
                               3.0f);

            pDrawList->PopClipRect();
        }

        DrawToolTip(state, toolTip);
    }

    void ResourceLifetimesCanvas::DrawToolTip(DrawingState& state, const ToolTipInfo& toolTipInfo) const
    {
        // Draw tool tip
        if (!toolTipInfo.pCmdVisInfo || !(toolTipInfo.pTransitionVisInfo || toolTipInfo.pAccessVisInfo) ||
            !state.settings.bDrawResourceTooltips)
        {
            return;
        }

        StrBuilder<> sb;
        ImGui::BeginTooltip();

        const char* resourceName = state.visData.resourceInfos[toolTipInfo.pRowInfo->resourceIndex].name;
        if (resourceName)
        {
            sb.Reset();
            sb.Append("resource : ").Append(resourceName);
            ImGui::TextUnformatted(sb.c_str());
        }

        if (toolTipInfo.pTransitionVisInfo)
        {
            ImGui::TextUnformatted((toolTipInfo.pTransitionVisInfo->transitions.size() > 1) ? "transitions:"
                                                                                            : "transition:");

            for (const auto& transAccesses : toolTipInfo.pTransitionVisInfo->transitions.Get(m_transitionAccessLists))
            {
                sb.Reset();
                sb.Append("  range { ");
                transAccesses.range.Print(sb.AsPrinter());
                sb.Append(" } : [ ");
                transAccesses.beforeAccess.Print(sb.AsPrinter());
                sb.Append(" ] => [ ");
                transAccesses.afterAccess.Print(sb.AsPrinter());
                sb.Append(" ]");
                ImGui::TextUnformatted(sb.c_str());
            }
        }
        else if (toolTipInfo.pAccessVisInfo)
        {
            sb.Reset();
            sb.AppendFormat("node : [%u] ", toolTipInfo.pCmdVisInfo->cmdId);
            toolTipInfo.pCmdVisInfo->name.Print(sb.AsPrinter());
            ImGui::TextUnformatted(sb.c_str());

            sb.Reset();
            sb.Append("access : [ ");
            toolTipInfo.pAccessVisInfo->access.Print(sb.AsPrinter());
            sb.Append(" ]");
            ImGui::TextUnformatted(sb.c_str());
        }

        ImGui::EndTooltip();
    }

}  // namespace rps
