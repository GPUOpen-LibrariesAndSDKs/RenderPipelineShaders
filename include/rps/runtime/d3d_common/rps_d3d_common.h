// Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the AMD INTERNAL EVALUATION LICENSE.
//
// See file LICENSE.RTF for full license details.

#ifndef _RPS_D3D_COMMON_H_
#define _RPS_D3D_COMMON_H_

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

#endif  //_RPS_D3D_COMMON_H_
