// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "rps/runtime/d3d11/rps_d3d11_runtime.h"
#include "rps/runtime/common/rps_render_states.h"

#include "runtime/common/rps_render_graph.hpp"
#include "runtime/common/rps_runtime_util.hpp"
#include "runtime/d3d11/rps_d3d11_runtime_backend.hpp"
#include "runtime/d3d11/rps_d3d11_runtime_device.hpp"
#include "runtime/d3d11/rps_d3d11_util.hpp"

namespace rps
{
    RpsResult D3D11RuntimeBackend::CreateCommandResources(const RenderGraphUpdateContext& context)
    {
        const auto& graph         = context.renderGraph.GetGraph();
        auto&       runtimeCmds   = context.renderGraph.GetRuntimeCmdInfos();
        const auto  cmdInfos      = context.renderGraph.GetCmdInfos().range_all();

        ArenaCheckPoint arenaCheckpoint{context.scratchArena};

        uint32_t numGraphicsCmds = 0;

        for (uint32_t iCmd = 0, numCmds = uint32_t(runtimeCmds.size()); iCmd < numCmds; iCmd++)
        {
            const auto& runtimeCmd = runtimeCmds[iCmd];

            if (runtimeCmd.isTransition)
            {
                continue;
            }

            auto pNewRuntimeCmd = m_runtimeCmds.grow(1);

            pNewRuntimeCmd->cmdId               = runtimeCmd.cmdId;
            pNewRuntimeCmd->resourceBindingInfo = RPS_INDEX_NONE_U32;
        }

        // Create Views

        ArenaVector<uint32_t> srvs(&context.scratchArena);
        ArenaVector<uint32_t> uavs(&context.scratchArena);
        ArenaVector<uint32_t> rtvs(&context.scratchArena);
        ArenaVector<uint32_t> dsvs(&context.scratchArena);

        srvs.reserve(context.renderGraph.GetCmdAccessInfos().size());
        uavs.reserve(context.renderGraph.GetCmdAccessInfos().size());
        rtvs.reserve(context.renderGraph.GetCmdAccessInfos().size());

        const auto& cmdAccesses = context.renderGraph.GetCmdAccessInfos();

        for (auto& runtimeCmd : m_runtimeCmds)
        {
            if (runtimeCmd.cmdId == RPS_CMD_ID_INVALID)
                continue;

            auto& cmdInfo      = cmdInfos[runtimeCmd.cmdId];
            auto& nodeDeclInfo = *cmdInfo.pNodeDecl;

            const uint32_t accessOffset = cmdInfo.accesses.GetBegin();

            for (uint32_t accessIdx = 0, accessCount = cmdInfo.accesses.size(); accessIdx < accessCount; accessIdx++)
            {
                const uint32_t globalAccessIdx = accessOffset + accessIdx;
                auto&          access          = cmdAccesses[globalAccessIdx];

                if (!rpsAnyBitsSet(access.access.accessFlags, RPS_ACCESS_NO_VIEW_BIT))
                {
                    if (rpsAnyBitsSet(access.access.accessFlags, RPS_ACCESS_SHADER_RESOURCE_BIT))
                    {
                        srvs.push_back(globalAccessIdx);
                    }
                    else if (rpsAnyBitsSet(access.access.accessFlags, RPS_ACCESS_UNORDERED_ACCESS_BIT))
                    {
                        uavs.push_back(globalAccessIdx);
                    }
                    else if (rpsAnyBitsSet(access.access.accessFlags, RPS_ACCESS_RENDER_TARGET_BIT))
                    {
                        rtvs.push_back(globalAccessIdx);
                    }
                    else if (rpsAnyBitsSet(access.access.accessFlags, RPS_ACCESS_DEPTH_STENCIL))
                    {
                        dsvs.push_back(globalAccessIdx);
                    }
                }
            }
        }

        m_views.resize(cmdAccesses.size(), nullptr);

        RPS_V_RETURN(CreateResourceViews(context, ViewType::SRV, srvs.range_all()));
        RPS_V_RETURN(CreateResourceViews(context, ViewType::UAV, uavs.range_all()));
        RPS_V_RETURN(CreateResourceViews(context, ViewType::RTV, rtvs.range_all()));
        RPS_V_RETURN(CreateResourceViews(context, ViewType::DSV, dsvs.range_all()));

        // TODO: Multi queue
        auto& cmdBatches = context.renderGraph.GetCmdBatches();
        if (!m_runtimeCmds.empty())
        {
            cmdBatches.resize(1, CommandBatch{});
            cmdBatches[0].cmdBegin = 0;
            cmdBatches[0].numCmds = uint32_t(m_runtimeCmds.size());
        }

        return RPS_OK;
    }

