// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "rps/runtime/common/rps_render_states.h"

#include "runtime/common/rps_render_graph.hpp"
#include "runtime/common/rps_runtime_device.hpp"
#include "runtime/common/rps_rpsl_host.hpp"
#include "runtime/common/rps_subprogram.hpp"

namespace rps
{
    RpsResult RenderGraph::Create(Device&                         device,
                                  const RpsRenderGraphCreateInfo* pCreateInfo,
                                  RenderGraph**                   ppRenderGraph)
    {
        RPS_CHECK_ARGS(ppRenderGraph);
        RPS_CHECK_ARGS(!pCreateInfo || ((pCreateInfo->numPhases == 0) == (pCreateInfo->pPhases == nullptr)));

        auto allocInfo = AllocInfo::FromType<RenderGraph>();

        void* pMemory = device.Allocate(allocInfo);
        RPS_CHECK_ALLOC(pMemory);

        auto pRuntimeDevice = RuntimeDevice::Get(device);
        RpsRenderGraphCreateInfo renderGraphCreateInfo = pCreateInfo ? *pCreateInfo : RpsRenderGraphCreateInfo{};
        pRuntimeDevice->PrepareRenderGraphCreation(renderGraphCreateInfo);

        *ppRenderGraph = new (pMemory) RenderGraph(device, renderGraphCreateInfo);

        if (pCreateInfo)
        {
            RPS_V_RETURN((*ppRenderGraph)->OnInit(renderGraphCreateInfo));
        }

        if (pRuntimeDevice)
        {
            if ((*ppRenderGraph)->m_createInfo.numPhases == 0)
            {
                RPS_V_RETURN(pRuntimeDevice->BuildDefaultRenderGraphPhases(**ppRenderGraph));
            }
        }

        if ((*ppRenderGraph)->m_pBackend == nullptr)
        {
            RPS_V_RETURN((*ppRenderGraph)->AddPhase<NullRuntimeBackend>(**ppRenderGraph));
        }

        // TODO: Apply user settings
        (*ppRenderGraph)->m_memoryTypes = pRuntimeDevice->GetMemoryTypeInfos();

        return RPS_OK;
    }

    void RenderGraph::Destroy()
    {
        const Device& device = GetDevice();

        OnDestroy();

        this->~RenderGraph();

        device.Free(this);
    }

    RenderGraph::RenderGraph(const Device& device, const RpsRenderGraphCreateInfo& createInfo)
        : m_device(device)
        , m_createInfo(createInfo)
        , m_persistentArena(device.Allocator())
        , m_frameArena(device.Allocator())
        , m_scratchArena(device.Allocator())
        , m_graph(device, m_frameArena)
        , m_phases(0, &m_persistentArena)
        , m_resourceCache(0, &m_persistentArena)
        , m_programInstances(0, &m_persistentArena)
        , m_cmds(0, &m_frameArena)
        , m_cmdAccesses(0, &m_frameArena)
        , m_transitions(0, &m_frameArena)
        , m_resourceFinalAccesses(0, &m_persistentArena)
        , m_runtimeCmdInfos(0, &m_frameArena)
        , m_cmdBatches(0, &m_frameArena)
        , m_cmdBatchWaitFenceIds(0, &m_frameArena)
        , m_aliasingInfos(0, &m_frameArena)
        , m_heaps(0, &m_persistentArena)
        , m_resourceClearValues(&m_persistentArena)
        , m_builder(*this, m_persistentArena, m_frameArena)
        , m_diagInfoArena(device.Allocator())
    {
        m_createInfo.mainEntryCreateInfo.pSignatureDesc = nullptr;

        m_diagData.resourceInfos.reset(&m_diagInfoArena);
        m_diagData.cmdInfos.reset(&m_diagInfoArena);
        m_diagData.heapInfos.reset(&m_diagInfoArena);
    }

