// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_RENDER_STATES_H
#define RPS_RENDER_STATES_H

#include "rps/core/rps_api.h"

/// @addtogroup RpsRenderGraphRuntime
/// @{

/// Screen region to render to.
typedef struct RpsViewport
{
    float x;       ///< Left offset of the viewport.
    float y;       ///< Top offset of the viewport.
    float width;   ///< Width of the viewport.
    float height;  ///< Height of the viewport.
    float minZ;    ///< Minimum Z value of the viewport.
    float maxZ;    ///< Maximum Z value of the viewport.
} RpsViewport;

/// Rectangular geometrical figure.
typedef struct RpsRect
{
    int32_t x;       ///< X coordinate of the left edge of the rectangle.
    int32_t y;       ///< Y coordinate of the top edge of the rectangle.
    int32_t width;   ///< Width of the rectangle.
    int32_t height;  ///< Height of the rectangle.
} RpsRect;

/// @brief Primitive topology types.
///
/// This maps to common API primitive topology enumerations
/// such as `D3D_PRIMITIVE_TOPOLOGY` and `VkPrimitiveTopology`.
typedef enum RpsPrimitiveTopology
{
    RPS_PRIMITIVE_TOPOLOGY_UNDEFINED         = 0,     ///< Undefined topology.
    RPS_PRIMITIVE_TOPOLOGY_POINTLIST         = 1,     ///< Point list topology.
    RPS_PRIMITIVE_TOPOLOGY_LINELIST          = 2,     ///< Line list topology.
    RPS_PRIMITIVE_TOPOLOGY_LINESTRIP         = 3,     ///< Line strip topology.
    RPS_PRIMITIVE_TOPOLOGY_TRIANGLELIST      = 4,     ///< Triangle list topology.
    RPS_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP     = 5,     ///< Triangle strip topology.
    RPS_PRIMITIVE_TOPOLOGY_LINELIST_ADJ      = 10,    ///< Line list with adjacency topology.
    RPS_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ     = 11,    ///< Line strip with adjacency topology.
    RPS_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ  = 12,    ///< Triangle list with adjacency topology.
    RPS_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ = 13,    ///< Triangle strip with adjacency topology.
    RPS_PRIMITIVE_TOPOLOGY_PATCHLIST         = 14,    ///< Patch list. The number of control points in the patch list
                                                      ///  is specified separately.
    RPS_PRIMITIVE_TOPOLOGY_FORCE_INT32 = 0x7FFFFFFF,  ///< Forces the enumeration to be int32 type. Do not use!!!
} RpsPrimitiveTopology;

/// @brief Resolve mode types for built-in resolve node.
///
/// Support of the modes is subject to the API backend used.
typedef enum RpsResolveMode
{
    RPS_RESOLVE_MODE_AVERAGE = 0,              ///< Resolve operation outputs the average value of all MSAA samples.
    RPS_RESOLVE_MODE_MIN,                      ///< Resolve operation outputs the minimum value of all MSAA samples.
    RPS_RESOLVE_MODE_MAX,                      ///< Resolve operation outputs the maximum value of all MSAA samples.
    RPS_RESOLVE_MODE_ENCODE_SAMPLER_FEEDBACK,  ///< Encoding sampler feedback map (DX12 only).
    RPS_RESOLVE_MODE_DECODE_SAMPLER_FEEDBACK,  ///< Decoding sampler feedback map (DX12 only).

    RPS_RESOLVE_MODE_FORCE_INT32 = 0x7FFFFFFF,
} RpsResolveMode;

/// @} end addtogroup RpsRenderGraphRuntime

/// @brief Screen regions to render to.
///
/// @ingroup RpsRenderGraphCommandRecording
typedef struct RpsCmdViewportInfo
{
    RpsRect defaultRenderArea;           ///< Default render area of the node. Usually deduced from the bound render
                                         ///  target dimensions.
    uint32_t           numViewports;     ///< Number of viewports used by the node.
    uint32_t           numScissorRects;  ///< Number of scissor rectangles used by the node.
    const RpsViewport* pViewports;       ///< Pointer to an array of <c><i>const RpsViewport*</i></c> with numViewports
                                         ///  elements. Must not be NULL if numViewports != 0.
    const RpsRect* pScissorRects;        ///< Pointer to an array of <c><i>const RpsRect*</i></c> with numScissorRects
                                         ///  elements. Must not be NULL if numScissorRects != 0.
} RpsCmdViewportInfo;

#endif  //RPS_RENDER_STATES_H
