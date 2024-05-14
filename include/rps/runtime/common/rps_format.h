// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_FORMAT_H
#define RPS_FORMAT_H

#include "rps/core/rps_api.h"

/// @addtogroup RpsRenderGraphRuntimeResources
/// @{

/// @defgroup RpsFormat RpsFormat
/// @{

/// @brief Supported RPS formats.
typedef enum RpsFormat
{
    RPS_FORMAT_UNKNOWN,                ///< Unknown format.
    RPS_FORMAT_R32G32B32A32_TYPELESS,  ///< 4-channel RGBA format with each channel being a typeless 32-bit value.
    RPS_FORMAT_R32G32B32A32_FLOAT,     ///< 4-channel RGBA format with each channel being a 32-bit IEEE 754 floating
                                       ///  point value.
    RPS_FORMAT_R32G32B32A32_UINT,      ///< 4-channel RGBA format with each channel being a 32-bit unsigned integer.
    RPS_FORMAT_R32G32B32A32_SINT,      ///< 4-channel RGBA format with each channel being a 32-bit signed integer.
    RPS_FORMAT_R32G32B32_TYPELESS,     ///< 3-channel RGB format with each channel being a typeless 32-bit value.
    RPS_FORMAT_R32G32B32_FLOAT,        ///< 3-channel RGB format with each channel being a 32-bit IEEE 754 floating
                                       ///  point value.
    RPS_FORMAT_R32G32B32_UINT,         ///< 3-channel RGB format with each channel being a 32-bit unsigned integer.
    RPS_FORMAT_R32G32B32_SINT,         ///< 3-channel RGB format with each channel being a 32-bit signed integer.

    RPS_FORMAT_R16G16B16A16_TYPELESS,  ///< 4-channel RGBA format with each channel being a typeless 16-bit value.
    RPS_FORMAT_R16G16B16A16_FLOAT,     ///< 4-channel RGBA format with each channel being a 16-bit floating point
                                       ///  value.
    RPS_FORMAT_R16G16B16A16_UNORM,     ///< 4-channel RGBA format with each channel being a normalized, 16-bit unsigned
                                       ///  integer.
    RPS_FORMAT_R16G16B16A16_UINT,      ///< 4-channel RGBA format with each channel being a 16-bit unsigned integer.
    RPS_FORMAT_R16G16B16A16_SNORM,     ///< 4-channel RGBA format with each channel being a normalized, 16-bit signed
                                       ///  integer.

    /// 4-channel RGBA format with each channel being a 16-bit signed integer.
    RPS_FORMAT_R16G16B16A16_SINT,

    /// 2-channel RG format with each channel being a typeless 32-bit value.
    RPS_FORMAT_R32G32_TYPELESS,

    /// 2-channel RG format with each channel being a 32-bit IEEE 754 floating point value.
    RPS_FORMAT_R32G32_FLOAT,

    /// 2-channel RG format with each channel being a 32-bit unsigned integer.
    RPS_FORMAT_R32G32_UINT,

    /// 2-channel RG format with each channel being a 32-bit signed integer.
    RPS_FORMAT_R32G32_SINT,

    /// 2-channel RG format with the first channel being a typeless 32-bit value, the second channel a typeless 8-bit
    /// value and 24 unused bits at the end.
    RPS_FORMAT_R32G8X24_TYPELESS,

    /// 2-channel RG format with the first channel being a 32-bit depth value, the second one a 8-bit unsigned integer
    /// value and 24 unused bits at the end.
    RPS_FORMAT_D32_FLOAT_S8X24_UINT,

    /// Single channel R format with the channel being a typeless 32-bit IEEE 754 floating point value and additional
    /// sets of 8 and 24 unused bits afterwards.
    RPS_FORMAT_R32_FLOAT_X8X24_TYPELESS,

    /// Single channel R format with 32 unused bits, the channel being an 8-bit unsigned integer value and 24 unused
    /// bits at the end.
    RPS_FORMAT_X32_TYPELESS_G8X24_UINT,

    /// 4-channel RGBA format with the RGB channels being typeless 10-bit values and the A channel being a typeless
    /// 2-bit value.
    RPS_FORMAT_R10G10B10A2_TYPELESS,

    /// 4-channel RGBA format with the RGB channels being 10-bit normalized, unsigned integer values and the A channel
    /// being a 2-bit normalized, unsigned integer value.
    RPS_FORMAT_R10G10B10A2_UNORM,

    /// 4-channel RGBA format with the RGB channels being 10-bit unsigned integer values and the A channel being a 2-bit
    /// unsigned integer value.
    RPS_FORMAT_R10G10B10A2_UINT,

    /// 3-channel RGB format with the RG channels being 11-bit floating point values and the B channel being a 10-bit
    /// floating point value.
    RPS_FORMAT_R11G11B10_FLOAT,

    RPS_FORMAT_R8G8B8A8_TYPELESS,    ///< 4-channel RGBA format with all channels being typeless 8-bit values.
    RPS_FORMAT_R8G8B8A8_UNORM,       ///< 4-channel RGBA format with all channels being normalized 8-bit unsigned
                                     ///  integers.
    RPS_FORMAT_R8G8B8A8_UNORM_SRGB,  ///< 4-channel RGBA format with all channels being normalized 8-bit unsigned integer
                                     ///  SRGB values.
    RPS_FORMAT_R8G8B8A8_UINT,        ///< 4-channel RGBA format with all channels being 8-bit unsigned integers.
    RPS_FORMAT_R8G8B8A8_SNORM,       ///< 4-channel RGBA format with all channels being normalized, 8-bit signed
                                     ///  integers.
    RPS_FORMAT_R8G8B8A8_SINT,        ///< 4-channel RGBA format with all channels being 8-bit signed integers.

    RPS_FORMAT_R16G16_TYPELESS,  ///< 2-channel RG format with each channel being a typeless 16-bit value.
    RPS_FORMAT_R16G16_FLOAT,  ///< 2-channel RG format with each channel being a 16-bit IEEE 754 floating point value.
    RPS_FORMAT_R16G16_UNORM,  ///< 2-channel RG format with each channel being a normalized, 16-bit unsigned integer.
    RPS_FORMAT_R16G16_UINT,   ///< 2-channel RG format with each channel being a 16-bit unsigned integer.
    RPS_FORMAT_R16G16_SNORM,  ///< 2-channel RG format with each channel being a normalized, 16-bit signed integer
                              ///  value.
    RPS_FORMAT_R16G16_SINT,   ///< 2-channel RG format with each channel being a 16-bit signed integer.

    RPS_FORMAT_R32_TYPELESS,  ///< Single channel R format with the channel being a typeless 32-bit value.
    RPS_FORMAT_D32_FLOAT,     ///< Single channel R format with the channel being a 32-bit IEEE 754 floating point depth
                              ///  value.
    RPS_FORMAT_R32_FLOAT,     ///< Single channel R format with the channel being a 32-bit IEEE 754 floating point
                              ///  value.
    RPS_FORMAT_R32_UINT,      ///< Single channel R format with the channel being a 32-bit unsigned integer.
    RPS_FORMAT_R32_SINT,      ///< Single channel R format with the channel being a 32-bit signed integer.

    /// 2-channel RG format with the first channel being a typeless 24-bit value and the second one a typeless 8-bit
    /// value.
    RPS_FORMAT_R24G8_TYPELESS,

    /// 2-channel RG format with the first channel being a normalized, 24-bit unsigned integer depth value and the
    /// second one an 8-bit unsigned integer stencil value.
    RPS_FORMAT_D24_UNORM_S8_UINT,

    /// 2-channel RG format with the first channel being a normalized, 24-bit unsigned integer value and the second one
    /// a typeless 8-bit value.
    RPS_FORMAT_R24_UNORM_X8_TYPELESS,

    /// Single channel R format with 24 unused bits with the channel being an 8-bit unsigned integer.
    RPS_FORMAT_X24_TYPELESS_G8_UINT,

    RPS_FORMAT_R8G8_TYPELESS,  ///< 2-channel RG format with each channel being a typeless 8-bit value.
    RPS_FORMAT_R8G8_UNORM,     ///< 2-channel RG format with each channel being a normalized, 8-bit unsigned integer.
    RPS_FORMAT_R8G8_UINT,      ///< 2-channel RG format with each channel being a 8-bit unsigned integer.
    RPS_FORMAT_R8G8_SNORM,     ///< 2-channel RG format with each channel being a normalized, 8-bit signed integer.
    RPS_FORMAT_R8G8_SINT,      ///< 2-channel RG format with each channel being a 8-bit signed integer.

    RPS_FORMAT_R16_TYPELESS,  ///< Single channel R format with the channel being a typeless 16-bit value.
    RPS_FORMAT_R16_FLOAT,     ///< Single channel R format with the channel being a 16-bit IEEE 754 floating point
                              ///  value.
    RPS_FORMAT_D16_UNORM,     ///< Single channel R format with the channel being a 16-bit IEEE 754 floating point
                              ///  depth value.
    RPS_FORMAT_R16_UNORM,     ///< Single channel R format with the channel being a 16-bit unsigned integer.
    RPS_FORMAT_R16_UINT,      ///< Single channel R format with the channel being a 16-bit signed integer.
    RPS_FORMAT_R16_SNORM,     ///< Single channel R format with the channel being a normalized, 16-bit signed integer.
    RPS_FORMAT_R16_SINT,      ///< Single channel R format with the channel being a 16-bit signed integer.

    RPS_FORMAT_R8_TYPELESS,  ///< Single channel R format with the channel being a typeless 8-bit value.
    RPS_FORMAT_R8_UNORM,     ///< Single channel R format with the channel being a normalized, 8-bit unsigned integer.
    RPS_FORMAT_R8_UINT,      ///< Single channel R format with the channel being a 8-bit signed integer.
    RPS_FORMAT_R8_SNORM,     ///< Single channel R format with the channel being a normalized, 8-bit signed integer.
    RPS_FORMAT_R8_SINT,      ///< Single channel R format with the channel being a 8-bit signed integer.
    RPS_FORMAT_A8_UNORM,     ///< Single channel A format with the channel being a normalized, 8-bit unsigned integer.

    RPS_FORMAT_R1_UNORM,  ///< Single channel R format with the channel being a 1-bit unsigned integer.

    /// 4-channel RGB format with the first three channels being a 9-bit mantissa. Together with the 5-bit exponent that
    /// is shared for all three channels they form three 9-bit mantissa + 5-bit exponent floating point value.
    RPS_FORMAT_R9G9B9E5_SHAREDEXP,

    /// 4-channel RGB format with each channel being a normalized, 8-bit unsigned integer. Each block of 32 bits
    /// describes the RGB values for a pair of pixels that always share one R and B value but have separate G values.
    RPS_FORMAT_R8G8_B8G8_UNORM,

    /// 4-channel RGB format with each channel being a normalized, 8-bit unsigned integer. Each block of 32 bits
    /// describes the RGB values for a pair of pixels that always share one R and B value but have separate G values.
    RPS_FORMAT_G8R8_G8B8_UNORM,

    /// 4-channel block compressed format with the first channel being a typeless 5-bit value, the second one a
    /// typeless, 6-bit value, the third one a typeless, 5-bit value and the last one a typeless, 0-bit or 1-bit value.
    RPS_FORMAT_BC1_TYPELESS,

    /// 4-channel block compressed format with the first channel being a normalized, 5-bit unsigned integer, the second
    /// one a normalized, 6-bit unsigned integer, the third one a normalized, 5-bit unsigned integer and the last one a
    /// normalized, 0-bit or 1-bit unsigned integer.
    RPS_FORMAT_BC1_UNORM,

    /// 4-channel block compressed format with the first channel being a normalized, 5-bit unsigned integer SRGB value,
    /// the second one a normalized, 6-bit unsigned integer SRGB value, the third one a normalized, 5-bit unsigned
    /// integer SRGB value and the last one a normalized, 0-bit or 1-bit unsigned integer SRGB value.
    RPS_FORMAT_BC1_UNORM_SRGB,

    /// 4-channel block compressed format with the first channel being a typeless 5-bit value, the second one a
    /// typeless, 6-bit value, the third one a typeless, 5-bit value and the last one a typeless, 4-bit value.
    RPS_FORMAT_BC2_TYPELESS,

    /// 4-channel block compressed format with the first channel being a normalized, 5-bit unsigned integer, the second
    /// one a normalized, 6-bit unsigned integer, the third one a normalized, 5-bit unsigned integer and the last one a
    /// normalized, 4-bit unsigned integer.
    RPS_FORMAT_BC2_UNORM,

    /// 4-channel block compressed format with the first channel being a normalized, 5-bit unsigned integer SRGB value,
    /// the second one a normalized, 6-bit unsigned integer SRGB value, the third one a normalized, 5-bit unsigned
    /// integer SRGB value and the last one a normalized, 4-bit unsigned integer SRGB value.
    RPS_FORMAT_BC2_UNORM_SRGB,

    /// 4-channel block compressed format with the first channel being a typeless 5-bit value, the second one a
    /// typeless, 6-bit value, the third one a typeless, 5-bit value and the last one a typeless, 8-bit value.
    RPS_FORMAT_BC3_TYPELESS,

    /// 4-channel block compressed format with the first channel being a normalized, 5-bit unsigned integer, the second
    /// one a normalized, 6-bit unsigned integer, the third one a normalized, 5-bit unsigned integer and the last one a
    /// normalized, 8-bit unsigned integer.
    RPS_FORMAT_BC3_UNORM,

    /// 4-channel block compressed format with the first channel being a normalized, 5-bit unsigned integer SRGB value,
    /// the second one a normalized, 6-bit unsigned integer SRGB value, the third one a normalized, 5-bit unsigned
    /// integer SRGB value and the last one a normalized, 0-bit or 1-bit unsigned integer SRGB value.
    RPS_FORMAT_BC3_UNORM_SRGB,

    /// Single channel block compressed format with the channel being a typeless 8-bit value.
    RPS_FORMAT_BC4_TYPELESS,

    /// Single channel block compressed format with the channel being a normalized, 8-bit signed integer value.
    RPS_FORMAT_BC4_UNORM,

    /// Single channel block compressed format with the channel being a normalized, 8-bit signed integer value.
    RPS_FORMAT_BC4_SNORM,

    /// 2-channel block compressed format with each channel being a typeless 8-bit value.
    RPS_FORMAT_BC5_TYPELESS,

    /// 2-channel block compressed format with each channel being a normalized, 8-bit unsigned integer value.
    RPS_FORMAT_BC5_UNORM,

    /// 2-channel block compressed format with each channel being a normalized, 8-bit signed integer value.
    RPS_FORMAT_BC5_SNORM,

    /// 3-channel BGR format with the first channel being a normalized, 5-bit unsigned integer, the second one a
    /// normalized, 6-bit unsigned integer and the third one a normalized, 5-bit unsigned integer.
    RPS_FORMAT_B5G6R5_UNORM,

    /// 4-channel BGRA format with the first three channels being a normalized, 5-bit unsigned integer and the last one
    /// a normalized, 1-bit unsigned integer.
    RPS_FORMAT_B5G5R5A1_UNORM,

    /// 4-channel BGRA format with each channel being a normalized, 8-bit unsigned integer.
    RPS_FORMAT_B8G8R8A8_UNORM,

    /// 3-channel BGR format with each channel being a normalized, 8-bit unsigned integer value and 8 unused bits at the
    /// end.
    RPS_FORMAT_B8G8R8X8_UNORM,

    /// 4-channel RGB 2.8-biased fixed-point format with the first three channels being a normalized, 10-bit
    /// unsigned integer and the last one a normalized 2-bit unsigned integer.
    RPS_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,

    RPS_FORMAT_B8G8R8A8_TYPELESS,    ///< 4-channel BGRA format with each channel being a typeless 8-bit value.
    RPS_FORMAT_B8G8R8A8_UNORM_SRGB,  ///< 4-channel BGRA format with each channel being a normalized, 8-bit unsigned
                                     ///  integer SRGB value.
    RPS_FORMAT_B8G8R8X8_TYPELESS,    ///< 3-channel BGR format with each channel being a typeless 8-bit value and 8
                                     ///  unused bits at the end.
    RPS_FORMAT_B8G8R8X8_UNORM_SRGB,  ///< 3-channel BGR format with each channel being a normalized, 8-bit unsigned
                                     ///  integer and 8 unused bits a the end.

    /// 3-channel block compressed HDR format with each channel being a typeless 16-bit value.
    RPS_FORMAT_BC6H_TYPELESS,

    /// 3-channel block compressed HDR format with each channel being a 16-bit unsigned "half" floating point value.
    RPS_FORMAT_BC6H_UF16,

    /// 3-channel block compressed HDR format with each channel being a 16-bit signed "half" floating point value.
    RPS_FORMAT_BC6H_SF16,

    /// 3-channel or 4-channel block compressed format with the first three channels being a typeless, 4-7-bit value and
    /// the last one an optional, typeless 0-8-bit value.
    RPS_FORMAT_BC7_TYPELESS,

    /// 3-channel or 4-channel block compressed format with the first three channels being an normalized, 4-7-bit
    /// unsigned integer and the last one an optional, normalized, 0-8-bit unsigned integer.
    RPS_FORMAT_BC7_UNORM,

    /// 3-channel or 4-channel block compressed format with the first three channels being an normalized, 4-7-bit
    /// unsigned integer and the last one an optional, normalized, 0-8-bit unsigned integer .
    RPS_FORMAT_BC7_UNORM_SRGB,

    RPS_FORMAT_AYUV,            ///< 4-channel video resource format with each channel being a 8-bit value.
    RPS_FORMAT_Y410,            ///< 4-channel video resource format with each of the first three channels being a
                                ///  10-bit value and the last one a 2-bit value.
    RPS_FORMAT_Y416,            ///< 4-channel video resource format with each channel being a 16-bit value.
    RPS_FORMAT_NV12,            ///< 2-channel video resource format with each channel being a 8-bit value.
    RPS_FORMAT_P010,            ///< 2-channel video resource format with each channel being a 16-bit value.
    RPS_FORMAT_P016,            ///< 2-channel video resource format with each channel being a 8-bit value.
    RPS_FORMAT_420_OPAQUE,      ///< Video resource format with opaque layout.
    RPS_FORMAT_YUY2,            ///< 4-channel video resource format with each channel being a 8-bit value.
    RPS_FORMAT_Y210,            ///< 4-channel video resource format with each channel being a 16-bit value.
    RPS_FORMAT_Y216,            ///< 4-channel video resource format with each channel being a 16-bit value.
    RPS_FORMAT_NV11,            ///< 2-channel video resource format with each channel being a 8-bit value.
    RPS_FORMAT_AI44,            ///< 4-bit palletized video resource format.
    RPS_FORMAT_IA44,            ///< 4-bit palletized video resource format.
    RPS_FORMAT_P8,              ///< RGB video resource format with 8-bit palletization.
    RPS_FORMAT_A8P8,            ///< RGB video resource format with 8-bit palletization.
    RPS_FORMAT_B4G4R4A4_UNORM,  ///< 4-channels BGRA format with each channel being a normalized 4-bit unsigned integer.

    RPS_FORMAT_COUNT,  ///< Number of formats available in <c><i>RpsFormat</i></c>.
} RpsFormat;