    RpsResult RenderGraph::OnInit(const RpsRenderGraphCreateInfo& createInfo)
    {
        RPS_ASSERT(m_pMainEntry == nullptr);

        if (createInfo.scheduleInfo.numQueues > 0)
        {
            RPS_CHECK_ARGS(createInfo.scheduleInfo.pQueueInfos);

            auto queueInfosCopy = m_persistentArena.NewArray<RpsQueueFlags>(createInfo.scheduleInfo.numQueues);
            RPS_CHECK_ALLOC(!queueInfosCopy.empty());

            m_createInfo.scheduleInfo.pQueueInfos = queueInfosCopy.data();
            std::copy(createInfo.scheduleInfo.pQueueInfos,
                      createInfo.scheduleInfo.pQueueInfos + createInfo.scheduleInfo.numQueues,
                      queueInfosCopy.begin());
        }

        RPS_V_RETURN(Subprogram::Create(m_device, &createInfo.mainEntryCreateInfo, &m_pMainEntry));

        m_pSignature = m_pMainEntry->GetSignature();

        // TODO: Clean up
        m_programInstances.push_back(m_persistentArena.New<ProgramInstance>(m_pMainEntry, m_persistentArena));

        return m_builder.Init(m_pSignature, m_persistentArena, m_programInstances.back());
    }

    void RenderGraph::OnDestroy()
    {
        for (auto pPhase : m_phases)
        {
            pPhase->Destroy();
        }

        if (m_pMainEntry)
        {
            m_pMainEntry->Destroy();
            m_pMainEntry = nullptr;
        }
    }

    ProgramInstance* RenderGraph::GetOrCreateProgramInstance(Subprogram* pSubprogram, uint32_t& globalProgramInstanceId)
    {
        if (globalProgramInstanceId == RPS_INDEX_NONE_U32)
        {
            const uint32_t newProgramId = uint32_t(m_programInstances.size());

            if (!m_programInstances.push_back(m_persistentArena.New<ProgramInstance>(pSubprogram, m_persistentArena)))
            {
                return nullptr;
            }

            globalProgramInstanceId = newProgramId;
        }

        RPS_ASSERT(globalProgramInstanceId < m_programInstances.size());

        // In case the node was re-bound to a new program
        const auto pResult = m_programInstances[globalProgramInstanceId];
        if (pResult->m_pProgram != pSubprogram)
        {
            pResult->Reset(pSubprogram);
        }

        return pResult;
    }

    RpsResult RenderGraph::Update(const RpsRenderGraphUpdateInfo& updateInfo)
    {
        m_status = UpdateImpl(updateInfo);
        return m_status;
    }

    RpsResult RenderGraph::UpdateImpl(const RpsRenderGraphUpdateInfo& updateInfo)
    {
        m_frameArena.Reset();
        m_cmds.reset_keep_capacity(&m_frameArena);
        m_cmdAccesses.reset_keep_capacity(&m_frameArena);
        m_transitions.reset_keep_capacity(&m_frameArena);
        m_runtimeCmdInfos.reset_keep_capacity(&m_frameArena);
        m_cmdBatches.reset_keep_capacity(&m_frameArena);
        m_cmdBatchWaitFenceIds.reset_keep_capacity(&m_frameArena);
        m_aliasingInfos.reset_keep_capacity(&m_frameArena);

        ArenaCheckPoint arenaCheckpoint{m_scratchArena};

        m_graph.Reset();

        const RenderGraphSignature* const pSignature = m_pMainEntry->GetSignature();

        ArrayRef<RpsVariable, uint32_t> paramPtrs =
            m_scratchArena.NewArray<RpsVariable>(pSignature->GetParamDecls().size());

        for (uint32_t iParam = 0; iParam < paramPtrs.size(); iParam++)
        {
            auto paramVar     = m_builder.GetParamVariable(iParam);
            paramPtrs[iParam] = paramVar;

            if ((iParam < updateInfo.numArgs) && updateInfo.ppArgs[iParam])
            {
                memcpy(paramVar, updateInfo.ppArgs[iParam], pSignature->GetParamDecl(iParam).GetSize());
            }
        }

        {
            RPS_V_RETURN(m_builder.Begin());

            RpsResult buildResult = RPS_OK;

            if (updateInfo.pfnBuildCallback)
            {
                buildResult =
                    updateInfo.pfnBuildCallback(rps::ToHandle(&m_builder), paramPtrs.data(), paramPtrs.size());
            }
            else
            {
                RpslExecuteInfo rpslExecInfo = {m_pMainEntry, paramPtrs.data(), paramPtrs.size()};
                RpslHost        rpslHost(&m_builder);
                buildResult = rpslHost.Execute(rpslExecInfo);
            }

            if (RPS_FAILED(buildResult))
            {
                m_builder.SetBuildError(buildResult);
            }

            RPS_V_RETURN(m_builder.End());
        }

        RenderGraphUpdateContext updateContext = {
            &updateInfo, *this, RuntimeDevice::Get(m_device), m_frameArena, m_scratchArena};

        for (auto& phase : m_phases)
        {
            RPS_V_RETURN(phase->Run(updateContext));
        }

        return RPS_OK;
    }

