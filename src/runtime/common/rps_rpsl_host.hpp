// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_RPSL_HOST_HPP
#define RPS_RPSL_HOST_HPP

#include "rps/core/rps_api.h"
#include "rps/runtime/common/rps_runtime.h"

#include "core/rps_core.hpp"
#include "core/rps_persistent_index_generator.hpp"
#include "runtime/common/rps_cmd_buf.hpp"
#include "runtime/common/rps_rpsl_host.h"

namespace rps
{
    class Subprogram;
    class RenderGraphBuilder;

    struct RpslExecuteInfo
    {
        Subprogram*        pProgram;
        const void* const* ppArgs;
        uint32_t           numArgs;
    };

    enum class SchedulerMarkerOpCodes
    {
        BARRIER = 0,
        SUBGRAPH_BEGIN,
        SUBGRAPH_END,
        MaxValue,
    };

    class RpslHost
    {
        RPS_CLASS_NO_COPY_MOVE(RpslHost);

    public:
        RpslHost(RenderGraphBuilder* pBuilder)
            : m_pGraphBuilder(pBuilder)
        {
        }

        RpsResult Execute(const RpslExecuteInfo& execInfo);

    public:
        RpsResult RpslCallNode(RpsNodeDeclId         nodeDeclId,
                               ArrayRef<RpsVariable> args,
                               uint32_t              callFlags,
                               uint32_t              nodeLocalId,
                               RpsNodeId*            pNodeId);

        RpsResult RpslDeclareResource(uint32_t  type,
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
                                      uint32_t* pOutId);

        void AddDependencies(ConstArrayRef<RpsNodeId> dependencies, RpsNodeId dstNode);

        RpsResult BlockMarker(uint32_t                markerType,
                              uint32_t                blockIndex,
                              ConstArrayRef<uint32_t> resourceCounts,
                              uint32_t                localLoopIndex,
                              uint32_t                numChildren,
                              uint32_t                parentId);

        RpsResult SchedulerMarker(SchedulerMarkerOpCodes opCode,
                                  RpsSubgraphFlags       flags,
                                  const char*            name,
                                  uint32_t               nameLength);

        RpsResult NotifyOutParamResources(uint32_t paramId, const void* pViews);

        RenderGraphBuilder* GetRenderGraphBuilder() const
        {
            return m_pGraphBuilder;
        }

        RpsResult ExecuteProgram(const RpslExecuteInfo& execInfo);

    private:
        void UpdateNodeDecls();

    private:
        RenderGraphBuilder* m_pGraphBuilder = nullptr;
    };

}  // namespace rps

#endif  //RPS_RPSL_HOST_HPP
