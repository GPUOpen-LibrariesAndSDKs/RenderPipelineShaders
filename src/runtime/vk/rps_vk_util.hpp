// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_VK_UTIL_HPP
#define RPS_VK_UTIL_HPP

#include "rps/runtime/vk/rps_vk_runtime.h"

#include "core/rps_util.hpp"

namespace rps
{
    template <typename T>
    struct VkObjectTypeMapper
    {
    };

#define RPS_DECLARE_VK_OBJECT_TYPE_MAP(TypeName, VkObjectTypeEnum, RpsTypeIdValue) \
    template <>                                                                    \
    struct VkObjectTypeMapper<TypeName>                                            \
    {                                                                              \
        static constexpr VkObjectType value  = VkObjectTypeEnum;                   \
        static constexpr RpsTypeId    typeId = RpsTypeIdValue;                     \
    };

    // clang-format off
    RPS_DECLARE_VK_OBJECT_TYPE_MAP(VkBuffer,     VK_OBJECT_TYPE_BUFFER,      RPS_TYPE_BUFFER_VIEW)
    RPS_DECLARE_VK_OBJECT_TYPE_MAP(VkImage,      VK_OBJECT_TYPE_IMAGE,       RPS_TYPE_IMAGE_VIEW)
    RPS_DECLARE_VK_OBJECT_TYPE_MAP(VkBufferView, VK_OBJECT_TYPE_BUFFER_VIEW, RPS_TYPE_BUFFER_VIEW)
    RPS_DECLARE_VK_OBJECT_TYPE_MAP(VkImageView,  VK_OBJECT_TYPE_IMAGE_VIEW,  RPS_TYPE_IMAGE_VIEW)
    // clang-format on

#undef RPS_DECLARE_VK_OBJECT_TYPE_MAP

    static inline RpsResult VkResultToRps(VkResult vkResult)
    {
        switch (vkResult)
        {
        case VK_SUCCESS:
        case VK_SUBOPTIMAL_KHR:
            return RPS_OK;
        case VK_ERROR_OUT_OF_HOST_MEMORY:
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        case VK_ERROR_OUT_OF_POOL_MEMORY:
            return RPS_ERROR_OUT_OF_MEMORY;
        default:
            break;
        }
        return RPS_ERROR_RUNTIME_API_ERROR;
    }

