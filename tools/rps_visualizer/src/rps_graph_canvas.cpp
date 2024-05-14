// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "rps_graph_canvas.hpp"

#include "rps_overlay_state.hpp"
#include "rps_visualizer_util.hpp"
#include "rps_imgui_helpers.hpp"

namespace rps
{
    static void LineToCubicBezier(ImVec2 start, ImVec2 stop, ImVec2& p2, ImVec2& p3, float interpFactor = 0.6f)
    {
        interpFactor = (start.x < stop.x) ? interpFactor : -interpFactor;
        p2 = {interpFactor * stop.x + (1.f - interpFactor) * start.x, start.y};  // Bottom right when going up
        p3 = {interpFactor * start.x + (1.f - interpFactor) * stop.x, stop.y};   // Top left when going up
    }

    static ImU32 GetInactiveColor(ImU32 foregroundColor)
    {
        return ImAlphaBlendColors(foregroundColor, IM_COL32(0, 0, 0, 127));
    }

    struct CanvasDrawContext
    {
        const ZoomState& timelineZoom;
        const ZoomState& heightZoom;

        ImVec2 topLeft;
        ImVec2 bottomRight;

        float transitionTop;
        float transitionRadius;
        float nodeWidth;
        float nodeHeight;
        float cmdNodeCenter;
        float tickDistance;
        float rowHeightPadded;
        float laneHeight;

        CanvasDrawContext(DrawingState& drawState, const ZoomState& graphHeightZoom)
            : timelineZoom(drawState.timelineZoom)
            , heightZoom(graphHeightZoom)
            , nodeHeight(GraphCanvas::GetRowHeight())
            , rowHeightPadded(GraphCanvas::GetRowHeightPadded())
        {
            const ImVec2 framePadding = ImGui::GetStyle().FramePadding;
            topLeft                   = {drawState.rightPaneTopLeftScr.x, ImGui::GetCursorScreenPos().y};
            bottomRight               = {drawState.rightPaneRightScr, drawState.rightPaneBodyBottomY};

            const float zoomOffset = heightZoom.UnitsToPixels(heightZoom.GetVisibleRangeBegin());

            tickDistance  = timelineZoom.UnitsToPixels(1);
            nodeWidth     = tickDistance / 2.f;
            cmdNodeCenter = topLeft.y + framePadding.y - zoomOffset + rowHeightPadded * 0.5f;
            transitionTop = cmdNodeCenter + rowHeightPadded;
            laneHeight    = heightZoom.UnitsToPixels(1);

            transitionRadius = GetScaledQuadRadius();
        }

        ImVec2 GetNodeCenter(uint64_t timelinePos, uint32_t rowIndex, bool bTransition = false) const
        {
            return {
                GetTimelinePos(timelinePos) + (bTransition ? 0 : (tickDistance * 0.5f)),
                bTransition ? (transitionTop + laneHeight * rowIndex) : (cmdNodeCenter + rowHeightPadded * rowIndex)};
        }

        UIRect GetNodeRect(uint64_t timelinePos, uint32_t rowIndex) const
        {
            const ImVec2 center = GetNodeCenter(timelinePos, rowIndex);
            return UIRect{{center.x - nodeWidth * 0.5f, center.y - nodeHeight * 0.5f},
                          ImVec2(center.x + nodeWidth * 0.5f, center.y + nodeHeight * 0.5f)};
        }

        UIRect GetTransitionRect(uint64_t timelinePos, uint32_t rowIndex) const
        {
            const float  radius = transitionRadius;
            const ImVec2 center = GetNodeCenter(timelinePos, rowIndex, true);
            const UIRect result = {{center.x - radius, center.y - radius}, {center.x + radius, center.y + radius}};
            return result;
        }

        float GetLaneHeight(uint32_t laneIndex) const
        {
            return laneIndex ? (transitionTop + laneHeight * (laneIndex - 1)) : cmdNodeCenter;
        }

        uint32_t GetLaneIndexFromOffset(float laneOffset) const
        {
            return (laneOffset >= transitionTop) ? (uint32_t((laneOffset - transitionTop) / laneHeight) + 1) : 0;
        }

        float GetTimelinePos(uint64_t timelinePos) const
        {
            return topLeft.x + timelineZoom.UnitsToPixels(timelinePos - timelineZoom.GetVisibleRangeBegin());
        }