/// @brief Returns whether a format is block compressed.
///
/// All block compressed formats start with the prefix RPS_FORMAT_BC.
///
/// @param format       Format to check.
///
/// @returns            RPS_TRUE if the format is block compressed, RPS_FALSE otherwise.
RpsBool rpsFormatIsBlockCompressed(RpsFormat format);

/// @brief Returns whether a format has a depth or a stencil component.
///
/// @param format       Format to check.
///
/// @returns            RPS_TRUE if the format has a depth or a stencil component, RPS_FALSE otherwise.
RpsBool rpsFormatHasDepthStencil(RpsFormat format);

/// @brief Returns whether a format has a depth component.
///
/// @param format       Format to check.
///
/// @returns            RPS_TRUE if the format has a depth component, RPS_FALSE otherwise.
RpsBool rpsFormatHasDepth(RpsFormat format);

/// @brief Returns whether a format has a stencil component.
///
/// @param format       Format to check.
///
/// @returns            RPS_TRUE if the format has a stencil component, RPS_FALSE otherwise.
RpsBool rpsFormatHasStencil(RpsFormat format);

/// @brief Returns whether a format has only a depth component and no stencil component.
///
/// @param format       Format to check.
///
/// @returns            RPS_TRUE if the format has a stencil component and no stencil component, RPS_FALSE otherwise.
RpsBool rpsFormatIsDepthOnly(RpsFormat format);

/// @brief Returns the single element byte size for a format.
///
/// For most formats one element is one pixel. This is different for block compressed formats, e.g.
/// RPS_FORMAT_BC1_UNORM. The byte size of one block will be returned for these instead.
///
/// @param format       Format to check.
///
/// @returns            0 if the format does not support element wise usage, size of bytes of a single element otherwise.
uint32_t rpsGetFormatElementBytes(RpsFormat format);

/// @brief Gets the name string of a format.
///
/// @param format       Format to get its name for.
///
/// @returns            Null terminated string with the format name.
const char* rpsFormatGetName(RpsFormat format);

/// @} end defgroup RpsFormat
/// @} end addtogroup RpsRenderGraphRuntimeResource

#endif  // #ifndef RPS_FORMAT_H
