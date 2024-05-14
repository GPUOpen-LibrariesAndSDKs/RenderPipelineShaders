// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "runtime/common/rps_render_graph_builder.hpp"
#include "runtime/common/rps_render_graph.hpp"
#include "runtime/common/rps_rpsl_host.hpp"

namespace rps
{

    RpsResult RenderGraphBuilder::Init(const RenderGraphSignature* pSignature,
                                       Arena&                      persistentArena,
                                       ProgramInstance*            pRootProgramInstance)
    {
        const auto paramDecls = pSignature->GetParamDecls();
        m_paramData           = persistentArena.NewArray<RenderGraphArgInfo>(paramDecls.size());
        RPS_CHECK_ALLOC(m_paramData.size() == paramDecls.size());

        uint32_t maxNumOutResources = 0;
        for (uint32_t i = 0; i < paramDecls.size(); i++)
        {
            // TODO: Using elementSize as alignment for now
            m_paramData[i].data =
                persistentArena.AlignedAllocZeroed(paramDecls[i].GetSize(), paramDecls[i].GetElementSize());
            RPS_CHECK_ALLOC(m_paramData[i].data);

            if (paramDecls[i].IsOutputResource())
            {
                RPS_ASSERT(!paramDecls[i].IsUnboundedArray() && "TODO");

                m_paramData[i].outputResourceIds.SetRange(maxNumOutResources, paramDecls[i].GetNumElements());
                maxNumOutResources += paramDecls[i].GetNumElements();
            }
        }

        m_outputResourceIds = persistentArena.NewArray<RpsResourceId>(maxNumOutResources);
        RPS_CHECK_ALLOC(m_outputResourceIds.size() == maxNumOutResources);

        m_pCurrProgram = pRootProgramInstance;

        m_dynamicNodeDeclIdBegin = uint32_t(pSignature->GetNodeDecls().size());

        return RPS_OK;
    }

    RpsResourceId RenderGraphBuilder::GetParamResourceId(RpsParamId paramId, uint32_t arrayIndex) const
    {
        RPS_RETURN_ERROR_IF(paramId >= m_paramData.size(), RPS_INDEX_NONE_U32);
        RPS_RETURN_ERROR_IF(m_paramData[paramId].resources.empty(), RPS_INDEX_NONE_U32);
        RPS_RETURN_ERROR_IF(m_paramData[paramId].resources.size() <= arrayIndex, RPS_INDEX_NONE_U32);

        return m_paramData[paramId].resources.GetBegin() + arrayIndex;
    }

    TResult<RpsVariable> RenderGraphBuilder::GetParamVariable(RpsParamId paramId, size_t* pVariableSize) const
    {
        RPS_RETURN_ERROR_IF(paramId >= m_paramData.size(), RPS_ERROR_INDEX_OUT_OF_BOUNDS);

        if (pVariableSize)
        {
            *pVariableSize =
                m_renderGraph.GetSignature().GetParamDecls()[paramId].GetElementSize();  // TODO: Handle array
        }

        return m_paramData[paramId].data;
    }

    RpsResult RenderGraphBuilder::Begin()
    {
        RPS_RETURN_ERROR_IF(m_state == State::Building, RPS_ERROR_INVALID_OPERATION);

        m_state       = State::Building;
        m_buildStatus = RPS_OK;

        m_explicitDependencies.reset_keep_capacity(&m_cmdArena);

        m_dynamicNodeDecls.reset_keep_capacity(&m_cmdArena);

        m_resourceDecls.reset_keep_capacity(&m_cmdArena);
        m_resourceDecls.resize(m_renderGraph.GetSignature().GetMaxExternalResourceCount(), {});

        uint32_t resOffset  = 0;
        auto     paramDecls = m_renderGraph.GetSignature().GetParamDecls();

        for (uint32_t iParam = 0; iParam < paramDecls.size(); iParam++)
        {
            const auto& paramDecl = paramDecls[iParam];

            if (paramDecl.IsResource())
            {
                auto  resSlots  = m_resourceDecls.range(resOffset, paramDecl.GetNumElements());
                auto& paramData = m_paramData[iParam];

                if (!resSlots.empty())
                {
                    paramData.resources = {resOffset, uint32_t(resSlots.size())};

                    RpsVariable pResDescVar = paramData.data;
                    for (uint32_t iRes = 0, numRes = uint32_t(resSlots.size()); iRes < numRes; iRes++)
                    {
                        auto& resSlot = resSlots[iRes];

                        resSlot.desc = pResDescVar;
                        resSlot.name = paramDecl.name;

                        pResDescVar = rpsBytePtrInc(pResDescVar, paramDecl.typeInfo.size);

                        resOffset++;
                    }
                }
            }
        }

        std::fill(m_outputResourceIds.begin(), m_outputResourceIds.end(), RPS_RESOURCE_ID_INVALID);

        RPS_ASSERT(resOffset == m_renderGraph.GetSignature().GetMaxExternalResourceCount());

        return RPS_OK;
    }