    RpsResult RenderGraph::RecordCommands(const RpsRenderGraphRecordCommandInfo& recordInfo) const
    {
        RPS_RETURN_ERROR_IF(RPS_FAILED(m_status), RPS_ERROR_INVALID_OPERATION);

        return m_pBackend->RecordCommands(*this, recordInfo);
    }

    RpsResult RenderGraph::GetDiagnosticInfo(RpsRenderGraphDiagnosticInfo&     diagInfos,
                                             RpsRenderGraphDiagnosticInfoFlags diagnosticFlags)
    {
        const bool bFirst =
            m_diagData.resourceInfos.empty() && m_diagData.cmdInfos.empty() && m_diagData.heapInfos.empty();
        const bool bReturnCached = !!(diagnosticFlags & RPS_RENDER_GRAPH_DIAGNOSTIC_INFO_USE_CACHED_BIT);

        //Resize diag cache for non cached usage and first time
        if (!bReturnCached || bFirst)
        {
            RPS_V_RETURN(UpdateDiagCache());
        }

        diagInfos.numResourceInfos   = uint32_t(m_diagData.resourceInfos.size());
        diagInfos.numHeapInfos       = uint32_t(m_diagData.heapInfos.size());
        diagInfos.numCommandInfos    = uint32_t(m_diagData.cmdInfos.size());
        diagInfos.pResourceDiagInfos = m_diagData.resourceInfos.data();
        diagInfos.pCmdDiagInfos      = m_diagData.cmdInfos.data();
        diagInfos.pHeapDiagInfos     = m_diagData.heapInfos.data();

        return RPS_OK;
    }

    RpsResult RenderGraph::GetCmdRenderTargetInfo(RpsNodeId cmdId, RpsCmdRenderTargetInfo& renderTargetInfo) const
    {
        RPS_RETURN_ERROR_IF(cmdId >= m_cmds.size(), RPS_ERROR_INVALID_ARGUMENTS);
        RPS_RETURN_ERROR_IF(m_cmds[cmdId].pRenderPassInfo == nullptr, RPS_ERROR_INVALID_OPERATION);

        renderTargetInfo = m_cmds[cmdId].pRenderPassInfo->renderTargetInfo;
        return RPS_OK;
    }

    RpsResult RenderGraph::GetCmdViewportInfo(RpsNodeId cmdId, RpsCmdViewportInfo& viewportInfo) const
    {
        RPS_RETURN_ERROR_IF(cmdId >= m_cmds.size(), RPS_ERROR_INVALID_ARGUMENTS);
        RPS_RETURN_ERROR_IF(m_cmds[cmdId].pRenderPassInfo == nullptr, RPS_ERROR_INVALID_OPERATION);

        viewportInfo = m_cmds[cmdId].pRenderPassInfo->viewportInfo;
        return RPS_OK;
    }

    static void GetRuntimeResourceInfoFromResourceInstance(const ResourceInstance& resourceInstance,
                                                           RpsRuntimeResourceInfo& outResInfo)
    {
        outResInfo.hResource = resourceInstance.hRuntimeResource;
        resourceInstance.desc.Get(outResInfo.resourceDesc);
        outResInfo.numSubresources = resourceInstance.numSubResources;
        resourceInstance.fullSubresourceRange.Get(outResInfo.fullRange);
        outResInfo.heapId    = resourceInstance.allocPlacement.heapId;
        outResInfo.allocInfo = resourceInstance.allocRequirement;
    }

    RpsResult RenderGraph::GetRuntimeResourceInfo(RpsResourceId           resourceId,
                                                  uint32_t                temporalLayerIndex,
                                                  RpsRuntimeResourceInfo* pResourceInfo) const
    {
        RPS_CHECK_ARGS(pResourceInfo);

        RPS_CHECK_ARGS(resourceId < GetResourceInstances().size());

        const auto* pResourceInstance = &GetResourceInstance(resourceId);

        if (pResourceInstance->IsTemporalParent())
        {
            RPS_CHECK_ARGS(temporalLayerIndex != RPS_INDEX_NONE_U32);
            RPS_RETURN_ERROR_IF(temporalLayerIndex >= pResourceInstance->desc.temporalLayers,
                                RPS_ERROR_INDEX_OUT_OF_BOUNDS);

            pResourceInstance = &GetResourceInstance(pResourceInstance->temporalLayerOffset + temporalLayerIndex);
        }

        GetRuntimeResourceInfoFromResourceInstance(*pResourceInstance, *pResourceInfo);

        return RPS_OK;
    }

