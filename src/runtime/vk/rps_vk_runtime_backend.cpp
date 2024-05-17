// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "rps/runtime/common/rps_render_states.h"
#include "rps/runtime/vk/rps_vk_runtime.h"

#include "runtime/common/rps_render_graph.hpp"
#include "runtime/common/rps_runtime_util.hpp"

#include "runtime/vk/rps_vk_runtime_backend.hpp"
#include "runtime/vk/rps_vk_util.hpp"
#include "runtime/vk/rps_vk_runtime_device.hpp"

namespace rps
{
    static constexpr bool IsSrcLayoutTrue  = true;
    static constexpr bool IsSrcLayoutFalse = false;

    template <bool bSrcLayout>
    VkImageLayout GetVkImageLayout(const RpsAccessAttr& access)
    {
        RPS_ASSERT(access.accessFlags != RPS_ACCESS_UNKNOWN);

        if (access.accessFlags == RPS_ACCESS_PRESENT_BIT)
            return bSrcLayout ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_RENDER_TARGET_BIT))
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        if (rpsAllBitsSet(access.accessFlags, RPS_ACCESS_DEPTH_WRITE_BIT | RPS_ACCESS_STENCIL_READ_BIT))
            return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
        else if (rpsAllBitsSet(access.accessFlags, RPS_ACCESS_DEPTH_READ_BIT | RPS_ACCESS_STENCIL_WRITE_BIT))
            return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
        else if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_DEPTH_WRITE_BIT | RPS_ACCESS_STENCIL_WRITE_BIT))
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_UNORDERED_ACCESS_BIT))
            return VK_IMAGE_LAYOUT_GENERAL;

        if (rpsAnyBitsSet(access.accessFlags,
                          RPS_ACCESS_CLEAR_BIT | RPS_ACCESS_RESOLVE_DEST_BIT | RPS_ACCESS_COPY_DEST_BIT))
            return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_DEPTH_STENCIL_READ))
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        else if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_SHADER_RESOURCE_BIT))
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        else if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_RESOLVE_SRC_BIT | RPS_ACCESS_COPY_SRC_BIT))
            return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        return VK_IMAGE_LAYOUT_UNDEFINED;
    }

    struct VKAccessInfo
    {
        VkPipelineStageFlags stages;
        VkAccessFlags        access;
        VkImageLayout        imgLayout;
        uint32_t             queueFamilyIndex;

        VKAccessInfo() = default;

        VKAccessInfo(VkPipelineStageFlags inStages,
                     VkAccessFlags        inAccess,
                     VkImageLayout        inImgLayout,
                     uint32_t             inQueueFamilyIndex)
            : stages(inStages)
            , access(inAccess)
            , imgLayout(inImgLayout)
            , queueFamilyIndex(inQueueFamilyIndex)
        {
        }
    };

    static inline VkPipelineStageFlags GetVkPipelineStagesForShaderStages(RpsShaderStageFlags stages)
    {
        static constexpr struct
        {
            VkPipelineStageFlags vkFlags;
            RpsShaderStageBits   rpsFlags;
        } stageMap[] = {
            {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, RPS_SHADER_STAGE_VS},
            {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, RPS_SHADER_STAGE_PS},
            {VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, RPS_SHADER_STAGE_GS},
            {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, RPS_SHADER_STAGE_CS},
            {VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT, RPS_SHADER_STAGE_HS},
            {VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT, RPS_SHADER_STAGE_DS},
            {VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, RPS_SHADER_STAGE_RAYTRACING},
            {VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV, RPS_SHADER_STAGE_AS},
            {VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV, RPS_SHADER_STAGE_MS},
        };

        VkPipelineStageFlags vkFlags = VkPipelineStageFlags{0};

        for (auto iter = std::begin(stageMap); iter != std::end(stageMap); ++iter)
        {
            if (stages & iter->rpsFlags)
            {
                vkFlags |= iter->vkFlags;
            }
        }

        return vkFlags;
    }

    static constexpr bool IsRenderPassAttachmentTrue  = true;
    static constexpr bool IsRenderPassAttachmentFalse = false;
    static constexpr bool IsSrcAccessTrue             = true;
    static constexpr bool IsSrcAccessFalse            = false;

    // bRenderPassAttachment: Indicate if the access is used as a RenderPass attachment.
    //                        Currently mainly to distinguish between RenderPass clears (Attachment access)
    //                        from Cmd Clears (Transfer access).
    // bIsSrc:                Indicates if the access is associated with the source access / stage of a barrier.
    template <bool bRenderPassAttachment, bool bIsSrc>
    VKAccessInfo GetVKAccessInfo(const RpsAccessAttr& access)
    {
        const uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;  // TODO

        // TODO:
        if (access.accessFlags == RPS_ACCESS_UNKNOWN)
        {
            return VKAccessInfo{bIsSrc ? VK_PIPELINE_STAGE_ALL_COMMANDS_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                queueFamilyIndex};
        }

        const bool isRenderPassAttachment = bRenderPassAttachment || (access.accessFlags & RPS_ACCESS_RENDER_PASS);

        if (!isRenderPassAttachment && rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_CLEAR_BIT))
            return VKAccessInfo{VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_ACCESS_TRANSFER_WRITE_BIT,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                queueFamilyIndex};

        if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_RENDER_TARGET_BIT))
        {
            const bool isWriteOnly = rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_DISCARD_DATA_BEFORE_BIT);

            return VKAccessInfo(
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | (isWriteOnly ? 0 : VK_ACCESS_COLOR_ATTACHMENT_READ_BIT),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                queueFamilyIndex);
        }

        if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_DEPTH_STENCIL_WRITE))
        {
            VkImageLayout layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            if (rpsAllBitsSet(access.accessFlags, RPS_ACCESS_DEPTH_WRITE_BIT | RPS_ACCESS_STENCIL_READ_BIT))
                layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
            else if (rpsAllBitsSet(access.accessFlags, RPS_ACCESS_DEPTH_READ_BIT | RPS_ACCESS_STENCIL_WRITE_BIT))
                layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;

            const bool isWriteOnly = !rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_DEPTH_STENCIL_READ) &&
                                     (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_DEPTH_WRITE_BIT) ==
                                      rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_DISCARD_DATA_BEFORE_BIT)) &&
                                     (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_STENCIL_WRITE_BIT) ==
                                      rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_STENCIL_DISCARD_DATA_BEFORE_BIT));

            return VKAccessInfo(
                bIsSrc ? (VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT)
                       : VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                    (isWriteOnly ? 0 : VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT),
                layout,
                queueFamilyIndex);
        }

        if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_UNORDERED_ACCESS_BIT))
        {
            const auto shaderStages = GetVkPipelineStagesForShaderStages(access.accessStages);
            return VKAccessInfo{shaderStages,
                                VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                                VK_IMAGE_LAYOUT_GENERAL,
                                queueFamilyIndex};
        }

        if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_RESOLVE_DEST_BIT))
        {
            return isRenderPassAttachment ? VKAccessInfo{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                         queueFamilyIndex}
                                          : VKAccessInfo{VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                         VK_ACCESS_TRANSFER_WRITE_BIT,
                                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                         queueFamilyIndex};
        }

        if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_COPY_DEST_BIT))
            return VKAccessInfo{VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_ACCESS_TRANSFER_WRITE_BIT,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                queueFamilyIndex};

#ifdef VK_EXT_transform_feedback
        if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_STREAM_OUT_BIT))
            return VKAccessInfo{VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
                                VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                queueFamilyIndex};
#endif  //VK_EXT_transform_feedback

#ifdef VK_KHR_acceleration_structure
        if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_RAYTRACING_AS_BUILD_BIT))
            return VKAccessInfo{VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                queueFamilyIndex};