    void D3D11RuntimeBackend::OnDestroy()
    {
        for (auto& frameResource : m_frameResources)
        {
            frameResource.DestroyDeviceResources();
        }

        m_frameResources.clear();

        std::for_each(m_views.begin(), m_views.end(), [](auto pView) { SafeRelease(pView); });

        m_views.clear();

        RuntimeBackend::OnDestroy();
    }

    RpsResult D3D11RuntimeBackend::UpdateFrame(const RenderGraphUpdateContext& context)
    {
        m_currentResourceFrame =
            m_frameResources.empty() ? 0 : (m_currentResourceFrame + 1) % uint32_t(m_frameResources.size());

        if (m_frameResources.size() <= GetNumQueuedFrames(context))
        {
            RPS_RETURN_ERROR_IF(m_frameResources.size() > RPS_MAX_QUEUED_FRAMES, RPS_ERROR_INVALID_OPERATION);

            RPS_CHECK_ALLOC(m_frameResources.insert(m_currentResourceFrame, FrameResources{}));
            m_frameResources[m_currentResourceFrame].Reset(m_persistentPool);
        }
        else
        {
            // TODO - Recycle
            m_frameResources[m_currentResourceFrame].DestroyDeviceResources();
            std::swap(m_pendingReleaseResources, m_frameResources[m_currentResourceFrame].pendingResources);
        }

        // TODO
        auto& pendingRes = m_frameResources[m_currentResourceFrame].pendingResources;
        pendingRes.reserve(pendingRes.size() + m_views.size());
        std::for_each(m_views.begin(), m_views.end(), [&](auto pView) {
            if (pView)
            {
                pendingRes.push_back(pView);
            }
        });

        // TODO:
        m_runtimeCmds.reset(&context.frameArena);
        m_views.clear();

        return RPS_OK;
    }

    RpsResult D3D11RuntimeBackend::CreateHeaps(const RenderGraphUpdateContext& context, ArrayRef<HeapInfo> heaps)
    {
        return RPS_OK;
    }

    void D3D11RuntimeBackend::DestroyHeaps(ArrayRef<HeapInfo> heaps)
    {
    }

    RpsResult D3D11RuntimeBackend::CreateResources(const RenderGraphUpdateContext& context,
                                                   ArrayRef<ResourceInstance>      resInstances)
    {
        ID3D11Device* pD3DDevice    = m_device.GetD3DDevice();
        auto          resourceDecls = GetRenderGraph().GetBuilder().GetResourceDecls();

        const bool bEnableDebugNames =
            !!(context.pUpdateInfo->diagnosticFlags & RPS_DIAGNOSTIC_ENABLE_RUNTIME_DEBUG_NAMES);

        uint32_t temporalSlice = RPS_INDEX_NONE_U32;

        // Create resources
        for (uint32_t iRes = 0, numRes = uint32_t(resInstances.size()); iRes < numRes; iRes++)
        {
            auto& resInfo = resInstances[iRes];

            if (resInfo.isExternal)
            {
                continue;
            }

            temporalSlice =
                resInfo.isFirstTemporalSlice ? 0 : (resInfo.isTemporalSlice ? (temporalSlice + 1) : RPS_INDEX_NONE_U32);

            if (resInfo.isPendingCreate && !resInfo.HasEmptyLifetime())
            {
                if (resInfo.hRuntimeResource)
                {
                    m_frameResources[m_currentResourceFrame].pendingResources.push_back(
                        D3D11RuntimeDevice::FromHandle(resInfo.hRuntimeResource));
                    resInfo.hRuntimeResource = {};
                }

                RPS_ASSERT(resInfo.allocPlacement.heapId == RPS_INDEX_NONE_U32);

                ID3D11Resource* pD3DRes = nullptr;
                RPS_V_RETURN(CreateD3D11ResourceDesc(pD3DDevice, resInfo, &pD3DRes));

                resInfo.hRuntimeResource = rpsD3D11ResourceToHandle(pD3DRes);
                resInfo.FinalizeRuntimeResourceCreation();

                if (bEnableDebugNames)
                {
                    SetResourceDebugName(pD3DRes, resourceDecls[resInfo.resourceDeclId].name, temporalSlice);
                }
            }
        }

        return RPS_OK;
    }

