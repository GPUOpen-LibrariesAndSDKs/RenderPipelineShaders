// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_RENDER_GRAPH_SIGNATURE_HPP
#define RPS_RENDER_GRAPH_SIGNATURE_HPP



#include "core/rps_core.hpp"
#include "core/rps_util.hpp"
#include "runtime/common/rps_cmd_buf.hpp"

#include "rps/runtime/common/rps_runtime.h"

namespace rps
{
    static constexpr bool IsFixedFunctionResourceBindingSemantic(RpsSemantic semantic)
    {
        return (semantic >= RPS_SEMANTIC_RESOURCE_BINDING_BEGIN) && (semantic < RPS_SEMANTIC_USER_RESOURCE_BINDING);
    }

    static constexpr bool IsDynamicRenderStateSemantic(RpsSemantic semantic)
    {
        return (semantic >= RPS_SEMANTIC_DYNAMIC_STATE_BEGIN) && (semantic < RPS_SEMANTIC_RESOURCE_BINDING_BEGIN);
    }

    static inline RpsAccessAttr GetAccessAttrFromParamAttrList(RpsConstant attrListConst)
    {
        if (!attrListConst)
        {
            return {};
        }

        const ParamAttrList& attrList = *static_cast<const ParamAttrList*>(attrListConst);

        RpsAccessAttr accessAttr = attrList.access;

        // Infer access from semantics:
        if (IsFixedFunctionResourceBindingSemantic(attrList.semantic.semantic))
        {
            static const RpsAccessFlags c_semanticToAccessMap[] = {
                RPS_ACCESS_VERTEX_BUFFER_BIT,                                // RPS_SEMANTIC_VERTEX_BUFFER
                RPS_ACCESS_INDEX_BUFFER_BIT,                                 // RPS_SEMANTIC_INDEX_BUFFER
                RPS_ACCESS_INDIRECT_ARGS_BIT,                                // RPS_SEMANTIC_INDIRECT_ARGS
                RPS_ACCESS_INDIRECT_ARGS_BIT,                                // RPS_SEMANTIC_INDIRECT_COUNT
                RPS_ACCESS_STREAM_OUT_BIT,                                   // RPS_SEMANTIC_STREAM_OUT_BUFFER
                RPS_ACCESS_RENDER_TARGET_BIT,                                // RPS_SEMANTIC_RENDER_TARGET
                RPS_ACCESS_DEPTH_WRITE_BIT | RPS_ACCESS_STENCIL_WRITE_BIT,   // RPS_SEMANTIC_DEPTH_STENCIL_TARGET
                RPS_ACCESS_SHADING_RATE_BIT,                                 // RPS_SEMANTIC_SHADING_RATE_IMAGE
                RPS_ACCESS_RENDER_TARGET_BIT | RPS_ACCESS_RESOLVE_DEST_BIT,  // RPS_SEMANTIC_RESOLVE_TARGET
            };


            RpsAccessFlags assumedAccessFlagsFromSemantic =
                c_semanticToAccessMap[attrList.semantic.semantic - RPS_SEMANTIC_RESOURCE_BINDING_BEGIN];

            // Avoid adding additional write access if readonly access is explicitly specified for depth/stencil:
            // If no access attributes are explicitly specified, RPS_SEMANTIC_DEPTH_STENCIL_TARGET / SV_DepthStencil
            // semantic will implicitly set (RPS_ACCESS_DEPTH_WRITE_BIT | RPS_ACCESS_STENCIL_WRITE_BIT) access.
            // But if user explicitly specified any readonly access, we need to avoid adding the WRITE access bit
            // to the same image aspect.
            if (accessAttr.accessFlags & RPS_ACCESS_DEPTH_READ_BIT)
                assumedAccessFlagsFromSemantic &= ~RPS_ACCESS_DEPTH_WRITE_BIT;

            if (accessAttr.accessFlags & RPS_ACCESS_STENCIL_READ_BIT)
                assumedAccessFlagsFromSemantic &= ~RPS_ACCESS_STENCIL_WRITE_BIT;

            accessAttr.accessFlags |= assumedAccessFlagsFromSemantic;
                
            accessAttr.accessStages = RPS_SHADER_STAGE_NONE;
        }
        else if (attrList.semantic.semantic >= RPS_SEMANTIC_USER_RESOURCE_BINDING)
        {
            RPS_TODO("Translate bindings to access flags");
        }

        return accessAttr;
    }

    static inline RpsSemanticAttr GetSemanticAttrFromParamAttrList(RpsConstant attrListConst)
    {
        if (!attrListConst)
        {
            return {};
        }

        return static_cast<const ParamAttrList*>(attrListConst)->semantic;
    }

