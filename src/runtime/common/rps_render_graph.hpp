// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_RENDER_GRAPH_HPP
#define RPS_RENDER_GRAPH_HPP

#include "core/rps_core.hpp"
#include "core/rps_util.hpp"
#include "core/rps_graph.hpp"

#include "core/rps_persistent_index_generator.hpp"

#include "rps/runtime/common/rps_runtime.h"
#include "runtime/common/rps_cmd_buf.hpp"
#include "runtime/common/rps_render_graph_resource.hpp"
#include "runtime/common/rps_render_graph_signature.hpp"
#include "runtime/common/rps_render_graph_builder.hpp"
#include "runtime/common/rps_subprogram.hpp"

namespace rps
{
    class RenderGraph;
    class RenderGraphBuilder;
    class RuntimeDevice;
    class RuntimeBackend;
    class RpslHost;

    struct RuntimeCmdCallbackContext;

    struct ResourceAliasingInfo
    {
        uint32_t srcResourceIndex;
        uint32_t dstResourceIndex;
        RpsBool  srcDeactivating : 1;
        RpsBool  dstActivating : 1;
    };

    struct FinalAccessInfo
    {
        uint32_t               prevTransition;
        SubresourceRangePacked range;
    };

    struct ResourceInstance
    {
        uint32_t                resourceDeclId      = RPS_INDEX_NONE_U32;
        uint32_t                temporalLayerOffset = RPS_INDEX_NONE_U32;
        ResourceDescPacked      desc                = {};
        SubresourceRangePacked  fullSubresourceRange;
        uint32_t                numSubResources = 0;
        uint32_t                clearValueId    = RPS_INDEX_NONE_U32;
        AccessAttr              allAccesses     = {};
        AccessAttr              initialAccess   = {};
        AccessAttr              prevFinalAccess = {};
        Span<FinalAccessInfo>   finalAccesses   = {};
        uint32_t                lifetimeBegin   = UINT32_MAX;
        uint32_t                lifetimeEnd     = UINT32_MAX;
        bool                    isTemporalSlice : 1;
        bool                    isFirstTemporalSlice : 1;
        bool                    isExternal : 1;
        bool                    isAliased : 1;
        bool                    isPendingCreate : 1;
        bool                    isPendingInit : 1;
        bool                    isAccessed : 1;
        bool                    isMutableFormat : 1;
        bool                    bBufferFormattedWrite : 1;
        bool                    bBufferFormattedRead : 1;
        RpsGpuMemoryRequirement allocRequirement = {0, 0, RPS_INDEX_NONE_U32};
        RpsHeapPlacement        allocPlacement   = {RPS_INDEX_NONE_U32, 0};
        RpsRuntimeResource      hRuntimeResource = {};

        static constexpr uint32_t LIFETIME_UNDEFINED = UINT32_MAX;

        ResourceInstance()
            : isTemporalSlice(false)
            , isFirstTemporalSlice(false)
            , isExternal(false)
            , isAliased(false)
            , isPendingCreate(false)
            , isPendingInit(false)
            , isMutableFormat(false)
            , bBufferFormattedWrite(false)
            , bBufferFormattedRead(false)
        {
        }

        bool IsActive() const
        {
            return resourceDeclId != RPS_INDEX_NONE_U32;
        }

        bool IsTemporalParent() const
        {
            return temporalLayerOffset != RPS_INDEX_NONE_U32;
        }

        bool HasNoAccess() const
        {
            return (allAccesses.accessFlags == RPS_ACCESS_UNKNOWN);
        }

        bool HasEmptyLifetime() const
        {
            return lifetimeBegin > lifetimeEnd;
        }

        bool IsPersistent() const
        {
            return isExternal || rpsAnyBitsSet(desc.flags, RPS_RESOURCE_FLAG_PERSISTENT_BIT);
        }

        void SetInitialAccess(AccessAttr newInitialAccess)
        {
            initialAccess = newInitialAccess;
        }