    void D3D11RuntimeBackend::DestroyResources(ArrayRef<ResourceInstance> resources)
    {
        for (auto& resInfo : resources)
        {
            if (!resInfo.isExternal && resInfo.hRuntimeResource)
            {
                rpsD3D11ResourceFromHandle(resInfo.hRuntimeResource)->Release();
            }
        }
    }

    void D3D11RuntimeBackend::SetResourceDebugName(ID3D11DeviceChild* pObject, StrRef name, uint32_t index)
    {
        if (!pObject || name.empty())
        {
            return;
        }

        if (index != RPS_INDEX_NONE_U32)
        {
            char buf[RPS_NAME_MAX_LEN];
            snprintf(buf, RPS_NAME_MAX_LEN, "%s[%u]", name.str, index);

            pObject->SetPrivateData(WKPDID_D3DDebugObjectName, uint32_t(strlen(buf)), buf);
        }
        else
        {
            pObject->SetPrivateData(WKPDID_D3DDebugObjectName, uint32_t(name.len), name.str);
        }
    }

    RpsResult D3D11RuntimeBackend::RecordCommands(const RenderGraph&                     renderGraph,
                                                  const RpsRenderGraphRecordCommandInfo& recordInfo) const
    {
        RuntimeCmdCallbackContext cmdCbCtx{this, recordInfo};

        for (auto cmdIter = m_runtimeCmds.cbegin() + recordInfo.cmdBeginIndex, cmdEnd = cmdIter + recordInfo.numCmds;
             cmdIter != cmdEnd;
             ++cmdIter)
        {
            RecordCommand(cmdCbCtx, *cmdIter);
        }

        return RPS_OK;
    }

    void D3D11RuntimeBackend::DestroyRuntimeResourceDeferred(ResourceInstance& resource)
    {
        if (resource.hRuntimeResource)
        {
            m_pendingReleaseResources.push_back(D3D11RuntimeDevice::FromHandle(resource.hRuntimeResource));
            resource.hRuntimeResource = {};
        }
    }

    RpsResult D3D11RuntimeBackend::RecordCmdRenderPassBegin(const RuntimeCmdCallbackContext& context) const
    {
        auto&       renderGraph  = *context.pRenderGraph;
        auto&       cmd          = *context.pCmd;
        auto*       pCmdInfo     = context.pCmdInfo;
        const auto& nodeDeclInfo = *pCmdInfo->pNodeDecl;

        RPS_RETURN_ERROR_IF(!nodeDeclInfo.MaybeGraphicsNode(), RPS_ERROR_INVALID_OPERATION);

        auto pD3DDeviceContext = GetD3DDeviceContext(context);

        const auto cmdCbFlags = context.bIsCmdBeginEnd ? cmd.callback.flags : RPS_CMD_CALLBACK_FLAG_NONE;

        const bool bBindRenderTargets   = !rpsAnyBitsSet(cmdCbFlags, RPS_CMD_CALLBACK_CUSTOM_RENDER_TARGETS_BIT);
        const bool bSetViewportScissors = !rpsAnyBitsSet(cmdCbFlags, RPS_CMD_CALLBACK_CUSTOM_VIEWPORT_SCISSOR_BIT);

        // Need to skip clears if it's render pass resume
        const bool bIsRenderPassResuming = rpsAnyBitsSet(context.renderPassFlags, RPS_RUNTIME_RENDER_PASS_RESUMING);

        auto cmdViewRange = m_views.range(pCmdInfo->accesses.GetBegin(), pCmdInfo->accesses.size());

        uint32_t numRtvs = 0;

        ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
        ID3D11DepthStencilView* dsv = {0};

        D3D11_RECT d3dScissorRects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};