    RpsResult RenderGraph::GetOutputParameterRuntimeResourceInfos(RpsParamId              paramId,
                                                                  uint32_t                arrayOffset,
                                                                  uint32_t                resourceCount,
                                                                  RpsRuntimeResourceInfo* pResourceInfos) const
    {
        RPS_CHECK_ARGS(paramId < GetSignature().GetParamDecls().size());

        const auto resourceIds = m_builder.GetOutputParamResourceIds(paramId);

        RPS_CHECK_ARGS(arrayOffset < resourceIds.size());
        RPS_CHECK_ARGS((arrayOffset + resourceCount) <= resourceIds.size());

        const auto numResourceInstances = GetResourceInstances().size();
        const auto resourceIdsToGet     = resourceIds.range(arrayOffset, resourceCount);

        for (uint32_t i = 0; i < resourceCount; i++)
        {
            if (resourceIdsToGet[i] < numResourceInstances)
            {
                const auto& resourceInstance = GetResourceInstance(resourceIdsToGet[i]);

                // TODO: Need to handle temporal slice translation
                RPS_RETURN_ERROR_IF(resourceInstance.IsTemporalParent(), RPS_ERROR_NOT_IMPLEMENTED);

                GetRuntimeResourceInfoFromResourceInstance(resourceInstance, pResourceInfos[i]);
            }
            else
            {
                pResourceInfos[i] = {};
            }
        }

        return RPS_OK;
    }

}  // namespace rps

RpsResult rpsRenderGraphCreate(RpsDevice                       hDevice,
                               const RpsRenderGraphCreateInfo* pCreateInfo,
                               RpsRenderGraph*                 phRenderGraph)
{
    RPS_CHECK_ARGS(hDevice != RPS_NULL_HANDLE);

    return rps::RenderGraph::Create(*rps::FromHandle(hDevice), pCreateInfo, rps::FromHandle(phRenderGraph));
}

RpsResult rpsRenderGraphUpdate(RpsRenderGraph hRenderGraph, const RpsRenderGraphUpdateInfo* pUpdateInfo)
{
    RPS_CHECK_ARGS(hRenderGraph != RPS_NULL_HANDLE);
    RPS_CHECK_ARGS(pUpdateInfo != nullptr);
    RPS_CHECK_ARGS((pUpdateInfo->gpuCompletedFrameIndex + 1) <= pUpdateInfo->frameIndex);

    auto pRenderGraph = rps::FromHandle(hRenderGraph);

    return pRenderGraph->Update(*pUpdateInfo);
}

void rpsRenderGraphDestroy(RpsRenderGraph hRenderGraph)
{
    if (hRenderGraph != RPS_NULL_HANDLE)
    {
        rps::FromHandle(hRenderGraph)->Destroy();
    }
}

RpsResult rpsRenderGraphGetResourceInfo(RpsRenderGraph          hRenderGraph,
                                        RpsResourceId           resourceId,
                                        uint32_t                temporalLayerIndex,
                                        RpsRuntimeResourceInfo* pResourceInfo)
{
    RPS_CHECK_ARGS(hRenderGraph != RPS_NULL_HANDLE);
    return rps::FromHandle(hRenderGraph)->GetRuntimeResourceInfo(resourceId, temporalLayerIndex, pResourceInfo);
}

RpsResult rpsRenderGraphGetOutputParameterResourceInfos(RpsRenderGraph          hRenderGraph,
                                                        RpsParamId              paramId,
                                                        uint32_t                arrayOffset,
                                                        uint32_t                resourceCount,
                                                        RpsRuntimeResourceInfo* pResourceInfos)
{
    RPS_CHECK_ARGS(hRenderGraph != RPS_NULL_HANDLE);
    return rps::FromHandle(hRenderGraph)
        ->GetOutputParameterRuntimeResourceInfos(paramId, arrayOffset, resourceCount, pResourceInfos);
}