        void FinalizeRuntimeResourceCreation(const AccessAttr* pOverridePrevAccess = nullptr)
        {
            RPS_ASSERT(hRuntimeResource);
            RPS_ASSERT(isPendingCreate);

            prevFinalAccess = pOverridePrevAccess ? *pOverridePrevAccess : initialAccess;
            isPendingCreate = false;
        }

        void InvalidateRuntimeResource(RuntimeBackend* pBackend);
    };

    struct CmdAccessInfo
    {
        uint32_t               resourceId;
        SubresourceRangePacked range;
        AccessAttr             access;
        RpsFormat              viewFormat;
        const RpsResourceView* pViewInfo;

        void Get(RpsResourceAccessInfo& accessInfo) const
        {
            accessInfo.resourceId = resourceId;
            range.Get(accessInfo.range);
            accessInfo.access = access;
            accessInfo.viewFormat = viewFormat;
        }
    };

    struct CmdRenderPassInfo
    {
        RpsCmdViewportInfo     viewportInfo;
        RpsCmdRenderTargetInfo renderTargetInfo;
    };

    class ProgramInstance
    {
        RPS_CLASS_NO_COPY_MOVE(ProgramInstance);

    public:
        ProgramInstance(const Subprogram* pProgram, Arena& persistentArena)
            : m_pProgram(pProgram)
            , m_resourceIds(&persistentArena)
            , m_cmdIds(&persistentArena)
            , m_persistentIndexGenerator(persistentArena)
        {
        }

        void Reset(const Subprogram* pProgram)
        {
            m_pProgram = pProgram;
            m_cmdIds.clear();
            m_resourceIds.clear();
            m_persistentIndexGenerator.Clear();
        }

        const Subprogram*          m_pProgram = nullptr;
        ArenaVector<RpsResourceId> m_resourceIds;
        ArenaVector<RpsNodeId>     m_cmdIds;

        enum PersistentIndexKinds
        {
            PERSISTENT_INDEX_KIND_RESOURCE_ID,
            PERSISTENT_INDEX_KIND_NODE_ID,
            PERSISTENT_INDEX_KIND_COUNT,
        };

        PersistentIdGenerator<PERSISTENT_INDEX_KIND_COUNT> m_persistentIndexGenerator;
    };

    struct CmdInfo
    {
        uint32_t nodeDeclIndex;
        union
        {
            struct
            {
                uint32_t cmdDeclIndex : 31;
                bool     bPreferAsync : 1;
            };
            uint32_t subgraphFlags;
        };
        const NodeDeclInfo* pNodeDecl;
        const Cmd*          pCmdDecl;
        Span<CmdAccessInfo> accesses;
        CmdRenderPassInfo*  pRenderPassInfo;

        static bool IsNodeDeclIdBuiltIn(RpsNodeDeclId nodeDeclId)
        {
            return int32_t(nodeDeclId) < 0;
        }

        bool IsNodeDeclBuiltIn() const
        {
            return CmdInfo::IsNodeDeclIdBuiltIn(nodeDeclIndex);
        }
    };

    struct TransitionInfo
    {
        CmdAccessInfo          access;
        NodeId                 nodeId;
        uint32_t               prevTransition;
    };

    struct RenderGraphUpdateContext
    {
        const RpsRenderGraphUpdateInfo* pUpdateInfo;
        RenderGraph&                    renderGraph;
        RuntimeDevice*                  pRuntimeDevice;
        Arena&                          frameArena;
        Arena&                          scratchArena;
    };

    static constexpr uint32_t CMD_ID_PREAMBLE  = 0x7FFFFFFE;
    static constexpr uint32_t CMD_ID_POSTAMBLE = 0x7FFFFFFF;

    struct RuntimeCmdInfo
    {
        uint32_t                   cmdId : 31;
        uint32_t                   isTransition : 1;
        Span<ResourceAliasingInfo> aliasingInfos = {};

        RuntimeCmdInfo()
            : RuntimeCmdInfo(0, false)
        {
        }