    RpsResult RenderGraphBuilder::End()
    {
        RPS_RETURN_ERROR_IF(m_state != State::Building, RPS_ERROR_INVALID_OPERATION);

        RpsResult result = m_buildStatus;
        m_buildStatus    = RPS_OK;
        m_state          = State::Closed;

        auto& cmdInfos = m_renderGraph.GetCmdInfos();
        for (auto cmdIter = cmdInfos.begin(), cmdEnd = cmdInfos.end(); cmdIter != cmdEnd; ++cmdIter)
        {
            cmdIter->pCmdDecl = cmdIter->IsNodeDeclBuiltIn() ? nullptr : m_cmdNodes.GetSlot(cmdIter->cmdDeclIndex);
        }

        return result;
    }

    void* RenderGraphBuilder::AllocateData(size_t size, size_t alignment)
    {
        RPS_RETURN_ERROR_IF(m_state != State::Building, nullptr);

        return m_cmdArena.AlignedAlloc(size, alignment);
    }

    RpsVariable RenderGraphBuilder::DeclareVariable(size_t size, size_t alignment, const void* pInitData)
    {
        void* pVarData = AllocateData(size, rpsMax(alignof(uint32_t), alignment));
        if (pVarData && pInitData)
        {
            memcpy(pVarData, pInitData, size);
        }
        return pVarData;
    }

    RpsNodeDeclId RenderGraphBuilder::DeclareDynamicNode(const RpsNodeDesc* pNodeDesc)
    {
        RPS_RETURN_ERROR_IF(pNodeDesc == nullptr, RPS_NODEDECL_ID_INVALID);

        auto* pNewNodeDecl = m_cmdArena.New<NodeDeclInfo>();
        RPS_RETURN_ERROR_IF(pNewNodeDecl == nullptr, RPS_NODEDECL_ID_INVALID);

        RPS_RETURN_ERROR_IF(RPS_FAILED(RenderGraphSignature::InitNodeDecl(m_cmdArena, *pNodeDesc, *pNewNodeDecl)),
                            RPS_NODEDECL_ID_INVALID);

        return m_dynamicNodeDecls.push_back(pNewNodeDecl)
                   ? (m_dynamicNodeDeclIdBegin + RpsNodeDeclId(m_dynamicNodeDecls.size() - 1))
                   : RPS_NODEDECL_ID_INVALID;
    }

    RpsResult RenderGraphBuilder::DeclareResource(uint32_t       localResourceId,
                                                  RpsVariable    hDescVar,
                                                  StrRef         name,
                                                  RpsResourceId* pOutResId)
    {
        uint32_t resourceId = GetOrAllocResourceSlot(localResourceId);

        RPS_ASSERT(resourceId >= m_renderGraph.GetSignature().GetMaxExternalResourceCount());

        if (resourceId >= m_resourceDecls.size())
        {
            m_resourceDecls.resize(resourceId + 1, {});
        }

        auto& resDecl = m_resourceDecls[resourceId];
        resDecl.desc  = hDescVar;
        resDecl.name  = m_cmdArena.StoreStr(name);

        *pOutResId = resourceId;

        return RPS_OK;
    }

    RpsResult RenderGraphBuilder::SetResourceName(RpsResourceId resourceId, StrRef name)
    {
        RPS_CHECK_ARGS((resourceId >= m_renderGraph.GetSignature().GetMaxExternalResourceCount()) ||
                       (resourceId < m_resourceDecls.size()));

        auto& resDecl = m_resourceDecls[resourceId];
        resDecl.name  = m_cmdArena.StoreStr(name);

        return RPS_OK;
    }