        void DrawConnection(const DrawingState&             drawState,
                            ImDrawList*                     pDrawList,
                            const GraphCacheDependencyInfo& dep,
                            ImU32                           color) const
        {
            //Draw in starting region of the cell to the right
            const ImVec2 beginPt = GetConnector(dep.srcTimelinePos, dep.srcRowIndex, false, dep.srcIsTransition);
            const ImVec2 endPt   = GetConnector(dep.dstTimelinePos, dep.dstRowIndex, true, dep.dstIsTransition);
            const float  dist    = endPt.x - beginPt.x;

            pDrawList->PathLineTo(beginPt);

            if (dist < tickDistance)
            {
                if ((dep.srcRowIndex == dep.dstRowIndex) && (dep.srcIsTransition == dep.dstIsTransition))
                {
                    pDrawList->PathLineTo(endPt);
                }
                else
                {
                    ImVec2 curvePoints[2];
                    LineToCubicBezier(beginPt, endPt, curvePoints[0], curvePoints[1]);
                    pDrawList->PathBezierCubicCurveTo(curvePoints[0], curvePoints[1], endPt);
                }
            }
            else
            {
                float curveFraction = GraphCanvas::GetDepCurveFraction(drawState, dep);

                float        laneHeight = GetLaneHeight(dep.laneIndex);
                const ImVec2 lineLeft   = {beginPt.x + dist * curveFraction, laneHeight};
                const ImVec2 lineRight  = {endPt.x - dist * curveFraction, laneHeight};

                ImVec2 curvePoints[2];
                LineToCubicBezier(beginPt, lineLeft, curvePoints[0], curvePoints[1]);
                pDrawList->PathBezierCubicCurveTo(curvePoints[0], curvePoints[1], lineLeft);

                pDrawList->PathLineTo(lineRight);

                LineToCubicBezier(lineRight, endPt, curvePoints[0], curvePoints[1]);
                pDrawList->PathBezierCubicCurveTo(curvePoints[0], curvePoints[1], endPt);
            }

            pDrawList->PathStroke(color, 0, GraphCanvas::ConnectionThickness);
        }

        ImVec2 GetConnector(uint32_t timelinePos, uint32_t rowIndex, bool left, bool bTransition) const
        {
            const ImVec2 center = GetNodeCenter(timelinePos, rowIndex, bTransition);
            const float  X      = center.x + (left ? -0.5f : 0.5f) * (bTransition ? transitionRadius : nodeWidth);
            return {X, center.y};
        }

        float GetScaledQuadRadius() const
        {
            const UIRect nodeRect = GetNodeRect(0, 0);
            const ImVec2 dist     = nodeRect.end - nodeRect.start;
            return 0.5f * rpsMin(rpsMin(dist.x, dist.y), laneHeight * 0.9f);
        }
    };

    RpsResult GraphCanvas::Draw(DrawingState& drawState)
    {
        RPS_RETURN_OK_IF(m_graphCache.empty());

        m_viewHeight =
            (m_viewHeight < 0) ? (drawState.defaultSubViewHeight - 2 * ImGui::GetFrameHeight()) : m_viewHeight;

        m_graphHeightZoom.SetDisplayedPixels(m_viewHeight);

        const ZoomState& timelineZoom = drawState.timelineZoom;

        auto& io = ImGui::GetIO();

        const CanvasDrawContext canvasContext{drawState, m_graphHeightZoom};

        const ImVec2 paneTopLeft     = canvasContext.topLeft;
        const ImVec2 paneBottomRight = canvasContext.bottomRight;

        const UIRect clipRect  = {{paneTopLeft.x, rpsMax(paneTopLeft.y, drawState.rightPaneBodyTopY)}, paneBottomRight};
        ImDrawList*  pDrawList = ImGui::GetWindowDrawList();

        ImGui::BeginChild("Graph", {0, m_viewHeight});

        ImGui::SetNextWindowContentSize(ImVec2(0, m_graphHeightZoom.GetTotalRangeInPixels()));

        ImGui::BeginChild("GraphZoomChild", {ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y});

        ImGui::PushClipRect(clipRect.start, clipRect.end, false);
        pDrawList->PushClipRect(clipRect.start, clipRect.end);

        const UIRect rightPaneRect          = {paneTopLeft, {paneBottomRight.x, drawState.rightPaneBodyBottomY}};
        const bool   mouseHoveringRightPane = ImGui::IsMouseHoveringRect(rightPaneRect.start, rightPaneRect.end);
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && mouseHoveringRightPane)
        {
            Unselect();
        }

        //Zoom
        const float wheel    = io.MouseWheel;
        bool        bZooming = false;
        if (mouseHoveringRightPane && io.KeyShift && (abs(wheel) > 0.1f))
        {
            if ((wheel < 0) || (m_graphHeightZoom.GetVisibleRangeUnits() > MinRowCount))
            {
                m_graphHeightZoom.ZoomByMultiplier(powf(1.1f, rpsClamp(wheel, -10.0f, 10.0f)),
                                                   drawState.mousePosScr.y - rightPaneRect.start.y);
                io.MouseWheel = 0.0f;

                bZooming = true;
            }
        }