        RuntimeCmdInfo(uint32_t inCmdId, bool inIsTransition)
            : cmdId(inCmdId)
            , isTransition(inIsTransition ? 1 : 0)
        {
        }

        uint32_t GetTransitionId() const
        {
            return isTransition ? cmdId : RPS_INDEX_NONE_U32;
        }

        uint32_t GetCmdId() const
        {
            return isTransition ? RPS_INDEX_NONE_U32 : cmdId;
        }

        bool HasTransitionInfo() const
        {
            return GetTransitionId() < uint32_t(CMD_ID_PREAMBLE);
        }
    };

    struct CommandBatch : public RpsCommandBatch
    {
        CommandBatch(uint32_t inQueueIndex      = 0,
                     uint32_t inCmdBegin        = 0,
                     uint32_t inNumCmds         = 0,
                     uint32_t inWaitFencesBegin = 0,
                     uint32_t inWaitFencesCount = 0,
                     uint32_t inSignalFenceId   = RPS_INDEX_NONE_U32)
        {
            queueIndex       = inQueueIndex;
            waitFencesBegin  = inWaitFencesBegin;
            numWaitFences    = inWaitFencesCount;
            signalFenceIndex = inSignalFenceId;
            cmdBegin         = inCmdBegin;
            numCmds          = inNumCmds;
        };
    };

    struct HeapInfo
    {
        uint32_t memTypeIndex;
        uint32_t index;
        uint64_t size;
        uint32_t alignment;
        uint64_t usedSize;
        uint64_t maxUsedSize;

        RpsRuntimeHeap hRuntimeHeap;
    };

    class IRenderGraphPhase
    {
        RPS_CLASS_NO_COPY_MOVE(IRenderGraphPhase);

    protected:
        IRenderGraphPhase() = default;

    public:
        virtual ~IRenderGraphPhase() = default;

        virtual RpsResult Run(RenderGraphUpdateContext& context) = 0;

        virtual RuntimeBackend* AsRuntimeBackend()
        {
            return nullptr;
        }

        void Destroy()
        {
            OnDestroy();

            this->~IRenderGraphPhase();
        }

    protected:
        virtual void OnDestroy()
        {
        }
    };

    struct RuntimeCmd
    {
        RpsNodeId cmdId;

        RuntimeCmd(RpsNodeId cmdIdIn = RPS_CMD_ID_INVALID)
            : cmdId(cmdIdIn)
        {
        }
    };

    class RuntimeBackend : public IRenderGraphPhase
    {
    protected:
        RuntimeBackend(RenderGraph& renderGraph)
            : m_renderGraph(renderGraph)
        {
        }

    public:
        virtual ~RuntimeBackend()
        {
        }

        virtual RpsResult Run(RenderGraphUpdateContext& context) override;

        virtual RuntimeBackend* AsRuntimeBackend() override final
        {
            return this;
        }

        virtual RpsResult RecordCommands(const RenderGraph&                     renderGraph,
                                         const RpsRenderGraphRecordCommandInfo& recordInfo) const = 0;

        virtual RpsResult RecordCmdRenderPassBegin(const RuntimeCmdCallbackContext& context) const
        {
            return RPS_OK;
        }

        virtual RpsResult RecordCmdRenderPassEnd(const RuntimeCmdCallbackContext& context) const
        {
            return RPS_OK;
        }

        virtual RpsResult RecordCmdFixedFunctionBindingsAndDynamicStates(const RuntimeCmdCallbackContext& context) const
        {
            return RPS_OK;
        }

        RpsResult CloneContext(const RuntimeCmdCallbackContext& context,
                               RpsRuntimeCommandBuffer          hNewCmdBuffer,
                               const RpsCmdCallbackContext**    ppNewContext) const;

        virtual void DestroyRuntimeResourceDeferred(ResourceInstance& resource) = 0;

        RenderGraph& GetRenderGraph() const
        {
            return m_renderGraph;
        }

