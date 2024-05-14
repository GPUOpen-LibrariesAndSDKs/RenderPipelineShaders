// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "rps/runtime/common/rps_format.h"
#include "core/rps_util.hpp"

uint32_t rpsGetFormatElementBytes(RpsFormat format)
{
    static const uint32_t s_Sizes[RPS_FORMAT_COUNT] = {
        0,   // RPS_FORMAT_UNKNOWN,
        16,  // RPS_FORMAT_R32G32B32A32_TYPELESS,
        16,  // RPS_FORMAT_R32G32B32A32_FLOAT,
        16,  // RPS_FORMAT_R32G32B32A32_UINT,
        16,  // RPS_FORMAT_R32G32B32A32_SINT,
        12,  // RPS_FORMAT_R32G32B32_TYPELESS,
        12,  // RPS_FORMAT_R32G32B32_FLOAT,
        12,  // RPS_FORMAT_R32G32B32_UINT,
        12,  // RPS_FORMAT_R32G32B32_SINT,
        8,   // RPS_FORMAT_R16G16B16A16_TYPELESS,
        8,   // RPS_FORMAT_R16G16B16A16_FLOAT,
        8,   // RPS_FORMAT_R16G16B16A16_UNORM,
        8,   // RPS_FORMAT_R16G16B16A16_UINT,
        8,   // RPS_FORMAT_R16G16B16A16_SNORM,
        8,   // RPS_FORMAT_R16G16B16A16_SINT,
        8,   // RPS_FORMAT_R32G32_TYPELESS,
        8,   // RPS_FORMAT_R32G32_FLOAT,
        8,   // RPS_FORMAT_R32G32_UINT,
        8,   // RPS_FORMAT_R32G32_SINT,
        8,   // RPS_FORMAT_R32G8X24_TYPELESS,
        8,   // RPS_FORMAT_D32_FLOAT_S8X24_UINT,
        8,   // RPS_FORMAT_R32_FLOAT_X8X24_TYPELESS,
        8,   // RPS_FORMAT_X32_TYPELESS_G8X24_UINT,
        4,   // RPS_FORMAT_R10G10B10A2_TYPELESS,
        4,   // RPS_FORMAT_R10G10B10A2_UNORM,
        4,   // RPS_FORMAT_R10G10B10A2_UINT,
        4,   // RPS_FORMAT_R11G11B10_FLOAT,
        4,   // RPS_FORMAT_R8G8B8A8_TYPELESS,
        4,   // RPS_FORMAT_R8G8B8A8_UNORM,
        4,   // RPS_FORMAT_R8G8B8A8_UNORM_SRGB,
        4,   // RPS_FORMAT_R8G8B8A8_UINT,
        4,   // RPS_FORMAT_R8G8B8A8_SNORM,
        4,   // RPS_FORMAT_R8G8B8A8_SINT,
        4,   // RPS_FORMAT_R16G16_TYPELESS,
        4,   // RPS_FORMAT_R16G16_FLOAT,
        4,   // RPS_FORMAT_R16G16_UNORM,
        4,   // RPS_FORMAT_R16G16_UINT,
        4,   // RPS_FORMAT_R16G16_SNORM,
        4,   // RPS_FORMAT_R16G16_SINT,
        4,   // RPS_FORMAT_R32_TYPELESS,
        4,   // RPS_FORMAT_D32_FLOAT,
        4,   // RPS_FORMAT_R32_FLOAT,
        4,   // RPS_FORMAT_R32_UINT,
        4,   // RPS_FORMAT_R32_SINT,
        4,   // RPS_FORMAT_R24G8_TYPELESS,
        4,   // RPS_FORMAT_D24_UNORM_S8_UINT,
        4,   // RPS_FORMAT_R24_UNORM_X8_TYPELESS,
        4,   // RPS_FORMAT_X24_TYPELESS_G8_UINT,
        2,   // RPS_FORMAT_R8G8_TYPELESS,
        2,   // RPS_FORMAT_R8G8_UNORM,
        2,   // RPS_FORMAT_R8G8_UINT,
        2,   // RPS_FORMAT_R8G8_SNORM,
        2,   // RPS_FORMAT_R8G8_SINT,
        2,   // RPS_FORMAT_R16_TYPELESS,
        2,   // RPS_FORMAT_R16_FLOAT,
        2,   // RPS_FORMAT_D16_UNORM,
        2,   // RPS_FORMAT_R16_UNORM,
        2,   // RPS_FORMAT_R16_UINT,
        2,   // RPS_FORMAT_R16_SNORM,
        2,   // RPS_FORMAT_R16_SINT,
        1,   // RPS_FORMAT_R8_TYPELESS,
        1,   // RPS_FORMAT_R8_UNORM,
        1,   // RPS_FORMAT_R8_UINT,
        1,   // RPS_FORMAT_R8_SNORM,
        1,   // RPS_FORMAT_R8_SINT,
        1,   // RPS_FORMAT_A8_UNORM,
        0,   // RPS_FORMAT_R1_UNORM,
        4,   // RPS_FORMAT_R9G9B9E5_SHAREDEXP,
        2,   // RPS_FORMAT_R8G8_B8G8_UNORM,
        2,   // RPS_FORMAT_G8R8_G8B8_UNORM,
        8,   // RPS_FORMAT_BC1_TYPELESS,
        8,   // RPS_FORMAT_BC1_UNORM,
        8,   // RPS_FORMAT_BC1_UNORM_SRGB,
        16,  // RPS_FORMAT_BC2_TYPELESS,
        16,  // RPS_FORMAT_BC2_UNORM,
        16,  // RPS_FORMAT_BC2_UNORM_SRGB,
        16,  // RPS_FORMAT_BC3_TYPELESS,
        16,  // RPS_FORMAT_BC3_UNORM,
        16,  // RPS_FORMAT_BC3_UNORM_SRGB,
        8,   // RPS_FORMAT_BC4_TYPELESS,
        8,   // RPS_FORMAT_BC4_UNORM,
        8,   // RPS_FORMAT_BC4_SNORM,
        16,  // RPS_FORMAT_BC5_TYPELESS,
        16,  // RPS_FORMAT_BC5_UNORM,
        16,  // RPS_FORMAT_BC5_SNORM,
        2,   // RPS_FORMAT_B5G6R5_UNORM,
        2,   // RPS_FORMAT_B5G5R5A1_UNORM,
        4,   // RPS_FORMAT_B8G8R8A8_UNORM,
        4,   // RPS_FORMAT_B8G8R8X8_UNORM,
        4,   // RPS_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,
        4,   // RPS_FORMAT_B8G8R8A8_TYPELESS,
        4,   // RPS_FORMAT_B8G8R8A8_UNORM_SRGB,
        4,   // RPS_FORMAT_B8G8R8X8_TYPELESS,
        4,   // RPS_FORMAT_B8G8R8X8_UNORM_SRGB,
        16,  // RPS_FORMAT_BC6H_TYPELESS,
        16,  // RPS_FORMAT_BC6H_UF16,
        16,  // RPS_FORMAT_BC6H_SF16,
        16,  // RPS_FORMAT_BC7_TYPELESS,
        16,  // RPS_FORMAT_BC7_UNORM,
        16,  // RPS_FORMAT_BC7_UNORM_SRGB,
        0,   // RPS_FORMAT_AYUV,
        0,   // RPS_FORMAT_Y410,
        0,   // RPS_FORMAT_Y416,
        0,   // RPS_FORMAT_NV12,
        0,   // RPS_FORMAT_P010,
        0,   // RPS_FORMAT_P016,
        0,   // RPS_FORMAT_420_OPAQUE,
        0,   // RPS_FORMAT_YUY2,
        0,   // RPS_FORMAT_Y210,
        0,   // RPS_FORMAT_Y216,
        0,   // RPS_FORMAT_NV11,
        0,   // RPS_FORMAT_AI44,
        0,   // RPS_FORMAT_IA44,
        0,   // RPS_FORMAT_P8,
        0,   // RPS_FORMAT_A8P8,
        2,   // RPS_FORMAT_B4G4R4A4_UNORM,
        //
        // RPS_FORMAT_COUNT,
    };

    return (format < RPS_FORMAT_COUNT) ? s_Sizes[format] : 0;
}