    static TResult<const NodeDeclInfo*> GetBuiltInCmdNodeInfo(BuiltInNodeDeclIds nodeDeclId)
    {
        static constexpr struct
        {
            BuiltInNodeDeclIds id;
            const NodeDeclInfo declInfo;
        } c_builtInNodeNames[] = {
            {RPS_BUILTIN_NODE_INVALID, {StrRef::From("invalid")}},
            {RPS_BUILTIN_NODE_SCHEDULER_BARRIER, {StrRef::From("scheduler_barrier")}},
            {RPS_BUILTIN_NODE_SUBGRAPH_BEGIN, {StrRef::From("subgraph_begin")}},
            {RPS_BUILTIN_NODE_SUBGRAPH_END, {StrRef::From("subgraph_end")}},
            {RPS_BUILTIN_NODE_BEGIN_SUBROUTINE, {StrRef::From("subroutine_begin")}},
            {RPS_BUILTIN_NODE_END_SUBROUTINE, {StrRef::From("subroutine_end")}},
        };

        const uint32_t tableIdx = uint32_t(-(int32_t(nodeDeclId) + 1));

        RPS_CHECK_ARGS(tableIdx < RPS_COUNTOF(c_builtInNodeNames));
        RPS_RETURN_ERROR_IF(c_builtInNodeNames[tableIdx].id != nodeDeclId, RPS_ERROR_INTERNAL_ERROR);

        return &c_builtInNodeNames[tableIdx].declInfo;
    }

    RpsResult RenderGraphBuilder::AddNode(RpslHost*             pRpslHost,
                                          RpsNodeDeclId         localNodeDeclId,
                                          ArrayRef<RpsVariable> args,
                                          RpsNodeFlags          callFlags,
                                          uint32_t              nodeLocalId,
                                          RpsNodeId*            pOutCmdId)
    {
        const Subprogram* pCurrProgram = m_pCurrProgram->m_pProgram;

        auto& nodeImpl = pCurrProgram->GetNodeImpl(localNodeDeclId);

        if (nodeImpl.type == Subprogram::RpslNodeImpl::Type::RpslEntry)
        {
            RPS_ASSERT(nodeImpl.pSubprogram->GetEntry());

            // Dummy BeginSubroutine node
            RpsNodeId beginSubroutine = RPS_CMD_ID_INVALID;

            auto subRoutineNodeDecl = GetBuiltInCmdNodeInfo(RPS_BUILTIN_NODE_BEGIN_SUBROUTINE);
            RPS_V_RETURN(subRoutineNodeDecl.Result());

            RPS_V_RETURN(AddCmdNode(
                RPS_BUILTIN_NODE_BEGIN_SUBROUTINE, subRoutineNodeDecl, nodeLocalId, {}, nullptr, 0, &beginSubroutine));
            RPS_ASSERT((beginSubroutine != RPS_CMD_ID_INVALID) && "invalid RenderGraphBuilder::AddCmdNode impl");

            // TODO: using CmdId as global persistent ProgramInstanceId for now.
            // TODO: need to version nodeImpl.pSubprogram in case it's recreated at the same address.
            auto             pSubprogramInstanceId = &m_cmdNodes.GetSlot(beginSubroutine)->programInstanceId;
            ProgramInstance* pSubprogramInstance =
                m_renderGraph.GetOrCreateProgramInstance(nodeImpl.pSubprogram, *pSubprogramInstanceId);

            if (pCurrProgram->GetEntry())
            {
                // Fast path, both caller and callee are RPSL functions,
                // call the function directly without extra context setup.

                ScopedContext<ProgramInstance*> programContext(&m_pCurrProgram, pSubprogramInstance);

                (nodeImpl.pSubprogram->GetEntry()->pfnEntry)(
                    uint32_t(args.size()), args.data(), RPSL_ENTRY_CALL_SUBPROGRAM);
            }
            else
            {
                ScopedContext<ProgramInstance*> programContext(&m_pCurrProgram, pSubprogramInstance);

                RpslExecuteInfo callInfo = {};
                callInfo.pProgram        = nodeImpl.pSubprogram;
                callInfo.ppArgs          = args.data();
                callInfo.numArgs         = uint32_t(args.size());

                // Temp - Remove pRpslHost param when making RpslHost local context.
                RPS_V_RETURN(pRpslHost->Execute(callInfo));
            }

            *pOutCmdId = beginSubroutine;
        }
        else
        {
            auto pNodeDecl = pCurrProgram->GetSignature()->GetNodeDecl(localNodeDeclId);  // TODO: Handle dynamic nodes
            RPS_ASSERT(pNodeDecl->params.size() == args.size());

            for (uint32_t iParam = 0, numParams = uint32_t(args.size()); iParam < numParams; iParam++)
            {
                auto& paramDecl = pNodeDecl->params[iParam];

                const size_t alignment = rpsMin(paramDecl.GetElementSize(), alignof(std::max_align_t));  // TODO
                const size_t paramSize = paramDecl.GetSize();

                const void* pSrc = args[iParam];

                args[iParam] = AllocateData(paramSize, alignment);

                // TODO: Check if we can pass the pointer to be filled on RPSL side.
                memcpy(args[iParam], pSrc, paramSize);
            }

            const auto& callback = ((nodeImpl.type == Subprogram::RpslNodeImpl::Type::Callback) &&
                                    (nodeImpl.callback.pfnCallback != nullptr))
                                       ? nodeImpl.callback
                                       : pCurrProgram->GetDefaultNodeCallback();

            RPS_V_RETURN(AddCmdNode(
                localNodeDeclId, pNodeDecl, nodeLocalId, callback, args.data(), uint32_t(args.size()), pOutCmdId, callFlags));
            RPS_ASSERT((*pOutCmdId != RPS_CMD_ID_INVALID) && "invalid RenderGraphBuilder::AddCmdNode impl");
        }

        return RPS_OK;
    }

