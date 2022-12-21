// Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the AMD INTERNAL EVALUATION LICENSE.
//
// See file LICENSE.RTF for full license details.

#ifndef RPS_VK_RUNTIME_DEVICE_H
#define RPS_VK_RUNTIME_DEVICE_H

#include "runtime/common/rps_runtime_device.hpp"

#include "rps/runtime/vk/rps_vk_runtime.h"

namespace rps
{
    class VKRuntimeDevice final : public RuntimeDevice
    {
    public:
        VKRuntimeDevice(Device* pDevice, const RpsVKRuntimeDeviceCreateInfo* pCreateInfo);
        virtual ~VKRuntimeDevice();

        virtual RpsResult Init() override final;
        virtual RpsResult BuildDefaultRenderGraphPhases(RenderGraph& renderGraph) override final;
        virtual RpsResult InitializeSubresourceInfos(ArrayRef<ResourceInstance> resInstances) override final;
        virtual RpsResult InitializeResourceAllocInfos(ArrayRef<ResourceInstance> resInstances) override final;
        virtual RpsResult GetSubresourceRangeFromImageView(SubresourceRangePacked& outRange,
                                                           const ResourceInstance& resourceInfo,
                                                           const RpsAccessAttr&    accessAttr,
                                                           const RpsImageView&     imageView) override final;
        virtual ConstArrayRef<RpsMemoryTypeInfo> GetMemoryTypeInfos() const override final;
        virtual RpsResult DescribeMemoryType(uint32_t memoryTypeIndex, PrinterRef printer) const override final;

        virtual bool      CalculateAccessTransition(const RpsAccessAttr&  beforeAccess,
                                                    const RpsAccessAttr&  afterAccess,
                                                    AccessTransitionInfo& results) const override final;

        virtual ConstArrayRef<BuiltInNodeInfo> GetBuiltInNodes() const override final;

        virtual RpsImageAspectUsageFlags GetImageAspectUsages(uint32_t aspectMask) const override final
        {
            return ((aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) ? RPS_IMAGE_ASPECT_COLOR : RPS_IMAGE_ASPECT_UNKNOWN) |
                   ((aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) ? RPS_IMAGE_ASPECT_DEPTH : RPS_IMAGE_ASPECT_UNKNOWN) |
                   ((aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) ? RPS_IMAGE_ASPECT_STENCIL : RPS_IMAGE_ASPECT_UNKNOWN);
        }

    public:
        VkDevice GetVkDevice() const
        {
            return m_device;
        }

        VkPhysicalDevice GetVkPhysicalDevice() const
        {
            return m_physicalDevice;
        }

        const RpsVKFunctions& GetFunctions() const
        {
            return m_functions;
        }

        const VkPhysicalDeviceProperties& GetPhysicalDeviceProperties() const
        {
            return m_deviceProperties;
        }

        RpsVKRuntimeFlags GetRuntimeFlags() const
        {
            return m_flags;
        }

        VkMemoryType GetVkHeapTypeInfo(uint32_t memoryTypeIndex) const;

    private:
        struct VKResourceAllocInfo
        {
            VkMemoryRequirements memoryRequirements;
            RpsRuntimeResource   hRuntimeResource;
        };
        RpsResult GetResourceAllocInfo(const ResourceInstance& resInstance, VKResourceAllocInfo& allocInfo) const;
        uint32_t  GetResourceAspectMask(const ResourceInstance& resInfo) const;
        uint32_t  GetImageViewAspectMask(const ResourceDescPacked& resDesc, const RpsImageView& imageView) const;
        uint32_t  GetSubresourceCount(const ResourceInstance& resInfo) const;

    private:
        VkDevice         m_device         = VK_NULL_HANDLE;
        VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
        RpsVKFunctions    m_functions      = {};

        RpsVKRuntimeFlags m_flags = RPS_VK_RUNTIME_FLAG_NONE;

        // TODO: Store what we care
        VkPhysicalDeviceProperties       m_deviceProperties       = {};
        VkPhysicalDeviceMemoryProperties m_deviceMemoryProperties = {};
        uint32_t                         m_hostVisibleMemoryTypeMask = 0;
    };

    RpsRuntimeResource ToHandle(VkImage hImage);
    RpsRuntimeResource ToHandle(VkBuffer hBuffer);
    void FromHandle(VkImage& hImage, RpsRuntimeResource hResource);
    void FromHandle(VkBuffer& hBuffer, RpsRuntimeResource hResource);

    template <typename T,
              typename = typename std::enable_if<std::is_same<T, VkImage>::value || std::is_same<T, VkBuffer>::value,
                                                 RpsRuntimeResource>::type>
    T FromHandle(RpsRuntimeResource hResource)
    {
        return T(hResource.ptr);
    }
}

#endif  //RPS_VK_RUNTIME_DEVICE_H