        //Vertical Movement
        const int stepsUp =
            ImGui::GetKeyPressedAmount(ImGuiKey_W, OverlayState::ButtonRepeatDelay, OverlayState::ButtonRepeatRate);
        const int stepsDown =
            ImGui::GetKeyPressedAmount(ImGuiKey_S, OverlayState::ButtonRepeatDelay, OverlayState::ButtonRepeatRate);
        const int  stepsTotal = stepsDown - stepsUp;
        const bool bMoving    = stepsTotal != 0;
        m_graphHeightZoom.MoveByUnits(stepsTotal * CalcKeyMoveHeightStep(m_graphHeightZoom.GetVisibleRangeUnits()));

        if (bZooming || bMoving)
        {
            ImGui::SetScrollY(m_graphHeightZoom.GetScrollInPixels());
        }
        else
        {
            const float scroll = ImGui::GetScrollY();
            m_graphHeightZoom.SetScrollInPixels(scroll);
        }

        const uint32_t minVisibleRow = canvasContext.GetLaneIndexFromOffset(canvasContext.topLeft.y);

        const uint32_t visibleTimelineBegin = uint32_t(timelineZoom.GetVisibleRangeBegin());
        const uint32_t visibleTimelineEnd   = uint32_t(timelineZoom.GetVisibleRangeEnd());

        auto drawConnectionList = [&](Span<GraphCacheDependencyInfo> depsSpan, ImU32 color) {
            const auto deps = depsSpan.Get(m_nodeDependencies);

            for (const auto& dep : deps)
            {
                if ((rpsMax(dep.laneIndex, rpsMax(dep.srcRowIndex, dep.dstRowIndex)) < minVisibleRow) ||
                    (dep.dstTimelinePos < visibleTimelineBegin) || (dep.srcTimelinePos > visibleTimelineEnd))
                    continue;

                if ((dep.srcNodeId == m_highlightGraphNodeId) || (dep.dstNodeId == m_highlightGraphNodeId))
                    continue;

                canvasContext.DrawConnection(drawState, pDrawList, dep, color);
            }
        };

        //Go over visible timeline
        for (size_t timelinePos = 0; timelinePos < m_graphCache.size(); ++timelinePos)
        {
            const GraphCacheEntry& cached = m_graphCache[timelinePos];

            // Draw Connections from transitions to nodes
            drawConnectionList(cached.transToNodeDeps, ConnectionColor);

            // Draw Connections from nodes to transitions
            drawConnectionList(cached.nodeToTransDeps, ConnectionColor);

            // Draw Connections from nodes
            drawConnectionList(cached.nodeToNodeDeps, ConnectionColor);
        }

        for (auto depIdx : m_highlightDependencyIndices)
        {
            canvasContext.DrawConnection(drawState, pDrawList, m_nodeDependencies[depIdx], HighlightColor);
        }

        const uint32_t timelinePosMax =
            uint32_t(rpsMin(timelineZoom.GetVisibleRangeEnd(), uint64_t(m_graphCache.size())));

        for (size_t timelinePos = 0; timelinePos < timelinePosMax; ++timelinePos)
        {
            const GraphCacheEntry& cached = m_graphCache[timelinePos];

            //Draw all nodes
            auto nodes = cached.nodes.Get(m_nodes);

            for (uint32_t rowIndex = 0; rowIndex < cached.nodes.size(); ++rowIndex)
            {
                const GraphCacheNodeInfo& nodeInfo = nodes[rowIndex];
                const UIRect              nodeRect = canvasContext.GetNodeRect(timelinePos, rowIndex);

                pDrawList->AddRectFilled(nodeRect.start, nodeRect.end, nodeInfo.nodeColor, 0, 0);

                if (m_highlightGraphNodeId == nodeInfo.GetNodeId())
                {
                    pDrawList->AddRect(nodeRect.start, nodeRect.end, HighlightColor, 0.0f, 0, BorderThickness);
                }

                if (ImGui::IsMouseHoveringRect(nodeRect.start, nodeRect.end) && drawState.settings.bDrawGraphTooltips)
                {
                    GraphStrBuilder sb;
                    GetNodeTooltip(drawState, sb, nodeInfo);

                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted(sb.c_str());
                    ImGui::EndTooltip();

                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        Select(nodeInfo.cmdId);
                    }
                }
            }

            //Draw all transitions for the timeline pos at the end to avoid overdraw from any connections
            auto transitions = cached.transitions.Get(m_transitions);

