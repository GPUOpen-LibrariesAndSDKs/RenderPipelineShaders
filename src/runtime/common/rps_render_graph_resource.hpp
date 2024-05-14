// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_RENDER_GRAPH_RESOURCE_HPP
#define RPS_RENDER_GRAPH_RESOURCE_HPP

#include "core/rps_core.hpp"
#include "core/rps_util.hpp"

#include "runtime/common/rps_cmd_buf.hpp"

#include "rps/runtime/common/rps_runtime.h"

namespace rps
{
    struct ResourceDecl
    {
        RpsVariable desc;
        StrRef      name;
    };

    struct ResourceDescPacked
    {
        RpsResourceType  type : 8;            ///< An enumeration indicating the type (and dimension) of the resource.
        uint32_t         temporalLayers : 8;  ///< The number of frames of temporal data.
        RpsResourceFlags flags : 16;          ///< A collection of <c><i>RpsResourceFlagBits</i></c> values.

        union
        {
            struct
            {
                uint32_t width;   ///< The width of an image, or low 32 bit of the byte size of a buffer.
                uint32_t height;  ///< The height of an image, or high 32 bit of the byte size of a buffer.
                union
                {
                    uint32_t depth;        ///< The depth of an 3D image.
                    uint32_t arrayLayers;  ///< The number of array layers for an non-3D image.
                };
                uint32_t  mipLevels : 8;    ///< The number of mipmap levels.
                RpsFormat format : 8;       ///< A platform independent format to be interepreted by the runtime.
                uint32_t  sampleCount : 8;  ///< The number of MSAA samples of an image.
            } image;
            struct
            {
                uint32_t sizeInBytesLo;
                uint32_t sizeInBytesHi;
            } buffer;
        };

        ResourceDescPacked()
            : type(RPS_RESOURCE_TYPE_UNKNOWN)
            , temporalLayers(0)
            , flags(RPS_RESOURCE_FLAG_NONE)
            , image{}
        {
        }

        ResourceDescPacked(const RpsResourceDesc& desc)
        {
            type           = desc.type;
            temporalLayers = (desc.temporalLayers == 0) ? 1 : desc.temporalLayers;
            flags          = desc.flags;

            if (IsBuffer())
            {
                buffer.sizeInBytesLo = desc.buffer.sizeInBytesLo;
                buffer.sizeInBytesHi = desc.buffer.sizeInBytesHi;
            }
            else if (IsImage())
            {
                image.width  = desc.image.width;
                image.height = desc.image.height;

                if (desc.type == RPS_RESOURCE_TYPE_IMAGE_3D)
                {
                    image.depth = desc.image.depth;
                }
                else
                {
                    image.arrayLayers = desc.image.arrayLayers;
                }

                image.mipLevels   = desc.image.mipLevels;
                image.format      = desc.image.format;
                image.sampleCount = desc.image.sampleCount;
            }
            else
            {
                // Clear the slot when it becomes inactive.
                *this = ResourceDescPacked();
            }
        }

        bool operator==(const ResourceDescPacked& rhs) const
        {
            return (type == rhs.type) && (temporalLayers == rhs.temporalLayers) && (flags == rhs.flags) &&
                   (IsImage() ? ((image.width == rhs.image.width) && (image.height == rhs.image.height) &&
                                 (image.depth == rhs.image.depth) && (image.mipLevels == rhs.image.mipLevels) &&
                                 (image.format == rhs.image.format) && (image.sampleCount == rhs.image.sampleCount))
                              : ((buffer.sizeInBytesLo == rhs.buffer.sizeInBytesLo) &&
                                 (buffer.sizeInBytesHi == rhs.buffer.sizeInBytesHi)));
        }

        bool operator!=(const ResourceDescPacked& rhs) const
        {
            return !(*this == rhs);
        }

        bool IsBuffer() const
        {
            return ResourceDesc::IsBuffer(type);
        }

        bool IsImage() const
        {
            return ResourceDesc::IsImage(type);
        }

        uint64_t GetBufferSize() const
        {
            RPS_ASSERT(IsBuffer());
            return (uint64_t(buffer.sizeInBytesHi) << 32u) | buffer.sizeInBytesLo;
        }

