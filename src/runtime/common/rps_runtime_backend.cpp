// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "runtime/common/rps_render_graph.hpp"
#include "runtime/common/rps_render_graph_signature.hpp"
#include "runtime/common/rps_runtime_device.hpp"

namespace rps
{
    RpsResult RuntimeBackend::Run(RenderGraphUpdateContext& context)
    {
        RPS_V_RETURN(UpdateFrame(context));

        RPS_ASSERT(&context.renderGraph == &m_renderGraph);

        RPS_V_RETURN(CreateHeaps(context, m_renderGraph.GetHeapInfos().range_all()));

        auto&          resources          = m_renderGraph.GetResourceInstances();
        const uint32_t maxExternResources = m_renderGraph.GetSignature().GetMaxExternalResourceCount();
        RPS_ASSERT(maxExternResources <= resources.size());

        auto internalResources = resources.range(maxExternResources, resources.size() - maxExternResources);

        RPS_V_RETURN(CreateResources(context, internalResources));

        RPS_V_RETURN(CreateCommandResources(context));

        RPS_V_RETURN(UpdateResourceFinalAccessStates(context, internalResources));

        return RPS_OK;
    }

    RpsResult RuntimeBackend::UpdateResourceFinalAccessStates(RenderGraphUpdateContext&  context,
                                                              ArrayRef<ResourceInstance> resourceInstances)
    {
        // The main purpose is to propagate current frame initialAccess to prevFinalAccess.
        // Backends are expected to transition resources to current initial state, if they are:
        // - Persistent (including temporal / external).
        // - If the API doesn't support transitioning from "unknown" layout, e.g. D3D12 without enhanced barriers.

        // Skip if runtime command is empty.
        RPS_RETURN_OK_IF(context.renderGraph.GetRuntimeCmdInfos().empty());

        const uint32_t lastCmdId = context.renderGraph.GetRuntimeCmdInfos().rbegin()->GetTransitionId();
        RPS_RETURN_ERROR_IF_MSG(lastCmdId != CMD_ID_POSTAMBLE,
                                RPS_ERROR_INVALID_OPERATION,
                                "Expect the last runtime command to have id (%u) = CMD_ID_POSTAMBLE.",
                                lastCmdId);

        const bool bResetAliasedResourceToNoAccess = ShouldResetAliasedResourcesPrevFinalAccess();

        for (uint32_t iRes = 0, numRes = uint32_t(resourceInstances.size()); iRes < numRes; iRes++)
        {
            auto& resInstance = resourceInstances[iRes];

            RPS_ASSERT(!(resInstance.isAliased && resInstance.IsPersistent()));

            const bool bIsCreated = resInstance.hRuntimeResource && !resInstance.isPendingCreate;

            if (!resInstance.isExternal && bIsCreated && resInstance.isAccessed)
            {
                resInstance.prevFinalAccess = (bResetAliasedResourceToNoAccess && resInstance.isAliased)
                                                  ? AccessAttr{}
                                                  : resInstance.initialAccess;
            }
        }

        return RPS_OK;
    }

    void RuntimeBackend::OnDestroy()
    {
        DestroyCommandResources();

        DestroyResources(m_renderGraph.GetResourceInstances().range_all());

        DestroyHeaps(m_renderGraph.GetHeapInfos().range_all());
    }

    RpsResult RuntimeBackend::CloneContext(const RuntimeCmdCallbackContext& context,
                                           RpsRuntimeCommandBuffer          hNewCmdBuffer,
                                           const RpsCmdCallbackContext**    ppNewContext) const
    {
        auto pNewContext = m_renderGraph.FrameAlloc<RuntimeCmdCallbackContext>();
        *pNewContext     = context;

        pNewContext->hCommandBuffer    = hNewCmdBuffer;
        pNewContext->bIsPrimaryContext = false;

        *ppNewContext = pNewContext;

        return RPS_OK;
    }