    static inline RpsNodeDeclFlags GetRequiredQueueFlagsFromAccessAttr(const RpsNodeDeclFlags nodeDeclFlag,
                                                                       const RpsAccessAttr&   access)
    {
        static constexpr RpsAccessFlags GfxComputeOnlyAccessMask =
            (RPS_ACCESS_ALL_GPU & ~(RPS_ACCESS_COPY_SRC_BIT | RPS_ACCESS_COPY_DEST_BIT));

        static constexpr RpsAccessFlags GfxOnlyAccessMask =
            (RPS_ACCESS_ALL_GPU & ~(RPS_ACCESS_INDIRECT_ARGS_BIT | RPS_ACCESS_CONSTANT_BUFFER_BIT |
                                    RPS_ACCESS_RAYTRACING_AS_BUILD_BIT | RPS_ACCESS_RAYTRACING_AS_READ_BIT));

        static constexpr RpsShaderStageFlags GfxShaderStages = (RPS_SHADER_STAGE_ALL & (~RPS_SHADER_STAGE_CS));

        static constexpr RpsAccessFlags ShaderStageDependentAccessMask =
            RPS_ACCESS_SHADER_RESOURCE_BIT | RPS_ACCESS_UNORDERED_ACCESS_BIT;

        if ((access.accessFlags & RPS_ACCESS_ALL_GPU) == 0)
        {
            return RPS_NODE_DECL_FLAG_NONE;
        }

        if ((access.accessFlags & GfxComputeOnlyAccessMask) == 0)
        {
            return RPS_NODE_DECL_COPY_BIT;
        }

        // For SRV | UAV access, the queue requirements depend on the shader stages.
        // Also if user specified a compute node type but included GFX shader stages in the shader stage mask,
        // we still treat it as compute node. This is because legacy "uav" access includes both CS & PS stages.
        if (((access.accessFlags & GfxOnlyAccessMask) == 0) ||
            ((access.accessFlags & ShaderStageDependentAccessMask) &&
             (!(access.accessStages & GfxShaderStages) || (nodeDeclFlag & RPS_NODE_DECL_COMPUTE_BIT))))
        {
            return RPS_NODE_DECL_COMPUTE_BIT;
        }

        return RPS_NODE_DECL_GRAPHICS_BIT;
    }

    struct ParamDecl
    {
        StrRef            name        = {};
        RpsTypeInfo       typeInfo    = {};
        uint32_t          numElements = {};
        RpsParameterFlags flags : 30;
        uint32_t          isArray : 1;
        uint32_t          isUnboundedArray : 1;
        RpsAccessAttr     access           = {};

        ParamDecl()
            : flags(RPS_PARAMETER_FLAG_NONE)
            , isArray(RPS_FALSE)
            , isUnboundedArray(RPS_FALSE)
        {
        }

        ParamDecl(Arena& allocator, const RpsParameterDesc& desc)
            : name(allocator.StoreCStr(desc.name))
            , typeInfo(desc.typeInfo)
            , numElements((desc.arraySize == UINT32_MAX) ? 0 : ((desc.arraySize == 0) ? 1 : desc.arraySize))
            , flags(desc.flags)
            , isArray(desc.arraySize != 0)
            , isUnboundedArray(desc.arraySize == UINT32_MAX)
            , access(GetAccessAttrFromParamAttrList(desc.attr))
        {
        }

        bool IsOptional() const
        {
            return rpsAnyBitsSet(flags, RPS_PARAMETER_FLAG_OPTIONAL_BIT);
        }

        bool IsResource() const
        {
            return rpsAnyBitsSet(flags, RPS_PARAMETER_FLAG_RESOURCE_BIT);
        }

        bool IsOutputResource() const
        {
            return rpsAllBitsSet(flags, RPS_PARAMETER_FLAG_RESOURCE_BIT | RPS_PARAMETER_FLAG_OUT_BIT);
        }

        bool IsArray() const
        {
            return isArray;
        }

        bool IsUnboundedArray() const
        {
            return isUnboundedArray;
        }

        uint32_t GetNumElements() const
        {
            return numElements;
        }

        size_t GetElementSize() const
        {
            return typeInfo.size;
        }

        size_t GetSize() const
        {
            return numElements * GetElementSize();
        }

        void GetDesc(RpsParameterDesc* pDesc) const
        {
            pDesc->typeInfo  = typeInfo;
            pDesc->arraySize = IsUnboundedArray() ? UINT32_MAX : (IsArray() ? GetNumElements() : 0);
            pDesc->attr      = nullptr;
            pDesc->name      = name.str;  // TODO - Make sure this is null terminated
            pDesc->flags     = flags;
        }
    };

    struct NodeParamDecl : public ParamDecl
    {
        RpsSemantic   semantic          = {};
        uint32_t      baseSemanticIndex = 0;
        uint32_t      accessOffset      = 0;

        NodeParamDecl()
        {
        }

        NodeParamDecl(Arena& allocator, const RpsParameterDesc& desc, uint32_t* pNumAccessesInNode)
            : ParamDecl(allocator, desc)
        {
            if (desc.attr)
            {
                const RpsAccessAttr   accessAttr   = GetAccessAttrFromParamAttrList(desc.attr);
                const RpsSemanticAttr semanticAttr = GetSemanticAttrFromParamAttrList(desc.attr);

                access            = accessAttr;
                semantic          = semanticAttr.semantic;
                accessOffset      = *pNumAccessesInNode;
                baseSemanticIndex = semanticAttr.semanticIndex;

                if (accessAttr.accessFlags != RPS_ACCESS_UNKNOWN)
                {
                    *pNumAccessesInNode += GetNumElements();
                }
            }
        }
    };

