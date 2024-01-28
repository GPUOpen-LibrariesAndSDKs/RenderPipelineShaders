// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "runtime/common/rps_rpsl_host.hpp"
#include "runtime/common/rps_subprogram.hpp"
#include "runtime/common/rps_render_graph.hpp"

extern "C" RpsResult RpslHostCallEntry(PFN_RpslEntry func, uint32_t numArgs, const void* const* ppArgs);

namespace rps
{
    static thread_local RpslHost* s_pRpslContext = nullptr;

    RpsResult RpslHost::Execute(const RpslExecuteInfo& execInfo)
    {
        RPS_CHECK_ARGS(execInfo.pProgram);

        ScopedContext<RpslHost*> context{&s_pRpslContext, this};

        auto* pSignature = execInfo.pProgram->GetSignature();
        RPS_CHECK_ARGS(execInfo.numArgs == pSignature->GetParamDecls().size());

        Subprogram* const pCurrProgram = execInfo.pProgram;

        if (pCurrProgram->GetEntry())
        {
            RPS_STATIC_ASSERT(RPS_OK == 0, "RPS_OK must be 0");
            RPS_STATIC_ASSERT(!RPS_FAILED(0), "RPS_FAILED no longer maps 0 as a success code");
            RPS_V_RETURN(RpslHostCallEntry(pCurrProgram->GetEntry()->pfnEntry, execInfo.numArgs, execInfo.ppArgs));
        }
        else
        {
            // TODO: handle pre-built cmdBuf
            RPS_TODO_RETURN_NOT_IMPLEMENTED();  // Mixing non-RPSL program
        }

        return RPS_OK;
    }

    RpsResult RpslHost::RpslCallNode(RpsNodeDeclId         localNodeDeclId,
                                     ArrayRef<RpsVariable> args,
                                     uint32_t              callFlags,
                                     uint32_t              nodeLocalId,
                                     RpsNodeId*            pOutCmdId)
    {
        const uint32_t stableLocalNodeId =
            m_pGraphBuilder->GetCurrentProgram()
                ->m_persistentIndexGenerator.Generate<ProgramInstance::PERSISTENT_INDEX_KIND_NODE_ID>(nodeLocalId);

        return m_pGraphBuilder->AddNode(this, localNodeDeclId, args, callFlags, stableLocalNodeId, pOutCmdId);
    }

    void RpslHost::AddDependencies(ConstArrayRef<RpsNodeId> dependencies, RpsNodeId dstNode)
    {
        for (auto dep : dependencies)
        {
            if (dep != RPS_INDEX_NONE_U32)
            {
                m_pGraphBuilder->AddDependency(dep, dstNode);
            }
        }
    }

    enum
    {
        RPS_MARKER_FUNCTION_INFO = 0,
        RPS_MARKER_LOOP_BEGIN,
        RPS_MARKER_LOOP_ITERATION,
        RPS_MARKER_LOOP_END,
        RPS_MARKER_BASIC_BLOCK_BEGIN,
        RPS_MARKER_BASIC_BLOCK_END,
    };

    RpsResult RpslHost::BlockMarker(uint32_t                markerType,
                                    uint32_t                blockIndex,
                                    ConstArrayRef<uint32_t> resourceCounts,
                                    uint32_t                localLoopIndex,
                                    uint32_t                numChildren,
                                    uint32_t                parentId)
    {
        RpsResult result = RPS_OK;

        auto& indexGen = m_pGraphBuilder->GetCurrentProgram()->m_persistentIndexGenerator;

        switch (markerType)
        {
        case RPS_MARKER_FUNCTION_INFO:
            RPS_ASSERT(parentId == UINT32_MAX);
            result = indexGen.EnterFunction(blockIndex, resourceCounts, localLoopIndex, numChildren);
            break;
        case RPS_MARKER_LOOP_BEGIN:
            result = indexGen.EnterLoop(blockIndex, resourceCounts, localLoopIndex, numChildren);
            break;
        case RPS_MARKER_LOOP_END:
            result = indexGen.ExitLoop(blockIndex);
            break;
        case RPS_MARKER_LOOP_ITERATION:
            result = indexGen.LoopIteration(blockIndex);
            break;
        default:
            break;
        }

        return result;
    }