        if ((nodeDeclInfo.pRenderPassInfo) && !(nodeDeclInfo.pRenderPassInfo->clearOnly) &&
            (bBindRenderTargets || !bIsRenderPassResuming))
        {
            auto& rpInfo = *nodeDeclInfo.pRenderPassInfo;

            auto     clearColorValueRefs  = rpInfo.GetRenderTargetClearValueRefs();
            uint32_t clearColorValueIndex = 0;

            for (auto& rtParamRef : rpInfo.GetRenderTargetRefs())
            {
                auto& paramAccessInfo = nodeDeclInfo.params[rtParamRef.paramId];

                const uint32_t rtvSlot = paramAccessInfo.baseSemanticIndex + rtParamRef.arrayOffset;

                numRtvs = rpsMax(numRtvs, rtvSlot + 1);

                rtvs[rtvSlot] = static_cast<ID3D11RenderTargetView*>(
                    cmdViewRange[paramAccessInfo.accessOffset + rtParamRef.arrayOffset]);

                if ((!bIsRenderPassResuming) && (rpInfo.renderTargetClearMask & (1u << rtvSlot)))
                {
                    auto clearValueRef = clearColorValueRefs[clearColorValueIndex];

                    auto pClearColor = static_cast<const RpsClearColorValue*>(
                                           cmd.args[clearValueRef.paramId])[clearValueRef.arrayOffset]
                                           .float32;

                    // TODO: Sub-rect clear implemented separatedly
                    pD3DDeviceContext->ClearRenderTargetView(rtvs[rtvSlot], pClearColor);

                    clearColorValueIndex++;
                }
            }

            if (rpInfo.depthStencilTargetMask)
            {
                auto& paramAccessInfo = nodeDeclInfo.params[rpInfo.GetDepthStencilRef()->paramId];
                RPS_ASSERT(paramAccessInfo.numElements == 1);

                dsv = static_cast<ID3D11DepthStencilView*>(cmdViewRange[paramAccessInfo.accessOffset]);

                if ((!bIsRenderPassResuming) && (rpInfo.clearDepth || rpInfo.clearStencil))
                {
                    float    depthClearValue   = 0.0f;
                    uint32_t stencilClearValue = 0;
                    uint32_t clearFlag         = {};

                    if (rpInfo.clearDepth)
                    {
                        auto pClearValueRef = rpInfo.GetDepthClearValueRef();
                        depthClearValue     = static_cast<const float*>(cmd.args[pClearValueRef->paramId])[0];
                        clearFlag |= D3D11_CLEAR_DEPTH;
                    }

                    if (rpInfo.clearStencil)
                    {
                        auto pClearValueRef = rpInfo.GetStencilClearValueRef();
                        stencilClearValue   = static_cast<const uint32_t*>(cmd.args[pClearValueRef->paramId])[0];
                        clearFlag |= D3D11_CLEAR_STENCIL;
                    }

                    pD3DDeviceContext->ClearDepthStencilView(dsv, clearFlag, depthClearValue, UINT8(stencilClearValue));
                }
            }

            if (bBindRenderTargets && ((numRtvs > 0) || (dsv != nullptr)))
            {
                pD3DDeviceContext->OMSetRenderTargets(numRtvs, rtvs, dsv);
            }

            if (bSetViewportScissors)
            {
                auto& cmdRPInfo = *pCmdInfo->pRenderPassInfo;

                RPS_STATIC_ASSERT(sizeof(D3D11_VIEWPORT) == sizeof(RpsViewport),
                                  "RpsViewport / D3D11_VIEWPORT size mismatch");

                pD3DDeviceContext->RSSetViewports(
                    cmdRPInfo.viewportInfo.numViewports,
                    reinterpret_cast<const D3D11_VIEWPORT*>(cmdRPInfo.viewportInfo.pViewports));

                RPS_RETURN_ERROR_IF(cmdRPInfo.viewportInfo.numScissorRects > RPS_COUNTOF(d3dScissorRects),
                                    RPS_ERROR_INDEX_OUT_OF_BOUNDS);

                for (uint32_t iScissor = 0; iScissor < cmdRPInfo.viewportInfo.numScissorRects; iScissor++)
                {
                    auto& rect                = cmdRPInfo.viewportInfo.pScissorRects[iScissor];
                    d3dScissorRects[iScissor] = D3D11_RECT{
                        rect.x,
                        rect.y,
                        rect.x + rect.width,
                        rect.y + rect.height,
                    };
                }

                pD3DDeviceContext->RSSetScissorRects(cmdRPInfo.viewportInfo.numScissorRects, d3dScissorRects);
            }
        }

