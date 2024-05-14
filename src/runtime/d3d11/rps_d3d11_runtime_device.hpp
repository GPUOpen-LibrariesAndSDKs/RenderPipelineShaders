// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_D3D11_RUNTIME_DEVICE_HPP
#define RPS_D3D11_RUNTIME_DEVICE_HPP

#include "rps/runtime/d3d11/rps_d3d11_runtime.h"

#include "runtime/common/rps_runtime_device.hpp"

namespace rps
{
    class D3D11RuntimeDevice final : public RuntimeDevice
    {
    public:
        D3D11RuntimeDevice(Device* pDevice, const RpsD3D11RuntimeDeviceCreateInfo* pCreateInfo);
        virtual ~D3D11RuntimeDevice();

        virtual RpsResult Init() override;
        virtual RpsResult BuildDefaultRenderGraphPhases(RenderGraph& renderGraph) override final;
        virtual RpsResult InitializeSubresourceInfos(ArrayRef<ResourceInstance> resInstances) override final;
        virtual RpsResult InitializeResourceAllocInfos(ArrayRef<ResourceInstance> resInstances) override final;
        virtual RpsResult GetSubresourceRangeFromImageView(SubresourceRangePacked& outRange,
                                                           const ResourceInstance& resourceInfo,
                                                           const RpsAccessAttr&    accessAttr,
                                                           const RpsImageView&     imageView) override final;
        virtual ConstArrayRef<RpsMemoryTypeInfo> GetMemoryTypeInfos() const override final;

        virtual ConstArrayRef<BuiltInNodeInfo> GetBuiltInNodes() const override final;

        virtual bool CalculateAccessTransition(const RpsAccessAttr&  beforeAccess,
                                               const RpsAccessAttr&  afterAccess,
                                               AccessTransitionInfo& results) const override final;

        virtual RpsImageAspectUsageFlags GetImageAspectUsages(uint32_t aspectMask) const override final
        {
            return ((aspectMask & 1) ? (RPS_IMAGE_ASPECT_COLOR | RPS_IMAGE_ASPECT_DEPTH) : RPS_IMAGE_ASPECT_UNKNOWN) |
                   ((aspectMask & 2) ? RPS_IMAGE_ASPECT_STENCIL : RPS_IMAGE_ASPECT_UNKNOWN);
        }

        virtual void PrepareRenderGraphCreation(RpsRenderGraphCreateInfo& renderGraphCreateInfo) const override final
        {
            //No mem aliasing support in DX11
            renderGraphCreateInfo.renderGraphFlags |= RPS_RENDER_GRAPH_NO_GPU_MEMORY_ALIASING;

            RuntimeDevice::PrepareRenderGraphCreation(renderGraphCreateInfo);
        }

        static ID3D11Resource* FromHandle(RpsRuntimeResource hRuntimeResource)
        {
            return static_cast<ID3D11Resource*>(hRuntimeResource.ptr);
        }

        static RpsRuntimeResource ToHandle(ID3D11Resource* pD3DResource)
        {
            return RpsRuntimeResource{pD3DResource};
        }

    public:
        ID3D11Device* GetD3DDevice() const
        {
            return m_pD3DDevice;
        }

        uint32_t GetFormatPlaneMask(RpsFormat format) const
        {
            return 1;
        }

        uint32_t GetFormatPlaneIndex(RpsFormat format) const
        {
            return 0;
        }

    private:
        uint32_t GetSubresourceCount(const ResourceDescPacked& resDesc) const;
        uint32_t GetResourcePlaneMask(const ResourceDescPacked& resDesc) const;

        uint32_t GetFormatPlaneCount(RpsFormat format) const
        {
            return 1;
        }

    private:
        ID3D11Device*        m_pD3DDevice = nullptr;
        RpsD3D11RuntimeFlags m_flags      = {};
    };
}  // namespace rps

#endif  //RPS_D3D11_RUNTIME_DEVICE_HPP
