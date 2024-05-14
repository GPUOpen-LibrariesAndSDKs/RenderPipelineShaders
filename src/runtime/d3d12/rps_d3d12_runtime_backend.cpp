// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "rps/runtime/common/rps_render_states.h"
#include "rps/runtime/d3d12/rps_d3d12_runtime.h"

#include "runtime/common/rps_render_graph.hpp"
#include "runtime/common/rps_runtime_util.hpp"
#include "runtime/d3d12/rps_d3d12_runtime_backend.hpp"
#include "runtime/d3d12/rps_d3d12_runtime_device.hpp"
#include "runtime/d3d12/rps_d3d12_util.hpp"

namespace rps
{
    RpsFormat GetD3D12DSVFormat(RpsFormat viewFormat);

    RpsResult D3D12RuntimeBackend::CreateCommandResources(const RenderGraphUpdateContext& context)
    {
        auto& renderGraph = context.renderGraph;

        const auto& graph       = renderGraph.GetGraph();
        auto&       runtimeCmds = renderGraph.GetRuntimeCmdInfos();
        auto        cmdBatches  = renderGraph.GetCmdBatches().range_all();

        ArenaCheckPoint arenaCheckpoint{context.scratchArena};

        uint32_t             numGraphicsCmds = 0;
        Span<RuntimeCmdInfo> transitionRange = {};

        auto flushBarrierBatch = [&]() {
            if (!transitionRange.empty())
            {
                uint32_t batchId = m_pBarriers->CreateBarrierBatch(context, transitionRange);
                transitionRange  = {};

                if (batchId != RPS_INDEX_NONE_U32)
                {
                    auto pNewRuntimeCmd = m_runtimeCmds.grow(1);

                    pNewRuntimeCmd->cmdId          = RPS_CMD_ID_INVALID;
                    pNewRuntimeCmd->barrierBatchId = batchId;
                }
            }
        };

        for (uint32_t iBatch = 0; iBatch < cmdBatches.size(); iBatch++)
        {
            RpsCommandBatch& batchInfo = cmdBatches[iBatch];

            const uint32_t backendCmdBegin = uint32_t(m_runtimeCmds.size());

            for (uint32_t iCmd = batchInfo.cmdBegin, numCmds = batchInfo.cmdBegin + batchInfo.numCmds; iCmd < numCmds;
                 iCmd++)
            {
                const auto& runtimeCmd = runtimeCmds[iCmd];

                if (runtimeCmd.isTransition)
                {
                    if (transitionRange.GetEnd() != iCmd)
                    {
                        transitionRange.SetRange(iCmd, 0);
                    }
                    transitionRange.SetCount(transitionRange.size() + 1);
                }
                else
                {
                    flushBarrierBatch();

                    auto pNewRuntimeCmd = m_runtimeCmds.grow(1);

                    pNewRuntimeCmd->cmdId               = runtimeCmd.cmdId;
                    pNewRuntimeCmd->barrierBatchId      = RPS_INDEX_NONE_U32;
                    pNewRuntimeCmd->resourceBindingInfo = RPS_INDEX_NONE_U32;
                }
            }

            flushBarrierBatch();

            batchInfo.cmdBegin = backendCmdBegin;
            batchInfo.numCmds  = uint32_t(m_runtimeCmds.size()) - backendCmdBegin;

            // TODO: Avoid per-backend runtime command reorganize.
        }

        // Create Views

        ArenaVector<uint32_t> cbvSrvUavs(&context.scratchArena);
        ArenaVector<uint32_t> rtvs(&context.scratchArena);
        ArenaVector<uint32_t> dsvs(&context.scratchArena);

        cbvSrvUavs.reserve(context.renderGraph.GetCmdAccessInfos().size());
        rtvs.reserve(context.renderGraph.GetCmdAccessInfos().size());

        const auto& cmdAccesses = context.renderGraph.GetCmdAccessInfos();

        for (auto& runtimeCmd : m_runtimeCmds)
        {
            if (runtimeCmd.cmdId == RPS_CMD_ID_INVALID)
                continue;

            auto* pCmdInfo     = context.renderGraph.GetCmdInfo(runtimeCmd.cmdId);
            auto& cmd          = *pCmdInfo->pCmdDecl;
            auto& nodeDeclInfo = *pCmdInfo->pNodeDecl;

            const uint32_t accessOffset = pCmdInfo->accesses.GetBegin();

            for (uint32_t accessIdx = 0, accessCount = pCmdInfo->accesses.size(); accessIdx < accessCount; accessIdx++)
            {
                const uint32_t globalAccessIdx = accessOffset + accessIdx;
                auto&          access          = cmdAccesses[globalAccessIdx];

                if (!rpsAnyBitsSet(access.access.accessFlags, RPS_ACCESS_NO_VIEW_BIT))
                {
                    if (rpsAnyBitsSet(access.access.accessFlags,
                                      RPS_ACCESS_CONSTANT_BUFFER_BIT | RPS_ACCESS_UNORDERED_ACCESS_BIT |
                                          RPS_ACCESS_SHADER_RESOURCE_BIT))
                    {
                        cbvSrvUavs.push_back(globalAccessIdx);
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

        m_accessToDescriptorMap.resize(cmdAccesses.size(), RPS_INDEX_NONE_U32);

        RPS_V_RETURN(CreateResourceViews(context, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, cbvSrvUavs.range_all()));
        RPS_V_RETURN(CreateResourceViews(context, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, rtvs.range_all()));
        RPS_V_RETURN(CreateResourceViews(context, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, dsvs.range_all()));

        return RPS_OK;
    }

    RpsResult D3D12RuntimeBackend::UpdateFrame(const RenderGraphUpdateContext& context)
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

        // TODO:
        m_runtimeCmds.reset(&context.frameArena);
        m_accessToDescriptorMap.reset(&context.frameArena);

        m_pBarriers->UpdateFrame(context);

        return RPS_OK;
    }

    RpsResult D3D12RuntimeBackend::CreateHeaps(const RenderGraphUpdateContext& context, ArrayRef<HeapInfo> heaps)
    {
        auto       pD3DDevice = m_device.GetD3DDevice();
        const bool bEnableDebugNames =
            !!(context.pUpdateInfo->diagnosticFlags & RPS_DIAGNOSTIC_ENABLE_RUNTIME_DEBUG_NAMES);

        for (auto& heapInfo : heaps)
        {
            // TODO:
            heapInfo.size = (heapInfo.size == UINT64_MAX) ? heapInfo.maxUsedSize : heapInfo.size;

            if (heapInfo.hRuntimeHeap || !heapInfo.size)
                continue;

            auto& heapTypeInfo = m_device.GetD3D12HeapTypeInfo(heapInfo.memTypeIndex);

            D3D12_HEAP_DESC heapDesc = {};
            heapDesc.Alignment       = heapInfo.alignment;
            heapDesc.SizeInBytes     = heapInfo.size;
            heapDesc.Flags           = heapTypeInfo.heapFlags;
            heapDesc.Properties.Type = heapTypeInfo.type;

            ID3D12Heap* pD3DHeap = nullptr;
            RPS_V_RETURN(HRESULTToRps(pD3DDevice->CreateHeap(&heapDesc, IID_PPV_ARGS(&pD3DHeap))));

            if (bEnableDebugNames)
                SetHeapDebugName(pD3DHeap, heapDesc, heapInfo.index);

            heapInfo.hRuntimeHeap = rpsD3D12HeapToHandle(pD3DHeap);
        }

        return RPS_OK;
    }

    void D3D12RuntimeBackend::DestroyHeaps(ArrayRef<HeapInfo> heaps)
    {
        for (auto& heapInfo : heaps)
        {
            if (heapInfo.hRuntimeHeap)
            {
                ID3D12Heap* pD3DHeap  = rpsD3D12HeapFromHandle(heapInfo.hRuntimeHeap);
                heapInfo.hRuntimeHeap = {};

                pD3DHeap->Release();
            }
        }
    }

    RpsResult D3D12RuntimeBackend::CreateResources(const RenderGraphUpdateContext& context,
                                                   ArrayRef<ResourceInstance>      resInstances)
    {
        auto& heaps         = GetRenderGraph().GetHeapInfos();
        auto  resourceDecls = GetRenderGraph().GetBuilder().GetResourceDecls();

        const bool bNeedsPlacedResourceInitState = !m_device.GetEnhancedBarrierEnabled();
        const bool bEnableDebugNames =
            !!(context.pUpdateInfo->diagnosticFlags & RPS_DIAGNOSTIC_ENABLE_RUNTIME_DEBUG_NAMES);

#if RPS_D3D12_ENHANCED_BARRIER_SUPPORT
        ScopedComPtr<ID3D12Device10> pD3DDevice10;
        m_device.GetD3DDevice()->QueryInterface(pD3DDevice10.ReleaseAndGetAddressOf());
#endif  //RPS_D3D12_ENHANCED_BARRIER_SUPPORT

        uint32_t temporalSlice = RPS_INDEX_NONE_U32;

        // Create resources
        for (uint32_t iRes = 0, numRes = uint32_t(resInstances.size()); iRes < numRes; iRes++)
        {
            auto& resInfo = resInstances[iRes];

            if (resInfo.isExternal)
            {
                continue;
            }

            if (bNeedsPlacedResourceInitState && resInfo.isAliased &&
                (resInfo.initialAccess.accessFlags != RPS_ACCESS_UNKNOWN))
            {
                // Force an initial state compatible with PlacedResource initialization
                m_pBarriers->EnsurePlacedResourceInitState(resInfo);
            }

            temporalSlice =
                resInfo.isFirstTemporalSlice ? 0 : (resInfo.isTemporalSlice ? (temporalSlice + 1) : RPS_INDEX_NONE_U32);

            if (resInfo.isPendingCreate && !resInfo.HasEmptyLifetime() && (resInfo.allocRequirement.size > 0))
            {
                RPS_ASSERT(!resInfo.hRuntimeResource);

                if (resInfo.allocPlacement.heapId != RPS_INDEX_NONE_U32)
                {
                    auto pD3DHeap           = rpsD3D12HeapFromHandle(heaps[resInfo.allocPlacement.heapId].hRuntimeHeap);
                    ID3D12Resource* pD3DRes = nullptr;

                    D3D12_CLEAR_VALUE clearValue = {};

                    bool bHasClearValue = ((resInfo.allAccesses.accessFlags &
                                            (RPS_ACCESS_RENDER_TARGET_BIT | RPS_ACCESS_DEPTH_STENCIL)) &&
                                           (resInfo.clearValueId != RPS_INDEX_NONE_U32) && !resInfo.desc.IsBuffer());

                    if (bHasClearValue)
                    {
                        auto& rpsClearValue = GetRenderGraph().GetResourceClearValue(resInfo.clearValueId);

                        if (resInfo.allAccesses.accessFlags & RPS_ACCESS_RENDER_TARGET_BIT)
                        {
                            clearValue.Format = rpsFormatToDXGI(rpsClearValue.format);
                            memcpy(clearValue.Color, rpsClearValue.value.color.float32, sizeof(float) * 4);
                        }
                        else
                        {
                            clearValue.Format               = rpsFormatToDXGI(GetD3D12DSVFormat(rpsClearValue.format));
                            clearValue.DepthStencil.Depth   = rpsClearValue.value.depthStencil.depth;
                            clearValue.DepthStencil.Stencil = UINT8(rpsClearValue.value.depthStencil.stencil);
                        }
                    }

#if RPS_D3D12_ENHANCED_BARRIER_SUPPORT
                    if (m_device.GetEnhancedBarrierEnabled())
                    {

                        D3D12_RESOURCE_DESC1 d3dResDesc = {};
                        CalcD3D12ResourceDesc(&d3dResDesc, resInfo);

                        // TODO: Castable formats

                        RPS_V_RETURN(
                            HRESULTToRps(pD3DDevice10->CreatePlacedResource2(pD3DHeap,
                                                                             resInfo.allocPlacement.offset,
                                                                             &d3dResDesc,
                                                                             D3D12_BARRIER_LAYOUT_UNDEFINED,
                                                                             bHasClearValue ? &clearValue : nullptr,
                                                                             0,
                                                                             nullptr,
                                                                             IID_PPV_ARGS(&pD3DRes))));
                    }
                    else
#endif  //RPS_D3D12_ENHANCED_BARRIER_SUPPORT
                    {
                        D3D12_RESOURCE_DESC d3dResDesc = {};
                        CalcD3D12ResourceDesc(&d3dResDesc, resInfo);

                        const auto d3dInitState =
                            D3D12ConventionalBarrierBuilder::CalcResourceInitState(m_device, resInfo);

                        RPS_V_RETURN(HRESULTToRps(
                            m_device.GetD3DDevice()->CreatePlacedResource(pD3DHeap,
                                                                          resInfo.allocPlacement.offset,
                                                                          &d3dResDesc,
                                                                          d3dInitState,
                                                                          bHasClearValue ? &clearValue : nullptr,
                                                                          IID_PPV_ARGS(&pD3DRes))));
                    }

                    resInfo.hRuntimeResource = rpsD3D12ResourceToHandle(pD3DRes);
                    resInfo.isPendingInit    = true;

                    if (bEnableDebugNames)
                    {
                        SetResourceDebugName(pD3DRes, resourceDecls[resInfo.resourceDeclId].name, temporalSlice);
                    }
                }
                else
                {
                    RPS_TODO("Unreachable code path. This is reserved for e.g. CommittedResource.");
                }

                static constexpr auto prevAccessNone = AccessAttr{};

                resInfo.FinalizeRuntimeResourceCreation(bNeedsPlacedResourceInitState ? nullptr : &prevAccessNone);
            }
            else
            {
                RPS_ASSERT(!resInfo.isExternal);
                resInfo.isPendingInit = resInfo.isAliased;
            }
        }

        return RPS_OK;
    }

    void D3D12RuntimeBackend::DestroyResources(ArrayRef<ResourceInstance> resources)
    {
        for (auto& resInfo : resources)
        {
            if (!resInfo.isExternal && resInfo.hRuntimeResource)
            {
                rpsD3D12ResourceFromHandle(resInfo.hRuntimeResource)->Release();
            }
        }
    }

    void D3D12RuntimeBackend::OnDestroy()
    {
        for (auto& frameResource : m_frameResources)
        {
            frameResource.DestroyDeviceResources();
        }

        m_frameResources.clear();

        RuntimeBackend::OnDestroy();
    }

    RpsResult D3D12RuntimeBackend::RecordCommands(const RenderGraph&                     renderGraph,
                                                  const RpsRenderGraphRecordCommandInfo& recordInfo) const
    {
        RuntimeCmdCallbackContext cmdCbCtx{this, recordInfo};

        for (auto cmdIter = m_runtimeCmds.cbegin() + recordInfo.cmdBeginIndex, cmdEnd = cmdIter + recordInfo.numCmds;
             cmdIter != cmdEnd;
             ++cmdIter)
        {
            auto& runtimeCmd = *cmdIter;

            if (runtimeCmd.barrierBatchId != RPS_INDEX_NONE_U32)
            {
                m_pBarriers->RecordBarrierBatch(GetContextD3DCmdList(cmdCbCtx), runtimeCmd.barrierBatchId);
            }

            RecordCommand(cmdCbCtx, runtimeCmd);
        }

        return RPS_OK;
    }

    void D3D12RuntimeBackend::DestroyRuntimeResourceDeferred(ResourceInstance& resource)
    {
        if (resource.hRuntimeResource)
        {
            m_pendingReleaseResources.push_back(D3D12RuntimeDevice::FromHandle(resource.hRuntimeResource));
            resource.hRuntimeResource = {};
        }
    }

    RpsResult D3D12RuntimeBackend::RecordCmdRenderPassBegin(const RuntimeCmdCallbackContext& context) const
    {
        auto& cmd          = *context.pCmd;
        auto* pCmdInfo     = context.pCmdInfo;
        auto& nodeDeclInfo = *pCmdInfo->pNodeDecl;
        auto  pD3DCmdList  = GetContextD3DCmdList(context);

        RPS_RETURN_ERROR_IF(!nodeDeclInfo.MaybeGraphicsNode(), RPS_ERROR_INVALID_OPERATION);

        const auto cmdCbFlags = context.bIsCmdBeginEnd ? cmd.callback.flags : RPS_CMD_CALLBACK_FLAG_NONE;

        const bool bBindRenderTargets   = !rpsAnyBitsSet(cmdCbFlags, RPS_CMD_CALLBACK_CUSTOM_RENDER_TARGETS_BIT);
        const bool bSetViewportScissors = !rpsAnyBitsSet(cmdCbFlags, RPS_CMD_CALLBACK_CUSTOM_VIEWPORT_SCISSOR_BIT);

        // Need to skip clears if it's render pass resume
        const bool bIsRenderPassResuming = rpsAnyBitsSet(context.renderPassFlags, RPS_RUNTIME_RENDER_PASS_RESUMING);

        auto descriptorIndices =
            m_accessToDescriptorMap.range(pCmdInfo->accesses.GetBegin(), pCmdInfo->accesses.size());

        uint32_t numRtvs = 0;

        D3D12_CPU_DESCRIPTOR_HANDLE rtvs[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = {0};

        if ((nodeDeclInfo.pRenderPassInfo) && !(nodeDeclInfo.pRenderPassInfo->clearOnly) &&
            (bBindRenderTargets || !bIsRenderPassResuming))
        {
            auto& rpInfo = *nodeDeclInfo.pRenderPassInfo;

            auto     clearColorValueRefs  = rpInfo.GetRenderTargetClearValueRefs();
            uint32_t clearColorValueIndex = 0;

            for (auto& rtParamRef : rpInfo.GetRenderTargetRefs())
            {
                auto& paramAccessInfo = nodeDeclInfo.params[rtParamRef.paramId];
                auto  descriptorIndicesRange =
                    descriptorIndices.range(paramAccessInfo.accessOffset, paramAccessInfo.numElements);

                const uint32_t rtvSlot = paramAccessInfo.baseSemanticIndex + rtParamRef.arrayOffset;

                numRtvs = rpsMax(numRtvs, rtvSlot + 1);

                rtvs[rtvSlot] = m_cpuDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Get(
                    descriptorIndicesRange[rtParamRef.arrayOffset]);

                if ((!bIsRenderPassResuming) && (rpInfo.renderTargetClearMask & (1u << rtvSlot)))
                {
                    auto clearValueRef = clearColorValueRefs[clearColorValueIndex];

                    auto pClearColor = static_cast<const RpsClearColorValue*>(
                                           cmd.args[clearValueRef.paramId])[clearValueRef.arrayOffset]
                                           .float32;

                    // TODO: Sub-rect clear implemented separatedly
                    pD3DCmdList->ClearRenderTargetView(rtvs[rtvSlot], pClearColor, 0, nullptr);

                    clearColorValueIndex++;
                }
            }

            if (rpInfo.depthStencilTargetMask)
            {
                auto& paramAccessInfo = nodeDeclInfo.params[rpInfo.GetDepthStencilRef()->paramId];
                RPS_ASSERT(paramAccessInfo.numElements == 1);

                dsv = m_cpuDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].Get(
                    descriptorIndices[paramAccessInfo.accessOffset]);

                if ((!bIsRenderPassResuming) && (rpInfo.clearDepth || rpInfo.clearStencil))
                {
                    float             depthClearValue   = 0.0f;
                    uint32_t          stencilClearValue = 0;
                    D3D12_CLEAR_FLAGS clearFlag         = {};

                    if (rpInfo.clearDepth)
                    {
                        auto pClearValueRef = rpInfo.GetDepthClearValueRef();
                        depthClearValue     = static_cast<const float*>(cmd.args[pClearValueRef->paramId])[0];
                        clearFlag |= D3D12_CLEAR_FLAG_DEPTH;
                    }

                    if (rpInfo.clearStencil)
                    {
                        auto pClearValueRef = rpInfo.GetStencilClearValueRef();
                        stencilClearValue   = static_cast<const uint32_t*>(cmd.args[pClearValueRef->paramId])[0];
                        clearFlag |= D3D12_CLEAR_FLAG_STENCIL;
                    }

                    pD3DCmdList->ClearDepthStencilView(
                        dsv, clearFlag, depthClearValue, UINT8(stencilClearValue), 0, nullptr);
                }
            }

            if (bBindRenderTargets && ((numRtvs > 0) || (dsv.ptr != 0)))
            {
                pD3DCmdList->OMSetRenderTargets(numRtvs, rtvs, FALSE, (dsv.ptr != 0) ? &dsv : nullptr);
            }

            if (bSetViewportScissors)
            {
                auto& cmdRPInfo = *pCmdInfo->pRenderPassInfo;

                RPS_STATIC_ASSERT(sizeof(D3D12_VIEWPORT) == sizeof(RpsViewport),
                                  "RpsViewport / D3D12_VIEWPORT size mismatch");

                pD3DCmdList->RSSetViewports(cmdRPInfo.viewportInfo.numViewports,
                                            reinterpret_cast<const D3D12_VIEWPORT*>(cmdRPInfo.viewportInfo.pViewports));

                D3D12_RECT d3dScissorRects[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};

                RPS_RETURN_ERROR_IF(cmdRPInfo.viewportInfo.numScissorRects > RPS_COUNTOF(d3dScissorRects),
                                    RPS_ERROR_INDEX_OUT_OF_BOUNDS);

                for (uint32_t iScissor = 0; iScissor < cmdRPInfo.viewportInfo.numScissorRects; iScissor++)
                {
                    auto& rect                = cmdRPInfo.viewportInfo.pScissorRects[iScissor];
                    d3dScissorRects[iScissor] = D3D12_RECT{
                        rect.x,
                        rect.y,
                        rect.x + rect.width,
                        rect.y + rect.height,
                    };
                }

                pD3DCmdList->RSSetScissorRects(cmdRPInfo.viewportInfo.numScissorRects, d3dScissorRects);
            }
        }

        return RPS_OK;
    }

    RpsResult D3D12RuntimeBackend::RecordCmdRenderPassEnd(const RuntimeCmdCallbackContext& context) const
    {
        // TODO: Add a pass to split graphics cmd to separated clear / resolve cmds

        auto& renderGraph  = *context.pRenderGraph;
        auto* pCmdInfo     = context.pCmdInfo;
        auto  cmdAccesses  = pCmdInfo->accesses.Get(renderGraph.GetCmdAccessInfos());
        auto  resInstances = renderGraph.GetResourceInstances().range_all();
        auto  pD3DCmdList  = GetContextD3DCmdList(context);

        // Skip resolve if it's render pass suspending
        const bool bIsRenderPassSuspending = rpsAnyBitsSet(context.renderPassFlags, RPS_RUNTIME_RENDER_PASS_SUSPENDING);

        const auto& nodeDeclInfo = *pCmdInfo->pNodeDecl;

        if (!bIsRenderPassSuspending && (nodeDeclInfo.pRenderPassInfo) &&
            (nodeDeclInfo.pRenderPassInfo->resolveTargetsMask != 0))
        {
            auto rpInfo      = *nodeDeclInfo.pRenderPassInfo;
            auto resolveDsts = rpInfo.GetResolveTargetRefs();
            auto resolveSrcs = rpInfo.GetRenderTargetRefs();

            uint32_t srcMask  = rpInfo.renderTargetsMask;
            uint32_t dstMask  = rpInfo.resolveTargetsMask;
            uint32_t srcIndex = 0;
            uint32_t dstIndex = 0;

            D3D12ResolveInfo resolveInfos[D3D12ResolveInfo::RESOLVE_BATCH_SIZE];
            uint32_t         numResolvesBatched = 0;

            auto flushResolveBatch = [&]() {
                if (numResolvesBatched > 0)
                {
                    m_pBarriers->RecordResolveBatch(pD3DCmdList, {resolveInfos, numResolvesBatched});

                    numResolvesBatched = 0;
                }
            };

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
                    auto  pD3DResDst         = rpsD3D12ResourceFromHandle(dstResInfo.hRuntimeResource);

                    auto& srcParamAccessInfo = nodeDeclInfo.params[resolveSrcs[srcIndex].paramId];
                    auto& srcAccessInfo      = cmdAccesses[srcParamAccessInfo.accessOffset];
                    auto& srcResInfo         = resInstances[srcAccessInfo.resourceId];
                    auto  pD3DResSrc         = rpsD3D12ResourceFromHandle(srcResInfo.hRuntimeResource);

                    RPS_ASSERT(dstAccessInfo.range.GetNumSubresources() == srcAccessInfo.range.GetNumSubresources());
                    RPS_ASSERT(dstAccessInfo.range.aspectMask == 1);
                    RPS_ASSERT(dstAccessInfo.range.GetMipLevelCount() == 1);

                    auto format = rpsFormatToDXGI(dstAccessInfo.viewFormat);

                    for (uint32_t iArray = 0; iArray < dstAccessInfo.range.GetArrayLayerCount(); iArray++)
                    {
                        uint32_t dstSubRes = D3D12CalcSubresource(dstAccessInfo.range.baseMipLevel,
                                                                  iArray + dstAccessInfo.range.baseArrayLayer,
                                                                  0,
                                                                  dstResInfo.desc.image.mipLevels,
                                                                  dstResInfo.desc.image.arrayLayers);
                        uint32_t srcSubRes = D3D12CalcSubresource(srcAccessInfo.range.baseMipLevel,
                                                                  iArray + srcAccessInfo.range.baseArrayLayer,
                                                                  0,
                                                                  srcResInfo.desc.image.mipLevels,
                                                                  srcResInfo.desc.image.arrayLayers);

                        if (numResolvesBatched == D3D12ResolveInfo::RESOLVE_BATCH_SIZE)
                        {
                            flushResolveBatch();
                        }

                        auto& resolve          = resolveInfos[numResolvesBatched];
                        resolve.pSrc           = pD3DResSrc;
                        resolve.pDst           = pD3DResDst;
                        resolve.srcSubResource = srcSubRes;
                        resolve.dstSubResource = dstSubRes;
                        resolve.format         = format;

                        numResolvesBatched++;
                    }

                    dstIndex++;
                }

                srcIndex++;
            }

            flushResolveBatch();
        }

        return RPS_OK;
    }

    RpsResult D3D12RuntimeBackend::RecordCmdFixedFunctionBindingsAndDynamicStates(
        const RuntimeCmdCallbackContext& context) const
    {
        RPS_RETURN_OK_IF(rpsAnyBitsSet(context.pCmd->callback.flags, RPS_CMD_CALLBACK_CUSTOM_STATE_SETUP_BIT));

        auto pD3DCmdList = GetContextD3DCmdList(context);

        // TODO: Setup common states
        const auto  pCmdInfo     = context.pCmdInfo;
        const auto& nodeDeclInfo = *context.pNodeDeclInfo;

        auto descriptorIndices =
            m_accessToDescriptorMap.range(pCmdInfo->accesses.GetBegin(), pCmdInfo->accesses.size());

        auto fixedFuncBindings = nodeDeclInfo.fixedFunctionBindings.Get(nodeDeclInfo.semanticKinds);
        auto dynamicStates     = nodeDeclInfo.dynamicStates.Get(nodeDeclInfo.semanticKinds);

        const auto pCmdList = pD3DCmdList;

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
            auto paramIndices = dynamicState.params.Get(nodeDeclInfo.semanticParamTable);

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

    RpsResult D3D12RuntimeBackend::GetCmdArgResources(const RuntimeCmdCallbackContext& context,
                                                      uint32_t                         argIndex,
                                                      uint32_t                         srcArrayIndex,
                                                      ID3D12Resource**                 ppResources,
                                                      uint32_t                         count) const
    {
        RPS_RETURN_ERROR_IF(argIndex >= context.pNodeDeclInfo->params.size(), RPS_ERROR_INDEX_OUT_OF_BOUNDS);

        auto& paramAccessInfo = context.pNodeDeclInfo->params[argIndex];
        RPS_RETURN_ERROR_IF(srcArrayIndex + count > paramAccessInfo.numElements, RPS_ERROR_INDEX_OUT_OF_BOUNDS);
        RPS_RETURN_ERROR_IF(!paramAccessInfo.IsResource(), RPS_ERROR_TYPE_MISMATCH);

        auto cmdAccessInfos = context.pCmdInfo->accesses.Get(context.pRenderGraph->GetCmdAccessInfos());
        RPS_ASSERT((paramAccessInfo.accessOffset + paramAccessInfo.numElements) <= cmdAccessInfos.size());

        for (uint32_t i = 0; i < count; i++)
        {
            auto& accessInfo = cmdAccessInfos[paramAccessInfo.accessOffset + srcArrayIndex + i];
            ppResources[i] =
                (accessInfo.resourceId != RPS_RESOURCE_ID_INVALID)
                    ? rpsD3D12ResourceFromHandle(
                          context.pRenderGraph->GetResourceInstance(accessInfo.resourceId).hRuntimeResource)
                    : nullptr;
        }

        return RPS_OK;
    }

    static constexpr RpsAccessFlags AccessFlagsMaybeCbvSrvUav =
        RPS_ACCESS_CONSTANT_BUFFER_BIT | RPS_ACCESS_SHADER_RESOURCE_BIT | RPS_ACCESS_UNORDERED_ACCESS_BIT |
        RPS_ACCESS_RAYTRACING_AS_READ_BIT;

    RpsResult D3D12RuntimeBackend::GetCmdArgDescriptors(const RuntimeCmdCallbackContext& context,
                                                        uint32_t                         argIndex,
                                                        uint32_t                         srcArrayIndex,
                                                        D3D12_CPU_DESCRIPTOR_HANDLE*     pDescriptors,
                                                        uint32_t                         count) const
    {
        RPS_RETURN_ERROR_IF(argIndex >= context.pNodeDeclInfo->params.size(), RPS_ERROR_INDEX_OUT_OF_BOUNDS);

        auto& paramAccessInfo = context.pNodeDeclInfo->params[argIndex];
        RPS_RETURN_ERROR_IF(srcArrayIndex + count > paramAccessInfo.numElements, RPS_ERROR_INDEX_OUT_OF_BOUNDS);
        RPS_RETURN_ERROR_IF(paramAccessInfo.access.accessFlags & RPS_ACCESS_NO_VIEW_BIT, RPS_ERROR_INVALID_OPERATION);

        auto descriptorIndices =
            m_accessToDescriptorMap.range(context.pCmdInfo->accesses.GetBegin(), context.pCmdInfo->accesses.size());

        RPS_ASSERT((paramAccessInfo.accessOffset + paramAccessInfo.numElements) <= descriptorIndices.size());

        // TODO: Bake descriptor type in indices?
        const auto descriptorHeapType =
            rpsAnyBitsSet(paramAccessInfo.access.accessFlags, AccessFlagsMaybeCbvSrvUav)
                ? D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
                : (rpsAnyBitsSet(paramAccessInfo.access.accessFlags, RPS_ACCESS_RENDER_TARGET_BIT)
                       ? D3D12_DESCRIPTOR_HEAP_TYPE_RTV
                       : (rpsAnyBitsSet(paramAccessInfo.access.accessFlags, RPS_ACCESS_DEPTH_STENCIL)
                              ? D3D12_DESCRIPTOR_HEAP_TYPE_DSV
                              : D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES));

        RPS_RETURN_ERROR_IF(descriptorHeapType == D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES, RPS_ERROR_INVALID_OPERATION);

        // Assuming all elements in the same parameter have the same access

        for (uint32_t i = 0; i < count; i++)
        {
            const uint32_t descriptorIndex = descriptorIndices[paramAccessInfo.accessOffset + srcArrayIndex + i];

            pDescriptors[i] = (descriptorIndex != RPS_INDEX_NONE_U32)
                                  ? m_cpuDescriptorHeaps[descriptorHeapType].Get(descriptorIndex)
                                  : D3D12_CPU_DESCRIPTOR_HANDLE{};
        }

        return RPS_OK;
    }

    RpsResult D3D12RuntimeBackend::GetCmdArgResources(const RpsCmdCallbackContext* pContext,
                                                      uint32_t                     argIndex,
                                                      uint32_t                     srcArrayIndex,
                                                      ID3D12Resource**             ppResources,
                                                      uint32_t                     count)
    {
        RPS_CHECK_ARGS(pContext && ppResources);
        auto pBackendContext = rps::RuntimeCmdCallbackContext::Get(pContext);

        return pBackendContext->GetBackend<D3D12RuntimeBackend>()->GetCmdArgResources(
            *pBackendContext, argIndex, srcArrayIndex, ppResources, count);
    }

    RpsResult D3D12RuntimeBackend::GetCmdArgDescriptors(const RpsCmdCallbackContext* pContext,
                                                        uint32_t                     argIndex,
                                                        uint32_t                     srcArrayIndex,
                                                        D3D12_CPU_DESCRIPTOR_HANDLE* pDescriptors,
                                                        uint32_t                     count)
    {
        RPS_CHECK_ARGS(pContext && pDescriptors);

        auto pBackendContext = rps::RuntimeCmdCallbackContext::Get(pContext);

        return pBackendContext->GetBackend<D3D12RuntimeBackend>()->GetCmdArgDescriptors(
            *pBackendContext, argIndex, srcArrayIndex, pDescriptors, count);
    }

    RpsResult D3D12RuntimeBackend::CopyCmdArgDescriptors(const RpsCmdCallbackContext* pContext,
                                                         uint32_t                     argIndex,
                                                         uint32_t                     srcArrayIndex,
                                                         uint32_t                     count,
                                                         RpsBool                      singleHandleToArray,
                                                         D3D12_CPU_DESCRIPTOR_HANDLE* pDstHandles)
    {
        RPS_CHECK_ARGS(pContext && pDstHandles);

        auto  pBackendContext = rps::RuntimeCmdCallbackContext::Get(pContext);
        auto  pBackend        = pBackendContext->GetBackend<D3D12RuntimeBackend>();
        auto& device          = pBackend->m_device;

        auto& paramAccessInfo = pBackendContext->pNodeDeclInfo->params[argIndex];
        RPS_RETURN_ERROR_IF(srcArrayIndex + count > paramAccessInfo.numElements, RPS_ERROR_INDEX_OUT_OF_BOUNDS);
        RPS_RETURN_ERROR_IF(paramAccessInfo.access.accessFlags & RPS_ACCESS_NO_VIEW_BIT, RPS_ERROR_INVALID_OPERATION);

        // Only expect copying CBV_SRV_UAV here.
        RPS_RETURN_ERROR_IF(!rpsAnyBitsSet(paramAccessInfo.access.accessFlags, AccessFlagsMaybeCbvSrvUav),
                            RPS_ERROR_INVALID_OPERATION);

        constexpr auto heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

        const auto descriptorSize = device.GetDescriptorSize(heapType);

        auto descriptorIndices = pBackend->m_accessToDescriptorMap.range(pBackendContext->pCmdInfo->accesses.GetBegin(),
                                                                         pBackendContext->pCmdInfo->accesses.size());

        const uint32_t baseIndexOffset = paramAccessInfo.accessOffset + srcArrayIndex;

        RPS_ASSERT((baseIndexOffset + count) <= descriptorIndices.size());

        if (singleHandleToArray)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE dstHdl = *pDstHandles;
            D3D12_CPU_DESCRIPTOR_HANDLE srcHdl = {};

            uint32_t lastDescriptorIndex = UINT32_MAX - 1;
            uint32_t numToCopy           = 0;

            for (uint32_t i = 0; i < count; i++)
            {
                uint32_t descriptorIndex = descriptorIndices[baseIndexOffset + i];

                if ((lastDescriptorIndex + 1) != descriptorIndex)
                {
                    if (numToCopy)
                    {
                        device.GetD3DDevice()->CopyDescriptorsSimple(numToCopy, dstHdl, srcHdl, heapType);
                    }

                    srcHdl = pBackend->m_cpuDescriptorHeaps[heapType].Get(descriptorIndex);
                    dstHdl.ptr += descriptorSize * numToCopy;

                    numToCopy = 0;
                }

                lastDescriptorIndex = descriptorIndex;
                numToCopy++;
            }

            if (numToCopy)
            {
                device.GetD3DDevice()->CopyDescriptorsSimple(numToCopy, dstHdl, srcHdl, heapType);
            }
        }
        else
        {
            D3D12_CPU_DESCRIPTOR_HANDLE dstHdl = *pDstHandles;

            for (uint32_t i = 0; i < count; i++)
            {
                uint32_t descriptorIndex = descriptorIndices[baseIndexOffset + i];

                D3D12_CPU_DESCRIPTOR_HANDLE srcHdl = pBackend->m_cpuDescriptorHeaps[heapType].Get(descriptorIndex);

                device.GetD3DDevice()->CopyDescriptorsSimple(1, dstHdl, srcHdl, heapType);

                dstHdl.ptr += descriptorSize;
            }
        }
        return RPS_OK;
    }

    const D3D12RuntimeBackend* D3D12RuntimeBackend::Get(const RpsCmdCallbackContext* pContext)
    {
        auto pBackendContext = rps::RuntimeCmdCallbackContext::Get(pContext);
        return pBackendContext->GetBackend<D3D12RuntimeBackend>();
    }

}  // namespace rps

RpsResult rpsD3D12GetCmdArgResourceArray(const RpsCmdCallbackContext* pContext,
                                         uint32_t                     argIndex,
                                         uint32_t                     srcArrayOffset,
                                         ID3D12Resource**             pResources,
                                         uint32_t                     count)
{
    return rps::D3D12RuntimeBackend::GetCmdArgResources(pContext, argIndex, srcArrayOffset, pResources, count);
}

RpsResult rpsD3D12GetCmdArgResource(const RpsCmdCallbackContext* pContext,
                                    uint32_t                     argIndex,
                                    ID3D12Resource**             pResources)
{
    return rpsD3D12GetCmdArgResourceArray(pContext, argIndex, 0, pResources, 1);
}

RpsResult rpsD3D12GetCmdArgDescriptorArray(const RpsCmdCallbackContext* pContext,
                                           uint32_t                     argIndex,
                                           uint32_t                     srcArrayOffset,
                                           D3D12_CPU_DESCRIPTOR_HANDLE* pHandles,
                                           uint32_t                     count)
{
    return rps::D3D12RuntimeBackend::GetCmdArgDescriptors(pContext, argIndex, srcArrayOffset, pHandles, count);
}

RpsResult rpsD3D12GetCmdArgDescriptor(const RpsCmdCallbackContext* pContext,
                                      uint32_t                     argIndex,
                                      D3D12_CPU_DESCRIPTOR_HANDLE* pHandles)
{
    return rps::D3D12RuntimeBackend::GetCmdArgDescriptors(pContext, argIndex, 0, pHandles, 1);
}

RpsResult rpsD3D12CopyCmdArgDescriptors(const RpsCmdCallbackContext* pContext,
                                        uint32_t                     argIndex,
                                        uint32_t                     srcArrayOffset,
                                        uint32_t                     count,
                                        RpsBool                      singleHandleToArray,
                                        D3D12_CPU_DESCRIPTOR_HANDLE* pDstHandles)
{
    return rps::D3D12RuntimeBackend::CopyCmdArgDescriptors(
        pContext, argIndex, srcArrayOffset, count, singleHandleToArray, pDstHandles);
}

RpsResult rpsD3D12ResourceDescToRps(const D3D12_RESOURCE_DESC* pD3D12Desc, RpsResourceDesc* pRpsDesc)
{
    RPS_CHECK_ARGS(pD3D12Desc && pRpsDesc);
    rps::D3D12ResourceDescToRps(pRpsDesc, pD3D12Desc);
    return RPS_OK;
}