    // Deduces the VkImageAspectFlags from a viewFormat and corresponding resource format for a view.
    // Certain RpsFormats can be used to implicitly specify the subresource range.
    // (such as RPS_FORMAT_R24_UNORM_X8_TYPELESS indicates viewing the depth plane only).
    // This is different from rpsFormatHasDepth/Stencil which means if a resource with
    // the given format has certain image aspect.
    static inline VkImageAspectFlags GetFormatAspectMask(RpsFormat viewFormat, RpsFormat resourceFormat)
    {
        switch (viewFormat)
        {
        case RPS_FORMAT_D16_UNORM:
        case RPS_FORMAT_D32_FLOAT:
        case RPS_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        case RPS_FORMAT_R24_UNORM_X8_TYPELESS:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        case RPS_FORMAT_X24_TYPELESS_G8_UINT:
        case RPS_FORMAT_X32_TYPELESS_G8X24_UINT:
            return VK_IMAGE_ASPECT_STENCIL_BIT;
        case RPS_FORMAT_D24_UNORM_S8_UINT:
        case RPS_FORMAT_D32_FLOAT_S8X24_UINT:
        case RPS_FORMAT_R24G8_TYPELESS:
        case RPS_FORMAT_R32G8X24_TYPELESS:
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        case RPS_FORMAT_R16_UNORM:
            return (resourceFormat == RPS_FORMAT_D16_UNORM) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        case RPS_FORMAT_R32_FLOAT:
            return (resourceFormat == RPS_FORMAT_D32_FLOAT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        case RPS_FORMAT_UNKNOWN:
            RPS_ASSERT(resourceFormat != RPS_FORMAT_UNKNOWN);
            return (resourceFormat != RPS_FORMAT_UNKNOWN) ? GetFormatAspectMask(resourceFormat, resourceFormat)
                                                          : VK_IMAGE_ASPECT_COLOR_BIT;
        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    static inline constexpr uint32_t GetFormatPlaneCount(RpsFormat format)
    {
        switch (format)
        {
        case RPS_FORMAT_D24_UNORM_S8_UINT:
        case RPS_FORMAT_D32_FLOAT_S8X24_UINT:
            return 2;
        default:
            return 1;
        }
    }

    static inline VkSampleCountFlagBits rpsVkGetSampleCount(uint32_t sampleCount)
    {
        return VkSampleCountFlagBits(sampleCount);
    }

    static inline VkImageType rpsVkGetImageType(RpsResourceType rpsType)
    {
        switch (rpsType)
        {
        case RPS_RESOURCE_TYPE_IMAGE_2D:
            return VK_IMAGE_TYPE_2D;
        case RPS_RESOURCE_TYPE_IMAGE_3D:
            return VK_IMAGE_TYPE_3D;
        case RPS_RESOURCE_TYPE_IMAGE_1D:
            return VK_IMAGE_TYPE_1D;
        default:
            break;
        }
        return VK_IMAGE_TYPE_MAX_ENUM;
    }

    static inline RpsFormat rpsVkGetImageCreationFormat(const ResourceInstance& resInfo)
    {
        if (resInfo.allAccesses.accessFlags & RPS_ACCESS_DEPTH_STENCIL)
        {
            switch (resInfo.desc.image.format)
            {
            case RPS_FORMAT_R16_TYPELESS:
                return RPS_FORMAT_D16_UNORM;
            case RPS_FORMAT_R24G8_TYPELESS:
                return RPS_FORMAT_D24_UNORM_S8_UINT;
            case RPS_FORMAT_R32_TYPELESS:
                return RPS_FORMAT_D32_FLOAT;
            case RPS_FORMAT_R32G8X24_TYPELESS:
                return RPS_FORMAT_D32_FLOAT_S8X24_UINT;
            default:
                break;
            }
        }

        return resInfo.desc.image.format;
    }

    static inline RpsFormat rpsVkGetImageViewFormat(RpsFormat requestedViewFormat, const ResourceInstance& resInfo)
    {
        if (resInfo.allAccesses.accessFlags & RPS_ACCESS_DEPTH_STENCIL)
        {
            switch (requestedViewFormat)
            {
            case RPS_FORMAT_R16_TYPELESS:
            case RPS_FORMAT_R16_UNORM:
                return RPS_FORMAT_D16_UNORM;
            case RPS_FORMAT_R24G8_TYPELESS:
            case RPS_FORMAT_R24_UNORM_X8_TYPELESS:
                return RPS_FORMAT_D24_UNORM_S8_UINT;
            case RPS_FORMAT_R32_TYPELESS:
            case RPS_FORMAT_R32_FLOAT:
                return RPS_FORMAT_D32_FLOAT;
            case RPS_FORMAT_R32G8X24_TYPELESS:
            case RPS_FORMAT_R32_FLOAT_X8X24_TYPELESS:
                return RPS_FORMAT_D32_FLOAT_S8X24_UINT;
            default:
                break;
            }
        }

        return (requestedViewFormat != RPS_FORMAT_UNKNOWN) ? requestedViewFormat : resInfo.desc.image.format;
    }

    static inline VkRect2D GetVkRect2D(const RpsRect& rect)
    {
        return VkRect2D{
            {rect.x, rect.y},
            {uint32_t(rect.width), uint32_t(rect.height)},
        };
    }

    static inline void GetVkSubresourceRange(VkImageSubresourceRange& vkRange, const SubresourceRangePacked& range)
    {
        vkRange.aspectMask     = range.aspectMask;
        vkRange.baseMipLevel   = range.baseMipLevel;
        vkRange.levelCount     = range.GetMipLevelCount();
        vkRange.baseArrayLayer = range.baseArrayLayer;
        vkRange.layerCount     = range.GetArrayLayerCount();
    }

    template <VkComponentSwizzle Target>
    VkComponentSwizzle GetVkComponentSwizzle(RpsResourceViewComponentMapping swizzleMapping)
    {
        switch (swizzleMapping)
        {
        case RPS_RESOURCE_VIEW_COMPONENT_MAPPING_R:
            return (Target == VK_COMPONENT_SWIZZLE_R) ? VK_COMPONENT_SWIZZLE_IDENTITY : VK_COMPONENT_SWIZZLE_R;
        case RPS_RESOURCE_VIEW_COMPONENT_MAPPING_G:
            return (Target == VK_COMPONENT_SWIZZLE_G) ? VK_COMPONENT_SWIZZLE_IDENTITY : VK_COMPONENT_SWIZZLE_G;
        case RPS_RESOURCE_VIEW_COMPONENT_MAPPING_B:
            return (Target == VK_COMPONENT_SWIZZLE_B) ? VK_COMPONENT_SWIZZLE_IDENTITY : VK_COMPONENT_SWIZZLE_B;
        case RPS_RESOURCE_VIEW_COMPONENT_MAPPING_A:
            return (Target == VK_COMPONENT_SWIZZLE_A) ? VK_COMPONENT_SWIZZLE_IDENTITY : VK_COMPONENT_SWIZZLE_A;
        case RPS_RESOURCE_VIEW_COMPONENT_MAPPING_ZERO:
            return VK_COMPONENT_SWIZZLE_ZERO;
        case RPS_RESOURCE_VIEW_COMPONENT_MAPPING_ONE:
            return VK_COMPONENT_SWIZZLE_ONE;
        default:
            break;
        }
        return VK_COMPONENT_SWIZZLE_IDENTITY;
    }

    static inline void GetVkComponentMapping(VkComponentMapping& vkCompMapping, uint32_t rpsMapping)
    {
        vkCompMapping.r =
            GetVkComponentSwizzle<VK_COMPONENT_SWIZZLE_R>(RPS_IMAGE_VIEW_GET_COMPONENT_MAPPING_CHANNEL_R(rpsMapping));
        vkCompMapping.g =
            GetVkComponentSwizzle<VK_COMPONENT_SWIZZLE_G>(RPS_IMAGE_VIEW_GET_COMPONENT_MAPPING_CHANNEL_G(rpsMapping));
        vkCompMapping.b =
            GetVkComponentSwizzle<VK_COMPONENT_SWIZZLE_B>(RPS_IMAGE_VIEW_GET_COMPONENT_MAPPING_CHANNEL_B(rpsMapping));
        vkCompMapping.a =
            GetVkComponentSwizzle<VK_COMPONENT_SWIZZLE_A>(RPS_IMAGE_VIEW_GET_COMPONENT_MAPPING_CHANNEL_A(rpsMapping));
    }

}  // namespace rps

#endif  //RPS_VK_UTIL_HPP