    RpsResult RpslHost::SchedulerMarker(SchedulerMarkerOpCodes opCode,
                                        RpsSubgraphFlags       flags,
                                        const char*            name,
                                        uint32_t               nameLength)
    {
        static constexpr BuiltInNodeDeclIds s_schedulerMarkerOpCodesToNodeDeclIds[] = {
            RPS_BUILTIN_NODE_SCHEDULER_BARRIER,  // BARRIER
            RPS_BUILTIN_NODE_SUBGRAPH_BEGIN,     // SUBGRAPH_BEGIN
            RPS_BUILTIN_NODE_SUBGRAPH_END,       // SUBGRAPH_END
        };

        static_assert(RPS_COUNTOF(s_schedulerMarkerOpCodesToNodeDeclIds) == uint32_t(SchedulerMarkerOpCodes::MaxValue),
                      "s_schedulerMarkerOpCodesToNodeDeclIds outdated.");

        RPS_RETURN_ERROR_IF(uint32_t(opCode) >= uint32_t(SchedulerMarkerOpCodes::MaxValue), RPS_ERROR_INVALID_PROGRAM);

        switch (opCode)
        {
        case SchedulerMarkerOpCodes::BARRIER:
            return m_pGraphBuilder->ScheduleBarrier();
        case SchedulerMarkerOpCodes::SUBGRAPH_BEGIN:
            //TODO: Do we need to create stable Ids for subgraphs?
            return m_pGraphBuilder->BeginSubgraph(flags);
        case SchedulerMarkerOpCodes::SUBGRAPH_END:
            return m_pGraphBuilder->EndSubgraph();
        default:
            break;
        }

        return RPS_ERROR_INVALID_PROGRAM;
    }

    RpsResult RpslHost::RpslDeclareResource(uint32_t  type,
                                            uint32_t  flags,
                                            uint32_t  format,
                                            uint32_t  width,
                                            uint32_t  height,
                                            uint32_t  depthOrArraySize,
                                            uint32_t  mipLevels,
                                            uint32_t  sampleCount,
                                            uint32_t  sampleQuality,
                                            uint32_t  temporalLayers,
                                            uint32_t  id,
                                            uint32_t* pOutResourceId)
    {
        auto* pVariable = m_pGraphBuilder->AllocateData(sizeof(RpsResourceDesc), alignof(RpsResourceDesc));
        RPS_CHECK_ALLOC(pVariable);

        auto* pResDesc = static_cast<RpsResourceDesc*>(pVariable);

        pResDesc->type           = RpsResourceType(type);
        pResDesc->temporalLayers = temporalLayers;
        pResDesc->flags          = flags;

        if (ResourceDesc::IsImage(pResDesc->type))
        {
            pResDesc->image.width       = width;
            pResDesc->image.height      = height;
            pResDesc->image.depth       = depthOrArraySize;
            pResDesc->image.mipLevels   = mipLevels;
            pResDesc->image.format      = RpsFormat(format);
            pResDesc->image.sampleCount = sampleCount;

            RPS_ASSERT((sampleQuality == 0) && "TODO!");
        }
        else if (ResourceDesc::IsBuffer(pResDesc->type))
        {
            pResDesc->buffer.sizeInBytesLo = width;
            pResDesc->buffer.sizeInBytesHi = height;
        }

        const TResult<uint32_t> stableResId =
            m_pGraphBuilder->GetCurrentProgram()
                ->m_persistentIndexGenerator.Generate<ProgramInstance::PERSISTENT_INDEX_KIND_RESOURCE_ID>(id);

        RPS_V_RETURN(stableResId.Result());

        RPS_V_RETURN(m_pGraphBuilder->DeclareResource(stableResId, pResDesc, {}, pOutResourceId));

        return RPS_OK;
    }