        static RpsResult GetCmdArgResourceInfos(const RpsCmdCallbackContext* pContext,
                                                uint32_t                     argIndex,
                                                uint32_t                     srcArrayIndex,
                                                const ResourceInstance**     ppResources,
                                                uint32_t                     count);

    protected:
        virtual RpsResult UpdateFrame(const RenderGraphUpdateContext& context)
        {
            return RPS_OK;
        }

        virtual RpsResult CreateHeaps(const RenderGraphUpdateContext& context, ArrayRef<HeapInfo> heaps)
        {
            return RPS_OK;
        }

        virtual void DestroyHeaps(ArrayRef<HeapInfo> heaps)
        {
        }

        virtual RpsResult CreateResources(const RenderGraphUpdateContext& context, ArrayRef<ResourceInstance> resources)
        {
            return RPS_OK;
        }

        virtual void DestroyResources(ArrayRef<ResourceInstance> resources)
        {
        }

        virtual RpsResult CreateCommandResources(const RenderGraphUpdateContext& context)
        {
            return RPS_OK;
        }

        virtual void DestroyCommandResources()
        {
        }

        virtual void RecordDebugMarker(const RuntimeCmdCallbackContext& context,
                                       RpsRuntimeDebugMarkerMode        mode,
                                       StrRef                           name) const;

        virtual bool ShouldResetAliasedResourcesPrevFinalAccess() const
        {
            return true;
        }

        RpsResult RecordCommand(RuntimeCmdCallbackContext& context, const RuntimeCmd& runtimeCmd) const;

    private:
        RpsResult RecordCmdBegin(const RuntimeCmdCallbackContext& context) const;
        RpsResult RecordCmdEnd(const RuntimeCmdCallbackContext& context) const;

        RpsResult UpdateResourceFinalAccessStates(RenderGraphUpdateContext&  context,
                                                  ArrayRef<ResourceInstance> resourceInstances);
    protected:
        virtual void OnDestroy() override;

        static uint64_t GetNumQueuedFrames(const RenderGraphUpdateContext& context)
        {
            // Returns the number of queued frames based on the current frame index and known completed frame index.
            // If gpuCompletedFrameIndex is RPS_GPU_COMPLETED_FRAME_INDEX_NONE (UINT64_MAX), it indicates no frames
            // are known to have completed yet and it returns the current frame index.
            return context.pUpdateInfo->frameIndex - (context.pUpdateInfo->gpuCompletedFrameIndex + 1);
        }

    private:
        RenderGraph& m_renderGraph;
    };

    inline void ResourceInstance::InvalidateRuntimeResource(RuntimeBackend* pBackend)
    {
        RPS_ASSERT(!(isPendingCreate && hRuntimeResource && (allocPlacement.heapId != RPS_INDEX_NONE_U32)));

        if (!isExternal)
        {
            if (hRuntimeResource)
            {
                pBackend->DestroyRuntimeResourceDeferred(*this);
                RPS_ASSERT(!hRuntimeResource &&
                           "Bad DestroyRuntimeResourceDeferred implementation - expect hRuntimeResource to be cleared");
            }

            if (!isPendingCreate)
            {
                allocPlacement = {RPS_INDEX_NONE_U32, 0};
                // Temporal parent don't have a runtime resource.
                // Otherwise, mark it as pending creating runtime resource.
                isPendingCreate = !IsTemporalParent();
                prevFinalAccess = AccessAttr{};
            }
        }
    }

    class NullRuntimeBackend : public RuntimeBackend
    {
    public:
        NullRuntimeBackend(RenderGraph& renderGraph)
            : RuntimeBackend(renderGraph)
        {
        }

        virtual RpsResult CreateHeaps(const RenderGraphUpdateContext& context, ArrayRef<HeapInfo> heaps) override final;

        virtual RpsResult RecordCommands(const RenderGraph&                     renderGraph,
                                         const RpsRenderGraphRecordCommandInfo& recordInfo) const override final;

        virtual void DestroyRuntimeResourceDeferred(ResourceInstance& resource) override final;

    private:
        uint64_t m_heapCounter = 0;
    };

