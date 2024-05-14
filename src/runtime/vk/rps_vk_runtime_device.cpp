// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "runtime/vk/rps_vk_runtime_device.hpp"
#include "runtime/vk/rps_vk_runtime_backend.hpp"
#include "runtime/vk/rps_vk_util.hpp"

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


namespace rps
{
    VKRuntimeDevice::VKRuntimeDevice(Device* pDevice, const RpsVKRuntimeDeviceCreateInfo* pCreateInfo)
        : RuntimeDevice(pDevice, pCreateInfo->pRuntimeCreateInfo)
        , m_device(pCreateInfo->hVkDevice)
        , m_physicalDevice(pCreateInfo->hVkPhysicalDevice)
        , m_flags(pCreateInfo->flags)
#ifdef RPS_VK_DYNAMIC_LOADING
        , m_vkFunctions(*pCreateInfo->pVkFunctions)
#endif
    {
    }

    RpsResult VKRuntimeDevice::Init()
    {
        RPS_USE_VK_FUNCTIONS(m_vkFunctions);
        RPS_VK_API_CALL(vkGetPhysicalDeviceProperties(m_physicalDevice, &m_deviceProperties));
        RPS_VK_API_CALL(vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_deviceMemoryProperties));

        static_assert(VK_MAX_MEMORY_TYPES <= 32, "Bitwidth of m_hostVisibleMemoryTypeMask needs extending.");

        for (uint32_t iMemTy = 0; iMemTy < m_deviceMemoryProperties.memoryTypeCount; iMemTy++)
        {
            if (m_deviceMemoryProperties.memoryTypes[iMemTy].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
                m_hostVisibleMemoryTypeMask |= (1u << iMemTy);
        }

        return RPS_OK;
    }

    VKRuntimeDevice::~VKRuntimeDevice()
    {
    }

    RpsResult VKRuntimeDevice::BuildDefaultRenderGraphPhases(RenderGraph& renderGraph)
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
        RPS_V_RETURN(renderGraph.AddPhase<VKRuntimeBackend>(*this, renderGraph));

