// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "rps/runtime/d3d_common/rps_d3d_common.h"

#include "runtime/common/rps_runtime_util.hpp"
#include "runtime/common/phases/rps_pre_process.hpp"
#include "runtime/common/phases/rps_dag_build.hpp"
#include "runtime/common/phases/rps_access_dag_build.hpp"
#include "runtime/common/phases/rps_cmd_print.hpp"
#include "runtime/common/phases/rps_cmd_dag_print.hpp"
#include "runtime/common/phases/rps_dag_schedule.hpp"
#include "runtime/common/phases/rps_lifetime_analysis.hpp"
#include "runtime/common/phases/rps_schedule_print.hpp"
#include "runtime/common/phases/rps_memory_schedule.hpp"

#include "runtime/d3d11/rps_d3d11_runtime_device.hpp"
#include "runtime/d3d11/rps_d3d11_runtime_backend.hpp"
#include "runtime/d3d11/rps_d3d11_util.hpp"

namespace rps
{
    D3D11RuntimeDevice::D3D11RuntimeDevice(Device* pDevice, const RpsD3D11RuntimeDeviceCreateInfo* pCreateInfo)
        : RuntimeDevice(pDevice, pCreateInfo->pRuntimeCreateInfo)
        , m_pD3DDevice(pCreateInfo->pD3D11Device)
        , m_flags(pCreateInfo->flags)
    {
        m_pD3DDevice->AddRef();
    }

    RpsResult D3D11RuntimeDevice::Init()
    {
        return RPS_OK;
    }

    D3D11RuntimeDevice::~D3D11RuntimeDevice()
    {
        SafeRelease(m_pD3DDevice);
    }

    RpsResult D3D11RuntimeDevice::BuildDefaultRenderGraphPhases(RenderGraph& renderGraph)
    {
        RPS_V_RETURN(renderGraph.ReservePhases(8));
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
        RPS_V_RETURN(renderGraph.AddPhase<ScheduleDebugPrintPhase>());
        RPS_V_RETURN(renderGraph.AddPhase<D3D11RuntimeBackend>(*this, renderGraph));

        return RPS_OK;
    }

    RpsResult D3D11RuntimeDevice::InitializeSubresourceInfos(ArrayRef<ResourceInstance> resInstances)
    {
        for (auto& resInstance : resInstances)
        {
            GetFullSubresourceRange(
                resInstance.fullSubresourceRange, resInstance.desc, GetResourcePlaneMask(resInstance.desc));

            resInstance.numSubResources = GetSubresourceCount(resInstance.desc);
        }

        return RPS_OK;
    }

    RpsResult D3D11RuntimeDevice::InitializeResourceAllocInfos(ArrayRef<ResourceInstance> resInstances)
    {
        return RPS_OK;
    }

    RpsResult D3D11RuntimeDevice::GetSubresourceRangeFromImageView(SubresourceRangePacked& outRange,
                                                                   const ResourceInstance& resourceInfo,
                                                                   const RpsAccessAttr&    accessAttr,
                                                                   const RpsImageView&     imageView)
    {
        uint32_t viewPlaneMask = (imageView.base.viewFormat == RPS_FORMAT_UNKNOWN)
                                     ? UINT32_MAX
                                     : GetFormatPlaneMask(imageView.base.viewFormat);
        uint32_t planeMask     = GetResourcePlaneMask(resourceInfo.desc) & viewPlaneMask;
        outRange               = SubresourceRangePacked(planeMask, imageView.subresourceRange, resourceInfo.desc);
        return RPS_OK;
    }

    ConstArrayRef<RpsMemoryTypeInfo> D3D11RuntimeDevice::GetMemoryTypeInfos() const
    {
        return {};
    }

    uint32_t D3D11RuntimeDevice::GetSubresourceCount(const ResourceDescPacked& resDesc) const
    {
        return resDesc.IsBuffer() ? 1
                                  : (((resDesc.type == RPS_RESOURCE_TYPE_IMAGE_3D) ? 1 : resDesc.image.arrayLayers) *
                                     resDesc.image.mipLevels);
    }

