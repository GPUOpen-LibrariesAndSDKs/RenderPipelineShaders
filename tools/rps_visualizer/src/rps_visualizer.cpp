// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "rps_visualizer.hpp"

namespace rps
{
    Visualizer::Visualizer(const Device& device, const RpsVisualizerCreateInfo& createInfo)
        : m_device(device)
        , m_arena(device.Allocator())
        , m_scratchArena(device.Allocator())
        , m_overlayState(device, createInfo)
    {
    }

    RpsResult Visualizer::Create(const Device&                  device,
                                 const RpsVisualizerCreateInfo& createInfo,
                                 Visualizer**                   ppVisualizer)
    {
        RPS_CHECK_ARGS(ppVisualizer);

        void* pVisMem = device.Allocate(AllocInfo::FromType<Visualizer>());
        RPS_CHECK_ALLOC(pVisMem);

        *ppVisualizer = new (pVisMem) Visualizer(device, createInfo);

        return RPS_OK;
    }

    void Visualizer::Destroy()
    {
        const rps::Device& device = m_device;

        this->~Visualizer();

        device.Free(this);
    }

    RpsResult Visualizer::Update(const RpsVisualizerUpdateInfo* pUpdateInfo)
    {
        m_arena.Reset();
        m_scratchArena.Reset();

        m_timelinePosToCmdIdMap.reset(&m_arena);
        m_cmdToRuntimeIdMap.reset(&m_arena);
        m_transIdToRuntimeIdMap.reset(&m_arena);
        m_accessVisInfos.reset(&m_arena);

        m_cmdVisInfos      = {};
        m_resourceVisInfos = {};
        m_cmdInfos         = {};
        m_resourceInfos    = {};
        m_heapInfos        = {};

        RenderGraph* pRenderGraph = pUpdateInfo ? rps::FromHandle(pUpdateInfo->hRenderGraph) : nullptr;

        if (pRenderGraph)
        {
            RpsRenderGraphDiagnosticInfo runtimeInfos = {};
            RPS_V_RETURN(pRenderGraph->GetDiagnosticInfo(runtimeInfos, RPS_RENDER_GRAPH_DIAGNOSTIC_INFO_DEFAULT));

            m_cmdInfos      = {runtimeInfos.pCmdDiagInfos, runtimeInfos.numCommandInfos};
            m_resourceInfos = {runtimeInfos.pResourceDiagInfos, runtimeInfos.numResourceInfos};
            m_heapInfos     = {runtimeInfos.pHeapDiagInfos, runtimeInfos.numHeapInfos};

            RPS_V_RETURN(UpdateCmdTimelineInfo(pRenderGraph));
        }

        VisualizerUpdateContext updateContext = {pRenderGraph, m_arena, m_scratchArena, GetVisualizationData()};

        RPS_V_RETURN(m_overlayState.Update(updateContext));

        return RPS_OK;
    }

    RpsResult Visualizer::Draw()
    {
        auto visData = GetVisualizationData();

        return m_overlayState.Draw(visData);
    }