        return RPS_OK;
    }

    RpsResult D3D11RuntimeBackend::RecordCmdRenderPassEnd(const RuntimeCmdCallbackContext& context) const
    {
        auto& renderGraph  = *context.pRenderGraph;
        auto* pCmdInfo     = context.pCmdInfo;
        auto  cmdAccesses  = pCmdInfo->accesses.Get(renderGraph.GetCmdAccessInfos());
        auto  resInstances = renderGraph.GetResourceInstances().range_all();

        auto pD3DDeviceContext = GetD3DDeviceContext(context);

        // TODO: Add a pass to split graphics cmd to separated clear / resolve cmds

        const auto& nodeDeclInfo = *pCmdInfo->pNodeDecl;
        if ((nodeDeclInfo.pRenderPassInfo) && (nodeDeclInfo.pRenderPassInfo->resolveTargetsMask != 0))
        {
            auto rpInfo      = *nodeDeclInfo.pRenderPassInfo;
            auto resolveDsts = rpInfo.GetResolveTargetRefs();
            auto resolveSrcs = rpInfo.GetRenderTargetRefs();

            uint32_t srcMask  = rpInfo.renderTargetsMask;
            uint32_t dstMask  = rpInfo.resolveTargetsMask;
            uint32_t srcIndex = 0;
            uint32_t dstIndex = 0;

            while (dstMask != 0)
            {
                uint32_t nextRTMask = (1u << rpsFirstBitLow(srcMask));
                srcMask &= ~nextRTMask;

                if (dstMask & nextRTMask)
                {
                    dstMask &= ~nextRTMask;

                    auto& dstParamAccessInfo = nodeDeclInfo.params[resolveDsts[dstIndex].paramId];
                    auto& dstAccessInfo      = cmdAccesses[dstParamAccessInfo.accessOffset];
                    auto& dstResInfo         = resInstances[dstAccessInfo.resourceId];
                    auto  pD3DResDst         = rpsD3D11ResourceFromHandle(dstResInfo.hRuntimeResource);

                    auto& srcParamAccessInfo = nodeDeclInfo.params[resolveSrcs[srcIndex].paramId];
                    auto& srcAccessInfo      = cmdAccesses[srcParamAccessInfo.accessOffset];
                    auto& srcResInfo         = resInstances[srcAccessInfo.resourceId];
                    auto  pD3DResSrc         = rpsD3D11ResourceFromHandle(srcResInfo.hRuntimeResource);

                    RPS_ASSERT(dstAccessInfo.range.GetNumSubresources() == srcAccessInfo.range.GetNumSubresources());
                    RPS_ASSERT(dstAccessInfo.range.aspectMask == 1);
                    RPS_ASSERT(dstAccessInfo.range.GetMipLevelCount() == 1);

                    auto format = rpsFormatToDXGI(dstAccessInfo.viewFormat);

                    for (uint32_t iArray = 0; iArray < dstAccessInfo.range.GetArrayLayerCount(); iArray++)
                    {
                        uint32_t dstSubRes = D3D11CalcSubresource(dstAccessInfo.range.baseMipLevel,
                                                                  iArray + dstAccessInfo.range.baseArrayLayer,
                                                                  dstResInfo.desc.image.mipLevels);
                        uint32_t srcSubRes = D3D11CalcSubresource(srcAccessInfo.range.baseMipLevel,
                                                                  iArray + srcAccessInfo.range.baseArrayLayer,
                                                                  srcResInfo.desc.image.mipLevels);

                        pD3DDeviceContext->ResolveSubresource(pD3DResDst, dstSubRes, pD3DResSrc, srcSubRes, format);
                    }

                    dstIndex++;
                }

                srcIndex++;
            }
        }

        return RPS_OK;
    }

    RpsResult D3D11RuntimeBackend::RecordCmdFixedFunctionBindingsAndDynamicStates(
        const RuntimeCmdCallbackContext& context) const
    {
        RPS_RETURN_OK_IF(rpsAnyBitsSet(context.pCmd->callback.flags, RPS_CMD_CALLBACK_CUSTOM_STATE_SETUP_BIT));

        const auto& nodeDeclInfo = *context.pCmdInfo->pNodeDecl;

        auto fixedFuncBindings = nodeDeclInfo.fixedFunctionBindings.Get(nodeDeclInfo.semanticKinds);
        auto dynamicStates     = nodeDeclInfo.dynamicStates.Get(nodeDeclInfo.semanticKinds);

        for (auto& binding : fixedFuncBindings)
        {
            auto paramIndices = binding.params.Get(nodeDeclInfo.semanticParamTable);

            switch (binding.semantic)
            {
            case RPS_SEMANTIC_VERTEX_BUFFER:
                break;
            case RPS_SEMANTIC_INDEX_BUFFER:
                break;
            case RPS_SEMANTIC_INDIRECT_ARGS:
                break;
            case RPS_SEMANTIC_INDIRECT_COUNT:
                break;
            case RPS_SEMANTIC_STREAM_OUT_BUFFER:
                break;
            case RPS_SEMANTIC_SHADING_RATE_IMAGE:
                break;
            case RPS_SEMANTIC_RENDER_TARGET:
            case RPS_SEMANTIC_DEPTH_STENCIL_TARGET:
            case RPS_SEMANTIC_RESOLVE_TARGET:
            default:
                break;
            }
        }

        for (auto& dynamicState : dynamicStates)
        {
            switch (dynamicState.semantic)
            {
            case RPS_SEMANTIC_PRIMITIVE_TOPOLOGY:
                break;
            case RPS_SEMANTIC_PATCH_CONTROL_POINTS:
                break;
            case RPS_SEMANTIC_PRIMITIVE_STRIP_CUT_INDEX:
                break;
            case RPS_SEMANTIC_BLEND_FACTOR:
                break;
            case RPS_SEMANTIC_STENCIL_REF:
                break;
            case RPS_SEMANTIC_DEPTH_BOUNDS:
                break;
            case RPS_SEMANTIC_SAMPLE_LOCATION:
                break;
            case RPS_SEMANTIC_SHADING_RATE:
                break;
            case RPS_SEMANTIC_COLOR_CLEAR_VALUE:
            case RPS_SEMANTIC_DEPTH_CLEAR_VALUE:
            case RPS_SEMANTIC_STENCIL_CLEAR_VALUE:
            case RPS_SEMANTIC_VIEWPORT:
            case RPS_SEMANTIC_SCISSOR:
            default:
                break;
            }
        }

        return RPS_OK;
    }

    RpsResult D3D11RuntimeBackend::GetCmdArgResources(const RuntimeCmdCallbackContext& context,
                                                      uint32_t                         argIndex,
                                                      uint32_t                         srcArrayIndex,
                                                      ID3D11Resource**                 ppResources,
                                                      uint32_t                         count) const
    {
        RPS_RETURN_ERROR_IF(argIndex >= context.pNodeDeclInfo->params.size(), RPS_ERROR_INDEX_OUT_OF_BOUNDS);

        auto& paramAccessInfo = context.pNodeDeclInfo->params[argIndex];
        RPS_RETURN_ERROR_IF(srcArrayIndex + count > paramAccessInfo.numElements, RPS_ERROR_INDEX_OUT_OF_BOUNDS);

        auto cmdAccessInfos = context.pCmdInfo->accesses.Get(context.pRenderGraph->GetCmdAccessInfos());

        RPS_ASSERT((paramAccessInfo.accessOffset + paramAccessInfo.numElements) <= cmdAccessInfos.size());

        for (uint32_t i = 0; i < count; i++)
        {
            auto& accessInfo = cmdAccessInfos[paramAccessInfo.accessOffset + srcArrayIndex + i];
            ppResources[i] =
                (accessInfo.resourceId != RPS_RESOURCE_ID_INVALID)
                    ? rpsD3D11ResourceFromHandle(
                          context.pRenderGraph->GetResourceInstance(accessInfo.resourceId).hRuntimeResource)
                    : nullptr;
        }

        return RPS_OK;
    }

    RpsResult D3D11RuntimeBackend::GetCmdArgViews(const RuntimeCmdCallbackContext& context,
                                                  uint32_t                         argIndex,
                                                  uint32_t                         srcArrayIndex,
                                                  ID3D11View**                     ppViews,
                                                  uint32_t                         count) const
    {
        RPS_RETURN_ERROR_IF(argIndex >= context.pNodeDeclInfo->params.size(), RPS_ERROR_INDEX_OUT_OF_BOUNDS);

        auto& paramAccessInfo = context.pNodeDeclInfo->params[argIndex];
        RPS_RETURN_ERROR_IF(srcArrayIndex + count > paramAccessInfo.numElements, RPS_ERROR_INDEX_OUT_OF_BOUNDS);
        RPS_RETURN_ERROR_IF(paramAccessInfo.access.accessFlags & RPS_ACCESS_NO_VIEW_BIT, RPS_ERROR_INVALID_OPERATION);

        auto cmdViewRange = m_views.range(context.pCmdInfo->accesses.GetBegin(), context.pCmdInfo->accesses.size());

        RPS_ASSERT((paramAccessInfo.accessOffset + paramAccessInfo.numElements) <= cmdViewRange.size());

        // Assuming all elements in the same parameter have the same access
        for (uint32_t i = 0; i < count; i++)
        {
            ppViews[i] = cmdViewRange[paramAccessInfo.accessOffset + srcArrayIndex + i];
        }

        return RPS_OK;
    }

    RpsResult D3D11RuntimeBackend::GetCmdArgResources(const RpsCmdCallbackContext* pContext,
                                                      uint32_t                     argIndex,
                                                      uint32_t                     srcArrayIndex,
                                                      ID3D11Resource**             ppResources,
                                                      uint32_t                     count)
    {
        RPS_CHECK_ARGS(pContext && ppResources);
        auto pBackendContext = rps::RuntimeCmdCallbackContext::Get(pContext);

        return pBackendContext->GetBackend<D3D11RuntimeBackend>()->GetCmdArgResources(
            *pBackendContext, argIndex, srcArrayIndex, ppResources, count);
    }

    RpsResult D3D11RuntimeBackend::GetCmdArgViews(const RpsCmdCallbackContext* pContext,
                                                  uint32_t                     argIndex,
                                                  uint32_t                     srcArrayIndex,
                                                  ID3D11View**                 ppViews,
                                                  uint32_t                     count)
    {
        RPS_CHECK_ARGS(pContext && ppViews);

        auto pBackendContext = rps::RuntimeCmdCallbackContext::Get(pContext);

        return pBackendContext->GetBackend<D3D11RuntimeBackend>()->GetCmdArgViews(
            *pBackendContext, argIndex, srcArrayIndex, ppViews, count);
    }

    const D3D11RuntimeBackend* D3D11RuntimeBackend::Get(const RpsCmdCallbackContext* pContext)
    {
        auto pBackendContext = rps::RuntimeCmdCallbackContext::Get(pContext);
        return pBackendContext->GetBackend<D3D11RuntimeBackend>();
    }

}  // namespace rps