    struct RuntimeCmdCallbackContext final : public RpsCmdCallbackContext
    {
        const RuntimeBackend*     pBackend;
        const RenderGraph*        pRenderGraph;
        const NodeDeclInfo*       pNodeDeclInfo = nullptr;
        const CmdInfo*            pCmdInfo      = nullptr;
        const Cmd*                pCmd          = nullptr;
        const RuntimeCmd*         pRuntimeCmd   = nullptr;
        uint32_t                  cmdId         = RPS_CMD_ID_INVALID;
        RpsResult                 result        = RPS_OK;
        RpsRecordCommandFlags     recordFlags;
        RpsRuntimeRenderPassFlags renderPassFlags : 30;
        bool                      bIsPrimaryContext : 1;
        bool                      bIsCmdBeginEnd : 1;

        RuntimeCmdCallbackContext(const RuntimeBackend* pBackendIn, const RpsRenderGraphRecordCommandInfo& recordInfo)
            : RpsCmdCallbackContext{recordInfo.hCmdBuffer, recordInfo.pUserContext}
            , pBackend(pBackendIn)
            , pRenderGraph(pBackendIn ? &pBackendIn->GetRenderGraph() : nullptr)
            , result(RPS_OK)
            , recordFlags(recordInfo.flags)
            , renderPassFlags(RPS_RUNTIME_RENDER_PASS_FLAG_NONE)
            , bIsPrimaryContext(true)
            , bIsCmdBeginEnd(false)
        {
            static_assert(std::is_trivially_copyable<RuntimeCmdCallbackContext>::value,
                          "RuntimeCmdCallbackContext must be trivially copyable for CloneContext to work");
        }

        static const RuntimeCmdCallbackContext* Get(const RpsCmdCallbackContext* pContext)
        {
            return static_cast<const RuntimeCmdCallbackContext*>(pContext);
        }

        // Get a modifiable pointer to RuntimeCmdCallbackContext. 
        // The contexts are created within the RPS library on stack and
        // passed to command callbacks as a const pointer to prevent user from modifying it.
        // A context is expected to be accessed single threaded.
        // In a few limited cases we need to modify it from within the record API calls:
        // (If you use this function please add to the list below)
        //      - rpsCmdCallbackReportError, to set the error status of current context.
        //      - rpsCmdBeginRenderPass, to setup flags for both Begin and End render pass operations.
        //      - rpsCmdSetCommandBuffer, to setup a new command buffer for subsequent recording.
        static RuntimeCmdCallbackContext* GetMutable(const RpsCmdCallbackContext* pContext)
        {
            return const_cast<RuntimeCmdCallbackContext*>(Get(pContext));
        }

        static const RuntimeCmdCallbackContext& Get(const RpsCmdCallbackContext& context)
        {
            return *Get(&context);
        }

        template <typename T = RuntimeBackend>
        const T* GetBackend() const
        {
            return static_cast<const T*>(pBackend);
        }

        template <typename T = void>
        const T* GetRuntimeCmd() const
        {
            return static_cast<const T*>(pRuntimeCmd);
        }
    };

    class RenderGraph
    {
    public:
        static RpsResult Create(Device&                         device,
                                const RpsRenderGraphCreateInfo* pCreateInfo,
                                RenderGraph**                   ppRenderGraph);
        void             Destroy();

        RenderGraphBuilder* BeginBuild();

        RpsResult Update(const RpsRenderGraphUpdateInfo& updateInfo);

        RpsResult RecordCommands(const RpsRenderGraphRecordCommandInfo& recordInfo) const;

        RpsResult GetDiagnosticInfo(RpsRenderGraphDiagnosticInfo& diagInfos, RpsRenderGraphDiagnosticInfoFlags diagnosticFlags);

        static constexpr uint32_t INVALID_TRANSITION = 0;

    private:
        RenderGraph(const Device& device, const RpsRenderGraphCreateInfo& createInfo);

        ~RenderGraph()
        {
        }

        RpsResult OnInit(const RpsRenderGraphCreateInfo& createInfo);