#endif  //VK_KHR_acceleration_structure

        if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_CPU_WRITE_BIT))
            return VKAccessInfo{
                VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, queueFamilyIndex};

        // TODO: RPS_ACCESS_PREDICATION_BIT => VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT

        // clang-format off
        static constexpr struct
        {
            RpsAccessFlags       rpsFlags;
            VkPipelineStageFlags stages;
            VkAccessFlags        access;
            VkImageLayout        imgLayout;
        } readAccessMap[] = {
            {RPS_ACCESS_INDIRECT_ARGS_BIT,      VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,    VK_ACCESS_INDIRECT_COMMAND_READ_BIT,    VK_IMAGE_LAYOUT_UNDEFINED},
            {RPS_ACCESS_INDEX_BUFFER_BIT,       VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,     VK_ACCESS_INDEX_READ_BIT,               VK_IMAGE_LAYOUT_UNDEFINED},
            {RPS_ACCESS_VERTEX_BUFFER_BIT,      VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,     VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,    VK_IMAGE_LAYOUT_UNDEFINED},
            {RPS_ACCESS_CONSTANT_BUFFER_BIT,    0,                                      VK_ACCESS_UNIFORM_READ_BIT,             VK_IMAGE_LAYOUT_UNDEFINED},
            {RPS_ACCESS_DEPTH_STENCIL_READ,     bIsSrc ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED},
            // | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;  // TODO: Adding DS Write bit since previous access might be RenderPass StoreOpStore. Which "uses the access type VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT"
            {RPS_ACCESS_SHADER_RESOURCE_BIT,    0,                                      VK_ACCESS_SHADER_READ_BIT,              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {RPS_ACCESS_COPY_SRC_BIT | RPS_ACCESS_RESOLVE_SRC_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL},
#ifdef VK_KHR_fragment_shading_rate
            {RPS_ACCESS_SHADING_RATE_BIT,       VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR, VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR, VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR},
#endif //VK_KHR_fragment_shading_rate
#ifdef VK_KHR_acceleration_structure
            {RPS_ACCESS_RAYTRACING_AS_READ_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR, VK_IMAGE_LAYOUT_UNDEFINED},
#endif //VK_KHR_acceleration_structure
            {RPS_ACCESS_PRESENT_BIT,            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,   VkAccessFlags{0},                       bIsSrc ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
            {RPS_ACCESS_CPU_READ_BIT,           VK_PIPELINE_STAGE_HOST_BIT,             VK_ACCESS_HOST_READ_BIT,                VK_IMAGE_LAYOUT_UNDEFINED},
        };
        // clang-format on

        VKAccessInfo result     = {};
        result.queueFamilyIndex = queueFamilyIndex;

        for (auto mapEntry = std::begin(readAccessMap); mapEntry != std::end(readAccessMap); ++mapEntry)
        {
            if (rpsAnyBitsSet(access.accessFlags, mapEntry->rpsFlags))
            {
                result.stages |= mapEntry->stages;
                result.access |= mapEntry->access;

                RPS_ASSERT((result.imgLayout == VK_IMAGE_LAYOUT_UNDEFINED) && "Unexpected image layout.");
                result.imgLayout = mapEntry->imgLayout;
            }
        }

        if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_CONSTANT_BUFFER_BIT | RPS_ACCESS_SHADER_RESOURCE_BIT))
        {
            result.stages |= GetVkPipelineStagesForShaderStages(access.accessStages);
        }

        if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_DEPTH_STENCIL_READ))
        {
            result.imgLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;  // TODO
        }

        if (result.stages == 0)
        {
            result.stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        }

        return result;
    }

    void DestroyVkResource(const VKRuntimeDevice&  device,
                           const ResourceInstance& resInfo,
                           RpsRuntimeResource      hRuntimeResource)
    {
        const VkDevice hVkDevice = device.GetVkDevice();
        RPS_USE_VK_FUNCTIONS(device.GetVkFunctions());

        if (resInfo.desc.IsImage())
        {
            VkImage hImage = rpsVKImageFromHandle(hRuntimeResource);
            RPS_VK_API_CALL(vkDestroyImage(hVkDevice, hImage, nullptr));
        }
        else
        {
            VkBuffer hBuffer = rpsVKBufferFromHandle(hRuntimeResource);
            RPS_VK_API_CALL(vkDestroyBuffer(hVkDevice, hBuffer, nullptr));
        }
    }

    VKRuntimeBackend::~VKRuntimeBackend()
    {
    }

    void VKRuntimeBackend::OnDestroy()
    {
        for (auto& frameResource : m_frameResources)
        {
            frameResource.DestroyDeviceResources(m_device);
        }

        m_frameResources.clear();

        RuntimeBackend::OnDestroy();
    }

    RpsResult VKRuntimeBackend::UpdateFrame(const RenderGraphUpdateContext& context)
    {
        m_currentResourceFrame =
            m_frameResources.empty() ? 0 : (m_currentResourceFrame + 1) % uint32_t(m_frameResources.size());

        if (m_frameResources.size() <= GetNumQueuedFrames(context))
        {
            RPS_RETURN_ERROR_IF(m_frameResources.size() > RPS_MAX_QUEUED_FRAMES, RPS_ERROR_INVALID_OPERATION);

            RPS_CHECK_ALLOC(m_frameResources.insert(m_currentResourceFrame, FrameResources{}));
            m_frameResources[m_currentResourceFrame].Reset(m_persistentPool);
        }
        else
        {
            // TODO - Recycle
            m_frameResources[m_currentResourceFrame].DestroyDeviceResources(m_device);

            std::swap(m_pendingReleaseImages, m_frameResources[m_currentResourceFrame].pendingImages);
            std::swap(m_pendingReleaseBuffers, m_frameResources[m_currentResourceFrame].pendingBuffers);
        }

        m_imageBarriers.reset(&context.frameArena);
        m_bufferBarriers.reset(&context.frameArena);
        m_memoryBarriers.reset(&context.frameArena);
        m_runtimeCmds.reset(&context.frameArena);
        m_barrierBatches.reset(&context.frameArena);
        m_accessToDescriptorMap.reset(&context.frameArena);
        m_imageViewLayouts.reset(&context.frameArena);

        return RPS_OK;
    }

    RpsResult VKRuntimeBackend::CreateHeaps(const RenderGraphUpdateContext& context, ArrayRef<HeapInfo> heaps)
    {
        auto hVkDevice = m_device.GetVkDevice();
        RPS_USE_VK_FUNCTIONS(m_device.GetVkFunctions());

        for (auto& heapInfo : heaps)
        {
            // TODO:
            heapInfo.size = (heapInfo.size == UINT64_MAX) ? heapInfo.maxUsedSize : heapInfo.size;

            if (heapInfo.hRuntimeHeap || !heapInfo.size)
                continue;

            VkMemoryAllocateInfo memAllocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            memAllocInfo.memoryTypeIndex      = heapInfo.memTypeIndex;
            memAllocInfo.allocationSize       = heapInfo.size;

            VkDeviceMemory hMemory;
            RPS_V_RETURN(VkResultToRps(RPS_VK_API_CALL(vkAllocateMemory(hVkDevice, &memAllocInfo, nullptr, &hMemory))));

            heapInfo.hRuntimeHeap = {hMemory};
        }

        return RPS_OK;
    }

    void VKRuntimeBackend::DestroyHeaps(ArrayRef<HeapInfo> heaps)
    {
        auto hVkDevice = m_device.GetVkDevice();
        RPS_USE_VK_FUNCTIONS(m_device.GetVkFunctions());

        for (auto& heapInfo : heaps)
        {
            if (heapInfo.hRuntimeHeap)
            {
                VkDeviceMemory hMemory = rpsVKMemoryFromHandle(heapInfo.hRuntimeHeap);
                heapInfo.hRuntimeHeap  = {};

                RPS_VK_API_CALL(vkFreeMemory(hVkDevice, hMemory, nullptr));
            }
        }
    }

    RpsResult VKRuntimeBackend::CreateResources(const RenderGraphUpdateContext& context,
                                                ArrayRef<ResourceInstance>      resInstances)
    {
        // Bind Resource Memory
        auto& heaps     = GetRenderGraph().GetHeapInfos();
        auto  hVkDevice = m_device.GetVkDevice();
        RPS_USE_VK_FUNCTIONS(m_device.GetVkFunctions());

        const auto& deviceCreateInfo = m_device.GetCreateInfo();
        auto        resourceDecls    = GetRenderGraph().GetBuilder().GetResourceDecls();

        const auto pfnSetDebugNameCb = deviceCreateInfo.callbacks.pfnSetDebugName;
        const bool bEnableDebugNames =
            !!(context.pUpdateInfo->diagnosticFlags & RPS_DIAGNOSTIC_ENABLE_RUNTIME_DEBUG_NAMES) &&
            (pfnSetDebugNameCb != nullptr);

        char nameBuf[RPS_NAME_MAX_LEN];

        for (auto& resInfo : resInstances)
        {
            if (resInfo.isPendingCreate && !resInfo.HasEmptyLifetime() && (resInfo.allocRequirement.size > 0))
            {
                // Late resource creation. Normally VK resource handles are created during GetResourceAllocInfo.
                // But due to recreate / re-placement, the handle could be destroyed at this time.
                if (!resInfo.hRuntimeResource && (resInfo.allocPlacement.heapId != RPS_INDEX_NONE_U32))
                {
                    VKRuntimeDevice::VKResourceAllocInfo allocInfo = {};
                    RPS_V_RETURN(m_device.GetResourceAllocInfo(resInfo, allocInfo));

                    if ((allocInfo.memoryRequirements.size != resInfo.allocRequirement.size) ||
                        (allocInfo.memoryRequirements.alignment != resInfo.allocRequirement.alignment) ||
                        !rpsAllBitsSet(allocInfo.memoryRequirements.memoryTypeBits,
                                       (1u << resInfo.allocRequirement.memoryTypeIndex)))
                    {
                        if (allocInfo.hRuntimeResource)
                        {
                            DestroyVkResource(m_device, resInfo, allocInfo.hRuntimeResource);
                        }

                        return RPS_ERROR_INVALID_OPERATION;
                    }

                    resInfo.hRuntimeResource = allocInfo.hRuntimeResource;
                }

                if (resInfo.hRuntimeResource)
                {
                    if (bEnableDebugNames)
                    {
                        resourceDecls[resInfo.resourceDeclId].name.ToCStr(nameBuf, RPS_COUNTOF(nameBuf));

                        RpsRuntimeOpSetDebugNameArgs setNameArgs = {};
                        setNameArgs.hResource                    = resInfo.hRuntimeResource;
                        setNameArgs.resourceType                 = resInfo.desc.type;
                        setNameArgs.name                         = nameBuf;

                        pfnSetDebugNameCb(deviceCreateInfo.pUserContext, &setNameArgs);
                    }

                    if (resInfo.allocPlacement.heapId != RPS_INDEX_NONE_U32)
                    {
                        auto pMemory = rpsVKMemoryFromHandle(heaps[resInfo.allocPlacement.heapId].hRuntimeHeap);
                        if (resInfo.desc.IsImage())
                        {   
                            RPS_V_RETURN(VkResultToRps(
                                RPS_VK_API_CALL(vkBindImageMemory(hVkDevice,
                                                                  rpsVKImageFromHandle(resInfo.hRuntimeResource),
                                                                  pMemory,
                                                                  resInfo.allocPlacement.offset))));
                        }
                        else
                        {
                            RPS_V_RETURN(VkResultToRps(
                                RPS_VK_API_CALL(vkBindBufferMemory(hVkDevice,
                                                                    rpsVKBufferFromHandle(resInfo.hRuntimeResource),
                                                                    pMemory,
                                                                    resInfo.allocPlacement.offset))));
                        }
                        resInfo.isPendingInit = true;
                    }
                }
                else
                {
                    RPS_TODO(
                        "Unreachable code path. "
                        "Currently we expect resInfo.hRuntimeResource to be valid at this point. "
                        "This is reserved for e.g. dedicated allocation");
                }

                // VK resources starts with undefined layout.
                static constexpr AccessAttr PrevFinalAccess = {};
                resInfo.FinalizeRuntimeResourceCreation(&PrevFinalAccess);
            }
            else if (!resInfo.isExternal)
            {
                resInfo.isPendingInit = resInfo.isAliased;
            }
        }

        return RPS_OK;
    }

    void VKRuntimeBackend::DestroyResources(ArrayRef<ResourceInstance> resInstances)
    {
        RPS_USE_VK_FUNCTIONS(m_device.GetVkFunctions());

        for (auto& resInfo : resInstances)
        {
            if (resInfo.hRuntimeResource && !resInfo.isExternal)
            {
                DestroyVkResource(m_device, resInfo, resInfo.hRuntimeResource);
            }
        }
    }

    RpsResult VKRuntimeBackend::CreateCommandResources(const RenderGraphUpdateContext& context)
    {
        auto& renderGraph = context.renderGraph;

        const auto& graph         = renderGraph.GetGraph();
        const auto& cmdAccesses   = renderGraph.GetCmdAccessInfos();
        auto&       runtimeCmds   = renderGraph.GetRuntimeCmdInfos();
        auto&       aliasingInfos = renderGraph.GetResourceAliasingInfos();
        auto&       resInstances  = renderGraph.GetResourceInstances();
        auto        cmdBatches    = renderGraph.GetCmdBatches().range_all();

        ArenaCheckPoint arenaCheckpoint{context.scratchArena};

        ArenaVector<uint32_t> renderPassCmdIndices(&context.scratchArena);
        ArenaVector<uint32_t> bufViews(&context.scratchArena);
        ArenaVector<uint32_t> imgViews(&context.scratchArena);
        renderPassCmdIndices.reserve(context.renderGraph.GetCmdInfos().size());
        bufViews.reserve(context.renderGraph.GetCmdAccessInfos().size());
        imgViews.reserve(context.renderGraph.GetCmdAccessInfos().size());

        m_resourceLayoutOffsets.reset_keep_capacity(&context.scratchArena);
        m_subResLayouts.reset_keep_capacity(&context.scratchArena);

        Span<RuntimeCmdInfo> transitionRange = {};

        for (uint32_t iBatch = 0; iBatch < cmdBatches.size(); iBatch++)
        {
            RpsCommandBatch& batchInfo = cmdBatches[iBatch];

            const uint32_t backendCmdBegin = uint32_t(m_runtimeCmds.size());

            for (uint32_t iCmd = batchInfo.cmdBegin, numCmds = batchInfo.cmdBegin + batchInfo.numCmds; iCmd < numCmds;
                 iCmd++)
            {
                const auto& runtimeCmd = runtimeCmds[iCmd];

                if (runtimeCmd.isTransition)
                {
                    if (transitionRange.GetEnd() != iCmd)
                    {
                        transitionRange.SetRange(iCmd, 0);
                    }
                    transitionRange.SetCount(transitionRange.size() + 1);
                }
                else
                {
                    ProcessBarrierBatch(context, transitionRange);

                    auto pNewRuntimeCmd = m_runtimeCmds.grow(1);

                    pNewRuntimeCmd->cmdId = runtimeCmd.cmdId;

                    const auto* pCmdInfo     = context.renderGraph.GetCmdInfo(runtimeCmd.cmdId);
                    const auto& nodeDeclInfo = *pCmdInfo->pNodeDecl;

                    if (nodeDeclInfo.pRenderPassInfo && !nodeDeclInfo.pRenderPassInfo->clearOnly)
                    {
                        pNewRuntimeCmd->renderPassId  = uint32_t(renderPassCmdIndices.size());  // TODO
                        pNewRuntimeCmd->frameBufferId = uint32_t(renderPassCmdIndices.size());
                        renderPassCmdIndices.push_back(uint32_t(m_runtimeCmds.size() - 1));
                    }

                    static constexpr RpsAccessFlags AccessMaskMayNeedCreateView =
                        RPS_ACCESS_CONSTANT_BUFFER_BIT | RPS_ACCESS_UNORDERED_ACCESS_BIT |
                        RPS_ACCESS_SHADER_RESOURCE_BIT | RPS_ACCESS_RENDER_TARGET_BIT | RPS_ACCESS_DEPTH_STENCIL |
                        RPS_ACCESS_RESOLVE_DEST_BIT;

                    const uint32_t accessOffset = pCmdInfo->accesses.GetBegin();

                    for (uint32_t accessIdx = 0, accessCount = pCmdInfo->accesses.size(); accessIdx < accessCount;
                         accessIdx++)
                    {
                        const uint32_t globalAccessIdx = accessOffset + accessIdx;

                        auto&       access  = cmdAccesses[globalAccessIdx];
                        const auto& resInfo = resInstances[access.resourceId];

                        if (!rpsAnyBitsSet(access.access.accessFlags, RPS_ACCESS_NO_VIEW_BIT) &&
                            rpsAnyBitsSet(access.access.accessFlags, AccessMaskMayNeedCreateView))
                        {
                            if (resInfo.desc.IsBuffer() && access.pViewInfo &&
                                (access.pViewInfo->viewFormat != RPS_FORMAT_UNKNOWN))
                            {
                                bufViews.push_back(globalAccessIdx);
                            }
                            else if (resInfo.desc.IsImage())
                            {
                                imgViews.push_back(globalAccessIdx);
                            }
                        }
                    }
                }
            }

            ProcessBarrierBatch(context, transitionRange);

            batchInfo.cmdBegin = backendCmdBegin;
            batchInfo.numCmds  = uint32_t(m_runtimeCmds.size()) - backendCmdBegin;
        }

        // Create Views / Per-Cmd objects

        m_accessToDescriptorMap.resize(cmdAccesses.size(), RPS_INDEX_NONE_U32);

        RPS_V_RETURN(CreateBufferViews(context, bufViews.range_all()));
        RPS_V_RETURN(CreateImageViews(context, imgViews.range_all()));
        RPS_V_RETURN(CreateRenderPasses(context, renderPassCmdIndices.range_all()));

        return RPS_OK;
    }

    RpsResult VKRuntimeBackend::RecordCommands(const RenderGraph&                     renderGraph,
                                               const RpsRenderGraphRecordCommandInfo& recordInfo) const
    {
        RuntimeCmdCallbackContext cmdCbCtx{this, recordInfo};

        for (auto cmdIter = m_runtimeCmds.cbegin() + recordInfo.cmdBeginIndex, cmdEnd = cmdIter + recordInfo.numCmds;
             cmdIter != cmdEnd;
             ++cmdIter)
        {
            auto& runtimeCmd = *cmdIter;

            if (runtimeCmd.barrierBatchId != RPS_INDEX_NONE_U32)
            {
                RecordBarrierBatch(GetContextVkCmdBuf(cmdCbCtx), runtimeCmd.barrierBatchId);
            }

            RPS_V_RETURN(RecordCommand(cmdCbCtx, runtimeCmd));
        }

        return RPS_OK;
    }

    void VKRuntimeBackend::DestroyRuntimeResourceDeferred(ResourceInstance& resource)
    {
        if (resource.hRuntimeResource)
        {
            if (resource.desc.IsImage())
            {
                m_pendingReleaseImages.push_back(rpsVKImageFromHandle(resource.hRuntimeResource));
            }
            else
            {
                m_pendingReleaseBuffers.push_back(rpsVKBufferFromHandle(resource.hRuntimeResource));
            }
            resource.hRuntimeResource = {};
        }
    }

    static inline void FlipViewport(VkViewport& vp)
    {
        vp.y      = vp.y + vp.height;
        vp.height = -vp.height;
    }

    RpsResult VKRuntimeBackend::RecordCmdRenderPassBegin(const RuntimeCmdCallbackContext& context) const
    {
        auto& renderGraph  = *context.pRenderGraph;
        auto& cmd          = *context.pCmd;
        auto* pCmdInfo     = context.pCmdInfo;
        auto& nodeDeclInfo = *pCmdInfo->pNodeDecl;
        auto  hVkCmdBuf    = GetContextVkCmdBuf(context);
        auto& runtimeCmd   = *context.GetRuntimeCmd<VKRuntimeCmd>();
        RPS_USE_VK_FUNCTIONS(m_device.GetVkFunctions());

        RPS_RETURN_ERROR_IF(!nodeDeclInfo.MaybeGraphicsNode(), RPS_ERROR_INVALID_OPERATION);

        RPS_RETURN_OK_IF(!nodeDeclInfo.pRenderPassInfo || nodeDeclInfo.pRenderPassInfo->clearOnly);

        const auto cmdCbFlags = context.bIsCmdBeginEnd ? cmd.callback.flags : RPS_CMD_CALLBACK_FLAG_NONE;

        const bool bToExecSecondaryCmdBuf =
            rpsAnyBitsSet(context.renderPassFlags, RPS_RUNTIME_RENDER_PASS_EXECUTE_SECONDARY_COMMAND_BUFFERS);

        const bool bIsSecondaryCmdBuffer =
            rpsAnyBitsSet(context.renderPassFlags, RPS_RUNTIME_RENDER_PASS_SECONDARY_COMMAND_BUFFER);

        RPS_CHECK_ARGS(!(bToExecSecondaryCmdBuf && bIsSecondaryCmdBuffer));

        // TODO: Simplify conditions & share with EndRP.
        //
        // Skip vkCmdBeginRenderPass if:
        //  - Is called on secondary cmd buffer, in which case we may only setup Viewports / Scissor states.
        //  - User indicated the cmd callback will do custom RP.
        //  - RP info missing.
        const bool bBeginVKRenderPass = !bIsSecondaryCmdBuffer &&
                                        !rpsAnyBitsSet(cmdCbFlags, RPS_CMD_CALLBACK_CUSTOM_RENDER_TARGETS_BIT) &&
                                        (runtimeCmd.renderPassId != RPS_INDEX_NONE_U32);

        auto& cmdRpInfo = *pCmdInfo->pRenderPassInfo;

        // Begin RenderPass
        if (bBeginVKRenderPass)
        {
            auto& defaultRenderArea = cmdRpInfo.viewportInfo.defaultRenderArea;

            VkRenderPassBeginInfo rpBegin = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};

            auto& currResources = m_frameResources[m_currentResourceFrame];

            rpBegin.renderPass      = currResources.renderPasses[runtimeCmd.renderPassId];
            rpBegin.framebuffer     = currResources.frameBuffers[runtimeCmd.frameBufferId];
            rpBegin.renderArea      = VkRect2D{{defaultRenderArea.x, defaultRenderArea.y},
                                               {uint32_t(defaultRenderArea.width), uint32_t(defaultRenderArea.height)}};
            rpBegin.clearValueCount = uint32_t(runtimeCmd.clearValues.size());
            rpBegin.pClearValues    = runtimeCmd.clearValues.data();

            const VkSubpassContents subpassContent =
                bToExecSecondaryCmdBuf ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE;

            RPS_VK_API_CALL(vkCmdBeginRenderPass(hVkCmdBuf, &rpBegin, subpassContent));
        }

        // Setup Viewport / Scissor states

        const bool bSetViewportScissors =
            !bToExecSecondaryCmdBuf && !rpsAnyBitsSet(cmdCbFlags, RPS_CMD_CALLBACK_CUSTOM_VIEWPORT_SCISSOR_BIT);

        if (bSetViewportScissors)
        {
            static_assert(sizeof(VkViewport) == sizeof(RpsViewport), "Invalid assumption about VkViewport layout");
            static_assert(sizeof(VkRect2D) == sizeof(RpsRect), "Invalid assumption about VkRect2D layout");

            const VkViewport* pViewports    = reinterpret_cast<const VkViewport*>(cmdRpInfo.viewportInfo.pViewports);
            const VkRect2D*   pScissorRects = reinterpret_cast<const VkRect2D*>(cmdRpInfo.viewportInfo.pScissorRects);

            static constexpr uint32_t MAX_VIEWPORT_SCISSOR_COUNT            = 32;  // TODO
            VkViewport                viewports[MAX_VIEWPORT_SCISSOR_COUNT] = {};

            const bool bFlipViewport =
                !rpsAnyBitsSet(m_device.GetRuntimeFlags(), RPS_VK_RUNTIME_FLAG_DONT_FLIP_VIEWPORT);

            if (bFlipViewport)
            {
                RPS_RETURN_ERROR_IF(cmdRpInfo.viewportInfo.numViewports > MAX_VIEWPORT_SCISSOR_COUNT,
                                    RPS_ERROR_NOT_SUPPORTED);

                for (uint32_t i = 0; i < cmdRpInfo.viewportInfo.numViewports; i++)
                {
                    viewports[i] = pViewports[i];
                    FlipViewport(viewports[0]);
                }

                pViewports = viewports;
            }
            RPS_VK_API_CALL(vkCmdSetViewport(hVkCmdBuf, 0, cmdRpInfo.viewportInfo.numViewports, pViewports));
            RPS_VK_API_CALL(vkCmdSetScissor(hVkCmdBuf, 0, cmdRpInfo.viewportInfo.numScissorRects, pScissorRects));
        }

        return RPS_OK;
    }

    RpsResult VKRuntimeBackend::RecordCmdRenderPassEnd(const RuntimeCmdCallbackContext& context) const
    {
        auto& renderGraph  = *context.pRenderGraph;
        auto& runtimeCmd   = *context.GetRuntimeCmd<VKRuntimeCmd>();
        auto& cmd          = *context.pCmd;
        auto& nodeDeclInfo = *context.pCmdInfo->pNodeDecl;
        RPS_USE_VK_FUNCTIONS(m_device.GetVkFunctions());

        RPS_RETURN_ERROR_IF(!nodeDeclInfo.MaybeGraphicsNode(), RPS_ERROR_INVALID_OPERATION);

        RPS_RETURN_OK_IF(!nodeDeclInfo.pRenderPassInfo || nodeDeclInfo.pRenderPassInfo->clearOnly);

        const auto cmdCbFlags = context.bIsCmdBeginEnd ? cmd.callback.flags : RPS_CMD_CALLBACK_FLAG_NONE;

        const bool bIsSecondaryCmdBuffer =
            rpsAnyBitsSet(context.renderPassFlags, RPS_RUNTIME_RENDER_PASS_SECONDARY_COMMAND_BUFFER);

        const bool bEndVKRenderPass = !bIsSecondaryCmdBuffer &&
                                      !rpsAnyBitsSet(cmdCbFlags, RPS_CMD_CALLBACK_CUSTOM_RENDER_TARGETS_BIT) &&
                                      (runtimeCmd.renderPassId != RPS_INDEX_NONE_U32);

        if (bEndVKRenderPass)
        {
            RPS_VK_API_CALL(vkCmdEndRenderPass(GetContextVkCmdBuf(context)));
        }

        return RPS_OK;
    }

    RpsResult VKRuntimeBackend::RecordCmdFixedFunctionBindingsAndDynamicStates(
        const RuntimeCmdCallbackContext& context) const
    {
        RPS_RETURN_OK_IF(rpsAnyBitsSet(context.pCmd->callback.flags, RPS_CMD_CALLBACK_CUSTOM_STATE_SETUP_BIT));

        auto& renderGraph  = *context.pRenderGraph;
        auto& nodeDeclInfo = *context.pCmdInfo->pNodeDecl;

        auto fixedFuncBindings = nodeDeclInfo.fixedFunctionBindings.Get(nodeDeclInfo.semanticKinds);
        auto dynamicStates     = nodeDeclInfo.dynamicStates.Get(nodeDeclInfo.semanticKinds);

        for (auto& binding : fixedFuncBindings)
        {
            auto paramIndices = binding.params.Get(nodeDeclInfo.semanticParamTable);

            switch (binding.semantic)
            {
            case RPS_SEMANTIC_VERTEX_BUFFER:
                break;
            case RPS_SEMANTIC_INDEX_BUFFER:
                break;
            case RPS_SEMANTIC_INDIRECT_ARGS:
                break;
            case RPS_SEMANTIC_INDIRECT_COUNT:
                break;
            case RPS_SEMANTIC_STREAM_OUT_BUFFER:
                break;
            case RPS_SEMANTIC_SHADING_RATE_IMAGE:
                break;
            case RPS_SEMANTIC_RENDER_TARGET:
            case RPS_SEMANTIC_DEPTH_STENCIL_TARGET:
            case RPS_SEMANTIC_RESOLVE_TARGET:
            default:
                break;
            }
        }

        for (auto& dynamicState : dynamicStates)
        {
            switch (dynamicState.semantic)
            {
            case RPS_SEMANTIC_VIEWPORT:
                break;
            case RPS_SEMANTIC_SCISSOR:
                break;
            case RPS_SEMANTIC_PRIMITIVE_TOPOLOGY:
                break;
            case RPS_SEMANTIC_PATCH_CONTROL_POINTS:
                break;
            case RPS_SEMANTIC_PRIMITIVE_STRIP_CUT_INDEX:
                break;
            case RPS_SEMANTIC_BLEND_FACTOR:
                break;
            case RPS_SEMANTIC_STENCIL_REF:
                break;
            case RPS_SEMANTIC_DEPTH_BOUNDS:
                break;
            case RPS_SEMANTIC_SAMPLE_LOCATION:
                break;
            case RPS_SEMANTIC_SHADING_RATE:
                break;
            case RPS_SEMANTIC_COLOR_CLEAR_VALUE:

            case RPS_SEMANTIC_DEPTH_CLEAR_VALUE:

            case RPS_SEMANTIC_STENCIL_CLEAR_VALUE:

            default:
                break;
            }
        }

        return RPS_OK;
    }

    static inline VkImageViewType GetImageViewType(const ResourceInstance& resInfo,
                                                   const CmdAccessInfo&    accessInfo,
                                                   const RpsImageView&     view)
    {
        const bool isArray   = view.subresourceRange.arrayLayers > 1;
        const bool isCubemap = rpsAnyBitsSet(view.base.flags, RPS_RESOURCE_VIEW_FLAG_CUBEMAP_BIT);

        if (resInfo.desc.type == RPS_RESOURCE_TYPE_IMAGE_2D)
        {
            if (isCubemap)
            {
                RPS_ASSERT(isArray);

                return (view.subresourceRange.arrayLayers > 6) ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
                                                               : VK_IMAGE_VIEW_TYPE_CUBE;
            }
            return isArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        }
        else if (resInfo.desc.type == RPS_RESOURCE_TYPE_IMAGE_3D)
        {
            if (rpsAnyBitsSet(accessInfo.access.accessFlags, RPS_ACCESS_RENDER_TARGET_BIT))
            {
                return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            }

            return VK_IMAGE_VIEW_TYPE_3D;
        }
        else
        {
            return isArray ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
        }
    }

    RpsResult CreateImageView(const VKRuntimeDevice&  device,
                              VkImage                 hImage,
                              const ResourceInstance& resInfo,
                              const CmdAccessInfo&    accessInfo,
                              VkImageView&            dstImgView)
    {
        const RpsFormat       viewFormat   = rpsVkGetImageViewFormat(accessInfo.viewFormat, resInfo);
        const RpsImageView*   pImgViewInfo = reinterpret_cast<const RpsImageView*>(accessInfo.pViewInfo);
        RPS_USE_VK_FUNCTIONS(device.GetVkFunctions());

        VkImageViewCreateInfo vkCreateInfo;
        vkCreateInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vkCreateInfo.pNext    = NULL;
        vkCreateInfo.flags    = 0;
        vkCreateInfo.image    = hImage;
        vkCreateInfo.viewType = GetImageViewType(resInfo, accessInfo, *pImgViewInfo);
        vkCreateInfo.format   = rpsFormatToVK(viewFormat);

        GetVkComponentMapping(vkCreateInfo.components, pImgViewInfo->componentMapping);
        GetVkSubresourceRange(vkCreateInfo.subresourceRange, accessInfo.range);

        return VkResultToRps(
            RPS_VK_API_CALL(vkCreateImageView(device.GetVkDevice(), &vkCreateInfo, nullptr, &dstImgView)));
    }

    RpsResult CreateBufferView(const VKRuntimeDevice&  device,
                               VkBuffer                hBuffer,
                               const ResourceInstance& resInfo,
                               const CmdAccessInfo&    accessInfo,
                               VkBufferView&           dstBufView)
    {
        RPS_ASSERT(accessInfo.viewFormat != RPS_FORMAT_UNKNOWN);
        RPS_USE_VK_FUNCTIONS(device.GetVkFunctions());

        const RpsBufferView* pBufViewInfo = reinterpret_cast<const RpsBufferView*>(accessInfo.pViewInfo);

        VkBufferViewCreateInfo vkCreateInfo;
        vkCreateInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
        vkCreateInfo.pNext  = NULL;
        vkCreateInfo.flags  = 0;
        vkCreateInfo.buffer = hBuffer;
        vkCreateInfo.format = rpsFormatToVK(accessInfo.viewFormat);
        vkCreateInfo.offset = pBufViewInfo->offset;
        vkCreateInfo.range =
            (pBufViewInfo->sizeInBytes == RPS_BUFFER_WHOLE_SIZE) ? VK_WHOLE_SIZE : pBufViewInfo->sizeInBytes;

        return VkResultToRps(
            RPS_VK_API_CALL(vkCreateBufferView(device.GetVkDevice(), &vkCreateInfo, nullptr, &dstBufView)));
    }

    RpsResult VKRuntimeBackend::CreateImageViews(const RenderGraphUpdateContext& context,
                                                 ConstArrayRef<uint32_t>         accessIndices)
    {
        RPS_RETURN_OK_IF(accessIndices.empty());

        auto& cmdAccesses       = context.renderGraph.GetCmdAccessInfos();
        auto  resourceInstances = context.renderGraph.GetResourceInstances().range_all();
        auto& currResources     = m_frameResources[m_currentResourceFrame];

        m_imageViewLayouts.resize(accessIndices.size());

        RPS_CHECK_ALLOC(currResources.imageViews.resize(accessIndices.size()));

        uint32_t imgViewIndex = 0;
        VkImage  hImage       = VK_NULL_HANDLE;

        for (auto accessIndex : accessIndices)
        {
            auto& access = cmdAccesses[accessIndex];

            const auto& resource = resourceInstances[access.resourceId];
            FromHandle(hImage, resource.hRuntimeResource);

            m_imageViewLayouts[imgViewIndex] = GetTrackedImageLayoutInfo(resource, access);

            VkImageView& hImgView = currResources.imageViews[imgViewIndex];
            RPS_V_RETURN(CreateImageView(m_device, hImage, resource, access, hImgView));

            m_accessToDescriptorMap[accessIndex] = imgViewIndex;

            imgViewIndex++;
        }

        return RPS_OK;
    }

    RpsResult VKRuntimeBackend::CreateBufferViews(const RenderGraphUpdateContext& context,
                                                  ConstArrayRef<uint32_t>         accessIndices)
    {
        RPS_RETURN_OK_IF(accessIndices.empty());

        auto& cmdAccesses       = context.renderGraph.GetCmdAccessInfos();
        auto  resourceInstances = context.renderGraph.GetResourceInstances().range_all();
        auto& currResources     = m_frameResources[m_currentResourceFrame];

        RPS_CHECK_ALLOC(currResources.bufferViews.resize(accessIndices.size()));

        uint32_t bufViewIndex = 0;
        VkBuffer hBuffer      = VK_NULL_HANDLE;

        for (auto accessIndex : accessIndices)
        {
            auto& access = cmdAccesses[accessIndex];

            const auto& resource = resourceInstances[access.resourceId];
            FromHandle(hBuffer, resource.hRuntimeResource);

            VkBufferView& hBufView = currResources.bufferViews[bufViewIndex];
            RPS_V_RETURN(CreateBufferView(m_device, hBuffer, resource, access, hBufView));

            m_accessToDescriptorMap[accessIndex] = bufViewIndex;

            bufViewIndex++;
        }

        return RPS_OK;
    }

    static constexpr bool StencilOp    = true;
    static constexpr bool NonStencilOp = false;

    template <bool IsStencil = NonStencilOp>
    VkAttachmentLoadOp GetVkLoadOp(const CmdAccessInfo& access, const NodeDeclRenderPassInfo& rpInfo)
    {
        // For depth stencil, need additional clear flags from rpInfo in case we want to clear only depth or stencil aspect.
        const bool bIsDepthStencil = !!(access.access.accessFlags & RPS_ACCESS_DEPTH_STENCIL);
        const bool bShouldClearDepthStencil =
            IsStencil ? (rpInfo.clearDepth && (access.access.accessFlags & RPS_ACCESS_DEPTH))
                      : (rpInfo.clearStencil && (access.access.accessFlags & RPS_ACCESS_STENCIL));

        static constexpr RpsAccessFlags DiscardAccessMask =
            (IsStencil ? RPS_ACCESS_STENCIL_DISCARD_DATA_BEFORE_BIT : RPS_ACCESS_DISCARD_DATA_BEFORE_BIT);

        if (bShouldClearDepthStencil || (!bIsDepthStencil && (access.access.accessFlags & RPS_ACCESS_CLEAR_BIT)))
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        else if (access.access.accessFlags & DiscardAccessMask)
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        else
            return VK_ATTACHMENT_LOAD_OP_LOAD;
    }

    template <bool IsStencil = NonStencilOp>
    VkAttachmentStoreOp GetVkStoreOp(const CmdAccessInfo& access, bool bStoreOpNoneSupported)
    {
        static constexpr RpsAccessFlags DiscardAccessMask =
            (IsStencil ? RPS_ACCESS_STENCIL_DISCARD_DATA_AFTER_BIT : RPS_ACCESS_DISCARD_DATA_AFTER_BIT);

        static constexpr RpsAccessFlags NonStencilWriteAccessMask =
            (RPS_ACCESS_ALL_GPU_WRITE & (~RPS_ACCESS_STENCIL_WRITE_BIT));

#ifdef RPS_VK_ATTACHMENT_STORE_OP_NONE
        if (bStoreOpNoneSupported &&
            !(access.access.accessFlags & (IsStencil ? RPS_ACCESS_STENCIL_WRITE_BIT : NonStencilWriteAccessMask)))
            return RPS_VK_ATTACHMENT_STORE_OP_NONE;
#endif

        if (access.access.accessFlags & DiscardAccessMask)
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;

        return VK_ATTACHMENT_STORE_OP_STORE;
    }

    VkAttachmentLoadOp GetVkStencilLoadOp(const CmdAccessInfo& access, const NodeDeclRenderPassInfo& rpInfo)
    {
        if (access.access.accessFlags & (RPS_ACCESS_STENCIL_WRITE_BIT | RPS_ACCESS_STENCIL_READ_BIT))
            return GetVkLoadOp<StencilOp>(access, rpInfo);
        else
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }

    VkAttachmentStoreOp GetVkStencilStoreOp(const CmdAccessInfo& access, bool bLoadStoreOpNoneSupported)
    {
        if (access.access.accessFlags & (RPS_ACCESS_STENCIL_WRITE_BIT | RPS_ACCESS_STENCIL_READ_BIT))
            return GetVkStoreOp<StencilOp>(access, bLoadStoreOpNoneSupported);
        else
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }

    static constexpr bool UseRenderPassBarriersTrue  = true;
    static constexpr bool UseRenderPassBarriersFalse = false;

    template <bool bUseRenderPassBarriers>
    static inline void GetVkAttachmentDescription(VkAttachmentDescription*      pOut,
                                                  const CmdAccessInfo&          access,
                                                  const ResourceInstance&       resourceInfo,
                                                  const NodeDeclRenderPassInfo& rpInfo,
                                                  bool                          bStoreOpNoneSupported)
    {
        RPS_ASSERT(resourceInfo.desc.IsImage());

        const RpsFormat viewFormat = rpsVkGetImageViewFormat(access.viewFormat, resourceInfo);

        const auto& initialAccess = access.access;
        // bUseRenderPassBarriers ? pAccess->accessFlagsPrev : pAccess->accessFlagsCurrent;
        const auto& finalAccess = access.access;
        // bUseRenderPassBarriers ? pAccess->accessFlagsNext : pAccess->accessFlagsCurrent;

        pOut->flags          = 0;
        pOut->format         = rpsFormatToVK(viewFormat);
        pOut->samples        = rpsVkGetSampleCount(resourceInfo.desc.image.sampleCount);
        pOut->loadOp         = GetVkLoadOp(access, rpInfo);
        pOut->storeOp        = GetVkStoreOp(access, bStoreOpNoneSupported);
        pOut->stencilLoadOp  = GetVkStencilLoadOp(access, rpInfo);
        pOut->stencilStoreOp = GetVkStencilStoreOp(access, bStoreOpNoneSupported);
        pOut->initialLayout  = GetVkImageLayout<IsSrcLayoutTrue>(initialAccess);
        pOut->finalLayout    = GetVkImageLayout<IsSrcLayoutFalse>(finalAccess);
    }

    RpsResult VKRuntimeBackend::CreateRenderPasses(const RenderGraphUpdateContext& context,
                                                   ConstArrayRef<uint32_t>         cmdIndices)
    {
        RPS_RETURN_OK_IF(cmdIndices.empty());

        auto        hVkDevice   = m_device.GetVkDevice();
        const auto& resources   = context.renderGraph.GetResourceInstances();
        const auto& runtimeCmds = context.renderGraph.GetRuntimeCmdInfos();
        const auto& cmdAccesses = context.renderGraph.GetCmdAccessInfos();
        RPS_USE_VK_FUNCTIONS(m_device.GetVkFunctions());

        const bool bStoreOpNoneSupported =
            rpsAnyBitsSet(m_device.GetRuntimeFlags(), RPS_VK_RUNTIME_FLAG_STORE_OP_NONE_SUPPORTED);

        auto& currResources = m_frameResources[m_currentResourceFrame];

        RPS_CHECK_ALLOC(currResources.renderPasses.resize(cmdIndices.size()));
        RPS_CHECK_ALLOC(currResources.frameBuffers.resize(cmdIndices.size()));

        VkAttachmentDescription attchmtDescs[RPS_MAX_SIMULTANEOUS_RENDER_TARGET_COUNT * 2 + 1];
        VkAttachmentReference   colorRefs[RPS_MAX_SIMULTANEOUS_RENDER_TARGET_COUNT];
        VkAttachmentReference   resolveRefs[RPS_MAX_SIMULTANEOUS_RENDER_TARGET_COUNT];
        VkAttachmentReference   depthRef;

        VkSubpassDescription subpassDesc = {};

        VkRenderPassCreateInfo rpInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rpInfo.subpassCount           = 1;
        rpInfo.pSubpasses             = &subpassDesc;

        uint32_t rpIndex = 0;
        for (auto cmdIndex : cmdIndices)
        {
            auto&       runtimeCmd   = m_runtimeCmds[cmdIndex];
            auto*       pCmdInfo     = context.renderGraph.GetCmdInfo(runtimeCmd.cmdId);
            const auto& cmd          = *pCmdInfo->pCmdDecl;
            const auto& nodeDeclInfo = *pCmdInfo->pNodeDecl;

            RPS_ASSERT(nodeDeclInfo.pRenderPassInfo);
            auto& nodeDeclRenderPassInfo = *nodeDeclInfo.pRenderPassInfo;

            RPS_ASSERT(runtimeCmd.renderPassId == rpIndex);  // TODO

            bool     bHasDsv        = false;
            uint32_t numRtvs        = 0;
            uint32_t numResolveRtvs = 0;
            uint32_t attchmtCount   = 0;

            VkImageView attchmtViews[RPS_MAX_SIMULTANEOUS_RENDER_TARGET_COUNT * 2 + 1];

            auto fixedFuncBindings = nodeDeclInfo.fixedFunctionBindings.Get(nodeDeclInfo.semanticKinds);

            auto cmdDescriptorIndices =
                m_accessToDescriptorMap.range(pCmdInfo->accesses.GetBegin(), pCmdInfo->accesses.size());
            auto cmdAccessInfos = pCmdInfo->accesses.Get(cmdAccesses);

            static constexpr VkAttachmentReference unusedAttchmt = {VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED};

            uint32_t lastParamId = UINT32_MAX;

            for (auto& rtParamRef : nodeDeclRenderPassInfo.GetRenderTargetRefs())
            {
                auto& paramAccessInfo = nodeDeclInfo.params[rtParamRef.paramId];
                auto  descriptorIndices =
                    cmdDescriptorIndices.range(paramAccessInfo.accessOffset, paramAccessInfo.numElements);

                if (lastParamId != rtParamRef.paramId)
                {
                    RPS_ASSERT(numRtvs <= paramAccessInfo.baseSemanticIndex);
                    std::fill(colorRefs + numRtvs, colorRefs + paramAccessInfo.baseSemanticIndex, unusedAttchmt);
                    std::fill(resolveRefs + numRtvs, resolveRefs + paramAccessInfo.baseSemanticIndex, unusedAttchmt);
                    lastParamId = rtParamRef.paramId;
                    numRtvs     = paramAccessInfo.baseSemanticIndex + paramAccessInfo.numElements;
                }

                const uint32_t imgViewIndex = descriptorIndices[rtParamRef.arrayOffset];
                attchmtViews[attchmtCount]  = currResources.imageViews[imgViewIndex];

                auto& accessInfo = cmdAccessInfos[paramAccessInfo.accessOffset + rtParamRef.arrayOffset];

                GetVkAttachmentDescription<UseRenderPassBarriersFalse>(&attchmtDescs[attchmtCount],
                                                                       accessInfo,
                                                                       resources[accessInfo.resourceId],
                                                                       nodeDeclRenderPassInfo,
                                                                       bStoreOpNoneSupported);

                const uint32_t rtSlot = paramAccessInfo.baseSemanticIndex + rtParamRef.arrayOffset;

                auto& colorRef      = colorRefs[rtSlot];
                colorRef.attachment = attchmtCount;
                colorRef.layout     = GetVkImageLayout<IsSrcLayoutFalse>(paramAccessInfo.access);

                auto& resolveRef      = resolveRefs[rtSlot];
                resolveRef.attachment = VK_ATTACHMENT_UNUSED;
                resolveRef.layout     = VK_IMAGE_LAYOUT_UNDEFINED;

                attchmtCount++;
            }

            if (nodeDeclRenderPassInfo.depthStencilTargetMask)
            {
                auto& dsvParamRef     = *nodeDeclRenderPassInfo.GetDepthStencilRef();
                auto& paramAccessInfo = nodeDeclInfo.params[dsvParamRef.paramId];
                RPS_ASSERT(paramAccessInfo.numElements == 1);

                const uint32_t imgViewIndex = cmdDescriptorIndices[paramAccessInfo.accessOffset];

                attchmtViews[attchmtCount] = currResources.imageViews[imgViewIndex];

                auto& accessInfo = cmdAccessInfos[paramAccessInfo.accessOffset];

                GetVkAttachmentDescription<UseRenderPassBarriersFalse>(&attchmtDescs[attchmtCount],
                                                                       accessInfo,
                                                                       resources[accessInfo.resourceId],
                                                                       nodeDeclRenderPassInfo,
                                                                       bStoreOpNoneSupported);

                depthRef.attachment = numRtvs;
                depthRef.layout     = GetVkImageLayout<IsSrcLayoutFalse>(accessInfo.access);
                attchmtCount++;

                bHasDsv = true;
            }

            for (auto resolveParamRef : nodeDeclRenderPassInfo.GetResolveTargetRefs())
            {
                auto& paramAccessInfo = nodeDeclInfo.params[resolveParamRef.paramId];
                auto  descriptorIndices =
                    cmdDescriptorIndices.range(paramAccessInfo.accessOffset, paramAccessInfo.numElements);

                const uint32_t imgViewIndex = descriptorIndices[resolveParamRef.arrayOffset];
                attchmtViews[attchmtCount]  = currResources.imageViews[imgViewIndex];

                auto& accessInfo = cmdAccessInfos[paramAccessInfo.accessOffset + resolveParamRef.arrayOffset];

                GetVkAttachmentDescription<UseRenderPassBarriersFalse>(&attchmtDescs[attchmtCount],
                                                                       accessInfo,
                                                                       resources[accessInfo.resourceId],
                                                                       nodeDeclRenderPassInfo,
                                                                       bStoreOpNoneSupported);

                const uint32_t rtSlot = paramAccessInfo.baseSemanticIndex + resolveParamRef.arrayOffset;

                RPS_ASSERT(nodeDeclRenderPassInfo.renderTargetsMask & (1u << rtSlot));

                auto& resolveRef      = resolveRefs[rtSlot];
                resolveRef.attachment = attchmtCount;
                resolveRef.layout     = GetVkImageLayout<IsSrcLayoutFalse>(accessInfo.access);

                attchmtCount++;
            }

            auto dynamicStates = nodeDeclInfo.dynamicStates.Get(nodeDeclInfo.semanticKinds);

            VkClearValue clearValues[RPS_MAX_SIMULTANEOUS_RENDER_TARGET_COUNT + 1];
            uint32_t     clearValueCount = 0;

            static_assert(sizeof(RpsClearColorValue) == sizeof(VkClearColorValue),
                          "Bad assumption about VkClearColorValue size");

            uint32_t clearColorMask = nodeDeclRenderPassInfo.renderTargetClearMask;
            for (auto clearColorRef : nodeDeclRenderPassInfo.GetRenderTargetClearValueRefs())
            {
                uint32_t rtSlot = rpsFirstBitLow(clearColorMask);
                clearColorMask &= ~(1u << rtSlot);

                const auto& colorAttchmtRef = colorRefs[rtSlot];

                clearValues[colorAttchmtRef.attachment].color =
                    static_cast<const VkClearColorValue*>(cmd.args[clearColorRef.paramId])[clearColorRef.arrayOffset];

                clearValueCount = rpsMax(clearValueCount, colorAttchmtRef.attachment + 1);
            }

            if (nodeDeclRenderPassInfo.clearDepth)
            {
                auto& depthClearValueRef = *nodeDeclRenderPassInfo.GetDepthClearValueRef();

                clearValues[depthRef.attachment].depthStencil.depth =
                    *static_cast<const float*>(cmd.args[depthClearValueRef.paramId]);

                clearValueCount = rpsMax(clearValueCount, depthRef.attachment + 1);
            }

            if (nodeDeclRenderPassInfo.clearStencil)
            {
                auto& stencilClearValueRef = *nodeDeclRenderPassInfo.GetStencilClearValueRef();

                clearValues[depthRef.attachment].depthStencil.stencil =
                    *static_cast<const uint32_t*>(cmd.args[stencilClearValueRef.paramId]);

                clearValueCount = rpsMax(clearValueCount, depthRef.attachment + 1);
            }

            subpassDesc.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpassDesc.colorAttachmentCount    = numRtvs;
            subpassDesc.pColorAttachments       = (numRtvs > 0) ? colorRefs : nullptr;
            subpassDesc.pResolveAttachments     = (numRtvs > 0) ? resolveRefs : nullptr;
            subpassDesc.pDepthStencilAttachment = bHasDsv ? &depthRef : nullptr;

            rpInfo.attachmentCount = attchmtCount;
            rpInfo.pAttachments    = attchmtDescs;
            rpInfo.dependencyCount = 0;  // TODO: Only using cmd barriers atm
            rpInfo.pDependencies   = nullptr;

            auto pVkRP = &currResources.renderPasses[rpIndex];
            RPS_V_RETURN(VkResultToRps(RPS_VK_API_CALL(vkCreateRenderPass(hVkDevice, &rpInfo, nullptr, pVkRP))));

            RPS_ASSERT(pCmdInfo->pRenderPassInfo);
            auto& cmdRPInfo = *pCmdInfo->pRenderPassInfo;

            VkFramebufferCreateInfo fbInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            fbInfo.renderPass              = currResources.renderPasses[rpIndex];
            fbInfo.attachmentCount         = attchmtCount;
            fbInfo.pAttachments            = attchmtViews;
            fbInfo.width                   = cmdRPInfo.viewportInfo.defaultRenderArea.width;
            fbInfo.height                  = cmdRPInfo.viewportInfo.defaultRenderArea.height;
            fbInfo.layers                  = 1;  // TODO

            auto pVkFB = &currResources.frameBuffers[rpIndex];
            RPS_V_RETURN(VkResultToRps(RPS_VK_API_CALL(vkCreateFramebuffer(hVkDevice, &fbInfo, nullptr, pVkFB))));

            if (clearValueCount > 0)
            {
                runtimeCmd.clearValues = context.frameArena.NewArray<VkClearValue>(clearValueCount);
                RPS_CHECK_ALLOC(runtimeCmd.clearValues.data());

                memcpy(runtimeCmd.clearValues.data(), clearValues, sizeof(VkClearValue) * clearValueCount);
            }

            rpIndex++;
        }

        return RPS_OK;
    }

    RpsResult VKRuntimeBackend::TrackImageLayoutInfo(RpsResourceId           resourceId,
                                                     const ResourceInstance& resInfo,
                                                     const CmdAccessInfo&    accessInfo)
    {
        RPS_ASSERT(resourceId != RPS_RESOURCE_ID_INVALID);

        if (!(resInfo.allAccesses.accessFlags & RPS_ACCESS_DEPTH_STENCIL))
        {
            return RPS_OK;
        }

        if (m_resourceLayoutOffsets.size() <= resourceId)
        {
            m_resourceLayoutOffsets.resize(resourceId + 1, UINT32_MAX);
        }

        if (m_resourceLayoutOffsets[resourceId] == UINT32_MAX)
        {
            m_resourceLayoutOffsets[resourceId] = uint32_t(m_subResLayouts.size());
            RPS_CHECK_ALLOC(m_subResLayouts.grow(resInfo.numSubResources, VK_IMAGE_LAYOUT_UNDEFINED));
        }

        ArrayRef<VkImageLayout> subResLayouts = {&m_subResLayouts[m_resourceLayoutOffsets[resourceId]],
                                                 resInfo.numSubResources};

        const VkImageLayout layout             = GetVkImageLayout<IsSrcLayoutFalse>(accessInfo.access);
        const uint32_t      numSubResPerAspect = resInfo.desc.GetImageArrayLayers() * resInfo.desc.image.mipLevels;

        for (uint32_t iAspect = 0; iAspect < 2; iAspect++)
        {
            const RpsImageAspectUsageFlags aspect = (iAspect == 0) ? RPS_IMAGE_ASPECT_DEPTH : RPS_IMAGE_ASPECT_STENCIL;

            if (accessInfo.range.aspectMask & aspect)
            {
                for (uint32_t iArray = accessInfo.range.baseArrayLayer, arrayEnd = accessInfo.range.arrayLayerEnd;
                     iArray < arrayEnd;
                     iArray++)
                {
                    const uint32_t subResOffset = iAspect * numSubResPerAspect + iArray * resInfo.desc.image.mipLevels;
                    const auto     subResBegin  = subResLayouts.begin() + subResOffset;

                    RPS_ASSERT(subResOffset + accessInfo.range.GetMipLevelCount() <= subResLayouts.size());

                    std::fill(subResBegin, subResBegin + accessInfo.range.GetMipLevelCount(), layout);
                }
            }
        }

        return RPS_OK;
    }

    VkImageLayout VKRuntimeBackend::GetTrackedImageLayoutInfo(const ResourceInstance& resInfo,
                                                              const CmdAccessInfo&    accessInfo) const
    {
        if ((accessInfo.resourceId < m_resourceLayoutOffsets.size()) &&
            (m_resourceLayoutOffsets[accessInfo.resourceId] != UINT32_MAX))
        {
            RPS_ASSERT(resInfo.allAccesses.accessFlags & RPS_ACCESS_DEPTH_STENCIL);

            const uint32_t numSubResPerAspect = resInfo.desc.GetImageArrayLayers() * resInfo.desc.image.mipLevels;

            const uint32_t layoutInfoOffset =
                m_resourceLayoutOffsets[accessInfo.resourceId] +
                ((accessInfo.range.aspectMask & RPS_IMAGE_ASPECT_DEPTH) ? 0 : numSubResPerAspect) +
                accessInfo.range.baseArrayLayer * resInfo.desc.image.mipLevels + accessInfo.range.baseMipLevel;

            return m_subResLayouts[layoutInfoOffset];
        }

        return GetVkImageLayout<IsSrcLayoutFalse>(accessInfo.access);
    }

    void VKRuntimeBackend::ProcessBarrierBatch(const RenderGraphUpdateContext& context,
                                               Span<RuntimeCmdInfo>&           transitionRange)
    {
        auto& aliasingInfos       = context.renderGraph.GetResourceAliasingInfos();
        auto& resourceInstances   = context.renderGraph.GetResourceInstances();
        auto  transitions         = context.renderGraph.GetTransitions().crange_all();
        auto  transitionRangeCmds = transitionRange.Get(context.renderGraph.GetRuntimeCmdInfos());

        VKBarrierBatch currBatch = {};

        currBatch.imageBarriers.SetRange(uint32_t(m_imageBarriers.size()), 0);
        currBatch.bufferBarriers.SetRange(uint32_t(m_bufferBarriers.size()), 0);
        currBatch.memoryBarriers.SetRange(uint32_t(m_memoryBarriers.size()), 0);

        for (uint32_t idx = 0; idx < transitionRangeCmds.size(); idx++)
        {
            auto& cmd = transitionRangeCmds[idx];
            RPS_ASSERT(cmd.isTransition);

            // For aliased resources, wait on deactivating final access pipeline stages.
            for (auto& aliasing : cmd.aliasingInfos.Get(aliasingInfos))
            {
                if (aliasing.srcDeactivating)
                {
                    if (aliasing.srcResourceIndex != RPS_RESOURCE_ID_INVALID)
                    {
                        auto& srcResInfo = resourceInstances[aliasing.srcResourceIndex];

                        for (auto& finalAccess :
                             srcResInfo.finalAccesses.Get(context.renderGraph.GetResourceFinalAccesses()))
                        {
                            const auto prevAccess =
                                RenderGraph::CalcPreviousAccess(finalAccess.prevTransition, transitions, srcResInfo);

                            auto srcAccessInfo =
                                GetVKAccessInfo<IsRenderPassAttachmentFalse, IsSrcAccessTrue>(prevAccess);

                            currBatch.srcStage |= srcAccessInfo.stages;
                        }
                    }
                    else
                    {
                        currBatch.srcStage |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                        break;
                    }
                }
            }

            if (cmd.cmdId < CMD_ID_PREAMBLE)
            {
                const auto& currTrans   = transitions[cmd.cmdId];
                const auto& resInstance = resourceInstances[currTrans.access.resourceId];

                const auto prevAccess =
                    RenderGraph::CalcPreviousAccess(currTrans.prevTransition, transitions, resInstance);

                if (resInstance.desc.IsImage())
                {
                    auto hImage = FromHandle<VkImage>(resInstance.hRuntimeResource);

                    AppendImageBarrier(
                        hImage, currBatch, prevAccess, currTrans.access.access, resInstance, currTrans.access.range);

                    // VK 1.1 specific workaround for depth/stencil readonly + srv image layout...
                    if ((prevAccess.accessFlags | currTrans.access.access.accessFlags) & RPS_ACCESS_DEPTH_STENCIL)
                    {
                        TrackImageLayoutInfo(currTrans.access.resourceId, resInstance, currTrans.access);
                    }
                }
                else if (resInstance.desc.IsBuffer())
                {
                    auto hBuffer = FromHandle<VkBuffer>(resInstance.hRuntimeResource);

                    AppendBufferBarrier(hBuffer, currBatch, prevAccess, currTrans.access.access, resInstance);
                }
            }
            else if (cmd.cmdId == CMD_ID_POSTAMBLE)
            {
                // At frame end, transit non-deactivated resource states to initial states
                // TODO: Extract non-aliased resource list ahead of time.
                for (uint32_t iRes = 0, numRes = uint32_t(resourceInstances.size()); iRes < numRes; iRes++)
                {
                    auto& resInstance = resourceInstances[iRes];

                    const bool bResHasMemoryBound = resInstance.hRuntimeResource && !resInstance.isPendingCreate;

                    if (resInstance.isAccessed && !resInstance.isAliased && bResHasMemoryBound &&
                        (resInstance.initialAccess.accessFlags != RPS_ACCESS_UNKNOWN))
                    {
                        for (auto& finalAccess :
                             resInstance.finalAccesses.Get(context.renderGraph.GetResourceFinalAccesses()))
                        {
                            const auto prevAccess =
                                RenderGraph::CalcPreviousAccess(finalAccess.prevTransition, transitions, resInstance);

                            if (resInstance.desc.IsImage())
                            {
                                auto hImage = FromHandle<VkImage>(resInstance.hRuntimeResource);

                                AppendImageBarrier(hImage,
                                                   currBatch,
                                                   prevAccess,
                                                   resInstance.initialAccess,
                                                   resInstance,
                                                   finalAccess.range);
                            }
                            else if (resInstance.desc.IsBuffer())
                            {
                                auto hBuffer = FromHandle<VkBuffer>(resInstance.hRuntimeResource);

                                AppendBufferBarrier(
                                    hBuffer, currBatch, prevAccess, resInstance.initialAccess, resInstance);
                            }
                        }
                    }
                }
            }
        }

        currBatch.imageBarriers.SetEnd(uint32_t(m_imageBarriers.size()));
        currBatch.bufferBarriers.SetEnd(uint32_t(m_bufferBarriers.size()));
        currBatch.memoryBarriers.SetEnd(uint32_t(m_memoryBarriers.size()));

        if (!(currBatch.imageBarriers.empty() && currBatch.bufferBarriers.empty() && currBatch.memoryBarriers.empty()))
        {
            auto pNewRuntimeCmd = m_runtimeCmds.grow(1);

            pNewRuntimeCmd->cmdId          = RPS_CMD_ID_INVALID;
            pNewRuntimeCmd->barrierBatchId = uint32_t(m_barrierBatches.size());

            m_barrierBatches.push_back(currBatch);
        }

        transitionRange = {};
    }

    void VKRuntimeBackend::AppendImageBarrier(VkImage                      hImage,
                                              VKBarrierBatch&              barrierBatch,
                                              const RpsAccessAttr&         beforeAccess,
                                              const RpsAccessAttr&         afterAccess,
                                              const ResourceInstance&      resInfo,
                                              const SubresourceRangePacked range)
    {
        auto* pImgBarrier = m_imageBarriers.grow(1, {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER});

        auto srcAccessInfo = GetVKAccessInfo<IsRenderPassAttachmentFalse, IsSrcAccessTrue>(beforeAccess);
        auto dstAccessInfo = GetVKAccessInfo<IsRenderPassAttachmentFalse, IsSrcAccessFalse>(afterAccess);

        barrierBatch.srcStage |= srcAccessInfo.stages;
        barrierBatch.dstStage |= dstAccessInfo.stages;

        pImgBarrier->srcAccessMask       = srcAccessInfo.access;
        pImgBarrier->dstAccessMask       = dstAccessInfo.access;
        pImgBarrier->oldLayout           = srcAccessInfo.imgLayout;
        pImgBarrier->newLayout           = dstAccessInfo.imgLayout;
        pImgBarrier->srcQueueFamilyIndex = srcAccessInfo.queueFamilyIndex;
        pImgBarrier->dstQueueFamilyIndex = dstAccessInfo.queueFamilyIndex;
        pImgBarrier->image               = hImage;

        pImgBarrier->subresourceRange.aspectMask     = range.aspectMask;
        pImgBarrier->subresourceRange.baseMipLevel   = range.baseMipLevel;
        pImgBarrier->subresourceRange.levelCount     = range.GetMipLevelCount();
        pImgBarrier->subresourceRange.baseArrayLayer = range.baseArrayLayer;
        pImgBarrier->subresourceRange.layerCount     = range.GetArrayLayerCount();
    }

    void VKRuntimeBackend::AppendBufferBarrier(VkBuffer                hBuffer,
                                               VKBarrierBatch&         barrierBatch,
                                               const RpsAccessAttr&    beforeAccess,
                                               const RpsAccessAttr&    afterAccess,
                                               const ResourceInstance& resInfo)
    {
        auto* pBufBarrier = m_bufferBarriers.grow(1, {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER});

        auto srcAccessInfo = GetVKAccessInfo<IsRenderPassAttachmentFalse, IsSrcAccessTrue>(beforeAccess);
        auto dstAccessInfo = GetVKAccessInfo<IsRenderPassAttachmentFalse, IsSrcAccessFalse>(afterAccess);

        barrierBatch.srcStage |= srcAccessInfo.stages;
        barrierBatch.dstStage |= dstAccessInfo.stages;

        pBufBarrier->srcAccessMask       = srcAccessInfo.access;
        pBufBarrier->dstAccessMask       = dstAccessInfo.access;
        pBufBarrier->srcQueueFamilyIndex = srcAccessInfo.queueFamilyIndex;
        pBufBarrier->dstQueueFamilyIndex = dstAccessInfo.queueFamilyIndex;
        pBufBarrier->buffer              = hBuffer;
        pBufBarrier->offset              = 0;
        pBufBarrier->size                = VK_WHOLE_SIZE;
    }

    void VKRuntimeBackend::RecordBarrierBatch(VkCommandBuffer hCmdBuf, uint32_t barrierBatch) const
    {
        const auto& batch = m_barrierBatches[barrierBatch];
        RPS_USE_VK_FUNCTIONS(m_device.GetVkFunctions());
        RPS_VK_API_CALL(vkCmdPipelineBarrier(hCmdBuf,
                                             batch.srcStage,
                                             batch.dstStage,
                                             VK_DEPENDENCY_BY_REGION_BIT,
                                             batch.memoryBarriers.size(),
                                             batch.memoryBarriers.Get(m_memoryBarriers).data(),
                                             batch.bufferBarriers.size(),
                                             batch.bufferBarriers.Get(m_bufferBarriers).data(),
                                             batch.imageBarriers.size(),
                                             batch.imageBarriers.Get(m_imageBarriers).data()));
    }

    template <typename ViewHandleType>
    RpsResult VKRuntimeBackend::GetCmdArgViews(const RuntimeCmdCallbackContext& context,
                                               ConstArrayRef<ViewHandleType>    views,
                                               uint32_t                         argIndex,
                                               uint32_t                         srcArrayOffset,
                                               ViewHandleType*                  pViews,
                                               uint32_t                         count) const
    {
        RPS_RETURN_ERROR_IF(argIndex >= context.pNodeDeclInfo->params.size(), RPS_ERROR_INDEX_OUT_OF_BOUNDS);
        RPS_RETURN_OK_IF(count == 0);

        auto& paramAccessInfo = context.pNodeDeclInfo->params[argIndex];
        RPS_RETURN_ERROR_IF(srcArrayOffset + count > paramAccessInfo.numElements, RPS_ERROR_INDEX_OUT_OF_BOUNDS);
        RPS_RETURN_ERROR_IF(paramAccessInfo.access.accessFlags & RPS_ACCESS_NO_VIEW_BIT, RPS_ERROR_INVALID_OPERATION);
        RPS_RETURN_ERROR_IF(!paramAccessInfo.IsResource(), RPS_ERROR_TYPE_MISMATCH);
        RPS_RETURN_ERROR_IF(VkObjectTypeMapper<ViewHandleType>::typeId != paramAccessInfo.typeInfo.id,
                            RPS_ERROR_TYPE_MISMATCH);

        auto descriptorIndices =
            m_accessToDescriptorMap.range(context.pCmdInfo->accesses.GetBegin(), context.pCmdInfo->accesses.size());

        RPS_ASSERT((paramAccessInfo.accessOffset + paramAccessInfo.numElements) <= descriptorIndices.size());

        // Assuming all elements in the same parameter have the same access
        for (uint32_t i = 0; i < count; i++)
        {
            const uint32_t imgViewIndex = descriptorIndices[paramAccessInfo.accessOffset + srcArrayOffset + i];

            pViews[i] = views[imgViewIndex];
        }

        return RPS_OK;
    }

    template <typename ResHandleType>
    RpsResult VKRuntimeBackend::GetCmdArgResources(const RpsCmdCallbackContext* pContext,
                                                   uint32_t                     argIndex,
                                                   uint32_t                     srcArrayOffset,
                                                   ResHandleType*               pResources,
                                                   uint32_t                     count)
    {
        RPS_CHECK_ARGS(pContext && pResources);
        RPS_RETURN_OK_IF(count == 0);

        const auto& context = *RuntimeCmdCallbackContext::Get(pContext);

        RPS_RETURN_ERROR_IF(argIndex >= context.pNodeDeclInfo->params.size(), RPS_ERROR_INDEX_OUT_OF_BOUNDS);

        auto& paramAccessInfo = context.pNodeDeclInfo->params[argIndex];
        RPS_RETURN_ERROR_IF(srcArrayOffset + count > paramAccessInfo.numElements, RPS_ERROR_INDEX_OUT_OF_BOUNDS);
        RPS_RETURN_ERROR_IF(!paramAccessInfo.IsResource(), RPS_ERROR_TYPE_MISMATCH);
        RPS_RETURN_ERROR_IF(VkObjectTypeMapper<ResHandleType>::typeId != paramAccessInfo.typeInfo.id,
                            RPS_ERROR_TYPE_MISMATCH);

        auto&       renderGraph  = *context.pRenderGraph;
        const auto& resInstances = renderGraph.GetResourceInstances();
        const auto& cmdAccesses  = renderGraph.GetCmdAccessInfos();
        auto        accessRange  = context.pCmdInfo->accesses.Get(cmdAccesses);

        RPS_ASSERT((paramAccessInfo.accessOffset + paramAccessInfo.numElements) <= accessRange.size());

        static constexpr RpsRuntimeResource RuntimeResourceNull = RpsRuntimeResource{nullptr};

        // Assuming all elements in the same parameter have the same access
        for (uint32_t i = 0; i < count; i++)
        {
            const uint32_t resId = accessRange[paramAccessInfo.accessOffset + srcArrayOffset + i].resourceId;

            FromHandle(pResources[i],
                       (resId != RPS_RESOURCE_ID_INVALID) ? resInstances[resId].hRuntimeResource : RuntimeResourceNull);
        }

        return RPS_OK;
    }

    RpsResult VKRuntimeBackend::GetCmdArgGpuMemoryRanges(const RpsCmdCallbackContext* pContext,
                                                         uint32_t                     argIndex,
                                                         uint32_t                     srcArrayOffset,
                                                         RpsVkDeviceMemoryRange*      pMemoryRanges,
                                                         uint32_t                     count)
    {
        RPS_CHECK_ARGS(pContext && pMemoryRanges);

        const auto& context = *RuntimeCmdCallbackContext::Get(pContext);

        RPS_RETURN_ERROR_IF(argIndex >= context.pNodeDeclInfo->params.size(), RPS_ERROR_INDEX_OUT_OF_BOUNDS);

        auto& paramAccessInfo = context.pNodeDeclInfo->params[argIndex];
        RPS_RETURN_ERROR_IF(srcArrayOffset + count > paramAccessInfo.numElements, RPS_ERROR_INDEX_OUT_OF_BOUNDS);
        RPS_RETURN_ERROR_IF(!paramAccessInfo.IsResource(), RPS_ERROR_TYPE_MISMATCH);

        auto&       renderGraph  = *context.pRenderGraph;
        const auto& resInstances = renderGraph.GetResourceInstances();
        const auto& cmdAccesses  = renderGraph.GetCmdAccessInfos();
        auto        accessRange  = context.pCmdInfo->accesses.Get(cmdAccesses);
        auto&       heaps        = renderGraph.GetHeapInfos();

        // Assuming all elements in the same parameter have the same access
        for (uint32_t i = 0; i < count; i++)
        {
            auto& dstMemRange = pMemoryRanges[i];

            const uint32_t resId = accessRange[paramAccessInfo.accessOffset + srcArrayOffset + i].resourceId;

            if (resId != RPS_RESOURCE_ID_INVALID)
            {
                const auto& resInfo = resInstances[resId];

                dstMemRange.hMemory = (resInfo.allocPlacement.heapId != RPS_INDEX_NONE_U32)
                                          ? rpsVKMemoryFromHandle(heaps[resInfo.allocPlacement.heapId].hRuntimeHeap)
                                          : VK_NULL_HANDLE;
                dstMemRange.offset  = resInfo.allocPlacement.offset;
                dstMemRange.size    = resInfo.allocRequirement.size;
            }
            else
            {
                dstMemRange = {};
            }
        }

        return RPS_OK;
    }

    RpsResult VKRuntimeBackend::GetCmdArgViews(const RpsCmdCallbackContext* pContext,
                                               uint32_t                     argIndex,
                                               uint32_t                     srcArrayOffset,
                                               VkImageView*                 pImageViews,
                                               uint32_t                     count)
    {
        RPS_CHECK_ARGS(pContext);

        const auto& context  = *RuntimeCmdCallbackContext::Get(pContext);
        auto        pBackend = context.GetBackend<VKRuntimeBackend>();

        return pBackend->GetCmdArgViews<VkImageView>(
            context,
            pBackend->m_frameResources[pBackend->m_currentResourceFrame].imageViews.range_all(),
            argIndex,
            srcArrayOffset,
            pImageViews,
            count);
    }

    RpsResult VKRuntimeBackend::GetCmdArgViews(const RpsCmdCallbackContext* pContext,
                                               uint32_t                     argIndex,
                                               uint32_t                     srcArrayOffset,
                                               VkBufferView*                pBufferViews,
                                               uint32_t                     count)
    {
        RPS_CHECK_ARGS(pContext);

        const auto& context  = *RuntimeCmdCallbackContext::Get(pContext);
        auto        pBackend = context.GetBackend<VKRuntimeBackend>();

        return pBackend->GetCmdArgViews<VkBufferView>(
            context,
            pBackend->m_frameResources[pBackend->m_currentResourceFrame].bufferViews.range_all(),
            argIndex,
            srcArrayOffset,
            pBufferViews,
            count);
    }

    RpsResult VKRuntimeBackend::GetCmdArgImageViewInfos(const RuntimeCmdCallbackContext& context,
                                                        uint32_t                         argIndex,
                                                        uint32_t                         srcArrayOffset,
                                                        RpsVkImageViewInfo*              pImageViewInfos,
                                                        uint32_t                         count) const
    {
        RPS_RETURN_ERROR_IF(argIndex >= context.pNodeDeclInfo->params.size(), RPS_ERROR_INDEX_OUT_OF_BOUNDS);

        const auto& paramAccessInfo = context.pNodeDeclInfo->params[argIndex];
        RPS_RETURN_ERROR_IF(srcArrayOffset + count > paramAccessInfo.numElements, RPS_ERROR_INDEX_OUT_OF_BOUNDS);
        RPS_RETURN_ERROR_IF(paramAccessInfo.access.accessFlags & RPS_ACCESS_NO_VIEW_BIT, RPS_ERROR_INVALID_OPERATION);

        const auto descriptorIndices =
            m_accessToDescriptorMap.range(context.pCmdInfo->accesses.GetBegin(), context.pCmdInfo->accesses.size());

        const auto imageViews = m_frameResources[m_currentResourceFrame].imageViews.crange_all();

        // Assuming all elements in the same parameter has the same access
        RpsVkImageViewInfo* pDstImageViewInfo = pImageViewInfos;
        for (uint32_t i = 0; i < count; i++, pDstImageViewInfo++)
        {
            const uint32_t imgViewIndex = descriptorIndices[paramAccessInfo.accessOffset + srcArrayOffset + i];

            pDstImageViewInfo->hImageView = imageViews[imgViewIndex];
            pDstImageViewInfo->layout     = m_imageViewLayouts[imgViewIndex];
        }

        return RPS_OK;
    }

    RpsResult VKRuntimeBackend::GetCmdRenderPass(const RpsCmdCallbackContext* pContext, VkRenderPass* pRenderPass)
    {
        RPS_CHECK_ARGS(pContext && pRenderPass);

        const auto& context  = *RuntimeCmdCallbackContext::Get(pContext);
        auto        pBackend = context.GetBackend<VKRuntimeBackend>();

        RPS_RETURN_ERROR_IF(context.pRuntimeCmd == nullptr, RPS_ERROR_INVALID_OPERATION);

        auto& frameResources = pBackend->m_frameResources[pBackend->m_currentResourceFrame];
        auto  pRuntimeCmd    = context.GetRuntimeCmd<VKRuntimeCmd>();

        RPS_RETURN_ERROR_IF(pRuntimeCmd->renderPassId >= frameResources.renderPasses.size(),
                            RPS_ERROR_INVALID_OPERATION);

        *pRenderPass = frameResources.renderPasses[pRuntimeCmd->renderPassId];

        return RPS_OK;
    }

    const VKRuntimeBackend* VKRuntimeBackend::Get(const RpsCmdCallbackContext* pContext)
    {
        const auto& context = *RuntimeCmdCallbackContext::Get(pContext);
        return context.GetBackend<VKRuntimeBackend>();
    }

}  // namespace rps