    struct ParamSemanticsKindInfo
    {
        RpsSemantic      semantic;
        Span<RpsParamId> params;
    };

    struct ParamElementRef
    {
        RpsParamId paramId;
        uint32_t   arrayOffset;
    };

    struct NodeDeclRenderPassInfo
    {
        ParamElementRef* paramRefs;
        uint32_t         numParamRefs : 8;
        uint32_t         renderTargetsMask : 8;
        uint32_t         resolveTargetsMask : 8;
        uint32_t         renderTargetClearMask : 8;
        uint32_t         depthStencilTargetMask : 1;
        uint32_t         clearDepth : 1;
        uint32_t         clearStencil : 1;
        uint32_t         resolveDepthStencil : 1;
        uint32_t         resolveTargetRefs : 4;
        uint32_t         clearOnly : 1;
        uint32_t         clearValueRefs : 7;
        uint32_t         viewportRefs : 8;
        uint32_t         scissorRectRefs : 8;

        uint32_t GetRenderTargetsCount() const
        {
            return rpsCountBits(renderTargetsMask);
        }

        uint32_t GetRenderTargetsSlotCount() const
        {
            return 32 - rpsFirstBitHigh(renderTargetsMask);
        }

        uint32_t GetResolveTargetCount() const
        {
            return rpsCountBits(resolveTargetsMask);
        }

        ArrayRef<ParamElementRef> GetRenderTargetRefs() const
        {
            return {paramRefs, GetRenderTargetsCount()};
        }

        ParamElementRef* GetDepthStencilRef() const
        {
            return depthStencilTargetMask ? paramRefs + GetRenderTargetsCount() : nullptr;
        }

        ArrayRef<ParamElementRef> GetResolveTargetRefs() const
        {
            return {paramRefs + resolveTargetRefs, GetResolveTargetCount()};
        }

        uint32_t GetRenderTargetClearCount() const
        {
            return rpsCountBits(renderTargetClearMask);
        }

        ArrayRef<ParamElementRef> GetRenderTargetClearValueRefs() const
        {
            return {paramRefs + clearValueRefs, GetRenderTargetClearCount()};
        }

        ParamElementRef* GetDepthClearValueRef() const
        {
            return clearDepth ? (paramRefs + clearValueRefs + GetRenderTargetClearCount()) : nullptr;
        }

        ParamElementRef* GetStencilClearValueRef() const
        {
            return clearStencil ? (paramRefs + clearValueRefs + GetRenderTargetClearCount() + (clearDepth ? 1 : 0))
                                : nullptr;
        }

        ArrayRef<ParamElementRef> GetViewportRefs() const
        {
            return {paramRefs + viewportRefs, size_t(scissorRectRefs - viewportRefs)};
        }

        ArrayRef<ParamElementRef> GetScissorRefs() const
        {
            return {paramRefs + scissorRectRefs, size_t(clearValueRefs - scissorRectRefs)};
        }
    };

    struct NodeDeclInfo
    {
        StrRef                                     name;
        ArrayRef<NodeParamDecl>                    params;
        RpsNodeDeclFlags                           flags;
        ArrayRef<ParamSemanticsKindInfo, uint32_t> semanticKinds;
        ArrayRef<RpsParamId, uint32_t>             semanticParamTable;
        Span<ParamSemanticsKindInfo>               dynamicStates;
        Span<ParamSemanticsKindInfo>               fixedFunctionBindings;
        uint32_t                                   numAccesses;
        NodeDeclRenderPassInfo*                    pRenderPassInfo;

        bool MaybeGraphicsNode() const
        {
            // Default to gfx node if no queue-flags set.
            return !((flags & RPS_NODE_DECL_COMPUTE_BIT) || (flags & RPS_NODE_DECL_COPY_BIT));
        }
    };

    class RenderGraphSignature
    {
        RPS_CLASS_NO_COPY_MOVE(RenderGraphSignature);

        RenderGraphSignature(Arena& allocator)
            : m_allocator(allocator)
        {
        }

    public:
        static RpsResult Create(Arena&                             allocator,
                                const RpsRenderGraphSignatureDesc* pSignatureDesc,
                                RenderGraphSignature**             ppSignature)
        {
            RPS_CHECK_ARGS(pSignatureDesc);
            RPS_CHECK_ARGS(ppSignature);

            void* pMem = allocator.Alloc<RenderGraphSignature>();

            RenderGraphSignature* pSignature = new (pMem) RenderGraphSignature(allocator);

            RPS_CHECK_ALLOC(pSignature);

            RpsResult result = pSignature->Init(*pSignatureDesc);

            if (RPS_SUCCEEDED(result))
            {
                *ppSignature = pSignature;
            }
            else
            {
                pSignature->Destroy();
            }

            return result;
        }