        void OnDestroy();

        RpsResult UpdateImpl(const RpsRenderGraphUpdateInfo& buildInfo);

        RpsResult UpdateDiagCache();

        RpsResult AllocateDiagnosticInfo();

        void GatherDiagnosticInfo();

        void GatherResourceDiagnosticInfo(RpsResourceDiagnosticInfo& dst,
                                          const ResourceInstance&    src,
                                          uint32_t                   resIndex);

        void GatherCmdDiagnosticInfo(RpsCmdDiagnosticInfo& dst, const rps::RuntimeCmdInfo& src, uint32_t cmdIndex);

        void GatherHeapDiagnosticInfo(RpsHeapDiagnosticInfo& dst, const rps::HeapInfo& src);

    public:

        const Device& GetDevice() const
        {
            return m_device;
        }

        const RpsRenderGraphCreateInfo& GetCreateInfo() const
        {
            return m_createInfo;
        }

        Subprogram* GetMainEntry() const
        {
            return m_pMainEntry;
        }

        ProgramInstance* GetOrCreateProgramInstance(Subprogram* pSubprogram, uint32_t& globalProgramInstanceId);

        const RenderGraphBuilder& GetBuilder() const
        {
            return m_builder;
        }

        Graph& GetGraph()
        {
            return m_graph;
        }

        const Graph& GetGraph() const
        {
            return m_graph;
        }

        const ResourceInstance& GetResourceInstance(RpsResourceId resourceId) const
        {
            return m_resourceCache[resourceId];
        }

        const ArenaVector<ResourceInstance>& GetResourceInstances() const
        {
            return m_resourceCache;
        }

        ArenaVector<ResourceInstance>& GetResourceInstances()
        {
            return m_resourceCache;
        }

        void SetResourceClearValue(ResourceInstance& resourceInfo, const RpsClearInfo& clearInfo)
        {
            RpsClearInfo* pClearInfo = nullptr;
            if (resourceInfo.clearValueId == UINT32_MAX)
            {
                resourceInfo.clearValueId = m_resourceClearValues.AllocSlot(&pClearInfo);
            }
            else
            {
                pClearInfo = m_resourceClearValues.GetSlot(resourceInfo.clearValueId);
            }

            if (pClearInfo)
            {
                *pClearInfo = clearInfo;
            }
        }

        const RpsClearInfo& GetResourceClearValue(uint32_t slot) const
        {
            return *m_resourceClearValues.GetSlot(slot);
        }

        ArenaVector<FinalAccessInfo>& GetResourceFinalAccesses()
        {
            return m_resourceFinalAccesses;
        }

        const ArenaVector<FinalAccessInfo>& GetResourceFinalAccesses() const
        {
            return m_resourceFinalAccesses;
        }

        ArenaVector<HeapInfo>& GetHeapInfos()
        {
            return m_heaps;
        }

        const ArenaVector<HeapInfo>& GetHeapInfos() const
        {
            return m_heaps;
        }

        const CmdInfo* GetCmdInfo(RpsNodeId cmdId) const
        {
            return &m_cmds[cmdId];
        }

        const ArenaVector<CmdInfo>& GetCmdInfos() const
        {
            return m_cmds;
        }

        ArenaVector<CmdInfo>& GetCmdInfos()
        {
            return m_cmds;
        }

        ArenaVector<CmdAccessInfo>& GetCmdAccessInfos()
        {
            return m_cmdAccesses;
        }

        ConstArrayRef<CmdAccessInfo> GetCmdAccessInfos() const
        {
            return m_cmdAccesses.range_all();
        }

        ConstArrayRef<CmdAccessInfo, uint32_t> GetCmdAccesses(RpsNodeId cmdId) const
        {
            return m_cmds[cmdId].accesses.Get(m_cmdAccesses);
        }

        RpsResult GetCmdRenderTargetInfo(RpsNodeId cmdId, RpsCmdRenderTargetInfo& renderTargetInfo) const;