RpsResult rpsVKGetCmdArgImageViewArray(const RpsCmdCallbackContext* pContext,
                                       uint32_t                     argIndex,
                                       uint32_t                     srcArrayOffset,
                                       VkImageView*                 pImageViews,
                                       uint32_t                     count)
{
    return rps::VKRuntimeBackend::GetCmdArgViews(pContext, argIndex, srcArrayOffset, pImageViews, count);
}

RpsResult rpsVKGetCmdArgImageView(const RpsCmdCallbackContext* pContext, uint32_t argIndex, VkImageView* pImageView)
{
    return rpsVKGetCmdArgImageViewArray(pContext, argIndex, 0, pImageView, 1);
}

RpsResult rpsVKGetCmdArgImageViewInfoArray(const RpsCmdCallbackContext* pContext,
                                           uint32_t                     argIndex,
                                           uint32_t                     srcArrayOffset,
                                           RpsVkImageViewInfo*          pImageViewInfos,
                                           uint32_t                     count)
{
    RPS_CHECK_ARGS(pContext);

    const auto& context  = *rps::RuntimeCmdCallbackContext::Get(pContext);
    auto        pBackend = context.GetBackend<rps::VKRuntimeBackend>();

    return pBackend->GetCmdArgImageViewInfos(context, argIndex, srcArrayOffset, pImageViewInfos, count);
}