RpsResult rpsD3D11GetCmdArgResourceArray(const RpsCmdCallbackContext* pContext,
                                         uint32_t                     argIndex,
                                         uint32_t                     srcArrayOffset,
                                         ID3D11Resource**             pResources,
                                         uint32_t                     count)
{
    return rps::D3D11RuntimeBackend::GetCmdArgResources(pContext, argIndex, srcArrayOffset, pResources, count);
}

RpsResult rpsD3D11GetCmdArgResource(const RpsCmdCallbackContext* pContext,
                                    uint32_t                     argIndex,
                                    ID3D11Resource**             pResources)
{
    return rpsD3D11GetCmdArgResourceArray(pContext, argIndex, 0, pResources, 1);
}

RpsResult rpsD3D11GetCmdArgViewArray(const RpsCmdCallbackContext* pContext,
                                     uint32_t                     argIndex,
                                     uint32_t                     srcArrayOffset,
                                     ID3D11View**                 ppViews,
                                     uint32_t                     count)
{
    return rps::D3D11RuntimeBackend::GetCmdArgViews(pContext, argIndex, srcArrayOffset, ppViews, count);
}

RpsResult rpsD3D11GetCmdArgView(const RpsCmdCallbackContext* pContext, uint32_t argIndex, ID3D11View** ppView)
{
    return rps::D3D11RuntimeBackend::GetCmdArgViews(pContext, argIndex, 0, ppView, 1);
}

