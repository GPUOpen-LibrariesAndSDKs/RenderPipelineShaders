// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_CMD_BUF_HPP
#define RPS_CMD_BUF_HPP

#include "core/rps_core.hpp"
#include "core/rps_util.hpp"
#include "core/rps_device.hpp"

#include "rps/runtime/common/rps_runtime.h"

namespace rps
{
    enum BuiltInNodeDeclIds
    {
        RPS_BUILTIN_NODE_INVALID           = -1,
        RPS_BUILTIN_NODE_SCHEDULER_BARRIER = -2,  ///< Built-in node to mark a scheduler barrier.
        RPS_BUILTIN_NODE_SUBGRAPH_BEGIN    = -3,  ///< Built-in node to mark beginning of a subgraph.
        RPS_BUILTIN_NODE_SUBGRAPH_END      = -4,  ///< Built-in node to mark ending of a subgraph.
        RPS_BUILTIN_NODE_BEGIN_SUBROUTINE  = -5,
        RPS_BUILTIN_NODE_END_SUBROUTINE    = -6,

        RPS_BUILTIN_NODE_FORCE_INT32 = INT32_MIN,
    };

    struct Cmd
    {
        RpsNodeDeclId         nodeDeclId        = RPS_NODEDECL_ID_INVALID;
        uint32_t              programInstanceId = RPS_INDEX_NONE_U32;
        uint32_t              tag               = 0;
        ArrayRef<RpsVariable> args;
        RpsCmdCallback        callback;
    };

    struct NodeDependency
    {
        RpsNodeId before;
        RpsNodeId after;
    };

}  // namespace rps

#endif  // RPS_CMD_BUF_HPP