    RpsResult RpslHost::NotifyOutParamResources(uint32_t paramId, const void* pData)
    {
        auto pView = static_cast<const RpsResourceView*>(pData);

        return GetRenderGraphBuilder()->SetOutputParamResourceView(paramId, pView);
    }

}  // namespace rps

#define USING_RPSL_CONTEXT(Ctx) rps::RpslHost* Ctx = rps::s_pRpslContext;

extern "C" {

RpsResult RpslHostBlockMarker(uint32_t markerType,
                              uint32_t blockIndex,
                              uint32_t resourceCount,
                              uint32_t nodeCount,
                              uint32_t localLoopIndex,
                              uint32_t numChildren,
                              uint32_t parentId)
{
    USING_RPSL_CONTEXT(pCtx);

    return pCtx->BlockMarker(markerType, blockIndex, {resourceCount, nodeCount}, localLoopIndex, numChildren, parentId);
}

RpsResult RpslSchedulerMarker(uint32_t opCode, uint32_t flags, const char* name, uint32_t nameLength)
{
    USING_RPSL_CONTEXT(pCtx);

    return pCtx->SchedulerMarker(rps::SchedulerMarkerOpCodes(opCode), flags, name, nameLength);
}

RpsResult RpslHostCallNode(uint32_t  nodeDeclId,
                           uint32_t  numArgs,
                           void**    ppArgs,
                           uint32_t  nodeCallFlags,
                           uint32_t  localNodeId,
                           uint32_t* pCmdIdOut)
{
    USING_RPSL_CONTEXT(pCtx);

    *pCmdIdOut = RPS_CMD_ID_INVALID;
    return pCtx->RpslCallNode(nodeDeclId, {ppArgs, numArgs}, nodeCallFlags, localNodeId, pCmdIdOut);
}

RpsResult RpslHostNodeDependencies(uint32_t numDeps, const uint32_t* pDeps, uint32_t dstNodeId)
{
    USING_RPSL_CONTEXT(pCtx);

    pCtx->AddDependencies({pDeps, numDeps}, dstNodeId);

    return RPS_OK;
}

RpsResult RpslHostDescribeHandle(void* pOutData, uint32_t dataSize, const uint32_t* inHandle, uint32_t describeOp)
{
    USING_RPSL_CONTEXT(pCtx);

    if (dataSize != sizeof(RpsResourceDesc))
    {
        return RPS_ERROR_INVALID_PROGRAM;
    }

    auto& resDecl = pCtx->GetRenderGraphBuilder()->GetResourceDecls()[*inHandle];
    memcpy(pOutData, resDecl.desc, dataSize);

    return RPS_OK;
}

RpsResult RpslHostCreateResource(uint32_t  type,
                                 uint32_t  flags,
                                 uint32_t  format,
                                 uint32_t  width,
                                 uint32_t  height,
                                 uint32_t  depthOrArraySize,
                                 uint32_t  mipLevels,
                                 uint32_t  sampleCount,
                                 uint32_t  sampleQuality,
                                 uint32_t  temporalLayers,
                                 uint32_t  id,
                                 uint32_t* pOutResourceId)
{
    USING_RPSL_CONTEXT(pCtx);

    return pCtx->RpslDeclareResource(type,
                                     flags,
                                     format,
                                     width,
                                     height,
                                     depthOrArraySize,
                                     mipLevels,
                                     sampleCount,
                                     sampleQuality,
                                     temporalLayers,
                                     id,
                                     pOutResourceId);
}

RpsResult RpslHostNameResource(uint32_t resourceHdl, const char* name, uint32_t nameLength)
{
    USING_RPSL_CONTEXT(pCtx);

    return pCtx->GetRenderGraphBuilder()->SetResourceName(resourceHdl, rps::StrRef(name, nameLength));
}

void RpslNotifyAbort(RpsResult result)
{
#if RPS_DEBUG
    static volatile RpsResult breakHere;
    breakHere = result;

    static volatile RpsBool bAssertOnRpslError = RPS_TRUE;
    RPS_ASSERT(bAssertOnRpslError);
#endif  //RPS_DEBUG
}

RpsResult RpslNotifyOutParamResources(uint32_t paramId, const void* pViews)
{
    USING_RPSL_CONTEXT(pCtx);

    return pCtx->NotifyOutParamResources(paramId, pViews);
}

uint32_t RpslHostReverseBits32(uint32_t value)
{
    return rpsReverseBits32(value);
}

uint32_t RpslHostCountBits(uint32_t value)
{
    return rpsCountBits(value);
}

uint32_t RpslHostFirstBitLow(uint32_t value)
{
    return rpsFirstBitLow(value);
}

uint32_t RpslHostFirstBitHigh(uint32_t value)
{
    return rpsFirstBitHigh(value);
}

}  // extern "C"