        return RPS_OK;
    }

    RpsResult VKRuntimeDevice::InitializeSubresourceInfos(ArrayRef<ResourceInstance> resInstances)
    {
        for (auto& resInstance : resInstances)
        {
            GetFullSubresourceRange(
                resInstance.fullSubresourceRange, resInstance.desc, GetResourceAspectMask(resInstance));

            resInstance.numSubResources = GetSubresourceCount(resInstance);
        }

        return RPS_OK;
    }

    uint32_t FinalizeMemoryType(const VkPhysicalDeviceMemoryProperties& memProps,
                                const uint32_t                          hostVisibleMemoryTypeMask,
                                const VkMemoryRequirements&             requirements,
                                const ResourceInstance&                 resInfo)
    {
        if (requirements.size == 0)
            return UINT32_MAX;

        const bool bHostRead  = resInfo.allAccesses.accessFlags & RPS_ACCESS_CPU_READ_BIT;
        const bool bHostWrite = resInfo.allAccesses.accessFlags & RPS_ACCESS_CPU_WRITE_BIT;

        // No CPU access or explicitly prefers device local:
        const bool bPreferLocal =
            !(bHostRead || bHostWrite) || (resInfo.desc.flags & RPS_RESOURCE_FLAG_PREFER_GPU_LOCAL_CPU_VISIBLE_BIT);

        uint32_t typeBits = requirements.memoryTypeBits;

        if (bHostRead || bHostWrite)
        {
            typeBits &= hostVisibleMemoryTypeMask;
        }

        uint32_t highScore   = 0;
        uint32_t firstMemIdx = UINT32_MAX;

        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
        {
            uint32_t currScore = 0;

            if ((typeBits & 1) == 1)
            {
                currScore = 0x1;

                if (bPreferLocal && (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
                {
                    currScore |= 0x8;
                }

                if (bHostRead && (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT))
                {
                    currScore |= 0x4;
                }

                if ((bHostRead || bHostWrite) &&
                    (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
                {
                    currScore |= 0x2;
                }
            }

            typeBits >>= 1;

            if (highScore < currScore)
            {
                firstMemIdx = i;
                highScore   = currScore;
            }
        }

        RPS_ASSERT(firstMemIdx != UINT32_MAX);

        return firstMemIdx;
    }

    RpsResult VKRuntimeDevice::InitializeResourceAllocInfos(ArrayRef<ResourceInstance> resInstances)
    {
        for (auto& resInst : resInstances)
        {
            if (!resInst.isPendingCreate)
            {
                //TODO: This may impact memory size based scheduling for external resources
                continue;
            }

            // NOTE: hRuntimeResource can be non-null and isPendingCreate=true if pendingCreate persists for many frames.

            VKResourceAllocInfo allocInfo = {};
            RPS_V_RETURN(GetResourceAllocInfo(resInst, allocInfo));
            RPS_RETURN_ERROR_IF(allocInfo.memoryRequirements.alignment > UINT32_MAX, RPS_ERROR_INTEGER_OVERFLOW);

            if (!resInst.hRuntimeResource)
            {
                resInst.allocRequirement.size            = uint64_t(allocInfo.memoryRequirements.size);
                resInst.allocRequirement.alignment       = uint32_t(allocInfo.memoryRequirements.alignment);
                resInst.allocRequirement.memoryTypeIndex = FinalizeMemoryType(
                    m_deviceMemoryProperties, m_hostVisibleMemoryTypeMask, allocInfo.memoryRequirements, resInst);

                resInst.hRuntimeResource = allocInfo.hRuntimeResource;
            }
            else
            {
                RPS_ASSERT(allocInfo.hRuntimeResource == RPS_NULL_HANDLE);
            }
        }

        return RPS_OK;
    }

    RpsResult VKRuntimeDevice::GetSubresourceRangeFromImageView(SubresourceRangePacked& outRange,
                                                                const ResourceInstance& resourceInfo,
                                                                const RpsAccessAttr&    accessAttr,
                                                                const RpsImageView&     imageView)
    {
        uint32_t aspectMask = GetFormatAspectMask(imageView.base.viewFormat, rpsVkGetImageCreationFormat(resourceInfo));
        outRange            = SubresourceRangePacked(aspectMask, imageView.subresourceRange, resourceInfo.desc);
        return RPS_OK;
    }

    VkImageUsageFlags GetVkImageUsageFlags(const ResourceInstance& resInfo)
    {
        VkImageUsageFlags usage = 0;
        if (rpsAnyBitsSet(resInfo.allAccesses.accessFlags, RPS_ACCESS_SHADER_RESOURCE_BIT))
            usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

        if (rpsAnyBitsSet(resInfo.allAccesses.accessFlags, RPS_ACCESS_RENDER_TARGET_BIT))
            usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        if (rpsAnyBitsSet(resInfo.allAccesses.accessFlags, RPS_ACCESS_UNORDERED_ACCESS_BIT))
            usage |= VK_IMAGE_USAGE_STORAGE_BIT;

        if (rpsAnyBitsSet(resInfo.allAccesses.accessFlags, RPS_ACCESS_DEPTH_STENCIL))
            usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        // TODO: distinguish command clear and render pass clear
        if (rpsAnyBitsSet(resInfo.allAccesses.accessFlags,
                          (RPS_ACCESS_COPY_DEST_BIT | RPS_ACCESS_RESOLVE_DEST_BIT | RPS_ACCESS_CLEAR_BIT)))
            usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        if (rpsAnyBitsSet(resInfo.allAccesses.accessFlags, (RPS_ACCESS_COPY_SRC_BIT | RPS_ACCESS_RESOLVE_SRC_BIT)))
            usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

#ifdef VK_KHR_fragment_shading_rate
        if (rpsAnyBitsSet(resInfo.allAccesses.accessFlags, RPS_ACCESS_SHADING_RATE_BIT))
            usage |= VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
#endif

        return usage;
    }

    static inline VkImageCreateFlags GetImageCreateFlags(const ResourceInstance& resInfo)
    {
        VkImageCreateFlags flags = 0;

        const bool isImage3D = (resInfo.desc.type == RPS_RESOURCE_TYPE_IMAGE_3D);
        const bool isCubemap = !!(resInfo.desc.flags & RPS_RESOURCE_FLAG_CUBEMAP_COMPATIBLE_BIT);

        if (resInfo.desc.flags & RPS_RESOURCE_FLAG_CUBEMAP_COMPATIBLE_BIT)
            flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

        // TODO: Add VkImageFormatListCreateInfo
        if (resInfo.isMutableFormat)
            flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

        if (isImage3D && rpsAnyBitsSet(resInfo.allAccesses.accessFlags, RPS_ACCESS_RENDER_TARGET_BIT))
            flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;

        if (isCubemap)
            flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

        return flags;
    }

    void GetVkImageCreateInfo(VkImageCreateInfo& imgCI, const ResourceInstance& resInfo)
    {
        RPS_ASSERT(resInfo.desc.IsImage());

        const bool isImage3D  = (resInfo.desc.type == RPS_RESOURCE_TYPE_IMAGE_3D);
        const bool isRowMajor = rpsAnyBitsSet(resInfo.desc.flags, RPS_RESOURCE_FLAG_ROWMAJOR_IMAGE_BIT);

        imgCI.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgCI.pNext                 = NULL;
        imgCI.flags                 = GetImageCreateFlags(resInfo);
        imgCI.imageType             = rpsVkGetImageType(resInfo.desc.type);
        imgCI.format                = rpsFormatToVK(rpsVkGetImageCreationFormat(resInfo));
        imgCI.extent.width          = resInfo.desc.image.width;
        imgCI.extent.height         = resInfo.desc.image.height;
        imgCI.extent.depth          = isImage3D ? resInfo.desc.image.depth : 1;
        imgCI.mipLevels             = resInfo.desc.image.mipLevels;
        imgCI.arrayLayers           = isImage3D ? 1 : resInfo.desc.image.arrayLayers;
        imgCI.samples               = rpsVkGetSampleCount(resInfo.desc.image.sampleCount);
        imgCI.tiling                = isRowMajor ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
        imgCI.usage                 = GetVkImageUsageFlags(resInfo);
        imgCI.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
        imgCI.queueFamilyIndexCount = 0;
        imgCI.pQueueFamilyIndices   = nullptr;
        imgCI.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    void GetVkBufferCreateInfo(VkBufferCreateInfo& bufCI, const ResourceInstance& resInfo)
    {
        VkFlags vkUsageFlags = 0;
        if (rpsAnyBitsSet(resInfo.allAccesses.accessFlags, RPS_ACCESS_COPY_SRC_BIT))
            vkUsageFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        if (rpsAnyBitsSet(resInfo.allAccesses.accessFlags, RPS_ACCESS_COPY_DEST_BIT))
            vkUsageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if (rpsAnyBitsSet(resInfo.allAccesses.accessFlags, RPS_ACCESS_CONSTANT_BUFFER_BIT))
            vkUsageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        if (rpsAnyBitsSet(resInfo.allAccesses.accessFlags,
                          RPS_ACCESS_UNORDERED_ACCESS_BIT | RPS_ACCESS_SHADER_RESOURCE_BIT))
            vkUsageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if (rpsAnyBitsSet(resInfo.allAccesses.accessFlags, RPS_ACCESS_INDEX_BUFFER_BIT))
            vkUsageFlags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (rpsAnyBitsSet(resInfo.allAccesses.accessFlags, RPS_ACCESS_VERTEX_BUFFER_BIT))
            vkUsageFlags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if (rpsAnyBitsSet(resInfo.allAccesses.accessFlags, RPS_ACCESS_INDIRECT_ARGS_BIT))
            vkUsageFlags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        if (resInfo.bBufferFormattedWrite)
            vkUsageFlags |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
        if (resInfo.bBufferFormattedRead)
            vkUsageFlags |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;

        bufCI.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufCI.pNext                 = nullptr;
        bufCI.flags                 = 0;
        bufCI.size                  = resInfo.desc.GetBufferSize();
        bufCI.usage                 = vkUsageFlags;
        bufCI.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
        bufCI.queueFamilyIndexCount = 0;
        bufCI.pQueueFamilyIndices   = nullptr;
    }

    VkMemoryType VKRuntimeDevice::GetVkHeapTypeInfo(uint32_t memTypeIndex) const
    {
        return m_deviceMemoryProperties.memoryTypes[memTypeIndex];
    }

    RpsResult VKRuntimeDevice::GetResourceAllocInfo(const ResourceInstance& resInstance,
                                                    VKResourceAllocInfo&    allocInfo) const
    {
        allocInfo = {};

        const bool bUsageBitsNonZero = (resInstance.allAccesses.accessFlags != RPS_ACCESS_UNKNOWN);

        // TODO: Check pending state
        if (!resInstance.hRuntimeResource && bUsageBitsNonZero)
        {
            RPS_USE_VK_FUNCTIONS(m_vkFunctions);
            if (resInstance.desc.IsImage())
            {
                VkImageCreateInfo imgCI;
                GetVkImageCreateInfo(imgCI, resInstance);

                VkImage hImage;
                RPS_V_RETURN(VkResultToRps(RPS_VK_API_CALL(vkCreateImage(m_device, &imgCI, nullptr, &hImage))));

                allocInfo.hRuntimeResource = ToHandle(hImage);
                RPS_VK_API_CALL(vkGetImageMemoryRequirements(m_device, hImage, &allocInfo.memoryRequirements));
            }
            else if (resInstance.desc.IsBuffer())
            {
                VkBufferCreateInfo bufCI;
                GetVkBufferCreateInfo(bufCI, resInstance);

                VkBuffer hBuffer;
                RPS_V_RETURN(VkResultToRps(RPS_VK_API_CALL(vkCreateBuffer(m_device, &bufCI, nullptr, &hBuffer))));

                allocInfo.hRuntimeResource = ToHandle(hBuffer);
                RPS_VK_API_CALL(vkGetBufferMemoryRequirements(m_device, hBuffer, &allocInfo.memoryRequirements));
            }
        }

        return RPS_OK;
    }

    uint32_t VKRuntimeDevice::GetResourceAspectMask(const ResourceInstance& resInfo) const
    {
        const auto&     resourceDesc   = resInfo.desc;
        const RpsFormat imgCreationFmt = rpsVkGetImageCreationFormat(resInfo);

        return resourceDesc.IsImage() ? GetFormatAspectMask(imgCreationFmt, RPS_FORMAT_UNKNOWN) : 1;
    }

    uint32_t VKRuntimeDevice::GetSubresourceCount(const ResourceInstance& resInfo) const
    {
        const auto& resDesc = resInfo.desc;

        return resDesc.IsBuffer()
                   ? 1
                   : (((resDesc.type == RPS_RESOURCE_TYPE_IMAGE_3D) ? 1 : resDesc.image.arrayLayers) *
                      resDesc.image.mipLevels * GetFormatPlaneCount(rpsVkGetImageCreationFormat(resInfo)));
    }

    void VKBuiltInClearColorRegions(const RpsCmdCallbackContext* pContext);
    void VKBuiltInClearColor(const RpsCmdCallbackContext* pContext);
    void VKBuiltInClearDepthStencil(const RpsCmdCallbackContext* pContext);
    void VKBuiltInClearDepthStencilRegions(const RpsCmdCallbackContext* pContext);
    void VKBuiltInClearTextureUAV(const RpsCmdCallbackContext* pContext);
    void VKBuiltInClearTextureUAVRegions(const RpsCmdCallbackContext* pContext);
    void VKBuiltInClearBufferUAV(const RpsCmdCallbackContext* pContext);
    void VKBuiltInCopyTexture(const RpsCmdCallbackContext* pContext);
    void VKBuiltInCopyBuffer(const RpsCmdCallbackContext* pContext);
    void VKBuiltInCopyTextureToBuffer(const RpsCmdCallbackContext* pContext);
    void VKBuiltInCopyBufferToTexture(const RpsCmdCallbackContext* pContext);
    void VKBuiltInResolve(const RpsCmdCallbackContext* pContext);

    ConstArrayRef<BuiltInNodeInfo> VKRuntimeDevice::GetBuiltInNodes() const
    {
        static const BuiltInNodeInfo c_builtInNodes[] = {
            {"clear_color", {&VKBuiltInClearColor, nullptr}},
            {"clear_color_regions", {&VKBuiltInClearColorRegions, nullptr}},
            {"clear_depth_stencil", {&VKBuiltInClearDepthStencil, nullptr}},
            {"clear_depth_stencil_regions", {&VKBuiltInClearDepthStencilRegions, nullptr}},
            {"clear_texture", {&VKBuiltInClearTextureUAV, nullptr}},
            {"clear_texture_regions", {&VKBuiltInClearTextureUAVRegions, nullptr}},
            {"clear_buffer", {&VKBuiltInClearBufferUAV, nullptr}},
            {"copy_texture", {&VKBuiltInCopyTexture, nullptr}},
            {"copy_buffer", {&VKBuiltInCopyBuffer, nullptr}},
            {"copy_texture_to_buffer", {&VKBuiltInCopyTextureToBuffer, nullptr}},
            {"copy_buffer_to_texture", {&VKBuiltInCopyBufferToTexture, nullptr}},
            {"resolve", {&VKBuiltInResolve, nullptr}},
        };

        return c_builtInNodes;
    }

    ConstArrayRef<RpsMemoryTypeInfo> VKRuntimeDevice::GetMemoryTypeInfos() const
    {
        static constexpr RpsMemoryTypeInfo s_MemTypes[VK_MAX_MEMORY_TYPES] = {};
        return {s_MemTypes, m_deviceMemoryProperties.memoryTypeCount};
    }

    RpsResult VKRuntimeDevice::DescribeMemoryType(uint32_t memoryTypeIndex, PrinterRef printer) const
    {
        const VkMemoryType memoryTypeInfo = GetVkHeapTypeInfo(memoryTypeIndex);

        static const NameValuePair<VkMemoryPropertyFlags> memoryPropertyFlagNames[] = {
            RPS_INIT_NAME_VALUE_PAIR_PREFIXED(VK_MEMORY_PROPERTY_, DEVICE_LOCAL_BIT),
            RPS_INIT_NAME_VALUE_PAIR_PREFIXED(VK_MEMORY_PROPERTY_, HOST_VISIBLE_BIT),
            RPS_INIT_NAME_VALUE_PAIR_PREFIXED(VK_MEMORY_PROPERTY_, HOST_COHERENT_BIT),
            RPS_INIT_NAME_VALUE_PAIR_PREFIXED(VK_MEMORY_PROPERTY_, HOST_CACHED_BIT),
            RPS_INIT_NAME_VALUE_PAIR_PREFIXED(VK_MEMORY_PROPERTY_, LAZILY_ALLOCATED_BIT),
            RPS_INIT_NAME_VALUE_PAIR_PREFIXED(VK_MEMORY_PROPERTY_, PROTECTED_BIT),
#ifdef VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME
            RPS_INIT_NAME_VALUE_PAIR_PREFIXED(VK_MEMORY_PROPERTY_, DEVICE_COHERENT_BIT_AMD),
            RPS_INIT_NAME_VALUE_PAIR_PREFIXED(VK_MEMORY_PROPERTY_, DEVICE_UNCACHED_BIT_AMD),
#endif
#ifdef VK_NV_EXTERNAL_MEMORY_RDMA_EXTENSION_NAME
            RPS_INIT_NAME_VALUE_PAIR_PREFIXED(VK_MEMORY_PROPERTY_, RDMA_CAPABLE_BIT_NV),
#endif

        };

        printer("MEMORY_PROPERTY_").PrintFlags(memoryTypeInfo.propertyFlags, memoryPropertyFlagNames, "_");
        printer("-VK_HEAP_INDEX_%d", memoryTypeInfo.heapIndex);

        return RPS_OK;
    }

    bool VKRuntimeDevice::CalculateAccessTransition(const RpsAccessAttr&  beforeAccess,
                                                    const RpsAccessAttr&  afterAccess,
                                                    AccessTransitionInfo& results) const
    {
        const auto transferSrcAccess = RPS_ACCESS_COPY_SRC_BIT | RPS_ACCESS_RESOLVE_SRC_BIT;

        // Handle transfer src layout, when it can't be merged with other readonly accesses
        if ((beforeAccess.accessFlags & transferSrcAccess) != (afterAccess.accessFlags & transferSrcAccess))
        {
            results.bKeepOrdering       = false;
            results.bTransition         = true;
            results.bMergedAccessStates = false;
            return true;
        }

        // Handle transfer dst to transfer dst access, may need a barrier in between
        const auto transferDstAccess = RPS_ACCESS_CLEAR_BIT | RPS_ACCESS_COPY_DEST_BIT | RPS_ACCESS_RESOLVE_DEST_BIT;

        if ((beforeAccess == afterAccess) && (beforeAccess.accessFlags & transferDstAccess))
        {
            results.bKeepOrdering       = true;
            results.bTransition         = true;
            results.bMergedAccessStates = false;
            return true;
        }

        return false;
    }

    RpsRuntimeResource ToHandle(VkImage hImage)
    {
        return RpsRuntimeResource{hImage};  // TODO: Handle !VK_USE_64_BIT_PTR_DEFINES
    }

    RpsRuntimeResource ToHandle(VkBuffer hBuffer)
    {
        return RpsRuntimeResource{hBuffer};
    }

    void FromHandle(VkImage& hImage, RpsRuntimeResource hResource)
    {
        hImage = VkImage(hResource.ptr);
    }

    void FromHandle(VkBuffer& hBuffer, RpsRuntimeResource hResource)
    {
        hBuffer = VkBuffer(hResource.ptr);
    }

}  // namespace rps

RpsResult rpsVKRuntimeDeviceCreate(const RpsVKRuntimeDeviceCreateInfo* pCreateInfo, RpsDevice* phDevice)
{
    RPS_CHECK_ARGS(pCreateInfo);
    RPS_CHECK_ARGS(pCreateInfo->hVkDevice != VK_NULL_HANDLE);
    RPS_CHECK_ARGS(pCreateInfo->hVkPhysicalDevice != VK_NULL_HANDLE);

#ifdef RPS_VK_DYNAMIC_LOADING
#define RPS_VK_CHECK_FUNCTION_POINTER(callName) RPS_CHECK_ARGS(pCreateInfo->pVkFunctions->callName);
    RPS_CHECK_ARGS(pCreateInfo->pVkFunctions);

    RPS_VK_FUNCTION_TABLE(RPS_VK_CHECK_FUNCTION_POINTER)
#endif
    RpsResult result =
        rps::RuntimeDevice::Create<rps::VKRuntimeDevice>(phDevice, pCreateInfo->pDeviceCreateInfo, pCreateInfo);

    return result;
}