RpsResult rpsD3D11GetCmdArgRTVArray(const RpsCmdCallbackContext* pContext,
                                    uint32_t                     argIndex,
                                    uint32_t                     srcArrayOffset,
                                    ID3D11RenderTargetView**     pRTVs,
                                    uint32_t                     count)
{
    return rps::D3D11RuntimeBackend::GetCmdArgViews(pContext, argIndex, srcArrayOffset, pRTVs, count);
}

RpsResult rpsD3D11GetCmdArgRTV(const RpsCmdCallbackContext* pContext, uint32_t argIndex, ID3D11RenderTargetView** pRTV)
{
    return rps::D3D11RuntimeBackend::GetCmdArgViews(pContext, argIndex, 0, pRTV, 1);
}

RpsResult rpsD3D11GetCmdArgDSVArray(const RpsCmdCallbackContext* pContext,
                                    uint32_t                     argIndex,
                                    uint32_t                     srcArrayOffset,
                                    ID3D11DepthStencilView**     pDSVs,
                                    uint32_t                     count)
{
    return rps::D3D11RuntimeBackend::GetCmdArgViews(pContext, argIndex, srcArrayOffset, pDSVs, count);
}

RpsResult rpsD3D11GetCmdArgDSV(const RpsCmdCallbackContext* pContext, uint32_t argIndex, ID3D11DepthStencilView** pDSV)
{
    return rps::D3D11RuntimeBackend::GetCmdArgViews(pContext, argIndex, 0, pDSV, 1);
}