    uint32_t D3D11RuntimeDevice::GetResourcePlaneMask(const ResourceDescPacked& resDesc) const
    {
        return resDesc.IsBuffer() ? 1u : GetFormatPlaneMask(resDesc.image.format);
    }

    void D3D11BuiltInClearColorRegions(const RpsCmdCallbackContext* pContext);
    void D3D11BuiltInClearColor(const RpsCmdCallbackContext* pContext);
    void D3D11BuiltInClearDepthStencil(const RpsCmdCallbackContext* pContext);
    void D3D11BuiltInClearDepthStencilRegions(const RpsCmdCallbackContext* pContext);
    void D3D11BuiltInClearTextureUAV(const RpsCmdCallbackContext* pContext);
    void D3D11BuiltInClearTextureUAVRegions(const RpsCmdCallbackContext* pContext);
    void D3D11BuiltInClearBufferUAV(const RpsCmdCallbackContext* pContext);
    void D3D11BuiltInCopyTexture(const RpsCmdCallbackContext* pContext);
    void D3D11BuiltInCopyBuffer(const RpsCmdCallbackContext* pContext);
    void D3D11BuiltInCopyTextureToBuffer(const RpsCmdCallbackContext* pContext);
    void D3D11BuiltInCopyBufferToTexture(const RpsCmdCallbackContext* pContext);
    void D3D11BuiltInResolve(const RpsCmdCallbackContext* pContext);

    ConstArrayRef<BuiltInNodeInfo> D3D11RuntimeDevice::GetBuiltInNodes() const
    {
        static const BuiltInNodeInfo c_builtInNodes[] = {
            {"clear_color",                 {&D3D11BuiltInClearColor, nullptr}},
            {"clear_color_regions",         {&D3D11BuiltInClearColorRegions, nullptr}},
            {"clear_depth_stencil",         {&D3D11BuiltInClearDepthStencil, nullptr}},
            {"clear_depth_stencil_regions", {&D3D11BuiltInClearDepthStencilRegions, nullptr}},
            {"clear_texture",               {&D3D11BuiltInClearTextureUAV, nullptr}},
            {"clear_texture_regions",       {&D3D11BuiltInClearTextureUAVRegions, nullptr}},
            {"clear_buffer",                {&D3D11BuiltInClearBufferUAV,nullptr}},
            {"copy_texture",                {&D3D11BuiltInCopyTexture,nullptr}},
            {"copy_buffer",                 {&D3D11BuiltInCopyBuffer,nullptr}},
            {"copy_texture_to_buffer",      {&D3D11BuiltInCopyTextureToBuffer,nullptr}},
            {"copy_buffer_to_texture",      {&D3D11BuiltInCopyBufferToTexture,nullptr}},
            {"resolve",                     {&D3D11BuiltInResolve,nullptr}},
        };

        return c_builtInNodes;
    }

    bool D3D11RuntimeDevice::CalculateAccessTransition(const RpsAccessAttr&  beforeAccess,
                                                       const RpsAccessAttr&  afterAccess,
                                                       AccessTransitionInfo& results) const
    {
        if (rpsAnyBitsSet(beforeAccess.accessFlags | afterAccess.accessFlags, RPS_ACCESS_CLEAR_BIT) &&
            ((beforeAccess.accessFlags & ~RPS_ACCESS_CLEAR_BIT) == (afterAccess.accessFlags & ~RPS_ACCESS_CLEAR_BIT)))
        {
            results.bKeepOrdering       = true;
            results.bMergedAccessStates = true;
            results.bTransition         = false;
            results.mergedAccess        = beforeAccess | afterAccess;

            return true;
        }

        return false;
    }
}  // namespace rps

RpsResult rpsD3D11RuntimeDeviceCreate(const RpsD3D11RuntimeDeviceCreateInfo* pCreateInfo, RpsDevice* phDevice)
{
    RPS_CHECK_ARGS(pCreateInfo && pCreateInfo->pD3D11Device);

    RpsResult result =
        rps::RuntimeDevice::Create<rps::D3D11RuntimeDevice>(phDevice, pCreateInfo->pDeviceCreateInfo, pCreateInfo);

    return result;
}