    RpsNodeId RenderGraphBuilder::GetOrAllocCmdSlot(uint32_t localNodeId)
    {
        auto pCmdId = m_pCurrProgram->m_cmdIds.get_or_grow(localNodeId, RPS_CMD_ID_INVALID);
        RPS_RETURN_ERROR_IF(!pCmdId, RPS_CMD_ID_INVALID);

        if (*pCmdId == RPS_CMD_ID_INVALID)
        {
            *pCmdId = AllocCmdSlot();
        }

        return *pCmdId;
    }

    uint32_t RenderGraphBuilder::GetOrAllocResourceSlot(uint32_t localResourceId)
    {
        auto pResId = m_pCurrProgram->m_resourceIds.get_or_grow(localResourceId, RPS_RESOURCE_ID_INVALID);
        RPS_RETURN_ERROR_IF(!pResId, RPS_RESOURCE_ID_INVALID);

        if (*pResId == RPS_RESOURCE_ID_INVALID)
        {
            *pResId = AllocResourceSlot();
        }

        return *pResId;
    }

    uint32_t RenderGraphBuilder::AllocResourceSlot()
    {
        return m_resourceDeclSlots.AllocSlot() + m_renderGraph.GetSignature().GetMaxExternalResourceCount();
    }

    RpsResult RenderGraphBuilder::AddCmdNode(RpsNodeDeclId         nodeDeclId,
                                             uint32_t              localNodeId,
                                             const RpsCmdCallback& callback,
                                             const RpsVariable*    pArgs,
                                             uint32_t              numArgs,
                                             RpsNodeId*            pOutCmdId)
    {
        auto pNodeDecl = (nodeDeclId < m_dynamicNodeDeclIdBegin)
                             ? m_pCurrProgram->m_pProgram->GetSignature()->GetNodeDecl(nodeDeclId)
                             : m_dynamicNodeDecls[nodeDeclId - m_dynamicNodeDeclIdBegin];

        RPS_CHECK_ARGS(pNodeDecl->params.size() == numArgs);

        return AddCmdNode(nodeDeclId, pNodeDecl, localNodeId, callback, pArgs, numArgs, pOutCmdId);
    }

