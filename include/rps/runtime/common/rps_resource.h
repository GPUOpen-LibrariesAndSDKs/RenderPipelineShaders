// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_RESOURCE_H
#define RPS_RESOURCE_H

#include "rps/core/rps_api.h"
#include "rps/runtime/common/rps_format.h"

/// @addtogroup RpsRenderGraphRuntimeResources
/// @{

//---------------------------------------------------------------------------------------
// Resource
//---------------------------------------------------------------------------------------

/// @brief Constant for an invalid resource ID.
#define RPS_RESOURCE_ID_INVALID RPS_INDEX_NONE_U32

/// @brief Type for resource identifiers.
typedef uint32_t RpsResourceId;

/// @brief Resource types used by RPS resources.
typedef enum RpsResourceType
{
    RPS_RESOURCE_TYPE_UNKNOWN = 0,  ///< Resource type is unknown / invalid.
    RPS_RESOURCE_TYPE_BUFFER,       ///< A buffer resource type.
    RPS_RESOURCE_TYPE_IMAGE_1D,     ///< A 1D image resource type.
    RPS_RESOURCE_TYPE_IMAGE_2D,     ///< A 2D image resource type.
    RPS_RESOURCE_TYPE_IMAGE_3D,     ///< A 3D image resource type.
    RPS_RESOURCE_TYPE_COUNT,        ///< Count of defined resource type values.
} RpsResourceType;

/// @brief Bitflags for special properties of a resource.
typedef enum RpsResourceFlagBits
{
    RPS_RESOURCE_FLAG_NONE                             = 0,         ///< No special properties.
    RPS_RESOURCE_FLAG_CUBEMAP_COMPATIBLE_BIT           = (1 << 1),  ///< Supports cubemap views.
    RPS_RESOURCE_FLAG_ROWMAJOR_IMAGE_BIT               = (1 << 2),  ///< Uses rowmajor image layout.
    RPS_RESOURCE_FLAG_PREFER_GPU_LOCAL_CPU_VISIBLE_BIT = (1 << 3),  ///< Preferred to be in GPU-local CPU-visible heap
                                                                    ///  if available.
    RPS_RESOURCE_FLAG_PREFER_DEDICATED_ALLOCATION_BIT = (1 << 4),   ///< Preferred to be in dedicated allocation or as
                                                                    ///  committed resource.
    RPS_RESOURCE_FLAG_PERSISTENT_BIT = (1 << 15),                   ///< Resource data is persistent from frame to
                                                                    ///  frame.
} RpsResourceFlagBits;

/// @brief Bitmask type for <c><i>RpsResourceFlagBits</i></c>.
typedef RpsFlags32 RpsResourceFlags;

/// @brief Bitflags for used aspects of an image resource.
typedef enum RpsImageAspectUsageFlagBits
{
    RPS_IMAGE_ASPECT_UNKNOWN  = 0,                       /// Image aspect usage is unknown.
    RPS_IMAGE_ASPECT_COLOR    = 1 << 0,                  /// The color aspect is used.
    RPS_IMAGE_ASPECT_DEPTH    = 1 << 1,                  /// The depth aspect is used.
    RPS_IMAGE_ASPECT_STENCIL  = 1 << 2,                  /// The stencil aspect is used.
    RPS_IMAGE_ASPECT_METADATA = 1 << 3,                  /// The metadata aspect is used.
    RPS_IMAGE_ASPECT_DEFAULT  = RPS_IMAGE_ASPECT_COLOR,  /// Default image aspect usage.
} RpsImageAspectUsageFlagBits;

/// @brief Bitmask type for <c><i>RpsImageAspectUsageFlagBits</i></c>.
typedef RpsFlags32 RpsImageAspectUsageFlags;

/// @brief RGBA color value to use for clearing a resource.
///
/// Depending on the underlying format of a resource, an appropriately
/// typed member of this union should be used.
typedef union RpsClearColorValue
{
    float    float32[4];  ///< 4-tuple of IEEE 754 floating point values representing an RGBA clear color.
    int32_t  int32[4];    ///< 4-tuple of signed integers representing an RGBA clear color.
    uint32_t uint32[4];   ///< 4-tuple of unsigned integers representing an RGBA clear color.
} RpsClearColorValue;

/// @brief Bitflags for the way a resource should be cleared.
typedef enum RpsClearFlags
{
    RPS_CLEAR_FLAG_NONE,               ///< No clear flags are specified. (Not a valid use case).
    RPS_CLEAR_FLAG_COLOR    = 1 << 0,  ///< Clears the color aspect of a render target view.
    RPS_CLEAR_FLAG_DEPTH    = 1 << 1,  ///< Clears the depth aspect of a depth stencil view.
    RPS_CLEAR_FLAG_STENCIL  = 1 << 2,  ///< Clears the stencil aspect of a depth stencil view.
    RPS_CLEAR_FLAG_UAVFLOAT = 1 << 3,  ///< Clears the UAV with floating point data.
    RPS_CLEAR_FLAG_UAVUINT  = 1 << 4,  ///< Clears the UAV with integer data.
} RpsClearFlags;