        void SetBufferSize(uint64_t newSize)
        {
            RPS_ASSERT(IsBuffer());
            buffer.sizeInBytesLo = uint32_t(newSize & UINT32_MAX);
            buffer.sizeInBytesHi = uint32_t(newSize >> 32u);
        }

        RpsFormat GetFormat() const
        {
            return IsBuffer() ? RPS_FORMAT_UNKNOWN : image.format;
        }

        uint32_t GetSampleCount() const
        {
            return IsBuffer() ? 1 : image.sampleCount;
        }

        uint32_t GetImageDepth() const
        {
            return (type == RPS_RESOURCE_TYPE_IMAGE_3D) ? image.depth : 1;
        }

        uint32_t GetImageArrayLayers() const
        {
            return ((type == RPS_RESOURCE_TYPE_IMAGE_1D) || (type == RPS_RESOURCE_TYPE_IMAGE_2D)) ? image.arrayLayers
                                                                                                  : 1;
        }

        void Get(RpsResourceDesc& unpacked) const
        {
            unpacked.type           = type;
            unpacked.temporalLayers = temporalLayers;
            unpacked.flags          = flags;

            if (IsBuffer())
            {
                unpacked.buffer.sizeInBytesLo = buffer.sizeInBytesLo;
                unpacked.buffer.sizeInBytesHi = buffer.sizeInBytesHi;
            }
            else if(IsImage())
            {
                unpacked.image.width  = image.width;
                unpacked.image.height = image.height;

                if (type == RPS_RESOURCE_TYPE_IMAGE_3D)
                {
                    unpacked.image.depth = image.depth;
                }
                else
                {
                    unpacked.image.arrayLayers = image.arrayLayers;
                }

                unpacked.image.mipLevels   = image.mipLevels;
                unpacked.image.format      = image.format;
                unpacked.image.sampleCount = image.sampleCount;
            }
        }

        PrinterRef Print(PrinterRef printer) const
        {
            static constexpr const char* typeNames[] = {"unknown", "buffer", "tex1D", "tex2D", "tex3D"};

            static_assert(RPS_COUNTOF(typeNames) == RPS_RESOURCE_TYPE_COUNT,
                          "ResourceDescPacked::Print - typeNames need update");

            printer("type : %s", typeNames[(type < RPS_RESOURCE_TYPE_COUNT) ? type : RPS_RESOURCE_TYPE_UNKNOWN]);

            switch (type)
            {
            case RPS_RESOURCE_TYPE_BUFFER:
                printer(", num_bytes : 0x%p", uintptr_t(GetBufferSize()));
                break;
            case RPS_RESOURCE_TYPE_IMAGE_1D:
                printer("( %u x 1 ), fmt : %s, array : %u, mip : %u",
                        image.width,
                        rpsFormatGetName(image.format),
                        image.arrayLayers,
                        image.mipLevels);
                break;
            case RPS_RESOURCE_TYPE_IMAGE_2D:
                printer("( %u x %u ), fmt : %s, array : %u, mip : %u, samples : %u",
                        image.width,
                        image.height,
                        rpsFormatGetName(image.format),
                        image.arrayLayers,
                        image.mipLevels,
                        image.sampleCount);
                break;
            case RPS_RESOURCE_TYPE_IMAGE_3D:
                printer("( %u x %u x %u ), fmt : %s, mip : %u",
                        image.width,
                        image.height,
                        image.depth,
                        rpsFormatGetName(image.format),
                        image.mipLevels);
                break;
            default:
                break;
            }

            static const NameValuePair<RpsResourceFlags> resFlagNames[] = {
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED_POSTFIXED(RPS_RESOURCE_FLAG_, CUBEMAP_COMPATIBLE, _BIT),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED_POSTFIXED(RPS_RESOURCE_FLAG_, ROWMAJOR_IMAGE, _BIT),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED_POSTFIXED(RPS_RESOURCE_FLAG_, PREFER_GPU_LOCAL_CPU_VISIBLE, _BIT),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED_POSTFIXED(RPS_RESOURCE_FLAG_, PREFER_DEDICATED_ALLOCATION, _BIT),
                RPS_INIT_NAME_VALUE_PAIR_PREFIXED_POSTFIXED(RPS_RESOURCE_FLAG_, PERSISTENT, _BIT),
            };

            if (flags != RPS_RESOURCE_FLAG_NONE)
            {
                printer(", flags(");
                printer.PrintFlags(flags, resFlagNames);
                printer(")");
            }

            return printer;
        }
    };