RpsSubprogram rpsRenderGraphGetMainEntry(RpsRenderGraph hRenderGraph)
{
    RPS_RETURN_ERROR_IF(hRenderGraph == RPS_NULL_HANDLE, RPS_NULL_HANDLE);
    return rps::ToHandle(rps::FromHandle(hRenderGraph)->GetMainEntry());
}

RpsResult rpsRenderGraphGetBatchLayout(RpsRenderGraph hRenderGraph, RpsRenderGraphBatchLayout* pBatchLayout)
{
    RPS_CHECK_ARGS(hRenderGraph != RPS_NULL_HANDLE);
    RPS_CHECK_ARGS(pBatchLayout != nullptr);

    return rps::FromHandle(hRenderGraph)->GetBatchLayout(*pBatchLayout);
}

RpsResult rpsRenderGraphRecordCommands(RpsRenderGraph hRenderGraph, const RpsRenderGraphRecordCommandInfo* pRecordRange)
{
    RPS_CHECK_ARGS(hRenderGraph != RPS_NULL_HANDLE);
    RPS_CHECK_ARGS(pRecordRange != nullptr);

    return rps::FromHandle(hRenderGraph)->RecordCommands(*pRecordRange);
}

RpsResult rpsRenderGraphGetDiagnosticInfo(RpsRenderGraph                    hRenderGraph,
                                          RpsRenderGraphDiagnosticInfo*     pInfo,
                                          RpsRenderGraphDiagnosticInfoFlags diagnosticFlags)
{
    RPS_CHECK_ARGS(hRenderGraph);
    RPS_CHECK_ARGS(pInfo);

    return rps::FromHandle(hRenderGraph)->GetDiagnosticInfo(*pInfo, diagnosticFlags);
}

RpsResult rpsCmdCallbackReportError(const RpsCmdCallbackContext* pContext, RpsResult errorCode)
{
    RPS_CHECK_ARGS(pContext);
    RPS_RETURN_OK_IF(errorCode == RPS_OK);

    auto pBackendContext = rps::RuntimeCmdCallbackContext::GetMutable(pContext);

    RPS_RETURN_ERROR_IF(!pBackendContext->bIsPrimaryContext, RPS_ERROR_INVALID_OPERATION);
    RPS_RETURN_ERROR_IF(pBackendContext->result != RPS_OK, RPS_ERROR_INVALID_OPERATION);

    pBackendContext->result = errorCode;

    return RPS_OK;
}

RpsResult rpsCmdGetRenderTargetsInfo(const RpsCmdCallbackContext* pContext, RpsCmdRenderTargetInfo* pRenderTargetInfo)
{
    RPS_CHECK_ARGS(pContext && pRenderTargetInfo);

    auto pBackendContext = rps::RuntimeCmdCallbackContext::Get(pContext);
    return pBackendContext->pRenderGraph->GetCmdRenderTargetInfo(pBackendContext->cmdId, *pRenderTargetInfo);
}

RpsResult rpsCmdGetViewportInfo(const RpsCmdCallbackContext* pContext, RpsCmdViewportInfo* pViewportInfo)
{
    RPS_CHECK_ARGS(pContext && pViewportInfo);

    auto pBackendContext = rps::RuntimeCmdCallbackContext::Get(pContext);
    return pBackendContext->pRenderGraph->GetCmdViewportInfo(pBackendContext->cmdId, *pViewportInfo);
}

RpsResult rpsCmdGetNodeName(const RpsCmdCallbackContext* pContext, const char** ppNodeName, size_t* pNodeNameLength)
{
    RPS_CHECK_ARGS(pContext && ppNodeName);

    auto pBackendContext = rps::RuntimeCmdCallbackContext::Get(pContext);

    auto& nodeDecl = *pBackendContext->pCmdInfo->pNodeDecl;

    *ppNodeName = nodeDecl.name.str;

    if (pNodeNameLength)
    {
        *pNodeNameLength = nodeDecl.name.len;
    }

    return RPS_OK;
}