RpsResult rpsD3D11GetCmdArgSRVArray(const RpsCmdCallbackContext* pContext,
                                    uint32_t                     argIndex,
                                    uint32_t                     srcArrayOffset,
                                    ID3D11ShaderResourceView**   pSRVs,
                                    uint32_t                     count)
{
    return rps::D3D11RuntimeBackend::GetCmdArgViews(pContext, argIndex, srcArrayOffset, pSRVs, count);
}

RpsResult rpsD3D11GetCmdArgSRV(const RpsCmdCallbackContext* pContext,
                               uint32_t                     argIndex,
                               ID3D11ShaderResourceView**   pSRV)
{
    return rps::D3D11RuntimeBackend::GetCmdArgViews(pContext, argIndex, 0, pSRV, 1);
}

RpsResult rpsD3D11GetCmdArgUAVArray(const RpsCmdCallbackContext* pContext,
                                    uint32_t                     argIndex,
                                    uint32_t                     srcArrayOffset,
                                    ID3D11UnorderedAccessView**  pUAVs,
                                    uint32_t                     count)
{
    return rps::D3D11RuntimeBackend::GetCmdArgViews(pContext, argIndex, srcArrayOffset, pUAVs, count);
}

RpsResult rpsD3D11GetCmdArgUAV(const RpsCmdCallbackContext* pContext,
                               uint32_t                     argIndex,
                               ID3D11UnorderedAccessView**  pUAV)
{
    return rps::D3D11RuntimeBackend::GetCmdArgViews(pContext, argIndex, 0, pUAV, 1);
}
