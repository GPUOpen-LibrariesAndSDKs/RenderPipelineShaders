// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_MEMORY_LAYOUT_CANVAS_HPP
#define RPS_MEMORY_LAYOUT_CANVAS_HPP

#include "rps_visualizer_util.hpp"
#include "rps_zoom_state.hpp"
#include "rps_selector_state.hpp"

namespace rps
{
    struct VisualizerUpdateContext;
    struct DrawingState;
    struct RenderGraphVisualizationData;

    class MemLayoutCanvas
    {
        struct ResourceVisInfo
        {
            ImU32    color;
            uint32_t resIndex;
        };

    public:
        RpsResult Update(const VisualizerUpdateContext& context, uint32_t heapIdx);
        void      Draw(DrawingState& drawingState);

    private:
        static uint64_t CalcKeyMoveMemStep(uint64_t memRange);

        void DrawRuler(const DrawingState& state, const UIRect& rect);
        void DrawHeapResources(const DrawingState& state, const UIRect& rect);
        void DrawHeapResourceToolTip(const DrawingState& state, const ResourceVisInfo& visInfo);
        void DrawControlPanel(DrawingState& state);

        void UpdateResourceVisuals(const RenderGraphVisualizationData& visData);

        StrRef GetHeapDescription(const VisualizerUpdateContext& context, uint32_t heapIndex) const;

    private:
        StrRef        m_description;
        ZoomState     m_heapSpaceZoom;
        SelectorState m_heapSpaceSelector;
        uint32_t      m_heapIndex = RPS_INDEX_NONE_U32;

        enum ResourceColoringScheme
        {
            Size,
            Lifetime,
            Aliased,
            Type,
            AccessUsage,
            Count,
        };
        ResourceColoringScheme m_resourceColoringScheme = ResourceColoringScheme::Size;

        ArenaVector<ResourceVisInfo> m_resVisInfos;

        uint64_t m_maxResourceSize = 1ull;

        float m_maxRulerTextLen = 0.0f;
        float m_tickLength      = 3.0f;
        float m_viewHeight      = -1.0f;
        bool  m_bShowAddress    = false;

        struct ColorNameValue
        {
            const char* name;
            ImU32       value;
        };

        ColorNameValue m_colorByAliasedPalette[2] = {
            {"Aliased", IM_COL32(255, 67, 67, 255)},
            {"Not Aliased", IM_COL32(127, 127, 127, 255)},
        };

        ColorNameValue m_colorByResTypeColors[RPS_RESOURCE_TYPE_COUNT] = {
            {"Unknown", IM_COL32(127, 127, 127, 255)},
            {"Buffer", IM_COL32(255, 186, 2, 255)},
            {"1D Images", IM_COL32(0, 99, 177, 255)},
            {"2D Images", IM_COL32(142, 140, 215, 255)},
            {"3D Images", IM_COL32(255, 67, 67, 255)},
        };

        static constexpr const char* MaxYLabelText = "0000 GiB";
    };

}  // namespace rps

#endif  //RPS_MEMORY_LAYOUT_CANVAS_HPP
