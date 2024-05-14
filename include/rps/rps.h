// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_H
#define RPS_H

#include "core/rps_api.h"

#include "frontend/rps_builder.h"

#ifndef RPS_NO_RUNTIME

#include "rps/runtime/common/rps_runtime.h"

#if RPS_D3D12_RUNTIME
#include "rps/runtime/d3d12/rps_d3d12_runtime.h"
#endif

#if RPS_VK_RUNTIME
#include "rps/runtime/vk/rps_vk_runtime.h"
#endif

#if RPS_D3D11_RUNTIME
#include "rps/runtime/d3d11/rps_d3d11_runtime.h"
#endif

#endif  //RPS_NO_COMMON_RHI

#endif//RPS_H
