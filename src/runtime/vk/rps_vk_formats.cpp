// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "rps/runtime/common/rps_format.h"
#include "rps/runtime/vk/rps_vk_runtime.h"

#include "core/rps_util.hpp"


VkFormat rpsFormatToVK(RpsFormat rpsFmt)
{
    static const VkFormat formatMap[] = {
        VK_FORMAT_UNDEFINED,                    // RPS_FORMAT_UNKNOWN,                                 ///< The format is unknown.
        VK_FORMAT_R32G32B32A32_UINT,            // RPS_FORMAT_R32G32B32A32_TYPELESS,                   ///< A 4-channel, RGBA format where each channel is a typeless 32bit value.
        VK_FORMAT_R32G32B32A32_SFLOAT,          // RPS_FORMAT_R32G32B32A32_FLOAT,                      ///< A 4-channel, RGBA format where each channel is a 32bit IEEE 754 floating point value.
        VK_FORMAT_R32G32B32A32_UINT,            // RPS_FORMAT_R32G32B32A32_UINT,                       ///< A 4-channel, RGBA format where each channel is a 32bit unsigned integer.
        VK_FORMAT_R32G32B32A32_SINT,            // RPS_FORMAT_R32G32B32A32_SINT,                       ///< A 4-channel, RGBA format where each channel is a 32bit signed integer.
        VK_FORMAT_R32G32B32_UINT,               // RPS_FORMAT_R32G32B32_TYPELESS,                      ///< A 3-channel, RGB format where each channel is a typeless 32bit value.
        VK_FORMAT_R32G32B32_SFLOAT,             // RPS_FORMAT_R32G32B32_FLOAT,                         ///< A 3-channel, RGB format where each channel is a 32bit IEEE 754 floating point value.
        VK_FORMAT_R32G32B32_UINT,               // RPS_FORMAT_R32G32B32_UINT,                          ///< A 3-channel, RGB format where each channel is a 32bit unsigned integer.
        VK_FORMAT_R32G32B32_SINT,               // RPS_FORMAT_R32G32B32_SINT,                          ///< A 3-channel, RGB format where each channel is a 32bit signed integer.
        VK_FORMAT_R16G16B16A16_UINT,            // RPS_FORMAT_R16G16B16A16_TYPELESS,                   ///< A 4-channel, RGBA format where each channel is a typeless 16bit value.
        VK_FORMAT_R16G16B16A16_SFLOAT,          // RPS_FORMAT_R16G16B16A16_FLOAT,                      ///< A 4-channel, RGBA format where each channel is a 16bit floating point value.
        VK_FORMAT_R16G16B16A16_UNORM,           // RPS_FORMAT_R16G16B16A16_UNORM,                      ///< A 4-channel, RGBA format where each channel is a 16bit normalized, unsigned integer.
        VK_FORMAT_R16G16B16A16_UINT,            // RPS_FORMAT_R16G16B16A16_UINT,                       ///< A 4-channel, RGBA format where each channel is a 16bit unsigned integer.
        VK_FORMAT_R16G16B16A16_SNORM,           // RPS_FORMAT_R16G16B16A16_SNORM,                      ///< A 4-channel, RGBA format where each channel is a 16bit normalized, signed integer.
        VK_FORMAT_R16G16B16A16_SINT,            // RPS_FORMAT_R16G16B16A16_SINT,                       ///< A 4-channel, RGBA format where each channel is a 16bit signed integer.
        VK_FORMAT_R32G32_UINT,                  // RPS_FORMAT_R32G32_TYPELESS,                         ///< A 2-channel, RG format where each channel is a typeless 32bit value.
        VK_FORMAT_R32G32_SFLOAT,                // RPS_FORMAT_R32G32_FLOAT,                            ///< A 2-channel, RG format where each channel is a 32bit IEEE 754 floating point value.
        VK_FORMAT_R32G32_UINT,                  // RPS_FORMAT_R32G32_UINT,                             ///< A 2-channel, RG format where each channel is a 32bit unsigned integer.
        VK_FORMAT_R32G32_SINT,                  // RPS_FORMAT_R32G32_SINT,                             ///< A 2-channel, RG format where each channel is a 32bit signed integer.
        VK_FORMAT_D32_SFLOAT_S8_UINT,           // RPS_FORMAT_R32G8X24_TYPELESS,                       ///< A 2-channel, RG format where the first channel is a typeless 32bit value, and the second channel is a 8bit channel.
        VK_FORMAT_D32_SFLOAT_S8_UINT,           // RPS_FORMAT_D32_FLOAT_S8X24_UINT,                    ///< A 1-channel, depth format where the channel contains a signed 8bit value and 24 unused.
        VK_FORMAT_D32_SFLOAT_S8_UINT,           // RPS_FORMAT_R32_FLOAT_X8X24_TYPELESS,                ///<
        VK_FORMAT_D32_SFLOAT_S8_UINT,           // RPS_FORMAT_X32_TYPELESS_G8X24_UINT,                 ///<
        VK_FORMAT_A2R10G10B10_UINT_PACK32,      // RPS_FORMAT_R10G10B10A2_TYPELESS,                    ///< A 4-channel, RGBA format where the RGB channels are typeless 10bit values, and the A channel is a typeless 2bit channel.
        VK_FORMAT_A2R10G10B10_UNORM_PACK32,     // RPS_FORMAT_R10G10B10A2_UNORM,                       ///< A 4-channel, RGBA format where the RGB channels are 10bit normalized, unsigned integer values, and the A channel is a 2bit channel.
        VK_FORMAT_A2R10G10B10_UINT_PACK32,      // RPS_FORMAT_R10G10B10A2_UINT,                        ///< A 4-channel, RGBA format where the RGB channels are 10bit unsigned integer values, and the A channel is a 2bit channel.
        VK_FORMAT_B10G11R11_UFLOAT_PACK32,      // RPS_FORMAT_R11G11B10_FLOAT,                         ///< A 3-channel, RGB format where the RG channels are 11bit floating point values, and the B channel is a 10bit floating point value.
        VK_FORMAT_R8G8B8A8_UINT,                // RPS_FORMAT_R8G8B8A8_TYPELESS,                       ///< A 4-channel, RGBA format where each channel is typeless 8bit value.
        VK_FORMAT_R8G8B8A8_UNORM,               // RPS_FORMAT_R8G8B8A8_UNORM,                          ///< A 4-channel, RGBA format where each channel is 8bit normalized, unsigned integer value.
        VK_FORMAT_A8B8G8R8_SRGB_PACK32,         // RPS_FORMAT_R8G8B8A8_UNORM_SRGB,                     ///< A 4-channel, RGBA format where each channel is 8bit normalized, unsigned integer SRGB value.
        VK_FORMAT_R8G8B8A8_UINT,                // RPS_FORMAT_R8G8B8A8_UINT,                           ///< A 4-channel, RGBA format where each channel is 8bit unsigned integer value.
        VK_FORMAT_R8G8B8A8_SNORM,               // RPS_FORMAT_R8G8B8A8_SNORM,                          ///< A 4-channel, RGBA format where each channel is 8bit normalized, signed integer value.
        VK_FORMAT_R8G8B8A8_SINT,                // RPS_FORMAT_R8G8B8A8_SINT,                           ///< A 4-channel, RGBA format where each channel is 8bit signed integer value.
        VK_FORMAT_R16G16_UINT,                  // RPS_FORMAT_R16G16_TYPELESS,                         ///< A 2-channel, RG format where each channel is typeless 16bit value.
        VK_FORMAT_R16G16_SFLOAT,                // RPS_FORMAT_R16G16_FLOAT,                            ///< A 2-channel, RG format where each channel is 16bit IEEE 754 floating point value.
        VK_FORMAT_R16G16_UNORM,                 // RPS_FORMAT_R16G16_UNORM,                            ///< A 2-channel, RG format where each channel is 16bit normalized, unsigned integer value.
        VK_FORMAT_R16G16_UINT,                  // RPS_FORMAT_R16G16_UINT,                             ///< A 2-channel, RG format where each channel is 16bit unsigned integer value.
        VK_FORMAT_R16G16_SNORM,                 // RPS_FORMAT_R16G16_SNORM,                            ///< A 2-channel, RG format where each channel is 16bit normalized, signed integer value.
        VK_FORMAT_R16G16_SINT,                  // RPS_FORMAT_R16G16_SINT,                             ///< A 2-channel, RG format where each channel is 16bit signed integer value.
        VK_FORMAT_R32_UINT,                     // RPS_FORMAT_R32_TYPELESS,                            ///< A single channel, R format where the channel is a typeless 32bit value.
        VK_FORMAT_D32_SFLOAT,                   // RPS_FORMAT_D32_FLOAT,                               ///< A single channel, R format where the channel is a 32bit depth value.
        VK_FORMAT_R32_SFLOAT,                   // RPS_FORMAT_R32_FLOAT,                               ///< A single channel, R format where the channel is a 32bit IEEE 754 floating point value.
        VK_FORMAT_R32_UINT,                     // RPS_FORMAT_R32_UINT,                                ///< A single channel, R format where the channel is a 32bit unsigned integer value.
        VK_FORMAT_R32_SINT,                     // RPS_FORMAT_R32_SINT,                                ///< A single channel, R format where the channel is a 32bit signed integer value.
        VK_FORMAT_D24_UNORM_S8_UINT,            // RPS_FORMAT_R24G8_TYPELESS,                          ///<
        VK_FORMAT_D24_UNORM_S8_UINT,            // RPS_FORMAT_D24_UNORM_S8_UINT,                       ///<
        VK_FORMAT_D24_UNORM_S8_UINT,            // RPS_FORMAT_R24_UNORM_X8_TYPELESS,                   ///<
        VK_FORMAT_D24_UNORM_S8_UINT,            // RPS_FORMAT_X24_TYPELESS_G8_UINT,                    ///<
        VK_FORMAT_R8G8_UINT,                    // RPS_FORMAT_R8G8_TYPELESS,                           ///< A 2-channel, RG format where each channel is typeless 8bit value.
        VK_FORMAT_R8G8_UNORM,                   // RPS_FORMAT_R8G8_UNORM,                              ///< A 2-channel, RG format where each channel is 8bit normalized, unsigned integer value.
        VK_FORMAT_R8G8_UINT,                    // RPS_FORMAT_R8G8_UINT,                               ///< A 2-channel, RG format where each channel is 8bit unsigned integer value.
        VK_FORMAT_R8G8_SNORM,                   // RPS_FORMAT_R8G8_SNORM,                              ///< A 2-channel, RG format where each channel is 8bit normalized, signed integer value.
        VK_FORMAT_R8G8_SINT,                    // RPS_FORMAT_R8G8_SINT,                               ///< A 2-channel, RG format where each channel is 8bit signed integer value.
        VK_FORMAT_R16_UINT,                     // RPS_FORMAT_R16_TYPELESS,                            ///< A single channel, R format where the channel is a typeless 16bit value.
        VK_FORMAT_R16_SFLOAT,                   // RPS_FORMAT_R16_FLOAT,                               ///< A single channel, R format where the channel is a 16bit depth value.
        VK_FORMAT_D16_UNORM,                    // RPS_FORMAT_D16_UNORM,                               ///< A single channel, R format where the channel is a 16bit IEEE 754 floating point value.
        VK_FORMAT_R16_UNORM,                    // RPS_FORMAT_R16_UNORM,                               ///< A single channel, R format where the channel is a 16bit unsigned integer value.
        VK_FORMAT_R16_UINT,                     // RPS_FORMAT_R16_UINT,                                ///< A single channel, R format where the channel is a 16bit signed integer value.
        VK_FORMAT_R16_SNORM,                    // RPS_FORMAT_R16_SNORM,                               ///< A single channel, R format where the channel is a 16bit normalized, signed integer value.
        VK_FORMAT_R16_SINT,                     // RPS_FORMAT_R16_SINT,                                ///< A single channel, R format where the channel is a 16bit signed integer value.
        VK_FORMAT_R8_UINT,                      // RPS_FORMAT_R8_TYPELESS,                             ///< A single channel, R format where the channel is a typeless 8bit value.
        VK_FORMAT_R8_UNORM,                     // RPS_FORMAT_R8_UNORM,                                ///< A single channel, R format where the channel is a 8bit unsigned integer value.
        VK_FORMAT_R8_UINT,                      // RPS_FORMAT_R8_UINT,                                 ///< A single channel, R format where the channel is a 8bit signed integer value.
        VK_FORMAT_R8_SNORM,                     // RPS_FORMAT_R8_SNORM,                                ///< A single channel, R format where the channel is a 8bit normalized, signed integer value.
        VK_FORMAT_R8_SINT,                      // RPS_FORMAT_R8_SINT,                                 ///< A single channel, R format where the channel is a 8bit signed integer value.
        VK_FORMAT_R8_UNORM,                     // RPS_FORMAT_A8_UNORM,                                ///< A single channel, A format where the channel is a 8bit unsigned integer value.
        VK_FORMAT_UNDEFINED,                    // RPS_FORMAT_R1_UNORM,                                ///< A single channel, R format where the channel is a 1bit unsigned integer value.
        VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,       // RPS_FORMAT_R9G9B9E5_SHAREDEXP,                      ///<
        VK_FORMAT_B8G8R8G8_422_UNORM_KHR,       // RPS_FORMAT_R8G8_B8G8_UNORM,                         ///<
        VK_FORMAT_G8B8G8R8_422_UNORM_KHR,       // RPS_FORMAT_G8R8_G8B8_UNORM,                         ///<
        VK_FORMAT_BC1_RGBA_UNORM_BLOCK,         // RPS_FORMAT_BC1_TYPELESS,                            ///<
        VK_FORMAT_BC1_RGBA_UNORM_BLOCK,         // RPS_FORMAT_BC1_UNORM,                               ///<
        VK_FORMAT_BC1_RGBA_SRGB_BLOCK,          // RPS_FORMAT_BC1_UNORM_SRGB,                          ///<
        VK_FORMAT_BC2_UNORM_BLOCK,              // RPS_FORMAT_BC2_TYPELESS,                            ///<
        VK_FORMAT_BC2_UNORM_BLOCK,              // RPS_FORMAT_BC2_UNORM,                               ///<
        VK_FORMAT_BC2_SRGB_BLOCK,               // RPS_FORMAT_BC2_UNORM_SRGB,                          ///<
        VK_FORMAT_BC3_UNORM_BLOCK,              // RPS_FORMAT_BC3_TYPELESS,                            ///<
        VK_FORMAT_BC3_UNORM_BLOCK,              // RPS_FORMAT_BC3_UNORM,                               ///<
        VK_FORMAT_BC3_SRGB_BLOCK,               // RPS_FORMAT_BC3_UNORM_SRGB,                          ///<
        VK_FORMAT_BC4_UNORM_BLOCK,              // RPS_FORMAT_BC4_TYPELESS,                            ///<
        VK_FORMAT_BC4_UNORM_BLOCK,              // RPS_FORMAT_BC4_UNORM,                               ///<
        VK_FORMAT_BC4_SNORM_BLOCK,              // RPS_FORMAT_BC4_SNORM,                               ///<
        VK_FORMAT_BC5_UNORM_BLOCK,              // RPS_FORMAT_BC5_TYPELESS,                            ///<
        VK_FORMAT_BC5_UNORM_BLOCK,              // RPS_FORMAT_BC5_UNORM,                               ///<
        VK_FORMAT_BC5_SNORM_BLOCK,              // RPS_FORMAT_BC5_SNORM,                               ///<
        VK_FORMAT_R5G6B5_UNORM_PACK16,          // RPS_FORMAT_B5G6R5_UNORM,                            ///<
        VK_FORMAT_A1R5G5B5_UNORM_PACK16,        // RPS_FORMAT_B5G5R5A1_UNORM,                          ///<
        VK_FORMAT_B8G8R8A8_UNORM,               // RPS_FORMAT_B8G8R8A8_UNORM,                          ///<
        VK_FORMAT_B8G8R8A8_UNORM,               // RPS_FORMAT_B8G8R8X8_UNORM,                          ///<
        VK_FORMAT_UNDEFINED,                    // RPS_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,              ///<
        VK_FORMAT_B8G8R8A8_UINT,                // RPS_FORMAT_B8G8R8A8_TYPELESS,                       ///<
        VK_FORMAT_B8G8R8A8_SRGB,                // RPS_FORMAT_B8G8R8A8_UNORM_SRGB,                     ///<
        VK_FORMAT_B8G8R8A8_UNORM,               // RPS_FORMAT_B8G8R8X8_TYPELESS,                       ///<
        VK_FORMAT_B8G8R8A8_SRGB,                // RPS_FORMAT_B8G8R8X8_UNORM_SRGB,                     ///<
        VK_FORMAT_BC6H_UFLOAT_BLOCK,            // RPS_FORMAT_BC6H_TYPELESS,                           ///<
        VK_FORMAT_BC6H_UFLOAT_BLOCK,            // RPS_FORMAT_BC6H_UF16,                               ///<
        VK_FORMAT_BC6H_SFLOAT_BLOCK,            // RPS_FORMAT_BC6H_SF16,                               ///<
        VK_FORMAT_BC7_UNORM_BLOCK,              // RPS_FORMAT_BC7_TYPELESS,                            ///<
        VK_FORMAT_BC7_UNORM_BLOCK,              // RPS_FORMAT_BC7_UNORM,                               ///<
        VK_FORMAT_BC7_SRGB_BLOCK,               // RPS_FORMAT_BC7_UNORM_SRGB,                          ///<
        VK_FORMAT_UNDEFINED,                    // RPS_FORMAT_AYUV,                                    ///<
        VK_FORMAT_UNDEFINED,                    // RPS_FORMAT_Y410,                                    ///<
        VK_FORMAT_UNDEFINED,                    // RPS_FORMAT_Y416,                                    ///<
        VK_FORMAT_UNDEFINED,                    // RPS_FORMAT_NV12,                                    ///<
        VK_FORMAT_UNDEFINED,                    // RPS_FORMAT_P010,                                    ///<
        VK_FORMAT_UNDEFINED,                    // RPS_FORMAT_P016,                                    ///<
        VK_FORMAT_UNDEFINED,                    // RPS_FORMAT_420_OPAQUE,                              ///<
        VK_FORMAT_UNDEFINED,                    // RPS_FORMAT_YUY2,                                    ///<
        VK_FORMAT_UNDEFINED,                    // RPS_FORMAT_Y210,                                    ///<
        VK_FORMAT_UNDEFINED,                    // RPS_FORMAT_Y216,                                    ///<
        VK_FORMAT_UNDEFINED,                    // RPS_FORMAT_NV11,                                    ///<
        VK_FORMAT_UNDEFINED,                    // RPS_FORMAT_AI44,                                    ///<
        VK_FORMAT_UNDEFINED,                    // RPS_FORMAT_IA44,                                    ///<
        VK_FORMAT_UNDEFINED,                    // RPS_FORMAT_P8,                                      ///<
        VK_FORMAT_UNDEFINED,                    // RPS_FORMAT_A8P8,                                    ///<
        VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT,    // RPS_FORMAT_B4G4R4A4_UNORM,                          ///<
    };

    static_assert(RPS_COUNTOF(formatMap) == RPS_FORMAT_COUNT, "RpsFormat to VkFormat map needs update");

    return (uint32_t)rpsFmt < RPS_COUNTOF(formatMap) ? formatMap[(uint32_t)rpsFmt] : VK_FORMAT_UNDEFINED;
}