/// @brief Parameters for clearing a depth stencil resource.
typedef struct RpsClearDepthStencilValue
{
    float    depth;    ///< Clear value for the depth aspect.
    uint32_t stencil;  ///< Clear value for the stencil aspect.
} RpsClearDepthStencilValue;

/// @brief General value a resource can be cleared to.
///
/// Depending on the context and target resource view format, an appropriately
/// typed member of this union should be used.
typedef union RpsClearValue
{
    RpsClearColorValue        color;         ///< Clear value for a color resource.
    RpsClearDepthStencilValue depthStencil;  ///< Clear value for a depth stencil resource.
} RpsClearValue;

/// @brief Parameters for clearing a resource.
typedef struct RpsClearInfo
{
    RpsFormat     format;  ///< Format of the resource view to use for clearing.
    RpsClearValue value;   ///< Clear value.
} RpsClearInfo;

/// @brief Parameters for a resource description.
typedef struct RpsResourceDesc
{
    RpsResourceType  type;            ///< Resource type.
    uint32_t         temporalLayers;  ///< Number of temporal layers the resource consists of.
    RpsResourceFlags flags;           ///< Resource flags for special properties.

    union
    {
        struct
        {
            uint32_t width;   ///< Width of an image resource.
            uint32_t height;  ///< Height of an image resource.
            union
            {
                uint32_t depth;        ///< Depth of a 3D image resource.
                uint32_t arrayLayers;  ///< Number of array layers for a non-3D image resource.
            };
            uint32_t  mipLevels;    ///< Number of mipmap levels.
            RpsFormat format;       ///< Platform independent format to be interpreted by the runtime.
            uint32_t  sampleCount;  ///< Number of MSAA samples of an image.
        } image;
        struct
        {
            uint32_t sizeInBytesLo;  ///< Lower 32 bits of the size of a buffer resource in bytes.
            uint32_t sizeInBytesHi;  ///< Higher 32 bits of the size of a buffer resource in bytes.
        } buffer;
    };
} RpsResourceDesc;

/// @brief Subsection of a resource from the graphics API perspective.
typedef struct RpsSubresourceRange
{
    uint16_t baseMipLevel;    ///< First mipmapping level accessible in the range.
    uint16_t mipLevels;       ///< Number of mipmap levels in the range.
    uint32_t baseArrayLayer;  ///< First layer accessible in the range.
    uint32_t arrayLayers;     ///< Number of array layers accessible in the range.
} RpsSubresourceRange;

/// @brief Constant for maximum number of temporal layers a resource may have.
#define RPS_RESOURCE_MAX_TEMPORAL_LAYERS (256)

/// @brief Constant for the maximum number of simultaneous bound render targets supported by RPS.
#define RPS_MAX_SIMULTANEOUS_RENDER_TARGET_COUNT (8)

/// @brief Output resources for writing results of a graphics node.
typedef struct RpsCmdRenderTargetInfo
{
    uint32_t  numRenderTargets;    ///< Number of render targets used by the node.
    uint32_t  numSamples;          ///< Number of MSAA samples.
    RpsFormat depthStencilFormat;  ///< Depth stencil format or RPS_FORMAT_UNKNOWN if no depth buffer is bound.

    /// Array of render target formats with one format for each of the numRenderTargets render targets.
    RpsFormat renderTargetFormats[RPS_MAX_SIMULTANEOUS_RENDER_TARGET_COUNT];
} RpsCmdRenderTargetInfo;

#ifdef __cplusplus

namespace rps
{

    /// @brief A C++ helper type for RpsResourceDesc.
    struct ResourceDesc : public RpsResourceDesc
    {
        ResourceDesc()
            : ResourceDesc(RPS_RESOURCE_TYPE_UNKNOWN, RPS_FORMAT_UNKNOWN, 0, 0)
        {
        }

        ResourceDesc(const RpsResourceDesc& desc)
            : RpsResourceDesc(desc)
        {
        }

        ResourceDesc(RpsResourceType  inType,
                     RpsFormat        inFormat,
                     uint64_t         inWidth,
                     uint32_t         inHeight             = 1,
                     uint32_t         inDepthOrArrayLayers = 1,
                     uint32_t         inMipLevels          = 1,
                     uint32_t         inSampleCount        = 1,
                     uint32_t         inTemporalLayers     = 1,
                     RpsResourceFlags inFlags              = RPS_RESOURCE_FLAG_NONE)
        {
            type           = inType;
            temporalLayers = inTemporalLayers;
            flags          = inFlags;

            if (IsBuffer())
            {
                buffer.sizeInBytesLo = uint32_t(inWidth & UINT32_MAX);
                buffer.sizeInBytesHi = uint32_t(inWidth >> 32u);
            }
            else if (IsImage())
            {
                image.width  = uint32_t(inWidth);
                image.height = inHeight;

                if (inType == RPS_RESOURCE_TYPE_IMAGE_3D)
                    image.depth = inDepthOrArrayLayers;
                else
                    image.arrayLayers = inDepthOrArrayLayers;

                image.mipLevels   = inMipLevels;
                image.format      = inFormat;
                image.sampleCount = inSampleCount;
            }
        }