RpsBool rpsFormatIsBlockCompressed(RpsFormat format)
{
    return (((format >= RPS_FORMAT_BC1_TYPELESS) && (format <= RPS_FORMAT_BC5_SNORM)) ||
            ((format >= RPS_FORMAT_BC6H_TYPELESS) && (format <= RPS_FORMAT_BC7_UNORM_SRGB)));
}

const char* rpsFormatGetName(RpsFormat format)
{
    static constexpr const char* fmtNames[] = {
        "UNKNOWN",
        "R32G32B32A32_TYPELESS",
        "R32G32B32A32_FLOAT",
        "R32G32B32A32_UINT",
        "R32G32B32A32_SINT",
        "R32G32B32_TYPELESS",
        "R32G32B32_FLOAT",
        "R32G32B32_UINT",
        "R32G32B32_SINT",
        "R16G16B16A16_TYPELESS",
        "R16G16B16A16_FLOAT",
        "R16G16B16A16_UNORM",
        "R16G16B16A16_UINT",
        "R16G16B16A16_SNORM",
        "R16G16B16A16_SINT",
        "R32G32_TYPELESS",
        "R32G32_FLOAT",
        "R32G32_UINT",
        "R32G32_SINT",
        "R32G8X24_TYPELESS",
        "D32_FLOAT_S8X24_UINT",
        "R32_FLOAT_X8X24_TYPELESS",
        "X32_TYPELESS_G8X24_UINT",
        "R10G10B10A2_TYPELESS",
        "R10G10B10A2_UNORM",
        "R10G10B10A2_UINT",
        "R11G11B10_FLOAT",
        "R8G8B8A8_TYPELESS",
        "R8G8B8A8_UNORM",
        "R8G8B8A8_UNORM_SRGB",
        "R8G8B8A8_UINT",
        "R8G8B8A8_SNORM",
        "R8G8B8A8_SINT",
        "R16G16_TYPELESS",
        "R16G16_FLOAT",
        "R16G16_UNORM",
        "R16G16_UINT",
        "R16G16_SNORM",
        "R16G16_SINT",
        "R32_TYPELESS",
        "D32_FLOAT",
        "R32_FLOAT",
        "R32_UINT",
        "R32_SINT",
        "R24G8_TYPELESS",
        "D24_UNORM_S8_UINT",
        "R24_UNORM_X8_TYPELESS",
        "X24_TYPELESS_G8_UINT",
        "R8G8_TYPELESS",
        "R8G8_UNORM",
        "R8G8_UINT",
        "R8G8_SNORM",
        "R8G8_SINT",
        "R16_TYPELESS",
        "R16_FLOAT",
        "D16_UNORM",
        "R16_UNORM",
        "R16_UINT",
        "R16_SNORM",
        "R16_SINT",
        "R8_TYPELESS",
        "R8_UNORM",
        "R8_UINT",
        "R8_SNORM",
        "R8_SINT",
        "A8_UNORM",
        "R1_UNORM",
        "R9G9B9E5_SHAREDEXP",
        "R8G8_B8G8_UNORM",
        "G8R8_G8B8_UNORM",
        "BC1_TYPELESS",
        "BC1_UNORM",
        "BC1_UNORM_SRGB",
        "BC2_TYPELESS",
        "BC2_UNORM",
        "BC2_UNORM_SRGB",
        "BC3_TYPELESS",
        "BC3_UNORM",
        "BC3_UNORM_SRGB",
        "BC4_TYPELESS",
        "BC4_UNORM",
        "BC4_SNORM",
        "BC5_TYPELESS",
        "BC5_UNORM",
        "BC5_SNORM",
        "B5G6R5_UNORM",
        "B5G5R5A1_UNORM",
        "B8G8R8A8_UNORM",
        "B8G8R8X8_UNORM",
        "R10G10B10_XR_BIAS_A2_UNORM",
        "B8G8R8A8_TYPELESS",
        "B8G8R8A8_UNORM_SRGB",
        "B8G8R8X8_TYPELESS",
        "B8G8R8X8_UNORM_SRGB",
        "BC6H_TYPELESS",
        "BC6H_UF16",
        "BC6H_SF16",
        "BC7_TYPELESS",
        "BC7_UNORM",
        "BC7_UNORM_SRGB",
        "AYUV",
        "Y410",
        "Y416",
        "NV12",
        "P010",
        "P016",
        "420_OPAQUE",
        "YUY2",
        "Y210",
        "Y216",
        "NV11",
        "AI44",
        "IA44",
        "P8",
        "A8P8",
        "B4G4R4A4_UNORM",
    };

    static_assert(RPS_COUNTOF(fmtNames) == RPS_FORMAT_COUNT, "Format name table needs update.");

    return fmtNames[(format < RPS_FORMAT_COUNT) ? format : RPS_FORMAT_UNKNOWN];
}

