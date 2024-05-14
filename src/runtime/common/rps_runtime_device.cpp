// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "rps/runtime/common/rps_runtime.h"
#include "runtime/common/rps_render_graph.hpp"

RPS_STATIC_ASSERT_STANDALONE(RPS_SEMANTIC_USER_RESOURCE_BINDING + 1 == RPS_SEMANTIC_COUNT,
                             "RPS_SEMANTIC_USER_RESOURCE_BINDING must be the last valid element of RpsSemantic",
                             RpsSemantic);