            for (uint32_t rowIndex = 0; rowIndex < transitions.size(); ++rowIndex)
            {
                const GraphCacheTransitionInfo& transitionInfo = transitions[rowIndex];

                const UIRect quadRect    = canvasContext.GetTransitionRect(timelinePos, rowIndex);
                const ImVec2 rectCenters = {
                    (quadRect.start.x + quadRect.end.x) * 0.5f,
                    (quadRect.start.y + quadRect.end.y) * 0.5f,
                };
                const ImVec2 quadTop    = {rectCenters.x, quadRect.start.y};
                const ImVec2 quadRight  = {quadRect.end.x, rectCenters.y};
                const ImVec2 quadBottom = {rectCenters.x, quadRect.end.y};
                const ImVec2 quadLeft   = {quadRect.start.x, rectCenters.y};

                pDrawList->AddQuadFilled(
                    quadTop,
                    quadRight,
                    quadBottom,
                    quadLeft,
                    transitionInfo.IsActive() ? TransitionQuadColor : GetInactiveColor(TransitionQuadColor));

                if (transitionInfo.graphNodeId == m_highlightGraphNodeId)
                {
                    pDrawList->AddQuad(quadTop, quadRight, quadBottom, quadLeft, HighlightColor, BorderThickness);
                }

                if (ImGui::IsMouseHoveringRect({quadLeft.x, quadTop.y}, {quadRight.x, quadBottom.y}) &&
                    drawState.settings.bDrawGraphTooltips)
                {
                    GraphStrBuilder sb;
                    GetTransitionTooltip(drawState, sb, transitionInfo);
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted(sb.c_str());
                    ImGui::EndTooltip();

                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        Select(transitionInfo.graphNodeId);
                    }
                }
            }
        }

        // Top line
        const ImU32 boundsLineColor = ImGui::GetColorU32(ImGuiCol_Text);
        pDrawList->AddLine(paneTopLeft, {paneBottomRight.x, paneTopLeft.y}, boundsLineColor);

        pDrawList->PopClipRect();

        ImGui::EndChild();
        ImGui::EndChild();
        return RPS_OK;
    }

    uint32_t GraphCanvas::GetNodeRuntimeId(const VisualizerUpdateContext& context,
                                           const Graph&                   graph,
                                           const Node&                    node) const
    {
        // No TransitionId - 1 to account for preamble
        const uint32_t rawId   = node.IsTransition() ? node.GetTransitionId() : node.GetCmdId();
        uint32_t       rtCmdId = node.IsTransition() ? context.visualizationData.transIdToRuntimeIdMap[rawId]
                                                     : context.visualizationData.cmdToRuntimeIdMap[rawId];

        if (rtCmdId == RPS_INDEX_NONE_U32)
        {
            rtCmdId = rpsMax(uint32_t(context.visualizationData.cmdVisInfos.size()), 2u) - 2;
        }

        return rtCmdId;
    }

    uint32_t GraphCanvas::GetNodeTimelinePos(const VisualizerUpdateContext& context,
                                             const Graph&                   graph,
                                             const Node&                    node) const
    {
        const uint32_t rtCmdId = GetNodeRuntimeId(context, graph, node);

        return context.visualizationData.cmdVisInfos[rtCmdId].timelinePosition;
    }

    uint32_t GraphCanvas::GetCacheEntryRowIndex(const VisualizerUpdateContext& context,
                                                const GraphCacheEntry&         cacheEntry,
                                                const Node&                    node) const
    {
        if (node.IsTransition())
        {
            auto transitions = cacheEntry.transitions.Get(m_transitions);
            for (uint32_t rowIndex = 0; rowIndex < transitions.size(); ++rowIndex)
            {
                //Should we account for transition IDs starting at -1 not 0 ?
                if (transitions[rowIndex].transitionId == node.GetTransitionId())
                {
                    return rowIndex;
                }
            }
        }
        else
        {
            // Cmd node

            for (uint32_t rowIndex = 0; rowIndex < cacheEntry.nodes.size(); ++rowIndex)
            {
                const GraphCacheNodeInfo& nodeInfo = cacheEntry.nodes.Get(m_nodes)[rowIndex];

                if (nodeInfo.cmdId == node.GetCmdId())
                {
                    return rowIndex;
                }
            }
        }

        return RPS_INDEX_NONE_U32;
    }

    void GraphCanvas::GetTransitionTooltip(const DrawingState&             state,
                                           GraphStrBuilder&                sb,
                                           const GraphCacheTransitionInfo& transitionInfo) const
    {
        sb.AppendFormat("transition node [%u]\n", transitionInfo.graphNodeId);
        sb.AppendFormat("resource : %s\n", state.visData.resourceInfos[transitionInfo.nextAccess.resourceId].name);
        sb.Append("range : ");
        transitionInfo.prevAccess.subresourceRange.Print(sb.AsPrinter());
        sb.Append("\n");

        sb.AppendFormat("transition : [ ");
        transitionInfo.prevAccess.access.Print(sb.AsPrinter());
        sb.Append(" ] => [ ");
        transitionInfo.nextAccess.access.Print(sb.AsPrinter());
        sb.Append(" ]\n");
    }

    void GraphCanvas::GetNodeTooltip(const DrawingState&       state,
                                     GraphStrBuilder&          sb,
                                     const GraphCacheNodeInfo& nodeInfo) const
    {
        const bool bInactive = (nodeInfo.rtCmdId == RPS_INDEX_NONE_U32);

        if (bInactive)
        {
            sb.AppendFormat("[Inactive] - node : [%u] - ", nodeInfo.cmdId);
            sb.Append(nodeInfo.name);
        }
        else
        {
            sb.AppendFormat(
                "[%u] - node : [%u] - ", state.visData.cmdVisInfos[nodeInfo.rtCmdId].timelinePosition, nodeInfo.cmdId);
            sb.Append(state.visData.cmdVisInfos[nodeInfo.rtCmdId].name);
        }

        sb.Append("\n");
        if (nodeInfo.accessInfos.size() == 0)
        {
            return;
        }
        sb.Append("accesses :\n");
        auto accesses = nodeInfo.accessInfos.Get(m_accesses);
        for (uint32_t accessIndex = 0; accessIndex < accesses.size(); ++accessIndex)
        {
            const SubResourceAccessInfo& access = accesses[accessIndex];
            sb.AppendFormat("[%u] : \n", accessIndex);
            sb.AppendFormat("  resource : %s\n", state.visData.resourceInfos[access.resourceId].name);
            sb.Append("  access : [");
            access.access.Print(sb.AsPrinter());
            sb.Append("]\n");

            sb.Append("  range : ");
            access.subresourceRange.Print(sb.AsPrinter());
            sb.Append("\n");
        }
    }

    float GraphCanvas::GetRowHeight()
    {
        return ImGui::GetFontSize();
    }

    float GraphCanvas::GetRowHeightPadded()
    {
        return ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y;
    }

    float GraphCanvas::GetDepCurveFraction(const DrawingState& drawState, const GraphCacheDependencyInfo& dep)
    {
        return CurveFraction;
    }

    void GraphCanvas::Select(uint32_t graphNodeId)
    {
        if (m_highlightGraphNodeId == graphNodeId)
            return;

        m_highlightGraphNodeId = graphNodeId;

        m_highlightDependencyIndices.clear();

        if (graphNodeId != RPS_INDEX_NONE_U32)
        {
            auto collectHighlightedDeps = [&](Span<GraphCacheDependencyInfo> depSpan) {
                const auto deps = depSpan.Get(m_nodeDependencies);

                for (uint32_t i = 0; i < deps.size(); i++)
                {
                    if ((deps[i].srcNodeId == graphNodeId) || (deps[i].dstNodeId == graphNodeId))
                    {
                        m_highlightDependencyIndices.push_back(depSpan.GetBegin() + i);
                    }
                }
            };

            for (size_t timelinePos = 0; timelinePos < m_graphCache.size(); ++timelinePos)
            {
                const GraphCacheEntry& cached = m_graphCache[timelinePos];
                collectHighlightedDeps(cached.transToNodeDeps);
                collectHighlightedDeps(cached.nodeToTransDeps);
                collectHighlightedDeps(cached.nodeToNodeDeps);
            }
        }
    }

    void GraphCanvas::Unselect()
    {
        Select(RPS_INDEX_NONE_U32);
    }

    ImU32 GraphCanvas::GetNodeColor(const NodeDeclInfo& nodeDecl, bool bActive)
    {
        constexpr RpsNodeDeclFlags QueueTypeFlags =
            RPS_NODE_DECL_GRAPHICS_BIT | RPS_NODE_DECL_COMPUTE_BIT | RPS_NODE_DECL_COPY_BIT;

        const RpsNodeDeclFlags nodeQueueFlags = (nodeDecl.flags & QueueTypeFlags);

        const ImU32 nodeColor = (nodeQueueFlags == RPS_NODE_DECL_GRAPHICS_BIT)  ? GraphicsNodeColor
                                : (nodeQueueFlags == RPS_NODE_DECL_COMPUTE_BIT) ? ComputeNodeColor
                                : (nodeQueueFlags == RPS_NODE_DECL_COPY_BIT)    ? CopyNodeColor
                                                                                : MultiNodeColor;

        return bActive ? nodeColor : GetInactiveColor(nodeColor);
    }

    class LaneHandler
    {
    public:
        //TODO shrink to bit vector
        LaneHandler(Arena& scratch, uint32_t timelineSize)
            : m_timelineSize(timelineSize)
            , m_laneCells(&scratch)
        {
        }
        //By default use zero only for adjacent nodes and leave 1 out
        uint32_t ReserveFirstLane(uint32_t srcTime, uint32_t dstTime, uint32_t firstLaneIndex = 2)
        {
            // Iterate until first lane found
            uint32_t currentLaneIndex = firstLaneIndex;

            while (!CheckLane(srcTime, dstTime, currentLaneIndex))
            {
                ++currentLaneIndex;
            }

            ReserveLane(srcTime, dstTime, currentLaneIndex);

            return currentLaneIndex;
        }

        bool CheckLane(uint32_t srcTime, uint32_t dstTime, uint32_t laneIndex) const
        {
            if ((laneIndex + 1) * m_timelineSize > m_laneCells.size())
                return true;

            for (uint32_t timelinePos = srcTime; timelinePos <= dstTime; ++timelinePos)
            {
                if (m_laneCells[size_t(timelinePos + laneIndex * m_timelineSize)])
                    return false;
            }

            return true;
        }

        void ReserveLane(uint32_t srcTime, uint32_t dstTime, uint32_t laneIndex)
        {
            ReserveLaneMem(laneIndex);

            for (uint32_t timelinePos = srcTime; timelinePos <= dstTime; ++timelinePos)
            {
                m_laneCells[size_t(timelinePos + laneIndex * m_timelineSize)] = true;
            }
        }

        uint64_t NumLanes() const
        {
            return m_laneCells.size() / m_timelineSize;
        }

    private:
        void ReserveLaneMem(uint64_t laneIndex)
        {
            const size_t maxSize = size_t((laneIndex + 1) * m_timelineSize);
            if (maxSize > m_laneCells.size())
            {
                const size_t oldSize = m_laneCells.size();
                m_laneCells.resize(maxSize);

                std::fill(m_laneCells.begin() + oldSize, m_laneCells.end(), false);
            }
        }
        uint64_t          m_timelineSize;
        ArenaVector<bool> m_laneCells;  // true = cell is taken, false = cell is free
    };

    RpsResult GraphCanvas::Update(const VisualizerUpdateContext& context)
    {
        m_accesses.reset_keep_capacity(&context.persistentArena);
        m_graphCache.reset_keep_capacity(&context.persistentArena);
        m_transitions.reset_keep_capacity(&context.persistentArena);
        m_nodes.reset_keep_capacity(&context.persistentArena);
        m_nodeDependencies.reset_keep_capacity(&context.persistentArena);
        m_highlightDependencyIndices.reset_keep_capacity(&context.persistentArena);
        m_highlightGraphNodeId = RPS_INDEX_NONE_U32;

        m_accessesPool.reset();
        m_transitionsPool.reset();
        m_nodesPool.reset();
        m_nodeDependenciesPool.reset();

        if (context.pRenderGraph == nullptr)
        {
            //Update with empty render graph deletes data.
            return RPS_OK;
        }

        // +1 For additional barriers at the end
        m_graphCache.resize(context.visualizationData.timelinePosToCmdIdMap.size() + 1);

        const Graph& graph       = context.pRenderGraph->GetGraph();
        const auto&  cmds        = context.pRenderGraph->GetCmdInfos();
        const auto&  transitions = context.pRenderGraph->GetTransitions();

        // Insert active nodes and transitions
        for (uint32_t rtCmdIndex = 0; rtCmdIndex < context.visualizationData.cmdVisInfos.size(); ++rtCmdIndex)
        {
            const CmdVisualizationInfo& cmdVisInfo = context.visualizationData.cmdVisInfos[rtCmdIndex];
            GraphCacheEntry&            cacheEntry = m_graphCache[cmdVisInfo.timelinePosition];
            auto                        accesses   = cmdVisInfo.accesses.Get(context.visualizationData.accessInfos);

            if (cmdVisInfo.isTransition)
            {
                // Ignore preamble
                if (accesses.size() == 0)
                {
                    continue;
                }

                // Prev and new state
                RPS_ASSERT(accesses.size() == 2);

                GraphCacheTransitionInfo transition = {};
                transition.rtCmdId                  = rtCmdIndex;
                // Ignore the initial transitions
                transition.transitionId = context.pRenderGraph->GetRuntimeCmdInfos()[transition.rtCmdId].cmdId;
                transition.graphNodeId  = transitions[transition.transitionId].nodeId;

                transition.prevAccess = accesses[0];
                transition.nextAccess = accesses[1];

                m_transitionsPool.push_to_span(cacheEntry.transitions, transition);
            }
            else
            {
                // No transition - Normal resource accesses from cacheNode
                GraphCacheNodeInfo node = {};
                // Cache all transitions by their timelinePos
                for (const auto& access : accesses)
                {
                    m_accessesPool.push_to_span(node.accessInfos, access);
                }

                node.rtCmdId   = rtCmdIndex;
                node.cmdId     = cmdVisInfo.cmdId;
                node.nodeColor = GetNodeColor(*context.pRenderGraph->GetCmdInfo(cmdVisInfo.cmdId)->pNodeDecl);

                m_nodesPool.push_to_span(cacheEntry.nodes, node);
            }
        }

        // Insert inactive nodes
        for (uint32_t cmdId = 0; cmdId < cmds.size(); ++cmdId)
        {
            auto&            node        = *graph.GetNode(cmdId);
            const uint32_t   rtId        = GetNodeRuntimeId(context, graph, node);
            const uint32_t   timelinePos = context.visualizationData.cmdVisInfos[rtId].timelinePosition;
            GraphCacheEntry& cacheEntry  = m_graphCache[timelinePos];

            // Active nodes were already processed
            if (context.visualizationData.cmdToRuntimeIdMap[cmdId] != RPS_INDEX_NONE_U32)
            {
                continue;
            }

            const CmdInfo&     cmd       = cmds[cmdId];
            GraphCacheNodeInfo cacheNode = {};
            for (const auto& access : cmd.accesses.Get(context.pRenderGraph->GetCmdAccessInfos()))
            {
                m_accessesPool.push_to_span(cacheNode.accessInfos, SubResourceAccessInfo(access));
            }

            cacheNode.rtCmdId   = RPS_INDEX_NONE_U32;
            cacheNode.cmdId     = cmdId;
            cacheNode.nodeColor = cmd.pNodeDecl ? GetNodeColor(*cmd.pNodeDecl, false) : DefaultNodeColor;
            cacheNode.name      = cmd.pNodeDecl ? context.scratchArena.StoreStr(cmd.pNodeDecl->name) : "<unnamed>";

            m_nodesPool.push_to_span(cacheEntry.nodes, cacheNode);
        }

        // Insert inactive transitions

        for (uint32_t transitionId = 1; transitionId < transitions.size(); ++transitionId)
        {
            const TransitionInfo& transition = transitions[transitionId];

            const uint32_t   timelinePos = GetNodeTimelinePos(context, graph, *graph.GetNode(transition.nodeId));
            GraphCacheEntry& cacheEntry  = m_graphCache[timelinePos];

            // Active transitions were already processed
            if (context.visualizationData.transIdToRuntimeIdMap[transitionId] != RPS_INDEX_NONE_U32)
            {
                continue;
            }

            GraphCacheTransitionInfo cacheTransition = {};
            cacheTransition.prevAccess               = transitions[transition.prevTransition].access;
            cacheTransition.nextAccess               = transition.access;
            cacheTransition.rtCmdId                  = RPS_INDEX_NONE_U32;
            cacheTransition.transitionId             = transitionId;
            cacheTransition.graphNodeId              = transition.nodeId;

            m_transitionsPool.push_to_span(cacheEntry.transitions, cacheTransition);
        }

        // Sort nodes and transitions at each timeline pos
        for (GraphCacheEntry& cacheEntry : m_graphCache)
        {
            // Sort nodes
            std::sort(m_nodes.begin() + cacheEntry.nodes.GetBegin(),
                      m_nodes.begin() + cacheEntry.nodes.GetEnd(),
                      [&](const GraphCacheNodeInfo& left, const GraphCacheNodeInfo& right) {
                          return ((left.IsActive() ? 0 : 0x80000000) | left.cmdId) <
                                 ((right.IsActive() ? 0 : 0x80000000) | right.cmdId);
                      });

            // Sort transitions
            std::sort(m_transitions.begin() + cacheEntry.transitions.GetBegin(),
                      m_transitions.begin() + cacheEntry.transitions.GetEnd(),
                      [&](const GraphCacheTransitionInfo& left, const GraphCacheTransitionInfo& right) {
                          return left.graphNodeId < right.graphNodeId;
                      });
        }

        LaneHandler laneHandler(context.scratchArena,
                                uint32_t(context.visualizationData.timelinePosToCmdIdMap.size() + 1));

        // Get dependencies
        for (const Node& dstNode : graph.GetNodes())
        {
            const uint32_t dstTimelinePos = GetNodeTimelinePos(context, graph, dstNode);

            GraphCacheEntry& entry = m_graphCache[dstTimelinePos];

            const uint32_t dstRowIndex = GetCacheEntryRowIndex(context, entry, dstNode);

            for (const auto& edge : dstNode.inEdges.Get(graph.GetEdges()))
            {
                // Get src info
                const Node& src = *graph.GetNode(edge.src);

                const uint32_t srcTimelinePos = GetNodeTimelinePos(context, graph, src);

                const uint32_t srcRowIndex = GetCacheEntryRowIndex(context, m_graphCache[srcTimelinePos], src);

                const bool bTransitionSrc = src.IsTransition();
                const bool bTransitionDst = dstNode.IsTransition();

                auto& targetDeps = (bTransitionSrc ? entry.transToNodeDeps
                                                   : (bTransitionDst ? entry.nodeToTransDeps : entry.nodeToNodeDeps));

                // Check if this dep would be a duplicate
                auto deps = targetDeps.Get(m_nodeDependencies);

                bool found = false;
                for (const auto& dep : deps)
                {
                    if (dep.dstTimelinePos == dstTimelinePos && dep.dstRowIndex == dstRowIndex &&
                        dep.srcTimelinePos == srcTimelinePos && dep.srcRowIndex == srcRowIndex)
                    {
                        found = true;
                        break;
                    }
                }
                if (found)
                    continue;

                // Determine if short dependency between adjacent commands

                const bool bShort = (bTransitionSrc && (srcTimelinePos == dstTimelinePos)) ||
                                    (!bTransitionSrc && (srcTimelinePos + 1 == dstTimelinePos));

                uint32_t shortRowIndex = RPS_INDEX_NONE_U32;
                if (bShort)
                {
                    const uint32_t leftLaneIndex  = bTransitionSrc ? srcRowIndex + 1 : srcRowIndex;
                    const uint32_t rightLaneIndex = bTransitionDst ? dstRowIndex + 1 : dstRowIndex;

                    shortRowIndex = rpsMax(leftLaneIndex, rightLaneIndex);

                    laneHandler.ReserveLane(
                        srcTimelinePos, bTransitionDst ? dstTimelinePos - 1 : dstTimelinePos, shortRowIndex);
                }

                m_nodeDependenciesPool.push_to_span(targetDeps,
                                                    GraphCacheDependencyInfo{srcTimelinePos,
                                                                             srcRowIndex,
                                                                             bTransitionSrc,
                                                                             edge.src,
                                                                             dstTimelinePos,
                                                                             dstRowIndex,
                                                                             bTransitionDst,
                                                                             edge.dst,
                                                                             shortRowIndex});
            }
        }

        // Set lanes for all non-short dependencies

        // Initial pass with single width to avoid overlap of connections and other transitions
        for (uint32_t timelinePos = 0; timelinePos < m_graphCache.size(); ++timelinePos)
        {
            const GraphCacheEntry& dstEntry = m_graphCache[timelinePos];

            for (GraphCacheDependencyInfo& dep : dstEntry.nodeToTransDeps.Get(m_nodeDependencies))
            {
                // Short deps already have lanes assigned
                if (dep.laneIndex != RPS_INDEX_NONE_U32)
                {
                    continue;
                }
                const uint32_t reserved = laneHandler.ReserveFirstLane(dep.srcTimelinePos, dep.srcTimelinePos);

                //Two wide transitions are also considered short due to them being at the start of a cell
                if (dep.dstTimelinePos == dep.srcTimelinePos + 1)
                {
                    dep.laneIndex = reserved;
                }
            }

            for (GraphCacheDependencyInfo& dep : dstEntry.transToNodeDeps.Get(m_nodeDependencies))
            {
                // Short deps already have lanes assigned
                if (dep.laneIndex != RPS_INDEX_NONE_U32)
                {
                    continue;
                }

                const uint32_t reserved = laneHandler.ReserveFirstLane(dep.srcTimelinePos, dep.srcTimelinePos);

                //Two wide transitions are also considered short due to them being at the start of a cell
                if (dep.dstTimelinePos == dep.srcTimelinePos + 1)
                {
                    dep.laneIndex = reserved;
                }
            }

            for (const GraphCacheDependencyInfo& dep : dstEntry.nodeToNodeDeps.Get(m_nodeDependencies))
            {
                // Short deps already have lanes assigned
                if (dep.laneIndex != RPS_INDEX_NONE_U32)
                {
                    continue;
                }

                laneHandler.ReserveFirstLane(dep.srcTimelinePos, dep.srcTimelinePos);
            }
        }

        // Full pass with one off start to account for initial pass
        for (uint32_t timelinePos = 0; timelinePos < m_graphCache.size(); ++timelinePos)
        {
            const GraphCacheEntry& srcEntry = m_graphCache[timelinePos];

            for (GraphCacheDependencyInfo& dep : srcEntry.nodeToTransDeps.Get(m_nodeDependencies))
            {
                // Short deps already have lanes assigned
                if (dep.laneIndex != RPS_INDEX_NONE_U32)
                {
                    continue;
                }
                dep.laneIndex = laneHandler.ReserveFirstLane(dep.srcTimelinePos + 1, dep.dstTimelinePos - 1);
            }

            for (GraphCacheDependencyInfo& dep : srcEntry.transToNodeDeps.Get(m_nodeDependencies))
            {
                // Short deps already have lanes assigned
                if (dep.laneIndex != RPS_INDEX_NONE_U32)
                {
                    continue;
                }

                dep.laneIndex = laneHandler.ReserveFirstLane(dep.srcTimelinePos + 1, dep.dstTimelinePos - 1);
            }

            for (GraphCacheDependencyInfo& dep : srcEntry.nodeToNodeDeps.Get(m_nodeDependencies))
            {
                // Short deps already have lanes assigned
                if (dep.laneIndex != RPS_INDEX_NONE_U32)
                {
                    continue;
                }

                dep.laneIndex = laneHandler.ReserveFirstLane(dep.srcTimelinePos + 1, dep.dstTimelinePos);
            }
        }

        m_graphHeightZoom.SetUpperBound(laneHandler.NumLanes() + uint64_t(GraphCanvas::GetRowHeightPadded()));

        return RPS_OK;
    }

    uint64_t GraphCanvas::CalcKeyMoveHeightStep(uint64_t heightRange)
    {
        return 1;
    }
}  // namespace rps
