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
#include "runtime/common/phases/rps_schedule_print.hpp"
#include "runtime/common/phases/rps_lifetime_analysis.hpp"
#include "runtime/common/phases/rps_memory_schedule.hpp"

#include "runtime/d3d12/rps_d3d12_runtime_device.hpp"
#include "runtime/d3d12/rps_d3d12_runtime_backend.hpp"
#include "runtime/d3d12/rps_d3d12_util.hpp"

namespace rps
{
    D3D12RuntimeDevice::D3D12RuntimeDevice(Device* pDevice, const RpsD3D12RuntimeDeviceCreateInfo* pCreateInfo)
        : RuntimeDevice(pDevice, pCreateInfo->pRuntimeCreateInfo)
        , m_pD3DDevice(pCreateInfo->pD3D12Device)
        , m_flags(pCreateInfo->flags)
    {
        m_pD3DDevice->AddRef();
    }

    RpsResult D3D12RuntimeDevice::Init()
    {
        RPS_V_RETURN(HRESULTToRps(m_pD3DDevice->QueryInterface(IID_PPV_ARGS(&m_pD3DDevice2))));

        D3D12_FEATURE_DATA_D3D12_OPTIONS featureOptionsData = {};

        HRESULT hr = m_pD3DDevice->CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS, &featureOptionsData, sizeof(featureOptionsData));
        RPS_V_RETURN(HRESULTToRps(hr));

        D3D12_FEATURE_DATA_D3D12_OPTIONS5 featureOptionsData5 = {};

        hr = m_pD3DDevice->CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS5, &featureOptionsData5, sizeof(featureOptionsData5));
        RPS_V_RETURN(HRESULTToRps(hr));

#if RPS_D3D12_FEATURE_D3D12_OPTIONS12_DEFINED
        D3D12_FEATURE_DATA_D3D12_OPTIONS12 featureOptionsData12 = {};

        m_bEnhancedBarriersEnabled = false;

        if (SUCCEEDED(m_pD3DDevice->CheckFeatureSupport(
                D3D12_FEATURE_D3D12_OPTIONS12, &featureOptionsData12, sizeof(featureOptionsData12))))
        {
            m_bEnhancedBarriersEnabled = featureOptionsData12.EnhancedBarriersSupported &&
                                         (m_flags & RPS_D3D12_RUNTIME_FLAG_PREFER_ENHANCED_BARRIERS);
        }
