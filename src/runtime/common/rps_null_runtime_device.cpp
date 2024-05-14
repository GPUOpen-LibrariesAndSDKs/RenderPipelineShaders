// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "core/rps_util.hpp"
#include "core/rps_device.hpp"

#include "runtime/common/rps_runtime_device.hpp"
#include "runtime/common/rps_runtime_util.hpp"

#include "runtime/common/phases/rps_pre_process.hpp"
#include "runtime/common/phases/rps_dag_build.hpp"
#include "runtime/common/phases/rps_access_dag_build.hpp"
#include "runtime/common/phases/rps_cmd_print.hpp"
#include "runtime/common/phases/rps_cmd_dag_print.hpp"
#include "runtime/common/phases/rps_dag_schedule.hpp"
#include "runtime/common/phases/rps_memory_schedule.hpp"
#include "runtime/common/phases/rps_lifetime_analysis.hpp"
#include "runtime/common/phases/rps_schedule_print.hpp"

namespace rps
{
    // Generic, Null-Runtime implementations.
    // Actual runtime device implementations should query runtime APIs to get these information.
    static uint32_t GetFormatPlaneCount(RpsFormat format)
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
            return 2;
        default:
            return 1;
        }
    }

    static uint32_t GetFormatAspectMask(RpsFormat format)
    {
        switch (format)
        {
        case RPS_FORMAT_R32G8X24_TYPELESS:
        case RPS_FORMAT_D32_FLOAT_S8X24_UINT:
        case RPS_FORMAT_R24G8_TYPELESS:
        case RPS_FORMAT_D24_UNORM_S8_UINT:
            return 0x3;
        case RPS_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        case RPS_FORMAT_R24_UNORM_X8_TYPELESS:
            return 0x1;
        case RPS_FORMAT_X32_TYPELESS_G8X24_UINT:
        case RPS_FORMAT_X24_TYPELESS_G8_UINT:
            return 0x2;
        default:
            break;
        }
        return 0x1;
    }

    static uint32_t CalcSubresourceCount(const ResourceDescPacked& desc)
    {
        return desc.IsBuffer() ? 1
                               : (((desc.type == RPS_RESOURCE_TYPE_IMAGE_3D) ? 1 : desc.image.arrayLayers) *
                                  desc.image.mipLevels * GetFormatPlaneCount(desc.image.format));
    }

    static uint32_t GetResourceAspectMask(const ResourceDescPacked& resDesc)
    {
        return resDesc.IsBuffer() ? 1 : GetFormatAspectMask(resDesc.image.format);
    }

    static uint32_t GetViewAspectMask(const ResourceDescPacked& resDesc, const RpsImageView& imageView)
    {
        if (resDesc.IsBuffer())
        {
            return 1;
        }

        const RpsFormat viewForamt =
            (imageView.base.viewFormat != RPS_FORMAT_UNKNOWN) ? imageView.base.viewFormat : resDesc.image.format;

        return GetFormatAspectMask(viewForamt);
    }

    static uint32_t GetFormatElementBytes(RpsFormat format)
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

    static uint64_t EstimateAllocationSize(const ResourceDescPacked& resDesc)
    {
        if (resDesc.IsBuffer())
        {
            return resDesc.GetBufferSize();
        }
        else if (resDesc.IsImage())
        {
            const uint64_t depthOrArraySlices =
                (resDesc.type == RPS_RESOURCE_TYPE_IMAGE_3D) ? resDesc.image.depth : resDesc.image.arrayLayers;

            uint64_t sizeInBytes = (uint64_t(resDesc.image.width) * resDesc.image.height * depthOrArraySlices *
                                    GetFormatElementBytes(resDesc.image.format));

            for (uint32_t i = 0; i < resDesc.image.mipLevels; i++)
            {
                sizeInBytes += (sizeInBytes >> (i << 1));
            }

            return sizeInBytes;
        }

        return 0ull;
    }

    RpsResult NullRuntimeDevice::BuildDefaultRenderGraphPhases(RenderGraph& renderGraph)
    {
        RPS_V_RETURN(renderGraph.ReservePhases(16));
        RPS_V_RETURN(renderGraph.AddPhase<PreProcessPhase>());
        RPS_V_RETURN(renderGraph.AddPhase<CmdDebugPrintPhase>());
        RPS_V_RETURN(renderGraph.AddPhase<DAGBuilderPass>());
        RPS_V_RETURN(renderGraph.AddPhase<AccessDAGBuilderPass>(renderGraph));
        RPS_V_RETURN(renderGraph.AddPhase<DAGPrintPhase>(renderGraph));
        RPS_V_RETURN(renderGraph.AddPhase<DAGSchedulePass>(renderGraph));
        if (!rpsAnyBitsSet(renderGraph.GetCreateInfo().renderGraphFlags, RPS_RENDER_GRAPH_NO_LIFETIME_ANALYSIS))
        {
            RPS_V_RETURN(renderGraph.AddPhase<LifetimeAnalysisPhase>());
        }

        RPS_V_RETURN(renderGraph.AddPhase<MemorySchedulePhase>(renderGraph));
        RPS_V_RETURN(renderGraph.AddPhase<ScheduleDebugPrintPhase>());
        //A NullRuntime backend will be added by the render graph automatically because no backend is set

        return RPS_OK;
    }

    RpsResult NullRuntimeDevice::InitializeSubresourceInfos(ArrayRef<ResourceInstance> resInstances)
    {
        for (auto& resInstance : resInstances)
        {
            GetFullSubresourceRange(
                resInstance.fullSubresourceRange, resInstance.desc, GetResourceAspectMask(resInstance.desc));

            resInstance.numSubResources = CalcSubresourceCount(resInstance.desc);
        }

        return RPS_OK;
    }

    RpsResult NullRuntimeDevice::InitializeResourceAllocInfos(ArrayRef<ResourceInstance> resInstances)
    {
        for (auto& resInst : resInstances)
        {
            resInst.allocRequirement.size            = EstimateAllocationSize(resInst.desc);
            resInst.allocRequirement.alignment       = 0;
            resInst.allocRequirement.memoryTypeIndex = 0;
            resInst.hRuntimeResource                 = {RPS_NULL_HANDLE};
        }

        return RPS_OK;
    }

    RpsResult NullRuntimeDevice::GetSubresourceRangeFromImageView(SubresourceRangePacked& outRange,
                                                                  const ResourceInstance& resourceInfo,
                                                                  const RpsAccessAttr&    accessAttr,
                                                                  const RpsImageView&     imageView)
    {
        const uint32_t viewAspectMask = GetViewAspectMask(resourceInfo.desc, imageView);
        const uint32_t aspectMask     = GetResourceAspectMask(resourceInfo.desc) & viewAspectMask;

        outRange = SubresourceRangePacked(aspectMask, imageView.subresourceRange, resourceInfo.desc);

        return RPS_OK;
    }

    RpsImageAspectUsageFlags NullRuntimeDevice::GetImageAspectUsages(uint32_t aspectMask) const
    {
        return ((aspectMask & 1) ? (RPS_IMAGE_ASPECT_COLOR | RPS_IMAGE_ASPECT_DEPTH) : RPS_IMAGE_ASPECT_UNKNOWN) |
               ((aspectMask & 2) ? RPS_IMAGE_ASPECT_STENCIL : RPS_IMAGE_ASPECT_UNKNOWN);
    }

    ConstArrayRef<RpsMemoryTypeInfo> NullRuntimeDevice::GetMemoryTypeInfos() const     
    {
        //Create a dummy memory type for memory scheduling
        static RpsMemoryTypeInfo dummyMemType = {0, 1};
        return {&dummyMemType, 1};
    }

}  // namespace rps

RpsResult rpsNullRuntimeDeviceCreate(const RpsNullRuntimeDeviceCreateInfo* pCreateInfo, RpsDevice* phDevice)
{
    return rps::RuntimeDevice::Create<rps::NullRuntimeDevice>(
        phDevice, (pCreateInfo && pCreateInfo->pDeviceCreateInfo) ? pCreateInfo->pDeviceCreateInfo : nullptr);
}
