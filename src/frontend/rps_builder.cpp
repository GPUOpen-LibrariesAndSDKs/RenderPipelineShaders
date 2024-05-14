// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "rps/frontend/rps_builder.h"

#include "core/rps_util.hpp"
#include "core/rps_device.hpp"
#include "runtime/common/rps_cmd_buf.hpp"
#include "runtime/common/rps_subprogram.hpp"
#include "runtime/common/rps_render_graph.hpp"
#include "runtime/common/rps_render_graph_builder.hpp"

namespace rps
{
    RenderGraphBuilderRef::RenderGraphBuilderRef(RpsRenderGraphBuilder builder)
        : m_builder(*FromHandle(builder))
    {
    }

    void* RenderGraphBuilderRef::AllocateData(size_t size, size_t alignment) const
    {
        return m_builder.AllocateData(size, alignment);
    }

    RpsNodeDeclId RenderGraphBuilderRef::DeclNode(const RpsNodeDesc& nodeDesc) const
    {
        return m_builder.DeclareDynamicNode(&nodeDesc);
    }

    RpsNodeId RenderGraphBuilderRef::AddNode(RpsNodeDeclId                      nodeDeclId,
                                             uint32_t                           tag,
                                             PFN_rpsCmdCallback                 callback,
                                             void*                              callbackUserContext,
                                             std::initializer_list<RpsVariable> args)
    {
        RpsNodeId nodeId = RPS_CMD_ID_INVALID;

        RPS_RETURN_ERROR_IF(
            RPS_FAILED(m_builder.AddCmdNode(
                nodeDeclId, tag, RpsCmdCallback{callback, callbackUserContext}, args.begin(), uint32_t(args.size()), &nodeId)),
            RPS_CMD_ID_INVALID);

        RPS_ASSERT((nodeId != RPS_CMD_ID_INVALID) && "invalid RenderGraphBuilder::AddCmdNode impl");

        return nodeId;
    }

    RpsResourceId RenderGraphBuilderRef::GetParamResourceId(RpsParamId paramId, uint32_t arrayIndex) const
    {
        return m_builder.GetParamResourceId(paramId, arrayIndex);
    }

    RpsResult RenderGraphBuilderRef::DeclareResource(uint32_t       localResourceId,
                                                     RpsVariable    hDescVar,
                                                     const char*    name,
                                                     RpsResourceId* pOutResId)
    {
        return m_builder.DeclareResource(localResourceId, hDescVar, name, pOutResId);
    }

    RpsVariable RenderGraphBuilderRef::GetParamVariable(RpsParamId paramId, size_t* pSize) const
    {
        return m_builder.GetParamVariable(paramId, pSize);
    }


}  // namespace rps

void* rpsRenderGraphAllocateData(RpsRenderGraphBuilder builder, size_t size)
{
    return rpsRenderGraphAllocateDataAligned(builder, size, alignof(std::max_align_t));
}

void* rpsRenderGraphAllocateDataAligned(RpsRenderGraphBuilder builder, size_t size, size_t alignment)
{
    return builder ? rps::FromHandle(builder)->AllocateData(size, alignment) : nullptr;
}

RpsNodeDeclId rpsRenderGraphDeclareDynamicNode(RpsRenderGraphBuilder builder, const RpsNodeDesc* pNodeDesc)
{
    RPS_CHECK_ARGS(builder);
    return rps::FromHandle(builder)->DeclareDynamicNode(pNodeDesc);
}

RpsVariable rpsRenderGraphGetParamVariable(RpsRenderGraphBuilder builder, RpsParamId paramId)
{
    RPS_RETURN_ERROR_IF(builder == RPS_NULL_HANDLE, nullptr);

    return rps::FromHandle(builder)->GetParamVariable(paramId);
}

RpsResourceId rpsRenderGraphGetParamResourceId(RpsRenderGraphBuilder builder, RpsParamId paramId)
{
    RPS_RETURN_ERROR_IF(builder == RPS_NULL_HANDLE, RPS_RESOURCE_ID_INVALID);

    return rps::FromHandle(builder)->GetParamResourceId(paramId);
}

RpsResourceId rpsRenderGraphDeclareResource(RpsRenderGraphBuilder builder,
                                            const char*           name,
                                            RpsResourceId         localId,
                                            RpsVariable           hDescVar)
{
    RPS_RETURN_ERROR_IF(builder == RPS_NULL_HANDLE, RPS_RESOURCE_ID_INVALID);
    RPS_RETURN_ERROR_IF(localId == RPS_RESOURCE_ID_INVALID, RPS_RESOURCE_ID_INVALID);

    RpsResourceId resId;
    RPS_RETURN_ERROR_IF(RPS_FAILED(rps::FromHandle(builder)->DeclareResource(localId, hDescVar, name, &resId)),
                        RPS_RESOURCE_ID_INVALID);

    return resId;
}

RpsNodeId rpsRenderGraphAddNode(RpsRenderGraphBuilder builder,
                                RpsNodeDeclId         nodeDeclId,
                                uint32_t              tag,
                                PFN_rpsCmdCallback    callback,
                                void*                 callbackContext,
                                RpsCmdCallbackFlags   callbackFlags,
                                const RpsVariable*    pArgs,
                                uint32_t              numArgs)
{
    RPS_RETURN_ERROR_IF(builder == RPS_NULL_HANDLE, RPS_CMD_ID_INVALID);

    RpsNodeId nodeId = RPS_CMD_ID_INVALID;

    const RpsCmdCallback callbackInfo = {callback, callbackContext, callbackFlags};

    RPS_RETURN_ERROR_IF(RPS_FAILED(rps::FromHandle(builder)->AddCmdNode(
                            nodeDeclId, tag, callbackInfo, pArgs, numArgs, &nodeId)),
                        RPS_CMD_ID_INVALID);

    RPS_ASSERT((nodeId != RPS_CMD_ID_INVALID) && "invalid RenderGraphBuilder::AddCmdNode impl");

    return nodeId;
}