RpsFormat rpsFormatFromVK(VkFormat vkFormat)
{
    static const RpsFormat formatMap[] = {
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_UNDEFINED
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R4G4_UNORM_PACK8
        RPS_FORMAT_B4G4R4A4_UNORM,              // VK_FORMAT_R4G4B4A4_UNORM_PACK16
        RPS_FORMAT_B4G4R4A4_UNORM,              // VK_FORMAT_B4G4R4A4_UNORM_PACK16
        RPS_FORMAT_B5G6R5_UNORM,                // VK_FORMAT_R5G6B5_UNORM_PACK16
        RPS_FORMAT_B5G6R5_UNORM,                // VK_FORMAT_B5G6R5_UNORM_PACK16
        RPS_FORMAT_B5G5R5A1_UNORM,              // VK_FORMAT_R5G5B5A1_UNORM_PACK16
        RPS_FORMAT_B5G5R5A1_UNORM,              // VK_FORMAT_B5G5R5A1_UNORM_PACK16
        RPS_FORMAT_B5G5R5A1_UNORM,              // VK_FORMAT_A1R5G5B5_UNORM_PACK16
        RPS_FORMAT_R8_UNORM,                    // VK_FORMAT_R8_UNORM
        RPS_FORMAT_R8_SNORM,                    // VK_FORMAT_R8_SNORM
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R8_USCALED
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R8_SSCALED
        RPS_FORMAT_R8_UINT,                     // VK_FORMAT_R8_UINT
        RPS_FORMAT_R8_SINT,                     // VK_FORMAT_R8_SINT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R8_SRGB
        RPS_FORMAT_R8G8_UNORM,                  // VK_FORMAT_R8G8_UNORM
        RPS_FORMAT_R8G8_SNORM,                  // VK_FORMAT_R8G8_SNORM
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R8G8_USCALED
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R8G8_SSCALED
        RPS_FORMAT_R8G8_UINT,                   // VK_FORMAT_R8G8_UINT
        RPS_FORMAT_R8G8_SINT,                   // VK_FORMAT_R8G8_SINT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R8G8_SRGB
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R8G8B8_UNORM
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R8G8B8_SNORM
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R8G8B8_USCALED
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R8G8B8_SSCALED
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R8G8B8_UINT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R8G8B8_SINT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R8G8B8_SRGB
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_B8G8R8_UNORM
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_B8G8R8_SNORM
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_B8G8R8_USCALED
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_B8G8R8_SSCALED
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_B8G8R8_UINT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_B8G8R8_SINT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_B8G8R8_SRGB
        RPS_FORMAT_R8G8B8A8_UNORM,              // VK_FORMAT_R8G8B8A8_UNORM
        RPS_FORMAT_R8G8B8A8_SNORM,              // VK_FORMAT_R8G8B8A8_SNORM
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R8G8B8A8_USCALED
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R8G8B8A8_SSCALED
        RPS_FORMAT_R8G8B8A8_UINT,               // VK_FORMAT_R8G8B8A8_UINT
        RPS_FORMAT_R8G8B8A8_SINT,               // VK_FORMAT_R8G8B8A8_SINT
        RPS_FORMAT_R8G8B8A8_UNORM_SRGB,         // VK_FORMAT_R8G8B8A8_SRGB
        RPS_FORMAT_B8G8R8A8_UNORM,              // VK_FORMAT_B8G8R8A8_UNORM
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_B8G8R8A8_SNORM
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_B8G8R8A8_USCALED
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_B8G8R8A8_SSCALED
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_B8G8R8A8_UINT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_B8G8R8A8_SINT
        RPS_FORMAT_B8G8R8A8_UNORM_SRGB,         // VK_FORMAT_B8G8R8A8_SRGB
        RPS_FORMAT_R8G8B8A8_UNORM,              // VK_FORMAT_A8B8G8R8_UNORM_PACK32
        RPS_FORMAT_R8G8B8A8_SNORM,              // VK_FORMAT_A8B8G8R8_SNORM_PACK32
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_A8B8G8R8_USCALED_PACK32
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_A8B8G8R8_SSCALED_PACK32
        RPS_FORMAT_R8G8B8A8_UINT,               // VK_FORMAT_A8B8G8R8_UINT_PACK32
        RPS_FORMAT_R8G8B8A8_SINT,               // VK_FORMAT_A8B8G8R8_SINT_PACK32
        RPS_FORMAT_R8G8B8A8_UNORM_SRGB,         // VK_FORMAT_A8B8G8R8_SRGB_PACK32
        RPS_FORMAT_R10G10B10A2_UNORM,           // VK_FORMAT_A2R10G10B10_UNORM_PACK32
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_A2R10G10B10_SNORM_PACK32
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_A2R10G10B10_USCALED_PACK32
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_A2R10G10B10_SSCALED_PACK32
        RPS_FORMAT_R10G10B10A2_UINT,            // VK_FORMAT_A2R10G10B10_UINT_PACK32
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_A2R10G10B10_SINT_PACK32
        RPS_FORMAT_R10G10B10A2_UNORM,           // VK_FORMAT_A2B10G10R10_UNORM_PACK32
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_A2B10G10R10_SNORM_PACK32
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_A2B10G10R10_USCALED_PACK32
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_A2B10G10R10_SSCALED_PACK32
        RPS_FORMAT_R10G10B10A2_UINT,            // VK_FORMAT_A2B10G10R10_UINT_PACK32
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_A2B10G10R10_SINT_PACK32
        RPS_FORMAT_R16_UNORM,                   // VK_FORMAT_R16_UNORM
        RPS_FORMAT_R16_SNORM,                   // VK_FORMAT_R16_SNORM
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R16_USCALED
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R16_SSCALED
        RPS_FORMAT_R16_UINT,                    // VK_FORMAT_R16_UINT
        RPS_FORMAT_R16_SINT,                    // VK_FORMAT_R16_SINT
        RPS_FORMAT_R16_FLOAT,                   // VK_FORMAT_R16_SFLOAT
        RPS_FORMAT_R16G16_UNORM,                // VK_FORMAT_R16G16_UNORM
        RPS_FORMAT_R16G16_SNORM,                // VK_FORMAT_R16G16_SNORM
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R16G16_USCALED
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R16G16_SSCALED
        RPS_FORMAT_R16G16_UINT,                 // VK_FORMAT_R16G16_UINT
        RPS_FORMAT_R16G16_SINT,                 // VK_FORMAT_R16G16_SINT
        RPS_FORMAT_R16G16_FLOAT,                // VK_FORMAT_R16G16_SFLOAT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R16G16B16_UNORM
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R16G16B16_SNORM
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R16G16B16_USCALED
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R16G16B16_SSCALED
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R16G16B16_UINT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R16G16B16_SINT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R16G16B16_SFLOAT
        RPS_FORMAT_R16G16B16A16_UNORM,          // VK_FORMAT_R16G16B16A16_UNORM
        RPS_FORMAT_R16G16B16A16_SNORM,          // VK_FORMAT_R16G16B16A16_SNORM
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R16G16B16A16_USCALED
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R16G16B16A16_SSCALED
        RPS_FORMAT_R16G16B16A16_UINT,           // VK_FORMAT_R16G16B16A16_UINT
        RPS_FORMAT_R16G16B16A16_SINT,           // VK_FORMAT_R16G16B16A16_SINT
        RPS_FORMAT_R16G16B16A16_FLOAT,          // VK_FORMAT_R16G16B16A16_SFLOAT
        RPS_FORMAT_R32_UINT,                    // VK_FORMAT_R32_UINT
        RPS_FORMAT_R32_SINT,                    // VK_FORMAT_R32_SINT
        RPS_FORMAT_R32_FLOAT,                   // VK_FORMAT_R32_SFLOAT
        RPS_FORMAT_R32G32_UINT,                 // VK_FORMAT_R32G32_UINT
        RPS_FORMAT_R32G32_SINT,                 // VK_FORMAT_R32G32_SINT
        RPS_FORMAT_R32G32_FLOAT,                // VK_FORMAT_R32G32_SFLOAT
        RPS_FORMAT_R32G32B32_UINT,              // VK_FORMAT_R32G32B32_UINT
        RPS_FORMAT_R32G32B32_SINT,              // VK_FORMAT_R32G32B32_SINT
        RPS_FORMAT_R32G32B32_FLOAT,             // VK_FORMAT_R32G32B32_SFLOAT
        RPS_FORMAT_R32G32B32A32_UINT,           // VK_FORMAT_R32G32B32A32_UINT
        RPS_FORMAT_R32G32B32A32_SINT,           // VK_FORMAT_R32G32B32A32_SINT
        RPS_FORMAT_R32G32B32A32_FLOAT,          // VK_FORMAT_R32G32B32A32_SFLOAT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R64_UINT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R64_SINT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R64_SFLOAT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R64G64_UINT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R64G64_SINT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R64G64_SFLOAT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R64G64B64_UINT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R64G64B64_SINT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R64G64B64_SFLOAT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R64G64B64A64_UINT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R64G64B64A64_SINT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_R64G64B64A64_SFLOAT
        RPS_FORMAT_R11G11B10_FLOAT,             // VK_FORMAT_B10G11R11_UFLOAT_PACK32
        RPS_FORMAT_R9G9B9E5_SHAREDEXP,          // VK_FORMAT_E5B9G9R9_UFLOAT_PACK32
        RPS_FORMAT_D16_UNORM,                   // VK_FORMAT_D16_UNORM
        RPS_FORMAT_D24_UNORM_S8_UINT,           // VK_FORMAT_X8_D24_UNORM_PACK32
        RPS_FORMAT_D32_FLOAT,                   // VK_FORMAT_D32_SFLOAT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_S8_UINT
        RPS_FORMAT_UNKNOWN,                     // VK_FORMAT_D16_UNORM_S8_UINT
        RPS_FORMAT_D24_UNORM_S8_UINT,           // VK_FORMAT_D24_UNORM_S8_UINT
        RPS_FORMAT_D32_FLOAT_S8X24_UINT,        // VK_FORMAT_D32_SFLOAT_S8_UINT
        RPS_FORMAT_BC1_UNORM,                   // VK_FORMAT_BC1_RGB_UNORM_BLOCK
        RPS_FORMAT_BC1_UNORM_SRGB,              // VK_FORMAT_BC1_RGB_SRGB_BLOCK
        RPS_FORMAT_BC1_UNORM,                   // VK_FORMAT_BC1_RGBA_UNORM_BLOCK
        RPS_FORMAT_BC1_UNORM_SRGB,              // VK_FORMAT_BC1_RGBA_SRGB_BLOCK
        RPS_FORMAT_BC2_UNORM,                   // VK_FORMAT_BC2_UNORM_BLOCK
        RPS_FORMAT_BC2_UNORM_SRGB,              // VK_FORMAT_BC2_SRGB_BLOCK
        RPS_FORMAT_BC3_UNORM,                   // VK_FORMAT_BC3_UNORM_BLOCK
        RPS_FORMAT_BC3_UNORM_SRGB,              // VK_FORMAT_BC3_SRGB_BLOCK
        RPS_FORMAT_BC4_UNORM,                   // VK_FORMAT_BC4_UNORM_BLOCK
        RPS_FORMAT_BC4_SNORM,                   // VK_FORMAT_BC4_SNORM_BLOCK
        RPS_FORMAT_BC5_UNORM,                   // VK_FORMAT_BC5_UNORM_BLOCK
        RPS_FORMAT_BC5_SNORM,                   // VK_FORMAT_BC5_SNORM_BLOCK
        RPS_FORMAT_BC6H_UF16,                   // VK_FORMAT_BC6H_UFLOAT_BLOCK
        RPS_FORMAT_BC6H_SF16,                   // VK_FORMAT_BC6H_SFLOAT_BLOCK
        RPS_FORMAT_BC7_UNORM,                   // VK_FORMAT_BC7_UNORM_BLOCK
        RPS_FORMAT_BC7_UNORM_SRGB,              // VK_FORMAT_BC7_SRGB_BLOCK
    };

    return (uint32_t)vkFormat < RPS_COUNTOF(formatMap) ? formatMap[(uint32_t)vkFormat] : RPS_FORMAT_UNKNOWN;
}