RpsResult rpsVKGetCmdArgImageViewInfo(const RpsCmdCallbackContext* pContext,
                                      uint32_t                     argIndex,
                                      RpsVkImageViewInfo*          pImageViewInfo)
{
    return rpsVKGetCmdArgImageViewInfoArray(pContext, argIndex, 0, pImageViewInfo, 1);
}

RpsResult rpsVKGetCmdArgBufferViewArray(const RpsCmdCallbackContext* pContext,
                                        uint32_t                     argIndex,
                                        uint32_t                     srcArrayOffset,
                                        VkBufferView*                pBufferViews,
                                        uint32_t                     count)
{
    return rps::VKRuntimeBackend::GetCmdArgViews(pContext, argIndex, srcArrayOffset, pBufferViews, count);
}

RpsResult rpsVKGetCmdArgBufferView(const RpsCmdCallbackContext* pContext, uint32_t argIndex, VkBufferView* pBufferViews)
{
    return rpsVKGetCmdArgBufferViewArray(pContext, argIndex, 0, pBufferViews, 1);
}

RpsResult rpsVKGetCmdArgImageArray(
    const RpsCmdCallbackContext* pContext, uint32_t argIndex, uint32_t srcArrayOffset, VkImage* pImages, uint32_t count)
{
    return rps::VKRuntimeBackend::GetCmdArgResources(pContext, argIndex, srcArrayOffset, pImages, count);
}