#endif  //RPS_D3D12_FEATURE_D3D12_OPTIONS12_DEFINED

        m_heapTier = rpsAnyBitsSet(m_flags, RPS_D3D12_RUNTIME_FLAG_FORCE_RESOURCE_HEAP_TIER1)
                         ? D3D12_RESOURCE_HEAP_TIER_1
                         : featureOptionsData.ResourceHeapTier;

        m_renderPassesTier = featureOptionsData5.RenderPassesTier;

        m_memoryTypeInfos[RPS_D3D12_HEAP_TYPE_INDEX_UPLOAD]   = {0, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT};
        m_memoryTypeInfos[RPS_D3D12_HEAP_TYPE_INDEX_READBACK] = {0, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT};

        if (m_heapTier == D3D12_RESOURCE_HEAP_TIER_2)
        {
            m_memoryTypeInfos[RPS_D3D12_HEAP_TYPE_INDEX_DEFAULT]      = {0, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT};
            m_memoryTypeInfos[RPS_D3D12_HEAP_TYPE_INDEX_DEFAULT_MSAA] = {0, D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT};
        }
        else
        {
            m_memoryTypeInfos[RPS_D3D12_HEAP_TYPE_INDEX_DEFAULT_TIER_1_RT_DS_TEXTURE] = {
                0, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT};
            m_memoryTypeInfos[RPS_D3D12_HEAP_TYPE_INDEX_DEFAULT_TIER_1_RT_DS_TEXTURE_MSAA] = {
                0, D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT};
            m_memoryTypeInfos[RPS_D3D12_HEAP_TYPE_INDEX_DEFAULT_TIER_1_BUFFER] = {
                0, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT};
            m_memoryTypeInfos[RPS_D3D12_HEAP_TYPE_INDEX_DEFAULT_TIER_1_NON_RT_DS_TEXTURE] = {
                0, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT};
        }

        for (uint32_t iDH = 0; iDH < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; iDH++)
        {
            m_descriptorIncSizes[iDH] = m_pD3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE(iDH));
        }

        D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = {DXGI_FORMAT_UNKNOWN, 0};

        for (uint32_t i = 0; i < RPS_FORMAT_COUNT; i++)
        {
            formatInfo.Format = rpsFormatToDXGI(RpsFormat(i));

            if (FAILED(m_pD3DDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof(formatInfo))))
            {
                // Failure means format is not supported, non-fatal.
                formatInfo.PlaneCount = 0;
            }
            m_formatPlaneCount[i] = formatInfo.PlaneCount;
        }

        // TODO:
        m_formatPlaneCount[RPS_FORMAT_R32_FLOAT_X8X24_TYPELESS] = 1;
        m_formatPlaneCount[RPS_FORMAT_X32_TYPELESS_G8X24_UINT]  = 1;
        m_formatPlaneCount[RPS_FORMAT_R24_UNORM_X8_TYPELESS]    = 1;
        m_formatPlaneCount[RPS_FORMAT_X24_TYPELESS_G8_UINT]     = 1;

        return RPS_OK;
    }

    D3D12RuntimeDevice::~D3D12RuntimeDevice()
    {
        SafeRelease(m_pD3DDevice2);
        SafeRelease(m_pD3DDevice);
    }

    RpsResult D3D12RuntimeDevice::BuildDefaultRenderGraphPhases(RenderGraph& renderGraph)
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
        RPS_V_RETURN(renderGraph.AddPhase<D3D12RuntimeBackend>(*this, renderGraph));

        return RPS_OK;
    }

    RpsResult D3D12RuntimeDevice::InitializeSubresourceInfos(ArrayRef<ResourceInstance> resInstances)
    {
        for (auto& resInstance : resInstances)
        {
            GetFullSubresourceRange(
                resInstance.fullSubresourceRange, resInstance.desc, GetResourcePlaneMask(resInstance.desc));

            resInstance.numSubResources = GetSubresourceCount(resInstance.desc);
        }

        return RPS_OK;
    }

    RpsResult D3D12RuntimeDevice::InitializeResourceAllocInfos(ArrayRef<ResourceInstance> resInstances)
    {
        for (auto& resInst : resInstances)
        {
            if (resInst.isPendingCreate)
            {
                if (resInst.desc.IsBuffer() && (resInst.allAccesses.accessFlags & RPS_ACCESS_CONSTANT_BUFFER_BIT))
                {
                    resInst.desc.SetBufferSize(rpsAlignUp(resInst.desc.GetBufferSize(),
                                                          uint64_t(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)));
                }

                auto allocInfo = GetResourceAllocInfo(resInst);
                RPS_RETURN_ERROR_IF(allocInfo.SizeInBytes > SIZE_MAX, RPS_ERROR_INTEGER_OVERFLOW);
                RPS_RETURN_ERROR_IF(allocInfo.Alignment > UINT32_MAX, RPS_ERROR_INTEGER_OVERFLOW);

                resInst.allocRequirement.size            = uint64_t(allocInfo.SizeInBytes);
                resInst.allocRequirement.alignment       = uint32_t(allocInfo.Alignment);
                resInst.allocRequirement.memoryTypeIndex = GetD3D12HeapTypeIndex(m_heapTier, resInst);
            }
        }

        return RPS_OK;
    }

    RpsResult D3D12RuntimeDevice::GetSubresourceRangeFromImageView(SubresourceRangePacked& outRange,
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

    ConstArrayRef<RpsMemoryTypeInfo> D3D12RuntimeDevice::GetMemoryTypeInfos() const
    {
        return {m_memoryTypeInfos,
                size_t((m_heapTier == D3D12_RESOURCE_HEAP_TIER_1) ? RPS_D3D12_HEAP_TYPE_COUNT_TIER_1
                                                                  : RPS_D3D12_HEAP_TYPE_COUNT_TIER_2)};
    }

    RpsResult D3D12RuntimeDevice::DescribeMemoryType(uint32_t memoryTypeIndex, PrinterRef printer) const
    {
        const auto memoryTypeInfos = GetMemoryTypeInfos();

        RPS_RETURN_ERROR_IF(memoryTypeIndex >= memoryTypeInfos.size(), RPS_ERROR_INDEX_OUT_OF_BOUNDS);

        const auto& heapTypeInfo = GetD3D12HeapTypeInfo(memoryTypeIndex);

        static const NameValuePair<D3D12_HEAP_TYPE> heapTypeNames[] = {
            RPS_INIT_NAME_VALUE_PAIR(D3D12_HEAP_TYPE_DEFAULT),
            RPS_INIT_NAME_VALUE_PAIR(D3D12_HEAP_TYPE_UPLOAD),
            RPS_INIT_NAME_VALUE_PAIR(D3D12_HEAP_TYPE_READBACK),
            RPS_INIT_NAME_VALUE_PAIR(D3D12_HEAP_TYPE_CUSTOM),
        };

        printer.PrintValueName(heapTypeInfo.type, heapTypeNames);

        static const NameValuePair<D3D12_HEAP_FLAGS> heapFlagNames[] = {
            RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_HEAP_FLAG_, SHARED),
            RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_HEAP_FLAG_, DENY_BUFFERS),
            RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_HEAP_FLAG_, ALLOW_DISPLAY),
            RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_HEAP_FLAG_, SHARED_CROSS_ADAPTER),
            RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_HEAP_FLAG_, DENY_RT_DS_TEXTURES),
            RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_HEAP_FLAG_, DENY_NON_RT_DS_TEXTURES),
            RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_HEAP_FLAG_, HARDWARE_PROTECTED),
            RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_HEAP_FLAG_, ALLOW_WRITE_WATCH),
            RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_HEAP_FLAG_, ALLOW_SHADER_ATOMICS),
            RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_HEAP_FLAG_, CREATE_NOT_RESIDENT),
            RPS_INIT_NAME_VALUE_PAIR_PREFIXED(D3D12_HEAP_FLAG_, CREATE_NOT_ZEROED),
        };

        printer("-FLAG_").PrintFlags(heapTypeInfo.heapFlags, heapFlagNames, "_");

        if (heapTypeInfo.AllowMsaa)
            printer("-ALLOW_MSAA");

        return RPS_OK;
    }

    const D3D12HeapTypeInfo& D3D12RuntimeDevice::GetD3D12HeapTypeInfo(uint32_t memoryTypeIndex) const
    {
        constexpr bool AllowMsaa      = true;
        constexpr bool NoMsaa         = false;

        // clang-format off
        static constexpr D3D12HeapTypeInfo s_d3dHeapTier1MemoryTypes[] = {
            {D3D12_HEAP_TYPE_UPLOAD,   D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS,            NoMsaa },
            {D3D12_HEAP_TYPE_READBACK, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS,            NoMsaa },
            {D3D12_HEAP_TYPE_DEFAULT,  D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES,     NoMsaa },
            {D3D12_HEAP_TYPE_DEFAULT,  D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES,     AllowMsaa },
            {D3D12_HEAP_TYPE_DEFAULT,  D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS,            NoMsaa },
            {D3D12_HEAP_TYPE_DEFAULT,  D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES, NoMsaa },
        };

        //TODO: UMA
        static constexpr D3D12HeapTypeInfo s_d3dHeapTier2MemoryTypes[] = {
            {D3D12_HEAP_TYPE_UPLOAD,   D3D12_HEAP_FLAG_NONE, NoMsaa },
            {D3D12_HEAP_TYPE_READBACK, D3D12_HEAP_FLAG_NONE, NoMsaa },
            {D3D12_HEAP_TYPE_DEFAULT,  D3D12_HEAP_FLAG_NONE, NoMsaa },
            {D3D12_HEAP_TYPE_DEFAULT,  D3D12_HEAP_FLAG_NONE, AllowMsaa },
        };
        // clang-format on

        if (m_heapTier == D3D12_RESOURCE_HEAP_TIER_2)
        {
            return s_d3dHeapTier2MemoryTypes[memoryTypeIndex];
        }
        else
        {
            return s_d3dHeapTier1MemoryTypes[memoryTypeIndex];
        }
    }

    D3D12_RESOURCE_ALLOCATION_INFO D3D12RuntimeDevice::GetResourceAllocInfo(const ResourceInstance& resInstance) const
    {
        D3D12_RESOURCE_DESC d3d12Desc;
        CalcD3D12ResourceDesc(&d3d12Desc, resInstance);  // TODO: Should cache D3D12 res desc?
        return m_pD3DDevice->GetResourceAllocationInfo(1, 1, &d3d12Desc);
    }

    uint32_t D3D12RuntimeDevice::GetSubresourceCount(const ResourceDescPacked& resDesc) const
    {
        return resDesc.IsBuffer() ? 1
                                  : (((resDesc.type == RPS_RESOURCE_TYPE_IMAGE_3D) ? 1 : resDesc.image.arrayLayers) *
                                     resDesc.image.mipLevels * GetFormatPlaneCount(resDesc.image.format));
    }

    uint32_t D3D12RuntimeDevice::GetResourcePlaneMask(const ResourceDescPacked& resDesc) const
    {
        return resDesc.IsBuffer() ? 1u : GetFormatPlaneMask(resDesc.image.format);
    }

    void D3D12BuiltInClearColorRegions(const RpsCmdCallbackContext* pContext);
    void D3D12BuiltInClearColor(const RpsCmdCallbackContext* pContext);
    void D3D12BuiltInClearDepthStencil(const RpsCmdCallbackContext* pContext);
    void D3D12BuiltInClearDepthStencilRegions(const RpsCmdCallbackContext* pContext);
    void D3D12BuiltInClearTextureUAV(const RpsCmdCallbackContext* pContext);
    void D3D12BuiltInClearTextureUAVRegions(const RpsCmdCallbackContext* pContext);
    void D3D12BuiltInClearBufferUAV(const RpsCmdCallbackContext* pContext);
    void D3D12BuiltInCopyTexture(const RpsCmdCallbackContext* pContext);
    void D3D12BuiltInCopyBuffer(const RpsCmdCallbackContext* pContext);
    void D3D12BuiltInCopyTextureToBuffer(const RpsCmdCallbackContext* pContext);
    void D3D12BuiltInCopyBufferToTexture(const RpsCmdCallbackContext* pContext);
    void D3D12BuiltInResolve(const RpsCmdCallbackContext* pContext);

    ConstArrayRef<BuiltInNodeInfo> D3D12RuntimeDevice::GetBuiltInNodes() const
    {
        static const BuiltInNodeInfo c_builtInNodes[] = {
            {"clear_color",                 {&D3D12BuiltInClearColor, nullptr}},
            {"clear_color_regions",         {&D3D12BuiltInClearColorRegions, nullptr}},
            {"clear_depth_stencil",         {&D3D12BuiltInClearDepthStencil, nullptr}},
            {"clear_depth_stencil_regions", {&D3D12BuiltInClearDepthStencilRegions, nullptr}},
            {"clear_texture",               {&D3D12BuiltInClearTextureUAV, nullptr}},
            {"clear_texture_regions",       {&D3D12BuiltInClearTextureUAVRegions, nullptr}},
            {"clear_buffer",                {&D3D12BuiltInClearBufferUAV,nullptr}},
            {"copy_texture",                {&D3D12BuiltInCopyTexture,nullptr}},
            {"copy_buffer",                 {&D3D12BuiltInCopyBuffer,nullptr}},
            {"copy_texture_to_buffer",      {&D3D12BuiltInCopyTextureToBuffer,nullptr}},
            {"copy_buffer_to_texture",      {&D3D12BuiltInCopyBufferToTexture,nullptr}},
            {"resolve",                     {&D3D12BuiltInResolve,nullptr}},
        };

        return c_builtInNodes;
    }

    bool D3D12RuntimeDevice::CalculateAccessTransition(const RpsAccessAttr&  beforeAccess,
                                                       const RpsAccessAttr&  afterAccess,
                                                       AccessTransitionInfo& results) const
    {

        const bool bHasClear = rpsAnyBitsSet(beforeAccess.accessFlags | afterAccess.accessFlags, RPS_ACCESS_CLEAR_BIT);

        const bool bBothAreRenderTarget = ((beforeAccess.accessFlags & RPS_ACCESS_RENDER_TARGET_BIT) &&
                                        (afterAccess.accessFlags & RPS_ACCESS_RENDER_TARGET_BIT));

        const bool bBothAreDepthStencil = ((beforeAccess.accessFlags & RPS_ACCESS_DEPTH_STENCIL_WRITE) &&
                                        (afterAccess.accessFlags & RPS_ACCESS_DEPTH_STENCIL_WRITE));

        const bool bDepthStencilRWTransition =
            IsDepthStencilReadWriteTransition(beforeAccess.accessFlags, afterAccess.accessFlags);

        const bool bBothAreUAV = ((beforeAccess.accessFlags & RPS_ACCESS_UNORDERED_ACCESS_BIT) &&
                                  (afterAccess.accessFlags & RPS_ACCESS_UNORDERED_ACCESS_BIT));

        if (bBothAreRenderTarget || (bBothAreDepthStencil && !bDepthStencilRWTransition) ||
            (bHasClear && !GetEnhancedBarrierEnabled() && bBothAreUAV))
        {
            // D3D12 doesn't need a barrier between Clear RTV/DSV and RTV/DSV access, or ClearUAV to UAV without enhanced barrier.
            results.bKeepOrdering       = true;
            results.bMergedAccessStates = true;
            results.bTransition         = false;
            results.mergedAccess        = beforeAccess | afterAccess;

            return true;
        }

        return false;
    }
}  // namespace rps

RpsResult rpsD3D12RuntimeDeviceCreate(const RpsD3D12RuntimeDeviceCreateInfo* pCreateInfo, RpsDevice* phDevice)
{
    RPS_CHECK_ARGS(pCreateInfo && pCreateInfo->pD3D12Device);

    RpsResult result =
        rps::RuntimeDevice::Create<rps::D3D12RuntimeDevice>(phDevice, pCreateInfo->pDeviceCreateInfo, pCreateInfo);

    return result;
}