        RpsResult GetCmdViewportInfo(RpsNodeId, RpsCmdViewportInfo& viewportInfo) const;

        const RenderGraphSignature& GetSignature() const
        {
            return *m_pSignature;
        }

        ArenaVector<RuntimeCmdInfo>& GetRuntimeCmdInfos()
        {
            return m_runtimeCmdInfos;
        }

        ConstArrayRef<RuntimeCmdInfo> GetRuntimeCmdInfos() const
        {
            return m_runtimeCmdInfos.range_all();
        }

        RpsResult GetRuntimeResourceInfo(RpsResourceId           resourceId,
                                         uint32_t                temporalLayerIndex,
                                         RpsRuntimeResourceInfo* pResourceInfo) const;

        RpsResult GetOutputParameterRuntimeResourceInfos(RpsParamId              paramId,
                                                         uint32_t                arrayOffset,
                                                         uint32_t                resourceCount,
                                                         RpsRuntimeResourceInfo* pResourceInfos) const;

        const TransitionInfo& GetTransitionInfo(uint32_t transitionId) const
        {
            return m_transitions[transitionId];
        }

        const ArenaVector<TransitionInfo>& GetTransitions() const
        {
            return m_transitions;
        }

        ArenaVector<TransitionInfo>& GetTransitions()
        {
            return m_transitions;
        }

        ConstArrayRef<RpsMemoryTypeInfo> GetMemoryTypes() const
        {
            return m_memoryTypes;
        }

        ArenaVector<ResourceAliasingInfo>& GetResourceAliasingInfos()
        {
            return m_aliasingInfos;
        }

        const ArenaVector<ResourceAliasingInfo>& GetResourceAliasingInfos() const
        {
            return m_aliasingInfos;
        }

        ArenaVector<RpsCommandBatch>& GetCmdBatches()
        {
            return m_cmdBatches;
        }

        ArenaVector<uint32_t>& GetCmdBatchWaitFenceIds()
        {
            return m_cmdBatchWaitFenceIds;
        }

        RpsResult GetBatchLayout(RpsRenderGraphBatchLayout& batchLayout) const
        {
            batchLayout.numFenceSignals   = uint32_t(m_cmdBatchWaitFenceIds.size());
            batchLayout.numCmdBatches     = uint32_t(m_cmdBatches.size());
            batchLayout.pCmdBatches       = m_cmdBatches.empty() ? nullptr : m_cmdBatches.data();
            batchLayout.pWaitFenceIndices = m_cmdBatchWaitFenceIds.data();

            return RPS_OK;
        }

        RpsResult ReservePhases(uint32_t numPhases)
        {
            return m_phases.reserve(numPhases) ? RPS_OK : RPS_ERROR_OUT_OF_MEMORY;
        }

        template <typename T, typename... TArgs>
        RpsResult AddPhase(TArgs&&... args)
        {
            T* pPhase = m_persistentArena.New<T>(args...);
            RPS_CHECK_ALLOC(pPhase);

            return AddPhase(pPhase);
        }

        RpsResult AddPhase(IRenderGraphPhase* pPhase)
        {
            if (m_phases.capacity() == m_phases.size())
            {
                RPS_DIAG_LOG(RPS_DIAG_WARNING,
                             "RenderGraph::AddPhase:",
                             "Capacity reservation (%zu) needs to be increased.",
                             m_phases.capacity());
            }

            RPS_RETURN_ERROR_IF(!m_phases.push_back(pPhase), RPS_ERROR_OUT_OF_MEMORY);

            if (m_pBackend == nullptr)
            {
                m_pBackend = pPhase->AsRuntimeBackend();
            }

            return RPS_OK;
        }

        RuntimeBackend* GetRuntimeBackend() const
        {
            return m_pBackend;
        }

        template<typename T>
        T* FrameAlloc()
        {
            return static_cast<T*>(m_frameArena.AlignedAlloc(sizeof(T), alignof(T)));
        }