        /// @brief Checks if the resource type is a buffer.
        static bool IsBuffer(RpsResourceType type)
        {
            return type == RPS_RESOURCE_TYPE_BUFFER;
        }

        /// @brief Checks if the resource type is an image (texture).
        static bool IsImage(RpsResourceType type)
        {
            return (type == RPS_RESOURCE_TYPE_IMAGE_1D) || (type == RPS_RESOURCE_TYPE_IMAGE_2D) ||
                   (type == RPS_RESOURCE_TYPE_IMAGE_3D);
        }

        /// @brief Checks if the described resource is a buffer.
        bool IsBuffer() const
        {
            return IsBuffer(type);
        }

        /// @brief Checks if the described resource is an image (texture).
        bool IsImage() const
        {
            return IsImage(type);
        }

        /// @brief Creates a resource description for a buffer resource.
        static ResourceDesc Buffer(uint64_t         inSizeInBytes,
                                   uint32_t         inTemporalLayers = 1,
                                   RpsResourceFlags inFlags          = RPS_RESOURCE_FLAG_NONE)
        {
            return ResourceDesc(
                RPS_RESOURCE_TYPE_BUFFER, RPS_FORMAT_UNKNOWN, inSizeInBytes, 1, 1, 1, 1, inTemporalLayers, inFlags);
        }

        /// @brief Creates a resource description structure for an 1D Texture resource.
        static ResourceDesc Image1D(RpsFormat        inFormat,
                                    uint32_t         inWidth,
                                    uint32_t         inMipLevels      = 1,
                                    uint32_t         inArrayLayers    = 1,
                                    uint32_t         inTemporalLayers = 1,
                                    RpsResourceFlags inFlags          = RPS_RESOURCE_FLAG_NONE)
        {
            return ResourceDesc(RPS_RESOURCE_TYPE_IMAGE_1D,
                                inFormat,
                                inWidth,
                                1,
                                inArrayLayers,
                                inMipLevels,
                                1,
                                inTemporalLayers,
                                inFlags);
        }

        /// @brief Creates a resource description for a 2D Texture resource.
        static ResourceDesc Image2D(RpsFormat        inFormat,
                                    uint32_t         inWidth,
                                    uint32_t         inHeight,
                                    uint32_t         inArrayLayers    = 1,
                                    uint32_t         inMipLevels      = 1,
                                    uint32_t         inSampleCount    = 1,
                                    uint32_t         inTemporalLayers = 1,
                                    RpsResourceFlags inFlags          = RPS_RESOURCE_FLAG_NONE)
        {
            return ResourceDesc(RPS_RESOURCE_TYPE_IMAGE_2D,
                                inFormat,
                                inWidth,
                                inHeight,
                                inArrayLayers,
                                inMipLevels,
                                inSampleCount,
                                inTemporalLayers,
                                inFlags);
        }

        /// @brief Creates a resource description for a 3D Texture resource.
        static ResourceDesc Image3D(RpsFormat        inFormat,
                                    uint32_t         inWidth,
                                    uint32_t         inHeight,
                                    uint32_t         inDepth,
                                    uint32_t         inMipLevels      = 1,
                                    uint32_t         inTemporalLayers = 1,
                                    RpsResourceFlags inFlags          = RPS_RESOURCE_FLAG_NONE)
        {
            return ResourceDesc(RPS_RESOURCE_TYPE_IMAGE_3D,
                                inFormat,
                                inWidth,
                                inHeight,
                                inDepth,
                                inMipLevels,
                                1,
                                inTemporalLayers,
                                inFlags);
        }
    };

    /// @brief C++ helper type for RpsSubresourceRange.
    struct SubresourceRange : public RpsSubresourceRange
    {
        SubresourceRange(uint16_t inBaseMip        = 0,
                         uint16_t inMipLevels      = 1,
                         uint32_t inBaseArrayLayer = 0,
                         uint32_t inArrayLayers    = 1)
        {
            baseArrayLayer = inBaseArrayLayer;
            arrayLayers    = inArrayLayers;
            baseMipLevel   = inBaseMip;
            mipLevels      = inMipLevels;
        }
    };

}  // namespace rps

#endif  //__cplusplus

/// @} end addtogroup RpsRenderGraphRuntimeResources

#endif  //RPS_RESOURCE_H
