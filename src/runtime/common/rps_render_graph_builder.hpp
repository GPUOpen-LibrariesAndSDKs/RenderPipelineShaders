// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_RENDER_GRAPH_BUILDER_HPP
#define RPS_RENDER_GRAPH_BUILDER_HPP

#include "core/rps_core.hpp"
#include "core/rps_util.hpp"
#include "runtime/common/rps_cmd_buf.hpp"
#include "runtime/common/rps_render_graph_signature.hpp"
#include "runtime/common/rps_render_graph_resource.hpp"

namespace rps
{
    class RenderGraph;
    class RpslHost;
    class ProgramInstance;
    class RenderGraphSignature;
    struct CmdInfo;

    class RenderGraphBuilder
    {
        friend class RenderGraph;
        RPS_CLASS_NO_COPY_MOVE(RenderGraphBuilder);

        RenderGraphBuilder(RenderGraph& renderGraph, Arena& persistentArena, Arena& frameArena)
            : m_renderGraph(renderGraph)
            , m_cmdArena(frameArena)
            , m_resourceDecls(&m_cmdArena)
            , m_resourceDeclSlots(&persistentArena)
            , m_cmdNodes(&persistentArena)
            , m_explicitDependencies(&m_cmdArena)
            , m_dynamicNodeDecls(&m_cmdArena)
        {
        }

        RpsResult Init(const RenderGraphSignature* pSignature,
                       Arena&                      persistentArena,
                       ProgramInstance*            pRootProgramInstance);

    public:
        ProgramInstance* GetCurrentProgram() const
        {
            return m_pCurrProgram;
        }

        const RenderGraph& GetRenderGraph() const
        {
            return m_renderGraph;
        }

        Cmd* GetCmdDecl(RpsNodeId cmdId)
        {
            return m_cmdNodes.GetSlot(cmdId);
        }

        const Cmd* GetCmdDecl(RpsNodeId cmdId) const
        {
            return m_cmdNodes.GetSlot(cmdId);
        }

        ConstArrayRef<ResourceDecl, uint32_t> GetResourceDecls() const
        {
            return m_resourceDecls.range_all();
        }

        ConstArrayRef<RpsResourceId> GetOutputParamResourceIds() const
        {
            return m_outputResourceIds;
        }

        ConstArrayRef<RpsResourceId> GetOutputParamResourceIds(RpsParamId paramId) const
        {
            return m_paramData[paramId].outputResourceIds.Get(m_outputResourceIds);
        }

        ConstArrayRef<NodeDependency> GetExplicitDependencies() const
        {
            return m_explicitDependencies.range_all();
        }

        TResult<RpsVariable> GetParamVariable(RpsParamId paramId, size_t* pSize = nullptr) const;

        template <typename T>
        TResult<T*> GetParamVariable(RpsParamId paramId) const
        {
            size_t varSize = 0;
            auto   result  = GetParamVariable(paramId, &varSize);

            RPS_RETURN_ERROR_IF(varSize != sizeof(T), RPS_ERROR_TYPE_MISMATCH);

            return result.StaticCast<T*>();
        }

        template <typename T>
        RpsResult SetParamVariable(RpsParamId paramId, const T& value)
        {
            auto pData = GetParamVariable<T>(paramId);
            if (pData)
            {
                *pData = value;
                return RPS_OK;
            }
            return RPS_ERROR_INDEX_OUT_OF_BOUNDS;
        }

        RpsResourceId GetParamResourceId(RpsParamId paramId, uint32_t arrayIndex = 0) const;

        RpsResult     Begin();
        RpsResult     End();
        void*         AllocateData(size_t size, size_t alignment);
        RpsVariable   DeclareVariable(size_t size, size_t alignment, const void* pData = nullptr);
        RpsNodeDeclId DeclareDynamicNode(const RpsNodeDesc* pNodeDesc);
        RpsResult     DeclareResource(uint32_t       localResourceId,
                                      RpsVariable    hDescVar,
                                      StrRef         name,
                                      RpsResourceId* pOutResId);
        RpsResult     SetResourceName(RpsResourceId resourceId, StrRef name);
        RpsResult     AddCmdNode(RpsNodeDeclId         nodeDeclId,
                                 uint32_t              tag,
                                 const RpsCmdCallback& callback,
                                 const RpsVariable*    pArgs,
                                 uint32_t              numArgs,
                                 RpsNodeId*            pOutCmdId);
        RpsResult     AddNode(RpslHost*             pRpslHost,
                              RpsNodeDeclId         localNodeDeclId,
                              ArrayRef<RpsVariable> args,
                              RpsNodeFlags          callFlags,
                              uint32_t              nodeLocalId,
                              RpsNodeId*            pOutCmdId);
        RpsResult     ScheduleBarrier();
        RpsResult     BeginSubgraph(RpsSubgraphFlags flags);
        RpsResult     EndSubgraph();
        void          AddDependency(RpsNodeId before, RpsNodeId after);
        RpsResult     SetOutputParamResourceView(RpsParamId paramId, const RpsResourceView* pViews);

        RpsResult Print(const RpsPrinter* pPrinter);

    private:
        void SetBuildError(RpsResult errorCode)
        {
            RPS_ASSERT(m_state == State::Building);

            m_state       = State::Error;
            m_buildStatus = errorCode;
        }

        uint32_t GetOrAllocCmdSlot(uint32_t localNodeId);
        uint32_t GetOrAllocResourceSlot(uint32_t localResourceId);

        uint32_t AllocCmdSlot()
        {
            return m_cmdNodes.AllocSlot();
        }

        void FreeCmdSlot(uint32_t cmdId)
        {
            m_cmdNodes.FreeSlot(cmdId);
        }

        RpsResult AddCmdNode(RpsNodeDeclId         nodeDeclId,
                             const NodeDeclInfo*   pNodeDecl,
                             uint32_t              tag,
                             const RpsCmdCallback& callback,
                             const RpsVariable*    pArgs,
                             uint32_t              numArgs,
                             RpsNodeId*            pOutCmdId,
                             RpsNodeFlags          flags = RPS_NODE_FLAG_NONE);

        TResult<CmdInfo*> AddBuiltInCmdNode(BuiltInNodeDeclIds nodeDeclId);

        uint32_t AllocResourceSlot();

        void FreeResourceSlot(uint32_t resourceId)
        {
            m_resourceDeclSlots.FreeSlot(resourceId);
        }

    public:
        struct RenderGraphArgInfo
        {
            RpsVariable         data;
            Span<ResourceDecl>  resources;
            Span<RpsResourceId> outputResourceIds;
        };

    private:
        enum class State
        {
            Created,
            Closed,
            Building,
            Error,
        };

        RenderGraph& m_renderGraph;
        Arena&       m_cmdArena;
        State        m_state       = State::Created;
        RpsResult    m_buildStatus = RPS_OK;

        ArrayRef<RenderGraphArgInfo> m_paramData;
        ArrayRef<RpsResourceId>      m_outputResourceIds;
        ArenaVector<ResourceDecl>    m_resourceDecls;
        ArenaFreeListPool<uint32_t>  m_resourceDeclSlots;
        ArenaFreeListPool<Cmd>       m_cmdNodes;
        ArenaVector<NodeDependency>  m_explicitDependencies;

        ArenaVector<const NodeDeclInfo*> m_dynamicNodeDecls;
        uint32_t                         m_dynamicNodeDeclIdBegin = 0;

        ProgramInstance* m_pCurrProgram = nullptr;
    };

    RPS_ASSOCIATE_HANDLE(RenderGraphBuilder);
}

#endif  //RPS_RENDER_GRAPH_BUILDER_HPP