        static AccessAttr CalcPreviousAccess(uint32_t                      prevTransitionId,
                                             ConstArrayRef<TransitionInfo> transitions,
                                             const ResourceInstance&       resInstance)
        {
            return (prevTransitionId != RenderGraph::INVALID_TRANSITION) ? transitions[prevTransitionId].access.access
                                                                         : resInstance.prevFinalAccess;
        }

    public:
        void PrintCmdNodeName(PrinterRef printer, NodeId id) const
        {
            RPS_ASSERT(id < m_cmds.size());
            RPS_ASSERT(m_graph.GetNodes().empty() || (id == m_graph.GetNode(id)->GetCmdId()));

            auto pNodeDecl = m_cmds[id].pNodeDecl;
            if (pNodeDecl)
            {
                printer("%*s_%d", pNodeDecl->name.len, pNodeDecl->name.str, id);
            }
            else
            {
                printer("n_%d", id);
            }
        }

        void PrintTransitionNodeName(PrinterRef printer, NodeId id) const
        {
            RPS_ASSERT(id >= m_cmds.size());

            printer("t_%d", m_graph.GetNode(id)->GetTransitionId());
        }

    private:
        const Device&            m_device;
        RpsRenderGraphCreateInfo m_createInfo;
        Arena                    m_persistentArena;
        Arena                    m_frameArena;
        Arena                    m_scratchArena;
        Graph                    m_graph;
        RpsResult                m_status = RPS_OK;

        const RenderGraphSignature* m_pSignature = nullptr;
        Subprogram*                 m_pMainEntry = nullptr;

        ConstArrayRef<RpsMemoryTypeInfo> m_memoryTypes;

        ArenaVector<IRenderGraphPhase*> m_phases;
        ArenaVector<ResourceInstance>   m_resourceCache;
        ArenaVector<ProgramInstance*>   m_programInstances;
        ArenaVector<CmdInfo>            m_cmds;
        ArenaVector<CmdAccessInfo>      m_cmdAccesses;
        ArenaVector<TransitionInfo>     m_transitions;
        ArenaVector<FinalAccessInfo>    m_resourceFinalAccesses;

        RuntimeBackend* m_pBackend = nullptr;

        ArenaVector<RuntimeCmdInfo>       m_runtimeCmdInfos;
        ArenaVector<RpsCommandBatch>      m_cmdBatches;
        ArenaVector<uint32_t>             m_cmdBatchWaitFenceIds;
        ArenaVector<ResourceAliasingInfo> m_aliasingInfos;
        ArenaVector<HeapInfo>             m_heaps;

        ArenaFreeListPool<RpsClearInfo> m_resourceClearValues;

        RenderGraphBuilder m_builder;

        //Diagnostics cache
        struct
        {
            ArenaVector<RpsResourceDiagnosticInfo> resourceInfos;
            ArenaVector<RpsCmdDiagnosticInfo>      cmdInfos;
            ArenaVector<RpsHeapDiagnosticInfo>     heapInfos;
        } m_diagData;

        Arena m_diagInfoArena;

        friend class RenderGraphBuilder;
    };

    RPS_ASSOCIATE_HANDLE(RenderGraph);

    class RenderGraphPhaseWrapper final : public IRenderGraphPhase
    {
    public:
        RenderGraphPhaseWrapper(const RpsRenderGraphPhaseInfo& phaseInfo)
            : m_phaseInfo(phaseInfo)
        {
        }

        virtual RpsResult Run(RenderGraphUpdateContext& context) override final
        {
            return m_phaseInfo.pfnRun(ToHandle(&context.renderGraph), context.pUpdateInfo, m_phaseInfo.hPhase);
        }

        virtual ~RenderGraphPhaseWrapper()
        {
            m_phaseInfo.pfnDestroy(m_phaseInfo.hPhase);
        }

    private:
        RpsRenderGraphPhaseInfo m_phaseInfo;
    };

}  // namespace rps

RPS_IMPL_OPAQUE_HANDLE(NullRuntimeHeap, RpsRuntimeHeap, void);

#endif  //RPS_RENDER_GRAPH_HPP