    RpsResult Visualizer::UpdateCmdTimelineInfo(const RenderGraph* pRenderGraph)
    {
        const auto cmdInfos = pRenderGraph->GetRuntimeCmdInfos();

        auto cmdVisInfos = m_arena.NewArray<CmdVisualizationInfo>(cmdInfos.size());
        m_cmdVisInfos    = cmdVisInfos;

        m_timelinePosToCmdIdMap.reserve(cmdInfos.size());  // TODO: Exclude transitions
        
        m_cmdToRuntimeIdMap.resize(pRenderGraph->GetCmdInfos().size());
        m_transIdToRuntimeIdMap.resize(pRenderGraph->GetTransitions().size());

        std::fill(m_cmdToRuntimeIdMap.begin(), m_cmdToRuntimeIdMap.end(), RPS_INDEX_NONE_U32);
        std::fill(m_transIdToRuntimeIdMap.begin(), m_transIdToRuntimeIdMap.end(), RPS_INDEX_NONE_U32);

        for (uint32_t rtIndex = 0; rtIndex < cmdInfos.size(); ++rtIndex)
        {
            const RuntimeCmdInfo& rtCmd    = cmdInfos[rtIndex];
            const uint32_t        cmdIndex = cmdInfos[rtIndex].cmdId;

            if (rtCmd.isTransition)
            {
                if (!rtCmd.HasTransitionInfo())
                    continue;
                m_transIdToRuntimeIdMap[rtCmd.GetTransitionId()] = rtIndex;
            }
            else
            {
                m_cmdToRuntimeIdMap[rtCmd.GetCmdId()] = rtIndex;
            }
        }

        for (uint32_t iCmd = 0; iCmd < cmdInfos.size(); iCmd++)
        {
            const auto& rtCmdInfo  = cmdInfos[iCmd];
            auto&       cmdVisInfo = cmdVisInfos[iCmd];

            cmdVisInfo.isTransition     = rtCmdInfo.isTransition;
            cmdVisInfo.timelinePosition = uint32_t(m_timelinePosToCmdIdMap.size());
            cmdVisInfo.accesses.SetRange(uint32_t(m_accessVisInfos.size()), 0);

            if (!rtCmdInfo.isTransition)
            {
                const auto cmdAccesses    = pRenderGraph->GetCmdAccesses(rtCmdInfo.GetCmdId());
                auto       pAccessVisInfo = m_accessVisInfos.grow(cmdAccesses.size());

                auto pCmdInfo = pRenderGraph->GetCmdInfo(rtCmdInfo.GetCmdId());
                if (pCmdInfo && pCmdInfo->pNodeDecl)
                {
                    cmdVisInfo.name  = m_arena.StoreStr(pCmdInfo->pNodeDecl->name);
                    cmdVisInfo.cmdId = rtCmdInfo.GetCmdId();
                }

                for (uint32_t iAccess = 0; iAccess < cmdAccesses.size(); iAccess++)
                {
                    pAccessVisInfo[iAccess] = SubResourceAccessInfo(cmdAccesses[iAccess]);
                }

                m_timelinePosToCmdIdMap.push_back(iCmd);
            }
            else if (rtCmdInfo.HasTransitionInfo())
            {
                const auto& transitionInfo = pRenderGraph->GetTransitionInfo(rtCmdInfo.GetTransitionId());

                CmdAccessInfo prevAccessInfo = transitionInfo.access;

                prevAccessInfo.access = RenderGraph::CalcPreviousAccess(
                    transitionInfo.prevTransition,
                    pRenderGraph->GetTransitions().crange_all(),
                    pRenderGraph->GetResourceInstance(transitionInfo.access.resourceId));

                m_accessVisInfos.emplace_back(prevAccessInfo);
                m_accessVisInfos.emplace_back(transitionInfo.access);
            }

            cmdVisInfo.accesses.SetEnd(uint32_t(m_accessVisInfos.size()));
        }

        auto resourceVisInfos = m_arena.NewArray<ResourceVisualizationInfo>(m_resourceInfos.size());
        m_resourceVisInfos    = resourceVisInfos;

        for (uint32_t iRes = 0; iRes < m_resourceInfos.size(); iRes++)
        {
            const auto& resInfo = m_resourceInfos[iRes];

            const size_t timelineMax           = m_timelinePosToCmdIdMap.size();
            const bool   bLifetimeBeginDefined = (resInfo.lifetimeBegin != ResourceInstance::LIFETIME_UNDEFINED);
            const bool   bLifetimeEndDefined   = (resInfo.lifetimeEnd != ResourceInstance::LIFETIME_UNDEFINED);
            const bool   bHasAnyCmdVisInfo     = !cmdVisInfos.empty();

            if (resInfo.lifetimeBegin <= resInfo.lifetimeEnd)
            {
                resourceVisInfos[iRes] = ResourceVisualizationInfo(
                    (bLifetimeBeginDefined && bHasAnyCmdVisInfo) ? cmdVisInfos[resInfo.lifetimeBegin].timelinePosition
                                                                 : 0,
                    (bLifetimeEndDefined && bHasAnyCmdVisInfo) ? cmdVisInfos[resInfo.lifetimeEnd].timelinePosition
                                                               : uint32_t(timelineMax),
                    pRenderGraph->GetResourceInstance(iRes).isAliased);
            }
        }

        return RPS_OK;
    }

}  // namespace rps

RpsResult rpsVisualizerCreate(RpsDevice                      hDevice,
                              const RpsVisualizerCreateInfo* pCreateInfo,
                              RpsVisualizer*                 phVisualizer)
{
    RPS_CHECK_ARGS(hDevice != RPS_NULL_HANDLE);
    RPS_CHECK_ARGS(phVisualizer);

    return rps::Visualizer::Create(*rps::FromHandle(hDevice),
                                   pCreateInfo ? *pCreateInfo : RpsVisualizerCreateInfo{},
                                   rps::FromHandle(phVisualizer));
}

void rpsVisualizerDestroy(RpsVisualizer hVisualizer)
{
    if (hVisualizer == RPS_NULL_HANDLE)
        return;

    rps::FromHandle(hVisualizer)->Destroy();
}

RpsResult rpsVisualizerUpdate(RpsVisualizer hVisualizer, const RpsVisualizerUpdateInfo* pUpdateInfo)
{
    RPS_CHECK_ARGS(hVisualizer != RPS_NULL_HANDLE);

    rps::Visualizer* pVis = rps::FromHandle(hVisualizer);

    return pVis->Update(pUpdateInfo);
}

RpsResult rpsVisualizerDrawImGui(RpsVisualizer hVisualizer)
{
    RPS_CHECK_ARGS(hVisualizer != RPS_NULL_HANDLE);

    rps::Visualizer* pVis = rps::FromHandle(hVisualizer);

    return pVis->Draw();
}