RpsResult rpsCmdGetParamDesc(const RpsCmdCallbackContext* pContext, RpsParamId paramId, RpsParameterDesc* pDesc)
{
    RPS_CHECK_ARGS(pContext && pDesc);

    auto pBackendContext = rps::RuntimeCmdCallbackContext::Get(pContext);

    auto& nodeDecl = *pBackendContext->pCmdInfo->pNodeDecl;

    RPS_RETURN_ERROR_IF(paramId >= nodeDecl.params.size(), RPS_ERROR_INDEX_OUT_OF_BOUNDS);
    nodeDecl.params[paramId].GetDesc(pDesc);

    return RPS_OK;
}

RpsResult rpsCmdGetArgResourceDescArray(const RpsCmdCallbackContext* pContext,
                                        RpsParamId                   argIndex,
                                        uint32_t                     srcArrayOffset,
                                        RpsResourceDesc*             pResourceDesc,
                                        uint32_t                     numDescs)
{
    RPS_CHECK_ARGS(pContext && pResourceDesc);

    const auto* pBackendContext = rps::RuntimeCmdCallbackContext::Get(pContext);

    RPS_RETURN_ERROR_IF(argIndex >= pBackendContext->pNodeDeclInfo->params.size(), RPS_ERROR_INDEX_OUT_OF_BOUNDS);

    const auto& paramAccessInfo = pBackendContext->pNodeDeclInfo->params[argIndex];

    RPS_RETURN_ERROR_IF(!paramAccessInfo.IsResource(), RPS_ERROR_TYPE_MISMATCH);
    RPS_RETURN_ERROR_IF(srcArrayOffset + numDescs > paramAccessInfo.numElements, RPS_ERROR_INDEX_OUT_OF_BOUNDS);

    auto cmdAccesses = pBackendContext->pRenderGraph->GetCmdAccesses(pBackendContext->cmdId);

    for (uint32_t descIndex = 0; descIndex < numDescs; ++descIndex)
    {
        const auto&    accessInfo = cmdAccesses[paramAccessInfo.accessOffset + srcArrayOffset + descIndex];
        const uint32_t resId      = accessInfo.resourceId;

        if (resId != RPS_RESOURCE_ID_INVALID)
        {
            pBackendContext->pRenderGraph->GetResourceInstance(resId).desc.Get(pResourceDesc[descIndex]);
        }
        else
        {
            *pResourceDesc = rps::ResourceDesc();
        }
    }

    return RPS_OK;
}

RpsResult rpsCmdGetArgResourceDesc(const RpsCmdCallbackContext* pContext,
                                   RpsParamId                   argIndex,
                                   RpsResourceDesc*             pResourceDesc)
{
    return rpsCmdGetArgResourceDescArray(pContext, argIndex, 0, pResourceDesc, 1);
}

RpsResult rpsCmdGetArgRuntimeResourceArray(const RpsCmdCallbackContext* pContext,
                                           RpsParamId                   argIndex,
                                           uint32_t                     srcArrayOffset,
                                           RpsRuntimeResource*          pRuntimeResources,
                                           uint32_t                     numResources)
{
    RPS_CHECK_ARGS(pContext && pRuntimeResources);

    const auto* pBackendContext = rps::RuntimeCmdCallbackContext::Get(pContext);

    RPS_RETURN_ERROR_IF(argIndex >= pBackendContext->pNodeDeclInfo->params.size(), RPS_ERROR_INDEX_OUT_OF_BOUNDS);

    const auto& paramAccessInfo = pBackendContext->pNodeDeclInfo->params[argIndex];

    RPS_RETURN_ERROR_IF(!paramAccessInfo.IsResource(), RPS_ERROR_TYPE_MISMATCH);
    RPS_RETURN_ERROR_IF(srcArrayOffset + numResources > paramAccessInfo.numElements, RPS_ERROR_INDEX_OUT_OF_BOUNDS);

    auto cmdAccesses = pBackendContext->pRenderGraph->GetCmdAccesses(pBackendContext->cmdId);

    for (uint32_t resourceIndex = 0; resourceIndex < numResources; ++resourceIndex)
    {
        const auto&    accessInfo = cmdAccesses[paramAccessInfo.accessOffset + srcArrayOffset + resourceIndex];
        const uint32_t resId      = accessInfo.resourceId;

        pRuntimeResources[resourceIndex] =
            resId != RPS_RESOURCE_ID_INVALID
                ? pBackendContext->pRenderGraph->GetResourceInstance(resId).hRuntimeResource
                : RpsRuntimeResource();
    }

    return RPS_OK;
}

