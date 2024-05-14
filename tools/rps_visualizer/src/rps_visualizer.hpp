// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_VISUALIZER_HPP
#define RPS_VISUALIZER_HPP

#include "rps_visualizer.h"

#include "core/rps_core.hpp"
#include "runtime/common/rps_render_graph.hpp"

#include "rps_visualizer_util.hpp"
#include "rps_visualizer_common.hpp"
#include "rps_overlay_state.hpp"

namespace rps
{
    class Device;

    class Visualizer
    {
    public:
        static RpsResult Create(const Device&                  device,
                                const RpsVisualizerCreateInfo& createInfo,
                                Visualizer**                   ppVisualizer);
        void             Destroy();

        RpsResult Update(const RpsVisualizerUpdateInfo* pUpdateInfo);
        RpsResult Draw();

        RenderGraphVisualizationData GetVisualizationData() const
        {
            return RenderGraphVisualizationData{m_timelinePosToCmdIdMap.crange_all(),
                                                m_cmdToRuntimeIdMap.crange_all(),
                                                m_transIdToRuntimeIdMap.crange_all(),
                                                m_cmdInfos,
                                                m_cmdVisInfos,
                                                m_accessVisInfos.crange_all(),
                                                m_resourceInfos,
                                                m_resourceVisInfos,
                                                m_heapInfos};
        }

    private:
        Visualizer(const Device& device, const RpsVisualizerCreateInfo& createInfo);

        RpsResult UpdateCmdTimelineInfo(const RenderGraph* pRenderGraph);

    private:
        const Device& m_device;
        Arena         m_arena;
        Arena         m_scratchArena;

        ArenaVector<uint32_t>                    m_timelinePosToCmdIdMap;
        ArenaVector<uint32_t>                    m_cmdToRuntimeIdMap;
        ArenaVector<uint32_t>                    m_transIdToRuntimeIdMap;
        ConstArrayRef<CmdVisualizationInfo>      m_cmdVisInfos;
        ArenaVector<SubResourceAccessInfo>       m_accessVisInfos;
        ConstArrayRef<ResourceVisualizationInfo> m_resourceVisInfos;
        ConstArrayRef<RpsCmdDiagnosticInfo>      m_cmdInfos      = {};
        ConstArrayRef<RpsResourceDiagnosticInfo> m_resourceInfos = {};
        ConstArrayRef<RpsHeapDiagnosticInfo>     m_heapInfos     = {};

        OverlayState m_overlayState;
    };

    RPS_ASSOCIATE_HANDLE(Visualizer);

}  // namespace rps

#endif //RPS_VISUALIZER_HPP
