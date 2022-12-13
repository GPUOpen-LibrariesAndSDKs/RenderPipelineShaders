// Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the AMD INTERNAL EVALUATION LICENSE.
//
// See file LICENSE.RTF for full license details.

#ifndef _RPS_H_
#define _RPS_H_

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

#endif//_RPS_H_