        void Destroy()
        {
            // Nothing to do atm.
            this->~RenderGraphSignature();
        }

        ConstArrayRef<NodeDeclInfo> GetNodeDecls() const
        {
            return m_nodeDecls;
        }

        const NodeDeclInfo* GetNodeDecl(RpsNodeDeclId nodeDeclId) const
        {
            return (nodeDeclId < m_nodeDecls.size()) ? &m_nodeDecls[nodeDeclId] : nullptr;
        }

        uint32_t FindNodeDeclIndexByName(const StrRef name) const
        {
            auto iter = std::find_if(
                m_nodeDecls.begin(), m_nodeDecls.end(), [&](const auto& nodeDecl) { return (nodeDecl.name == name); });
            return (iter != m_nodeDecls.end()) ? uint32_t(iter - m_nodeDecls.begin()) : RPS_INDEX_NONE_U32;
        }

        ConstArrayRef<ParamDecl> GetParamDecls() const
        {
            return m_paramDecls;
        }

        const ParamDecl& GetParamDecl(RpsParamId paramId) const
        {
            return m_paramDecls[paramId];
        }

        uint32_t GetMaxExternalResourceCount() const
        {
            return m_maxExternalResources;
        }

        // TODO: Remove when external resource access is provided by caller
        RpsParamId GetResourceParamId(RpsResourceId resourceId) const
        {
            return (resourceId < m_externalResourceParamIds.size()) ? m_externalResourceParamIds[resourceId]
                                                                    : RPS_PARAM_ID_INVALID;
        }

        static RpsResult InitNodeDecl(Arena& allocator, const RpsNodeDesc& nodeDesc, NodeDeclInfo& nodeDecl)
        {
            auto rpsAllocator = allocator.AsRpsAllocator();

            SortedParamSemanticList semanticList{&rpsAllocator};

            return InitNodeDecl(allocator, nodeDesc, nodeDecl, semanticList);
        }

    private:
        RpsResult Init(const RpsRenderGraphSignatureDesc& signatureDesc)
        {
            RPS_V_RETURN(InitParams(signatureDesc));
            RPS_V_RETURN(InitNodeDeclInfos(signatureDesc));

            return RPS_OK;
        }

        RpsResult InitParams(const RpsRenderGraphSignatureDesc& signatureDesc)
        {
            if (signatureDesc.numParams)
            {
                m_paramDecls = m_allocator.NewArray<ParamDecl>(
                    [&](size_t idx, ParamDecl* ptr) {
                        new (ptr) ParamDecl(m_allocator, signatureDesc.pParamDescs[idx]);
                    },
                    signatureDesc.numParams);

                m_maxExternalResources = signatureDesc.maxExternalResources;

                if ((signatureDesc.maxExternalResources == 0) || 
                    (signatureDesc.maxExternalResources == UINT32_MAX))
                {
                    uint32_t resCount = 0;
                    for (uint32_t iParam = 0; iParam < signatureDesc.numParams; iParam++)
                    {
                        const auto& paramDecl = m_paramDecls[iParam];

                        const uint32_t currResCount = paramDecl.IsResource() ? paramDecl.GetNumElements() : 0;

                        resCount += currResCount;
                    }

                    m_maxExternalResources = resCount;
                }

                if (m_maxExternalResources != 0)
                {
                    m_externalResourceParamIds = m_allocator.NewArray<RpsParamId>(m_maxExternalResources);

                    uint32_t resCount = 0;
                    for (uint32_t iParam = 0; iParam < signatureDesc.numParams; iParam++)
                    {
                        const auto& paramDecl = m_paramDecls[iParam];

                        const uint32_t currResCount = paramDecl.IsResource() ? paramDecl.GetNumElements() : 0;

                        std::fill(m_externalResourceParamIds.begin() + resCount,
                                  m_externalResourceParamIds.begin() + resCount + currResCount,
                                  iParam);

                        resCount += currResCount;
                    }

                    RPS_ASSERT(m_maxExternalResources == resCount);
                }
            }

            return RPS_OK;
        }

        struct ParamSemanticKey
        {
            uint32_t    paramIndex;
            RpsSemantic semantic;
            uint32_t    semanticIndex;
        };

        using SortedParamSemanticList = InplaceVector<ParamSemanticKey, 32>;

        RpsResult InitNodeDeclInfos(const RpsRenderGraphSignatureDesc& signatureDesc)
        {
            // TODO: For RPSL path, most of the below logic can be done in offline compiler.

            auto nodeDescs = ConstArrayRef<RpsNodeDesc>{signatureDesc.pNodeDescs, signatureDesc.numNodeDescs};
            m_nodeDecls    = m_allocator.NewArray<NodeDeclInfo>(nodeDescs.size());

            auto rpsAllocator = m_allocator.AsRpsAllocator();

            SortedParamSemanticList sortedSemantics(&rpsAllocator);

            for (uint32_t iNodeDecl = 0; iNodeDecl < nodeDescs.size(); iNodeDecl++)
            {
                auto& nodeDesc = nodeDescs[iNodeDecl];
                auto& nodeDecl = m_nodeDecls[iNodeDecl];

                RPS_V_RETURN(InitNodeDecl(m_allocator, nodeDesc, nodeDecl, sortedSemantics));
            }

            return RPS_OK;
        }