    RpsResult RenderGraphBuilder::AddCmdNode(RpsNodeDeclId         nodeDeclId,
                                             const NodeDeclInfo*   pNodeDecl,
                                             uint32_t              localNodeId,
                                             const RpsCmdCallback& callback,
                                             const RpsVariable*    pArgs,
                                             uint32_t              numArgs,
                                             RpsNodeId*            pOutCmdId,
                                             RpsNodeFlags          flags)
    {
        if (!CmdInfo::IsNodeDeclIdBuiltIn(nodeDeclId) &&
            !!(m_renderGraph.GetCreateInfo().renderGraphFlags & RPS_RENDER_GRAPH_DISALLOW_UNBOUND_NODES_BIT) &&
            !callback.pfnCallback)
        {
            return RPS_ERROR_UNRECOGNIZED_COMMAND;
        }

        const uint32_t currCmdSlot = GetOrAllocCmdSlot(localNodeId);

        Cmd* pOp        = m_cmdNodes.GetSlot(currCmdSlot);
        pOp->nodeDeclId = nodeDeclId;
        pOp->callback   = callback;
        pOp->args       = m_cmdArena.NewArray<RpsVariable>(numArgs);
        std::copy(pArgs, pArgs + numArgs, pOp->args.begin());

        auto& cmdInfos = m_renderGraph.GetCmdInfos();

        const uint32_t currNodeIdx = uint32_t(cmdInfos.size());
        CmdInfo*       pCmdInfo    = cmdInfos.grow(1, {});

        pCmdInfo->nodeDeclIndex = nodeDeclId;
        pCmdInfo->cmdDeclIndex  = currCmdSlot;
        pCmdInfo->bPreferAsync =
            !!(flags & RPS_NODE_PREFER_ASYNC) || (pNodeDecl && !!(pNodeDecl->flags & RPS_NODE_DECL_PREFER_ASYNC));
        pCmdInfo->pNodeDecl = pNodeDecl;

        *pOutCmdId = currNodeIdx;

        return RPS_OK;
    }

    RpsResult RenderGraphBuilder::ScheduleBarrier()
    {
        return AddBuiltInCmdNode(RPS_BUILTIN_NODE_SCHEDULER_BARRIER).Result();
    }

    RpsResult RenderGraphBuilder::BeginSubgraph(RpsSubgraphFlags flags)
    {
        TResult<CmdInfo*> cmdInfoResult = AddBuiltInCmdNode(RPS_BUILTIN_NODE_SUBGRAPH_BEGIN);
        RPS_V_RETURN(cmdInfoResult.Result());

        cmdInfoResult.data->subgraphFlags = flags;
        return RPS_OK;
    }

    RpsResult RenderGraphBuilder::EndSubgraph()
    {
        return AddBuiltInCmdNode(RPS_BUILTIN_NODE_SUBGRAPH_END).Result();
    }

    TResult<CmdInfo*> RenderGraphBuilder::AddBuiltInCmdNode(BuiltInNodeDeclIds nodeDeclId)
    {
        auto pNodeDeclInfo = GetBuiltInCmdNodeInfo(nodeDeclId);
        RPS_V_RETURN(pNodeDeclInfo.Result());

        CmdInfo* pCmdInfo = m_renderGraph.GetCmdInfos().grow(1, {});
        RPS_CHECK_ALLOC(pCmdInfo);

        pCmdInfo->nodeDeclIndex = nodeDeclId;
        pCmdInfo->pNodeDecl     = pNodeDeclInfo;

        return pCmdInfo;
    }

    void RenderGraphBuilder::AddDependency(RpsNodeId before, RpsNodeId after)
    {
        m_explicitDependencies.emplace_back(NodeDependency{before, after});
    }

    RpsResult RenderGraphBuilder::SetOutputParamResourceView(RpsParamId paramId, const RpsResourceView* pViews)
    {
        const ParamDecl& paramDecl = GetRenderGraph().GetSignature().GetParamDecl(paramId);
        RPS_RETURN_ERROR_IF(!paramDecl.IsOutputResource(), RPS_ERROR_INVALID_PROGRAM);

        auto outResIdRangeRef = m_paramData[paramId].outputResourceIds.Get(m_outputResourceIds);

        for (uint32_t iElem = 0; iElem < paramDecl.GetNumElements(); iElem++)
        {
            // TODO: Ignoring view info currently, only taking the resource info.
            // Need to investigate passing on view info as well.
            outResIdRangeRef[iElem] = pViews[iElem].resourceId;
        }

        return RPS_OK;
    }
}  // namespace rps
