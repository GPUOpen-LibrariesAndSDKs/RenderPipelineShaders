// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_D3D_COMMON_H
#define RPS_D3D_COMMON_H

#include <dxgi.h>

#include "rps/runtime/common/rps_format.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup RpsFormat
/// @{

/// @brief Converts an RPS format to a DXGI format.
///
/// @param rpsFormat                RPS format to convert.
///
/// @returns                        Converted DXGI format.
static inline DXGI_FORMAT rpsFormatToDXGI(RpsFormat rpsFormat)
{
    return (DXGI_FORMAT)rpsFormat;
}

/// @brief Converts a DXGI format to an RPS format.
///
/// @param dxgiFormat               DXGI format to convert.
///
/// @returns                        Converted RPS format.
static inline RpsFormat rpsFormatFromDXGI(DXGI_FORMAT dxgiFormat)
{
    return (RpsFormat)dxgiFormat;
}

/// @} end addtogroup RpsFormat

#ifdef __cplusplus
}
#endif

#endif  //RPS_D3D_COMMON_H
