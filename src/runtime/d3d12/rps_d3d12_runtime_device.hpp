// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_D3D12_RUNTIME_DEVICE_HPP
#define RPS_D3D12_RUNTIME_DEVICE_HPP

#include "rps/runtime/d3d12/rps_d3d12_runtime.h"

#include "runtime/common/rps_runtime_device.hpp"

namespace rps
{
    struct D3D12HeapTypeInfo
    {
        D3D12_HEAP_TYPE  type;
        D3D12_HEAP_FLAGS heapFlags;
        bool             AllowMsaa;
    };

    class D3D12RuntimeDevice final : public RuntimeDevice
    {
    public:
        D3D12RuntimeDevice(Device* pDevice, const RpsD3D12RuntimeDeviceCreateInfo* pCreateInfo);
        virtual ~D3D12RuntimeDevice();

        virtual RpsResult Init() override final;
        virtual RpsResult BuildDefaultRenderGraphPhases(RenderGraph& renderGraph) override final;
        virtual RpsResult InitializeSubresourceInfos(ArrayRef<ResourceInstance> resInstances) override final;
        virtual RpsResult InitializeResourceAllocInfos(ArrayRef<ResourceInstance> resInstances) override final;
        virtual RpsResult GetSubresourceRangeFromImageView(SubresourceRangePacked& outRange,
                                                           const ResourceInstance& resourceInfo,
                                                           const RpsAccessAttr&    accessAttr,
                                                           const RpsImageView&     imageView) override final;
        virtual ConstArrayRef<RpsMemoryTypeInfo> GetMemoryTypeInfos() const override final;
        virtual RpsResult DescribeMemoryType(uint32_t memoryTypeIndex, PrinterRef printer) const override;

        virtual ConstArrayRef<BuiltInNodeInfo> GetBuiltInNodes() const override final;

        virtual bool CalculateAccessTransition(const RpsAccessAttr&  beforeAccess,
                                               const RpsAccessAttr&  afterAccess,
                                               AccessTransitionInfo& results) const override final;

        virtual RpsImageAspectUsageFlags GetImageAspectUsages(uint32_t aspectMask) const override final
        {
            return ((aspectMask & 1) ? (RPS_IMAGE_ASPECT_COLOR | RPS_IMAGE_ASPECT_DEPTH) : RPS_IMAGE_ASPECT_UNKNOWN) |
                   ((aspectMask & 2) ? RPS_IMAGE_ASPECT_STENCIL : RPS_IMAGE_ASPECT_UNKNOWN);
        }

        static ID3D12Resource* FromHandle(RpsRuntimeResource hRuntimeResource)
        {
            return static_cast<ID3D12Resource*>(hRuntimeResource.ptr);
        }

        static RpsRuntimeResource ToHandle(ID3D12Resource* pD3DResource)
        {
            return RpsRuntimeResource{pD3DResource};
        }

    public:
        ID3D12Device* GetD3DDevice() const
        {
            return m_pD3DDevice;
        }

        uint32_t GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const
        {
            return m_descriptorIncSizes[type];
        }

        uint32_t GetFormatPlaneMask(RpsFormat format) const
        {
            switch (format)
            {
            case RPS_FORMAT_D24_UNORM_S8_UINT:
            case RPS_FORMAT_D32_FLOAT_S8X24_UINT:
                return 0x3;
            case RPS_FORMAT_X24_TYPELESS_G8_UINT:
            case RPS_FORMAT_X32_TYPELESS_G8X24_UINT:
                return 0x2;
            case RPS_FORMAT_R32_FLOAT_X8X24_TYPELESS:
            case RPS_FORMAT_R24_UNORM_X8_TYPELESS:
                return 0x1;
            default:
                break;
            }
            return (1u << GetFormatPlaneCount(format)) - 1;
        }

        uint32_t GetFormatPlaneIndex(RpsFormat format) const
        {
            uint32_t mask = GetFormatPlaneMask(format);

            RPS_ASSERT(rpsCountBits(mask) == 1);

            return rpsFirstBitLow(mask);
        }

        const D3D12HeapTypeInfo& GetD3D12HeapTypeInfo(uint32_t memoryTypeIndex) const;

        bool GetEnhancedBarrierEnabled() const
        {
            return m_bEnhancedBarriersEnabled;
        }

    private:
        D3D12_RESOURCE_ALLOCATION_INFO GetResourceAllocInfo(const ResourceInstance& resInstance) const;
        uint32_t                       GetSubresourceCount(const ResourceDescPacked& resDesc) const;
        uint32_t                       GetResourcePlaneMask(const ResourceDescPacked& resDesc) const;

        uint32_t GetFormatPlaneCount(RpsFormat format) const
        {
            return (uint32_t(format) < RPS_FORMAT_COUNT) ? m_formatPlaneCount[format] : 0;
        }

    private:
        ID3D12Device*            m_pD3DDevice;
        ID3D12Device2*           m_pD3DDevice2;
        RpsD3D12RuntimeFlags     m_flags = {};
        D3D12_RESOURCE_HEAP_TIER m_heapTier;
        D3D12_RENDER_PASS_TIER   m_renderPassesTier;
        bool                     m_bEnhancedBarriersEnabled = false;
        RpsMemoryTypeInfo        m_memoryTypeInfos[RPS_D3D12_HEAP_TYPE_COUNT_MAX];
        uint32_t                 m_descriptorIncSizes[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
        char                     m_formatPlaneCount[RPS_FORMAT_COUNT];
    };
}

#endif  //RPS_D3D12_RUNTIME_DEVICE_HPP