const char* rpsMakeRpslEntryName(char* pBuf, size_t bufSize, const char* moduleName, const char* entryName)
{
    static constexpr char modulePrefix[] = "rpsl_M_";
    static constexpr char entryPrefix[]  = "_E_";

    if (!moduleName || !entryName)
        return nullptr;

    size_t moduleNameLen = strlen(moduleName);
    size_t entryNameLen  = strlen(entryName);

    if ((sizeof(modulePrefix) + sizeof(entryPrefix) - 1 + moduleNameLen + entryNameLen) > bufSize)
        return nullptr;

    char* pDst = pBuf;
    memcpy(pDst, "rpsl_M_", sizeof(modulePrefix) - 1);
    pDst += sizeof(modulePrefix) - 1;
    memcpy(pDst, moduleName, moduleNameLen);
    pDst += moduleNameLen;
    memcpy(pDst, entryPrefix, sizeof(entryPrefix) - 1);
    pDst += sizeof(entryPrefix) - 1;
    memcpy(pDst, entryName, entryNameLen + 1);

    return pBuf;
}

RpsResult rpsRpslEntryGetSignatureDesc(RpsRpslEntry hRpslEntry, RpsRenderGraphSignatureDesc* pDesc)
{
    RPS_CHECK_ARGS(hRpslEntry != RPS_NULL_HANDLE);
    RPS_CHECK_ARGS(pDesc != nullptr);

    auto pEntry = rps::FromHandle(hRpslEntry);

    // TODO: Move to compiler.
    uint32_t maxParamResources = 0;

    for (auto paramIter = pEntry->pParamDescs, paramEnd = pEntry->pParamDescs + pEntry->numParams;
         paramIter != paramEnd;
         ++paramIter)
    {
        if (paramIter->flags & RPS_PARAMETER_FLAG_RESOURCE_BIT)
        {
            maxParamResources +=
                ((paramIter->arraySize == UINT32_MAX) ? 0 : ((paramIter->arraySize == 0) ? 1 : paramIter->arraySize));
        }
    }

    pDesc->numParams            = pEntry->numParams;
    pDesc->numNodeDescs         = pEntry->numNodeDecls;
    pDesc->maxExternalResources = maxParamResources;
    pDesc->pParamDescs          = pEntry->pParamDescs;
    pDesc->pNodeDescs           = pEntry->pNodeDecls;
    pDesc->name                 = pEntry->name;

    return RPS_OK;
}

namespace rps
{
    namespace details
    {
        RpsResult ProgramGetBindingSlot(RpsSubprogram    hProgram,
                                        const char*      name,
                                        size_t           size,
                                        RpsCmdCallback** ppCallback)
        {
            RPS_CHECK_ARGS(hProgram);
            return rps::FromHandle(hProgram)->BindDeferred(name, size, ppCallback);
        }
    }  // namespace details
}  // namespace rps
