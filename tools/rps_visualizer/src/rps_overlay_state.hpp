// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_OVERLAY_STATE_HPP
#define RPS_OVERLAY_STATE_HPP

#include "rps_visualizer_util.hpp"

#include "rps_zoom_state.hpp"
#include "rps_selector_state.hpp"
#include "rps_resource_lifetime_canvas.hpp"
#include "rps_memory_layout_canvas.hpp"
#include "rps_graph_canvas.hpp"

#include <functional>

#include "imgui.h"

struct RpsVisualizerCreateInfo;

namespace rps
{
    struct VisualizerUpdateContext;
    struct RenderGraphVisualizationData;
    struct CmdVisualizationInfo;

    struct DrawSettings
    {
        bool bDrawSelector = true;

        bool bDrawResourceAccesses    = true;
        bool bDrawResourceTransitions = true;
        bool bDrawResourceConnectors  = true;
        bool bDrawResourceTooltips    = true;

        bool bDrawSubResourceDataLifetimeMarkers = true;

        bool bDrawHeapTooltips = true;

        bool bDrawGraphTooltips = true;

        bool bDrawSettingsSelector = false;  // Only non persistent member

        void ForMembers(std::function<bool(const char*, bool&)> func);
    };

    struct DrawingState
    {
        const RenderGraphVisualizationData& visData;
        DrawSettings&                       settings;
        ZoomState&                          timelineZoom;

        ImVec2 mousePosScr;
        ImVec2 rightPaneTopLeftScr;
        ImVec2 rightPaneBottomRightScr;
        float  rightPaneWidth;
        float  rightPaneRightScr;
        float  rightPaneBodyTopY;     // Right pane top excluding ruler
        float  rightPaneBodyBottomY;  // Right pane bottom excluding X scroll bar
        float  resourceHeaderTopY;
        float  resourceHeaderBottomY;
        float  defaultSubViewHeight;
    };

    class OverlayState
    {
    public:
        explicit OverlayState(const Device& device, const RpsVisualizerCreateInfo& createInfo);
        ~OverlayState();

        RpsResult Draw(const RenderGraphVisualizationData& visData);
        RpsResult Update(const VisualizerUpdateContext& context);

        static const CmdVisualizationInfo* PickCmdVisInfoAtMouseCursor(DrawingState& state);

    public:
        static constexpr float ButtonRepeatDelay = 1.0f;
        static constexpr float ButtonRepeatRate  = 0.05f;

    private:
        static uint64_t CalcKeyMoveTimeStep(uint64_t timeUnits);

        void DrawHeaps(DrawingState& state);
        void DrawRuler(DrawingState& state);
        bool DrawSettingsSelection(DrawingState& state);

    private:
        ResourceLifetimesCanvas m_resLifetimeCanvas;
        Vector<MemLayoutCanvas> m_heapStates;
        GraphCanvas             m_graphCanvas;

        SelectorState m_timelineSelectState;
        ZoomState     m_timelineZoom;

        bool m_bChildWindow;
        bool m_bPendingSettingsLoad = true;

        ImVec2 m_settingsPanelSize = {};
        DrawSettings m_drawSettings;
        std::string  m_imGuiConfigFilePath;
    };

}  // namespace rps

#endif  //RPS_OVERLAY_STATE_HPP