    static_assert(sizeof(ResourceDescPacked) == sizeof(uint32_t) * 5, "Unexpected packing of ResourceDescPacked");

    struct SubresourceRangePacked
    {
        uint32_t aspectMask : 8;       ///< The aspect mask.
        uint32_t baseArrayLayer : 24;  ///< The first layer accessible to the view.
        uint32_t arrayLayerEnd : 22;   ///< The number of layers.
        uint32_t baseMipLevel : 5;     ///< The base mipmapping level for the resource access.
        uint32_t mipLevelEnd : 5;      ///< The number of mipmap levels accessible to the view.

        SubresourceRangePacked()
            : SubresourceRangePacked(1, 0, 1, 0, 1)
        {
        }

        constexpr SubresourceRangePacked(uint32_t aspectMask, const RpsSubresourceRange& range)
            : SubresourceRangePacked(aspectMask,
                                     range.baseArrayLayer,
                                     range.baseArrayLayer + range.arrayLayers,
                                     range.baseMipLevel,
                                     range.baseMipLevel + range.mipLevels)
        {
        }

        constexpr SubresourceRangePacked(uint32_t inAspectMask,
                                         uint32_t inBaseArrayLayer,
                                         uint32_t inArrayLayerEnd,
                                         uint32_t inBaseMipLevel,
                                         uint32_t inMipLevelEnd)
            : aspectMask(inAspectMask)
            , baseArrayLayer(inBaseArrayLayer)
            , arrayLayerEnd(inArrayLayerEnd)
            , baseMipLevel(inBaseMipLevel)
            , mipLevelEnd(inMipLevelEnd)
        {
            RPS_ASSERT(arrayLayerEnd > baseArrayLayer);
            RPS_ASSERT(mipLevelEnd > baseMipLevel);
        }

        constexpr SubresourceRangePacked(uint32_t                   aspectMask,
                                         const RpsSubresourceRange& range,
                                         const ResourceDescPacked&  resourceDesc)
            : SubresourceRangePacked(
                  aspectMask,
                  range.baseArrayLayer,
                  (range.arrayLayers != UINT32_MAX) ? (range.baseArrayLayer + range.arrayLayers)
                                                    : (resourceDesc.GetImageArrayLayers() - range.baseArrayLayer),
                  range.baseMipLevel,
                  (range.mipLevels != UINT16_MAX) ? (range.baseMipLevel + range.mipLevels)
                                                  : (resourceDesc.image.mipLevels - range.baseMipLevel))
        {
        }

        void Get(RpsSubresourceRange& unpacked) const
        {
            unpacked.baseArrayLayer = baseArrayLayer;
            unpacked.arrayLayers    = GetArrayLayerCount();
            unpacked.baseMipLevel   = baseMipLevel;
            unpacked.mipLevels      = GetMipLevelCount();
        }

        uint32_t GetArrayLayerCount() const
        {
            return arrayLayerEnd - baseArrayLayer;
        }

        uint32_t GetMipLevelCount() const
        {
            return mipLevelEnd - baseMipLevel;
        }

        uint32_t GetNumSubresources() const
        {
            return rpsCountBits(aspectMask) * GetArrayLayerCount() * GetMipLevelCount();
        }

        void Print(PrinterRef printer) const
        {
            printer("plane_mask 0x%x, array[ %u - %u ], mips[ %u - %u ]",
                    aspectMask,
                    baseArrayLayer,
                    arrayLayerEnd - 1,
                    baseMipLevel,
                    mipLevelEnd - 1);
        }

        bool operator==(const SubresourceRangePacked& rhs) const
        {
            return (aspectMask == rhs.aspectMask) && (baseArrayLayer == rhs.baseArrayLayer) &&
                   (arrayLayerEnd == rhs.arrayLayerEnd) && (baseMipLevel == rhs.baseMipLevel) &&
                   (mipLevelEnd == rhs.mipLevelEnd);
        }

        bool operator!=(const SubresourceRangePacked& rhs) const
        {
            return !(*this == rhs);
        }