RpsBool rpsFormatHasDepth(RpsFormat format)
{
    switch (format)
    {
    case RPS_FORMAT_D32_FLOAT:
    case RPS_FORMAT_D16_UNORM:
    case RPS_FORMAT_R32G8X24_TYPELESS:
    case RPS_FORMAT_D32_FLOAT_S8X24_UINT:
    case RPS_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case RPS_FORMAT_X32_TYPELESS_G8X24_UINT:
    case RPS_FORMAT_R24G8_TYPELESS:
    case RPS_FORMAT_D24_UNORM_S8_UINT:
    case RPS_FORMAT_R24_UNORM_X8_TYPELESS:
    case RPS_FORMAT_X24_TYPELESS_G8_UINT:
        return RPS_TRUE;
    default:
        break;
    }
    return RPS_FALSE;
}

RpsBool rpsFormatHasStencil(RpsFormat format)
{
    switch (format)
    {
    case RPS_FORMAT_R32G8X24_TYPELESS:
    case RPS_FORMAT_D32_FLOAT_S8X24_UINT:
    case RPS_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case RPS_FORMAT_X32_TYPELESS_G8X24_UINT:
    case RPS_FORMAT_R24G8_TYPELESS:
    case RPS_FORMAT_D24_UNORM_S8_UINT:
    case RPS_FORMAT_R24_UNORM_X8_TYPELESS:
    case RPS_FORMAT_X24_TYPELESS_G8_UINT:
        return RPS_TRUE;
    default:
        break;
    }
    return RPS_FALSE;
}

RpsBool rpsFormatHasDepthStencil(RpsFormat format)
{
    return rpsFormatHasDepth(format);
}

RpsBool rpsFormatIsDepthOnly(RpsFormat format)
{
    return rpsFormatHasDepth(format) && !rpsFormatHasStencil(format);
}