    RpsResult RuntimeBackend::GetCmdArgResourceInfos(const RpsCmdCallbackContext* pContext,
                                                     uint32_t                     argIndex,
                                                     uint32_t                     srcArrayIndex,
                                                     const ResourceInstance**     ppResources,
                                                     uint32_t                     count)
    {
        RPS_CHECK_ARGS(pContext && ppResources);
        RPS_CHECK_ARGS(pContext && ppResources);
        auto pBackendContext = rps::RuntimeCmdCallbackContext::Get(pContext);

        RPS_RETURN_ERROR_IF(argIndex >= pBackendContext->pNodeDeclInfo->params.size(), RPS_ERROR_INDEX_OUT_OF_BOUNDS);

        auto& paramInfo = pBackendContext->pNodeDeclInfo->params[argIndex];
        RPS_RETURN_ERROR_IF(srcArrayIndex + count > paramInfo.numElements, RPS_ERROR_INDEX_OUT_OF_BOUNDS);

        const auto cmdAccessInfos =
            pBackendContext->pCmdInfo->accesses.Get(pBackendContext->pRenderGraph->GetCmdAccessInfos());

        for (uint32_t i = 0; i < count; i++)
        {
            auto& accessInfo = cmdAccessInfos[paramInfo.accessOffset + srcArrayIndex + i];
            ppResources[i]   = (accessInfo.resourceId != RPS_RESOURCE_ID_INVALID)
                                   ? &pBackendContext->pRenderGraph->GetResourceInstance(accessInfo.resourceId)
                                   : nullptr;
        }

        return RPS_OK;
    }

    void RuntimeBackend::RecordDebugMarker(const RuntimeCmdCallbackContext& context,
                                           RpsRuntimeDebugMarkerMode        mode,
                                           StrRef                           name) const
    {
        if (context.recordFlags & RPS_RECORD_COMMAND_FLAG_ENABLE_COMMAND_DEBUG_MARKERS)
        {
            const auto& runtimeCreateInfo    = RuntimeDevice::Get(m_renderGraph.GetDevice())->GetCreateInfo();
            auto        pfnRecordDebugMarker = runtimeCreateInfo.callbacks.pfnRecordDebugMarker;

            if (pfnRecordDebugMarker)
            {
                RpsRuntimeOpRecordDebugMarkerArgs markerArgs = {};
                markerArgs.hCommandBuffer                    = context.hCommandBuffer;
                markerArgs.pUserRecordContext                = context.pUserRecordContext;
                markerArgs.mode                              = mode;
                markerArgs.text                              = name.str;

                pfnRecordDebugMarker(runtimeCreateInfo.pUserContext, &markerArgs);
            }
        }
    }

    RpsResult RuntimeBackend::RecordCommand(RuntimeCmdCallbackContext& context, const RuntimeCmd& runtimeCmd) const
    {
        if (runtimeCmd.cmdId != RPS_CMD_ID_INVALID)
        {
            auto pCmdInfo = context.pRenderGraph->GetCmdInfo(runtimeCmd.cmdId);
            auto pCmd     = pCmdInfo->pCmdDecl;

            context.pNodeDeclInfo = pCmdInfo->pNodeDecl;
            context.pCmdInfo      = pCmdInfo;
            context.pCmd          = pCmd;
            context.pRuntimeCmd   = &runtimeCmd;
            context.cmdId         = runtimeCmd.cmdId;

            context.bIsCmdBeginEnd = true;
            RPS_V_RETURN(RecordCmdBegin(context));
            context.bIsCmdBeginEnd = false;

            if (pCmd->callback.pfnCallback)
            {
                context.pCmdCallbackContext = pCmd->callback.pUserContext;
                context.ppArgs              = pCmd->args.data();
                context.numArgs             = uint32_t(pCmd->args.size());
                context.userTag             = pCmd->tag;

                pCmd->callback.pfnCallback(&context);

                RPS_V_RETURN(context.result);
            }

            context.bIsCmdBeginEnd = true;
            RPS_V_RETURN(RecordCmdEnd(context));
            context.bIsCmdBeginEnd = false;
        }

        return RPS_OK;
    }

    RpsResult RuntimeBackend::RecordCmdBegin(const RuntimeCmdCallbackContext& context) const
    {
        RecordDebugMarker(context, RPS_RUNTIME_DEBUG_MARKER_BEGIN, context.pNodeDeclInfo->name.str);

        // Default render state setup for graphics nodes
        if (context.pNodeDeclInfo->MaybeGraphicsNode())
        {
            RPS_V_RETURN(RecordCmdRenderPassBegin(context));

            RPS_V_RETURN(RecordCmdFixedFunctionBindingsAndDynamicStates(context));
        }

        return RPS_OK;
    }

    RpsResult RuntimeBackend::RecordCmdEnd(const RuntimeCmdCallbackContext& context) const
    {
        if (context.pNodeDeclInfo->MaybeGraphicsNode())
        {
            RPS_V_RETURN(RecordCmdRenderPassEnd(context));
        }

        RecordDebugMarker(context, RPS_RUNTIME_DEBUG_MARKER_END, nullptr);

        return RPS_OK;
    }

}  // namespace rps