        static inline RpsNodeDeclFlags CalcNodeDeclFlags(RpsNodeDeclFlags inFlags, RpsNodeDeclFlags requiredQueueFlags)
        {
            static constexpr uint32_t AllNodeQueueTypeMask =
                RPS_NODE_DECL_GRAPHICS_BIT | RPS_NODE_DECL_COMPUTE_BIT | RPS_NODE_DECL_COPY_BIT;

            const RpsNodeDeclFlags combinedQueueFlags = (inFlags | requiredQueueFlags) & AllNodeQueueTypeMask;

            RpsNodeDeclFlags maxRequiredQueueFlag = RPS_NODE_DECL_FLAG_NONE;

            if (combinedQueueFlags & RPS_NODE_DECL_GRAPHICS_BIT)
                maxRequiredQueueFlag = RPS_NODE_DECL_GRAPHICS_BIT;
            else if (combinedQueueFlags & RPS_NODE_DECL_COMPUTE_BIT)
                maxRequiredQueueFlag = RPS_NODE_DECL_COMPUTE_BIT;
            else if (combinedQueueFlags & RPS_NODE_DECL_COPY_BIT)
                maxRequiredQueueFlag = RPS_NODE_DECL_COPY_BIT;

            return (inFlags & (~AllNodeQueueTypeMask)) | maxRequiredQueueFlag;
        }