RpsResult rpsCmdGetArgRuntimeResource(const RpsCmdCallbackContext* pContext,
                                      RpsParamId                   argIndex,
                                      RpsRuntimeResource*          pRuntimeResource)
{
    return rpsCmdGetArgRuntimeResourceArray(pContext, argIndex, 0, pRuntimeResource, 1);
}

RpsResult rpsCmdGetArgResourceAccessInfoArray(const RpsCmdCallbackContext* pContext,
                                              RpsParamId                   argIndex,
                                              uint32_t                     srcArrayOffset,
                                              RpsResourceAccessInfo*       pResourceAccessInfos,
                                              uint32_t                     numAccessess)
{
    RPS_CHECK_ARGS(pContext && pResourceAccessInfos);

    const auto* pBackendContext = rps::RuntimeCmdCallbackContext::Get(pContext);

    RPS_RETURN_ERROR_IF(argIndex >= pBackendContext->pNodeDeclInfo->params.size(), RPS_ERROR_INDEX_OUT_OF_BOUNDS);

    const auto& paramAccessInfo = pBackendContext->pNodeDeclInfo->params[argIndex];

    RPS_RETURN_ERROR_IF(!paramAccessInfo.IsResource(), RPS_ERROR_TYPE_MISMATCH);
    RPS_RETURN_ERROR_IF(srcArrayOffset + numAccessess > paramAccessInfo.numElements, RPS_ERROR_INDEX_OUT_OF_BOUNDS);

    auto cmdAccesses = pBackendContext->pRenderGraph->GetCmdAccesses(pBackendContext->cmdId);

    for (uint32_t accessIndex = 0; accessIndex < numAccessess; ++accessIndex)
    {
        const auto&    accessInfo = cmdAccesses[paramAccessInfo.accessOffset + srcArrayOffset + accessIndex];
        const uint32_t resId      = accessInfo.resourceId;

        if (resId != RPS_RESOURCE_ID_INVALID)
        {
            accessInfo.Get(pResourceAccessInfos[accessIndex]);
        }
        else
        {
            pResourceAccessInfos[accessIndex] = {};
        }
    }

    return RPS_OK;
}

RpsResult rpsCmdGetArgResourceAccessInfo(const RpsCmdCallbackContext* pContext,
                                         RpsParamId                   argIndex,
                                         RpsResourceAccessInfo*       pResourceAccessInfo)
{
    return rpsCmdGetArgResourceAccessInfoArray(pContext, argIndex, 0, pResourceAccessInfo, 1);
}

RpsResult rpsCmdCloneContext(const RpsCmdCallbackContext*  pContext,
                             RpsRuntimeCommandBuffer       hCmdBufferForDerivedContext,
                             const RpsCmdCallbackContext** ppDerivedContext)
{
    RPS_CHECK_ARGS(pContext && ppDerivedContext);

    auto pBackendContext = rps::RuntimeCmdCallbackContext::Get(pContext);

    return pBackendContext->pRenderGraph->GetRuntimeBackend()->CloneContext(
        *pBackendContext, hCmdBufferForDerivedContext, ppDerivedContext);
}

RpsResult rpsCmdBeginRenderPass(const RpsCmdCallbackContext* pContext, const RpsCmdRenderPassBeginInfo* pBeginInfo)
{
    RPS_CHECK_ARGS(pContext);

    auto pBackendContext             = rps::RuntimeCmdCallbackContext::GetMutable(pContext);
    pBackendContext->renderPassFlags = pBeginInfo->flags;

    return pBackendContext->pRenderGraph->GetRuntimeBackend()->RecordCmdRenderPassBegin(*pBackendContext);
}

RpsResult rpsCmdEndRenderPass(const RpsCmdCallbackContext* pContext)
{
    RPS_CHECK_ARGS(pContext);

    auto pBackendContext = rps::RuntimeCmdCallbackContext::Get(pContext);

    return pBackendContext->pRenderGraph->GetRuntimeBackend()->RecordCmdRenderPassEnd(*pBackendContext);
}

RpsResult rpsCmdSetCommandBuffer(const RpsCmdCallbackContext* pContext, RpsRuntimeCommandBuffer hCmdBuffer)
{
    RPS_CHECK_ARGS(pContext);

    auto pBackendContext = rps::RuntimeCmdCallbackContext::GetMutable(pContext);

    pBackendContext->hCommandBuffer = hCmdBuffer;

    return RPS_OK;
}