RpsResult rpsVKGetCmdArgImage(const RpsCmdCallbackContext* pContext, uint32_t argIndex, VkImage* pImage)
{
    return rpsVKGetCmdArgImageArray(pContext, argIndex, 0, pImage, 1);
}

RpsResult rpsVKGetCmdArgBufferArray(const RpsCmdCallbackContext* pContext,
                                    uint32_t                     argIndex,
                                    uint32_t                     srcArrayOffset,
                                    VkBuffer*                    pBuffers,
                                    uint32_t                     count)
{
    return rps::VKRuntimeBackend::GetCmdArgResources(pContext, argIndex, srcArrayOffset, pBuffers, count);
}

RpsResult rpsVKGetCmdArgBuffer(const RpsCmdCallbackContext* pContext, uint32_t argIndex, VkBuffer* pBuffer)
{
    return rpsVKGetCmdArgBufferArray(pContext, argIndex, 0, pBuffer, 1);
}

RpsResult rpsVKGetCmdArgGpuMemoryArray(const RpsCmdCallbackContext* pContext,
                                       uint32_t                     argIndex,
                                       uint32_t                     srcArrayOffset,
                                       RpsVkDeviceMemoryRange*      pMemoryRanges,
                                       uint32_t                     count)
{
    return rps::VKRuntimeBackend::GetCmdArgGpuMemoryRanges(pContext, argIndex, srcArrayOffset, pMemoryRanges, count);
}

RpsResult rpsVKGetCmdArgGpuMemory(const RpsCmdCallbackContext* pContext,
                                  uint32_t                     argIndex,
                                  RpsVkDeviceMemoryRange*      pMemoryRange)
{
    return rpsVKGetCmdArgGpuMemoryArray(pContext, argIndex, 0, pMemoryRange, 1);
}

RpsResult rpsVKGetCmdRenderPass(const RpsCmdCallbackContext* pContext, VkRenderPass* pRenderPass)
{
    return rps::VKRuntimeBackend::GetCmdRenderPass(pContext, pRenderPass);
}