        static RpsResult InitNodeDecl(Arena&                   allocator,
                                      const RpsNodeDesc&       nodeDesc,
                                      NodeDeclInfo&            nodeDecl,
                                      SortedParamSemanticList& sortedSemantics)
        {
            sortedSemantics.clear();

            nodeDecl = {};

            uint32_t numAccessesInNode = 0;

            RpsNodeDeclFlags requiredQueueFlags = RPS_NODE_DECL_FLAG_NONE;

            nodeDecl.params = allocator.NewArray<NodeParamDecl>(
                [&](size_t idx, NodeParamDecl* ptr) {
                    new (ptr) NodeParamDecl(allocator, nodeDesc.pParamDescs[idx], &numAccessesInNode);

                    if (ptr->semantic != RPS_SEMANTIC_UNSPECIFIED)
                    {
                        sortedSemantics.push_back(
                            ParamSemanticKey{uint32_t(idx), ptr->semantic, ptr->baseSemanticIndex});
                    }

                    requiredQueueFlags |= GetRequiredQueueFlagsFromAccessAttr(nodeDesc.flags, ptr->access);
                },
                nodeDesc.numParams);

            nodeDecl.name        = allocator.StoreCStr(nodeDesc.name);
            nodeDecl.flags       = CalcNodeDeclFlags(nodeDesc.flags, requiredQueueFlags);
            nodeDecl.numAccesses = numAccessesInNode;

            const bool bMaybeGraphicsNode = nodeDecl.MaybeGraphicsNode();

            uint32_t renderTargetsInfoOffset = RPS_INDEX_NONE_U32;
            bool     bHasRenderPassInfo      = false;

            if (!sortedSemantics.empty())
            {
                // First sort by semantic and param index
                std::sort(sortedSemantics.begin(), sortedSemantics.end(), [&](const auto& lhs, const auto& rhs) {
                    return (lhs.semantic == rhs.semantic) ? (lhs.paramIndex < rhs.paramIndex)
                                                          : (lhs.semantic < rhs.semantic);
                });

                // Handle RPS_SEMANTIC_INDEX_APPEND.
                // TODO: Skip this for RPSL path.
                uint32_t    numSemanticKinds  = 0;
                RpsSemantic prevSemantic      = RPS_SEMANTIC_UNSPECIFIED;
                uint32_t    nextSemanticIndex = 0;

                for (auto iter = sortedSemantics.begin(), end = sortedSemantics.end(); iter != end; ++iter)
                {
                    if (iter->semantic != prevSemantic)
                    {
                        prevSemantic      = iter->semantic;
                        nextSemanticIndex = 0;
                        numSemanticKinds++;
                    }

                    if (iter->semanticIndex == RPS_SEMANTIC_INDEX_APPEND)
                    {
                        iter->semanticIndex = nextSemanticIndex;
                    }

                    nextSemanticIndex = iter->semanticIndex + nodeDecl.params[iter->paramIndex].GetNumElements();
                }

                // Sort again by semantic and semantic index for backend consumption.
                std::sort(sortedSemantics.begin(), sortedSemantics.end(), [&](const auto& lhs, const auto& rhs) {
                    return (lhs.semantic == rhs.semantic) ? (lhs.semanticIndex < rhs.semanticIndex)
                                                          : (lhs.semantic < rhs.semantic);
                });

                // TODO: Using scratch arena for now.
                nodeDecl.semanticKinds      = allocator.NewArray<ParamSemanticsKindInfo>(numSemanticKinds);
                nodeDecl.semanticParamTable = allocator.NewArray<RpsParamId>(sortedSemantics.size());

                numSemanticKinds                 = 0;
                uint32_t semanticParamRangeBegin = 0;

                for (uint32_t i = 0, numSemantics = uint32_t(sortedSemantics.size()); i < numSemantics; i++)
                {
                    const auto& currParam = sortedSemantics[i];

                    if (((i + 1) == numSemantics) || (sortedSemantics[i + 1].semantic != currParam.semantic))
                    {
                        auto& currSemanticKind = nodeDecl.semanticKinds[numSemanticKinds];

                        currSemanticKind.semantic = currParam.semantic;
                        currSemanticKind.params.SetRange(semanticParamRangeBegin, 1 + i - semanticParamRangeBegin);

                        if (bMaybeGraphicsNode)
                        {
                            if (IsDynamicRenderStateSemantic(currParam.semantic))
                            {
                                if (nodeDecl.dynamicStates.empty())
                                    nodeDecl.dynamicStates.SetRange(numSemanticKinds, 1);
                                else
                                    nodeDecl.dynamicStates.SetEnd(numSemanticKinds + 1);

                                constexpr uint32_t renderPassInfoStateMask =
                                    (1u << RPS_SEMANTIC_VIEWPORT) | (1u << RPS_SEMANTIC_SCISSOR) |
                                    (1u << RPS_SEMANTIC_COLOR_CLEAR_VALUE) | (1u << RPS_SEMANTIC_DEPTH_CLEAR_VALUE) |
                                    (1u << RPS_SEMANTIC_STENCIL_CLEAR_VALUE);

                                if ((1u << currParam.semantic) & renderPassInfoStateMask)
                                {
                                    bHasRenderPassInfo = true;
                                }
                            }

                            if (IsFixedFunctionResourceBindingSemantic(currParam.semantic))
                            {
                                if (nodeDecl.fixedFunctionBindings.empty())
                                    nodeDecl.fixedFunctionBindings.SetRange(numSemanticKinds, 1);
                                else
                                    nodeDecl.fixedFunctionBindings.SetEnd(numSemanticKinds + 1);

                                if ((renderTargetsInfoOffset == RPS_INDEX_NONE_U32) &&
                                    ((currParam.semantic == RPS_SEMANTIC_RENDER_TARGET) ||
                                     (currParam.semantic == RPS_SEMANTIC_DEPTH_STENCIL_TARGET) ||
                                     (currParam.semantic == RPS_SEMANTIC_RESOLVE_TARGET)))
                                {
                                    renderTargetsInfoOffset = numSemanticKinds;
                                    bHasRenderPassInfo      = true;
                                }
                            }
                        }

                        numSemanticKinds++;
                        semanticParamRangeBegin = i + 1;
                    }
                    else if ((currParam.semanticIndex + nodeDecl.params[currParam.paramIndex].GetNumElements()) >
                             sortedSemantics[i + 1].semanticIndex)
                    {
                        RPS_DIAG(RPS_DIAG_ERROR, "Semantic index range overlap");
                        return RPS_ERROR_INVALID_PROGRAM;
                    }

                    nodeDecl.semanticParamTable[i] = currParam.paramIndex;
                }

                RPS_ASSERT(numSemanticKinds == nodeDecl.semanticKinds.size());
            }

            if (bHasRenderPassInfo)
            {
                NodeDeclRenderPassInfo* pRpInfo = {};
                RPS_V_RETURN(GatherNodeRenderPassInfo(allocator, nodeDecl, renderTargetsInfoOffset, &pRpInfo));

                nodeDecl.pRenderPassInfo = pRpInfo;
            }

            return RPS_OK;
        }