        static bool Intersect(const SubresourceRangePacked& lhs, const SubresourceRangePacked& rhs)
        {
            return !((lhs.baseMipLevel >= rhs.mipLevelEnd) || (rhs.baseMipLevel >= lhs.mipLevelEnd) ||
                     (lhs.baseArrayLayer >= rhs.arrayLayerEnd) || (rhs.baseArrayLayer >= lhs.arrayLayerEnd) ||
                     ((lhs.aspectMask & rhs.aspectMask) == 0));
        }

        enum
        {
            // Max remaining ranges is 5 from the 2.5D clipping - 1 for aspects, 2 for array layers and 2 for mips.
            MAX_CLIP_COMPLEMENTS = 5,
        };

        static bool Clip(const SubresourceRangePacked& lhs,
                         const SubresourceRangePacked& rhs,
                         SubresourceRangePacked        outComplements[MAX_CLIP_COMPLEMENTS],
                         uint32_t*                     pNumOutComplement,
                         SubresourceRangePacked*       pOutUnionPart)
        {
            if (!Intersect(lhs, rhs))
            {
                return false;
            }

            uint32_t numComplements = 0;

            //
            //  Planes
            //

            // Adding planes not included in other range to complement list
            uint32_t complementPlaneMask = (lhs.aspectMask & (~rhs.aspectMask));
            if (complementPlaneMask != 0)
            {
                outComplements[numComplements++] = SubresourceRangePacked(
                    complementPlaneMask, lhs.baseArrayLayer, lhs.arrayLayerEnd, lhs.baseMipLevel, lhs.mipLevelEnd);
            }

            const uint32_t intersectPlaneMask = (lhs.aspectMask & rhs.aspectMask);

            //
            // Array Slices
            //
            uint32_t intersectbaseArrayLayer = lhs.baseArrayLayer;
            uint32_t intersectArrayLayerEnd  = lhs.arrayLayerEnd;

            if (rhs.baseArrayLayer > lhs.baseArrayLayer)
            {
                outComplements[numComplements++] = SubresourceRangePacked(
                    intersectPlaneMask, lhs.baseArrayLayer, rhs.baseArrayLayer, lhs.baseMipLevel, lhs.mipLevelEnd);

                intersectbaseArrayLayer = rhs.baseArrayLayer;
            }

            if (intersectArrayLayerEnd > rhs.arrayLayerEnd)
            {
                outComplements[numComplements++] = SubresourceRangePacked(
                    intersectPlaneMask, rhs.arrayLayerEnd, lhs.arrayLayerEnd, lhs.baseMipLevel, lhs.mipLevelEnd);

                intersectArrayLayerEnd = rhs.arrayLayerEnd;
            }

            //
            // Mips
            //
            uint32_t intersectBaseMip       = lhs.baseMipLevel;
            uint32_t intersectMipUpperBound = lhs.mipLevelEnd;

            if (rhs.baseMipLevel > lhs.baseMipLevel)
            {
                outComplements[numComplements++] = SubresourceRangePacked(intersectPlaneMask,
                                                                          intersectbaseArrayLayer,
                                                                          intersectArrayLayerEnd,
                                                                          lhs.baseMipLevel,
                                                                          rhs.baseMipLevel);

                intersectBaseMip = rhs.baseMipLevel;
            }

            if (lhs.mipLevelEnd > rhs.mipLevelEnd)
            {
                outComplements[numComplements++] = SubresourceRangePacked(intersectPlaneMask,
                                                                          intersectbaseArrayLayer,
                                                                          intersectArrayLayerEnd,
                                                                          rhs.mipLevelEnd,
                                                                          lhs.mipLevelEnd);

                intersectMipUpperBound = rhs.mipLevelEnd;
            }

            //
            // Union
            //
            if (pOutUnionPart)
            {
                *pOutUnionPart = SubresourceRangePacked(intersectPlaneMask,
                                                        intersectbaseArrayLayer,
                                                        intersectArrayLayerEnd,
                                                        intersectBaseMip,
                                                        intersectMipUpperBound);
            }

            if (pNumOutComplement)
            {
                *pNumOutComplement = numComplements;
            }

            return true;
        }
    };
}  // namespace rps

#endif  //RPS_RENDER_GRAPH_RESOURCE_HPP