        static inline RpsResult GatherNodeRenderPassInfo(Arena&                   allocator,
                                                         const NodeDeclInfo&      nodeDeclInfo,
                                                         uint32_t                 renderTargetsInfoOffset,
                                                         NodeDeclRenderPassInfo** ppOutRenderPassInfo)
        {
            NodeDeclRenderPassInfo rpInfo = {};
            ParamElementRef        paramRefs[128];
            uint32_t               paramRefOffset = 0;

            auto appendParamRef = [&](RpsParamId paramId, uint32_t elementIndex) {
                RPS_RETURN_ERROR_IF(paramRefOffset >= RPS_COUNTOF(paramRefs), RPS_ERROR_INDEX_OUT_OF_BOUNDS);
                paramRefs[paramRefOffset] = {paramId, elementIndex};
                paramRefOffset++;
                return RPS_OK;
            };

            // Gather RT/DS/Resolve targets
            if (renderTargetsInfoOffset == RPS_INDEX_NONE_U32)
            {
                // Special case for clear-only nodes (No SV_Target... semantics, only access flags)

                for (uint32_t paramId = 0, numParams = uint32_t(nodeDeclInfo.params.size()); paramId < numParams;
                     paramId++)
                {
                    const auto& paramAccess = nodeDeclInfo.params[paramId];

                    if ((paramAccess.access.accessFlags & RPS_ACCESS_CLEAR_BIT) &&
                        (paramAccess.access.accessFlags &
                         (RPS_ACCESS_RENDER_TARGET_BIT | RPS_ACCESS_DEPTH_STENCIL_WRITE)) &&
                        (paramAccess.baseSemanticIndex == 0) && (paramAccess.GetNumElements() == 1))
                    {
                        rpInfo.clearOnly = RPS_TRUE;

                        if (paramAccess.access.accessFlags & RPS_ACCESS_RENDER_TARGET_BIT)
                        {
                            rpInfo.renderTargetsMask = 1;
                        }
                        else
                        {
                            RPS_ASSERT(paramAccess.access.accessFlags &
                                       (RPS_ACCESS_RENDER_TARGET_BIT | RPS_ACCESS_DEPTH_STENCIL_WRITE));

                            rpInfo.depthStencilTargetMask = 1;
                        }

                        RPS_V_RETURN(appendParamRef(paramId, 0));

                        break;
                    }
                }
            }
            else
            {
                for (uint32_t i = renderTargetsInfoOffset; i < nodeDeclInfo.semanticKinds.size(); i++)
                {
                    const auto& semanticKind = nodeDeclInfo.semanticKinds[i];

                    if (semanticKind.semantic == RPS_SEMANTIC_RENDER_TARGET)
                    {
                        auto params = semanticKind.params.Get(nodeDeclInfo.semanticParamTable);

                        for (auto paramId : params)
                        {
                            auto& paramDecl = nodeDeclInfo.params[paramId];
                            RPS_RETURN_ERROR_IF(paramDecl.baseSemanticIndex + paramDecl.GetNumElements() >
                                                    RPS_MAX_SIMULTANEOUS_RENDER_TARGET_COUNT,
                                                RPS_ERROR_INDEX_OUT_OF_BOUNDS);

                            for (uint32_t iElem = 0; iElem < paramDecl.GetNumElements(); iElem++)
                            {
                                uint32_t semanticIndex = iElem + paramDecl.baseSemanticIndex;
                                if (semanticIndex < RPS_MAX_SIMULTANEOUS_RENDER_TARGET_COUNT)
                                {
                                    rpInfo.renderTargetsMask |= (1u << semanticIndex);
                                    RPS_V_RETURN(appendParamRef(paramId, iElem));
                                }
                            }
                        }
                    }
                    else if (semanticKind.semantic == RPS_SEMANTIC_DEPTH_STENCIL_TARGET)
                    {
                        auto params = semanticKind.params.Get(nodeDeclInfo.semanticParamTable);
                        RPS_ASSERT(params.size() == 1);

                        const auto& paramDecl = nodeDeclInfo.params[params.front()];
                        RPS_ASSERT(paramDecl.numElements == 1);

                        rpInfo.depthStencilTargetMask = 1;
                        RPS_V_RETURN(appendParamRef(params.front(), 0));
                    }
                    else if (semanticKind.semantic == RPS_SEMANTIC_RESOLVE_TARGET)
                    {
                        auto params = semanticKind.params.Get(nodeDeclInfo.semanticParamTable);

                        RPS_RETURN_ERROR_IF(paramRefOffset > 0xF, RPS_ERROR_INTEGER_OVERFLOW);
                        rpInfo.resolveTargetRefs = paramRefOffset;

                        for (auto paramId : params)
                        {
                            const auto& paramDecl = nodeDeclInfo.params[paramId];
                            RPS_RETURN_ERROR_IF(paramDecl.baseSemanticIndex + paramDecl.numElements >
                                                    RPS_MAX_SIMULTANEOUS_RENDER_TARGET_COUNT,
                                                RPS_ERROR_INDEX_OUT_OF_BOUNDS);

                            for (uint32_t iElem = 0; iElem < paramDecl.numElements; iElem++)
                            {
                                const uint32_t semanticIndex = iElem + paramDecl.baseSemanticIndex;
                                if (semanticIndex < RPS_MAX_SIMULTANEOUS_RENDER_TARGET_COUNT)
                                {
                                    rpInfo.resolveTargetsMask |= (1u << semanticIndex);
                                    RPS_V_RETURN(appendParamRef(paramId, iElem));
                                }
                            }
                        }
                    }
                }
            }

            RPS_RETURN_ERROR_IF(rpInfo.resolveTargetsMask & ~rpInfo.renderTargetsMask, RPS_ERROR_INVALID_PROGRAM);
            RPS_RETURN_ERROR_IF(!rpInfo.depthStencilTargetMask && (rpInfo.clearDepth || rpInfo.clearStencil),
                                RPS_ERROR_INVALID_PROGRAM);

            // Gather Viewports / Scissor Rects / Clear Values
            auto dynStates = nodeDeclInfo.dynamicStates.Get(nodeDeclInfo.semanticKinds);

            RPS_RETURN_ERROR_IF(rpInfo.renderTargetClearMask & ~rpInfo.renderTargetsMask, RPS_ERROR_INVALID_PROGRAM);

            rpInfo.viewportRefs    = paramRefOffset;
            rpInfo.scissorRectRefs = paramRefOffset;

            uint32_t numViewports = 0;

            for (auto iter = dynStates.begin(); iter != dynStates.end(); ++iter)
            {
                auto params = iter->params.Get(nodeDeclInfo.semanticParamTable);

                if (iter->semantic == RPS_SEMANTIC_COLOR_CLEAR_VALUE)
                {
                    for (auto paramId : params)
                    {
                        auto& paramDecl = nodeDeclInfo.params[paramId];
                        RPS_RETURN_ERROR_IF(paramDecl.baseSemanticIndex + paramDecl.numElements >
                                                RPS_MAX_SIMULTANEOUS_RENDER_TARGET_COUNT,
                                            RPS_ERROR_INDEX_OUT_OF_BOUNDS);

                        for (uint32_t iElement = 0; iElement < paramDecl.numElements; iElement++)
                        {
                            const uint32_t iRT = paramDecl.baseSemanticIndex + iElement;

                            rpInfo.renderTargetClearMask |= (1u << iRT);
                            RPS_V_RETURN(appendParamRef(paramId, iElement));
                        }
                    }
                }
                else if (iter->semantic == RPS_SEMANTIC_DEPTH_CLEAR_VALUE)
                {
                    RPS_ASSERT(params.size() == 1);

                    auto& paramAccessInfo = nodeDeclInfo.params[params.front()];
                    RPS_ASSERT(paramAccessInfo.numElements == 1);

                    rpInfo.clearDepth = RPS_TRUE;
                    RPS_V_RETURN(appendParamRef(params.front(), 0));
                }
                else if (iter->semantic == RPS_SEMANTIC_STENCIL_CLEAR_VALUE)
                {
                    RPS_ASSERT(params.size() == 1);

                    auto& paramAccessInfo = nodeDeclInfo.params[params.front()];
                    RPS_ASSERT(paramAccessInfo.numElements == 1);

                    rpInfo.clearStencil = RPS_TRUE;
                    RPS_V_RETURN(appendParamRef(params.front(), 0));
                }
                else if (iter->semantic == RPS_SEMANTIC_VIEWPORT)
                {
                    rpInfo.viewportRefs = paramRefOffset;

                    for (auto paramId : params)
                    {
                        auto& paramAccessInfo = nodeDeclInfo.params[paramId];

                        for (uint32_t iElement = 0; iElement < paramAccessInfo.numElements; iElement++)
                        {
                            RPS_V_RETURN(appendParamRef(paramId, iElement));
                        }

                        numViewports += paramAccessInfo.numElements;
                    }

                    rpInfo.scissorRectRefs = paramRefOffset;
                }
                else if (iter->semantic == RPS_SEMANTIC_SCISSOR)
                {
                    rpInfo.scissorRectRefs = paramRefOffset;

                    for (auto paramId : params)
                    {
                        auto& paramAccessInfo = nodeDeclInfo.params[paramId];

                        for (uint32_t iElement = 0; iElement < paramAccessInfo.numElements; iElement++)
                        {
                            RPS_V_RETURN(appendParamRef(paramId, iElement));
                        }
                    }
                }
            }

            rpInfo.clearValueRefs = paramRefOffset - (rpsCountBits(rpInfo.renderTargetClearMask) +
                                                      (rpInfo.clearDepth ? 1 : 0) + (rpInfo.clearStencil ? 1 : 0));
            rpInfo.numParamRefs   = paramRefOffset;

            if (rpInfo.numParamRefs > 0)
            {
                auto pRenderPassInfo = allocator.New<NodeDeclRenderPassInfo>(rpInfo);
                RPS_CHECK_ALLOC(pRenderPassInfo);

                pRenderPassInfo->paramRefs = allocator.NewArray<ParamElementRef>(rpInfo.numParamRefs).data();
                RPS_CHECK_ALLOC(pRenderPassInfo->paramRefs);

                std::copy(paramRefs, paramRefs + paramRefOffset, pRenderPassInfo->paramRefs);

                *ppOutRenderPassInfo = pRenderPassInfo;
            }

            return RPS_OK;
        }

    private:
        Arena&                 m_allocator;
        ArrayRef<NodeDeclInfo> m_nodeDecls;
        ArrayRef<ParamDecl>    m_paramDecls;
        // TODO: Assuming 1:1 external resource to param element mapping:
        ArrayRef<RpsParamId> m_externalResourceParamIds;
        uint32_t             m_maxExternalResources = 0;
        uint32_t             m_totalParamDataBufferSize = 0;
    };


    // TODO: Move to somewhere proper
    RPS_ASSOCIATE_HANDLE(ParamAttrList);

    inline RpsParamAttrList ParamAttrList::ToHandle(ParamAttrList* pAttrList)
    {
        return rps::ToHandle(pAttrList);
    }

}  // namespace rps

#endif  //RPS_RENDER_GRAPH_SIGNATURE_HPP
