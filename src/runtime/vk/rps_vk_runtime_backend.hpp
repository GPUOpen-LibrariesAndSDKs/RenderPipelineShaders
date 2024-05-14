// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_VK_RUNTIME_BACKEND_HPP
#define RPS_VK_RUNTIME_BACKEND_HPP

#include "runtime/common/rps_render_graph.hpp"
#include "runtime/vk/rps_vk_runtime_device.hpp"

namespace rps
{
    class VKRuntimeBackend : public RuntimeBackend
    {
    public:
        struct VKBarrierBatch
        {
            VkPipelineStageFlags        srcStage;
            VkPipelineStageFlags        dstStage;
            Span<VkImageMemoryBarrier>  imageBarriers;
            Span<VkBufferMemoryBarrier> bufferBarriers;
            Span<VkMemoryBarrier>       memoryBarriers;
        };

        struct VKRuntimeCmd : public RuntimeCmd
        {
            uint32_t barrierBatchId;
            uint32_t resourceBindingInfo;
            uint32_t renderPassId;
            uint32_t frameBufferId;

            ArrayRef<VkClearValue> clearValues;

            VKRuntimeCmd()
                : barrierBatchId(RPS_INDEX_NONE_U32)
                , resourceBindingInfo(RPS_INDEX_NONE_U32)
                , renderPassId(RPS_INDEX_NONE_U32)
                , frameBufferId(RPS_INDEX_NONE_U32)
            {
            }

            VKRuntimeCmd(uint32_t inCmdId, uint32_t inBarrierBatchId, uint32_t inResourceBindingInfo)
                : RuntimeCmd(inCmdId)
                , barrierBatchId(inBarrierBatchId)
                , resourceBindingInfo(inResourceBindingInfo)
                , renderPassId(RPS_INDEX_NONE_U32)
                , frameBufferId(RPS_INDEX_NONE_U32)
            {
            }
        };

    public:
        VKRuntimeBackend(VKRuntimeDevice& device, RenderGraph& renderGraph)
            : RuntimeBackend(renderGraph)
            , m_device(device)
            , m_persistentPool(device.GetDevice().Allocator())
            , m_pendingReleaseImages(&m_persistentPool)
            , m_pendingReleaseBuffers(&m_persistentPool)
            , m_frameResources(&m_persistentPool)
        {
        }

        virtual ~VKRuntimeBackend();

        virtual RpsResult RecordCommands(const RenderGraph&                     renderGraph,
                                         const RpsRenderGraphRecordCommandInfo& recordInfo) const override final;

        virtual RpsResult RecordCmdRenderPassBegin(const RuntimeCmdCallbackContext& context) const override final;

        virtual RpsResult RecordCmdRenderPassEnd(const RuntimeCmdCallbackContext& context) const override final;

        virtual RpsResult RecordCmdFixedFunctionBindingsAndDynamicStates(
            const RuntimeCmdCallbackContext& context) const override final;

        virtual void DestroyRuntimeResourceDeferred(ResourceInstance& resource) override final;

        RpsResult GetCmdArgImageViewInfos(const RuntimeCmdCallbackContext& context,
                                          uint32_t                         argIndex,
                                          uint32_t                         srcArrayOffset,
                                          RpsVkImageViewInfo*              pImageViewInfos,
                                          uint32_t                         count) const;

        static RpsResult GetCmdArgViews(const RpsCmdCallbackContext* pContext,
                                        uint32_t                     argIndex,
                                        uint32_t                     srcArrayOffset,
                                        VkImageView*                 pImageViews,
                                        uint32_t                     count);
        static RpsResult GetCmdArgViews(const RpsCmdCallbackContext* pContext,
                                        uint32_t                     argIndex,
                                        uint32_t                     srcArrayOffset,
                                        VkBufferView*                pBufferViews,
                                        uint32_t                     count);

        template <typename ResHandleType>
        static RpsResult GetCmdArgResources(const RpsCmdCallbackContext* pContext,
                                            uint32_t                     argIndex,
                                            uint32_t                     srcArrayOffset,
                                            ResHandleType*               pResources,
                                            uint32_t                     count);

        static RpsResult GetCmdArgGpuMemoryRanges(const RpsCmdCallbackContext* pContext,
                                                  uint32_t                     argIndex,
                                                  uint32_t                     srcArrayOffset,
                                                  RpsVkDeviceMemoryRange*      pMemoryRanges,
                                                  uint32_t                     count);

        static RpsResult GetCmdRenderPass(const RpsCmdCallbackContext* pContext, VkRenderPass* pRenderPass);

        static const VKRuntimeBackend* Get(const RpsCmdCallbackContext* pContext);

        static VkCommandBuffer GetContextVkCmdBuf(const RuntimeCmdCallbackContext& context)
        {
            return rpsVKCommandBufferFromHandle(context.hCommandBuffer);
        }

        const VKRuntimeDevice& GetVkRuntimeDevice() const
        {
            return m_device;
        }

    protected:
        virtual RpsResult UpdateFrame(const RenderGraphUpdateContext& context) override final;
        virtual RpsResult CreateHeaps(const RenderGraphUpdateContext& context, ArrayRef<HeapInfo> heaps) override final;
        virtual void      DestroyHeaps(ArrayRef<HeapInfo> heaps) override final;
        virtual RpsResult CreateResources(const RenderGraphUpdateContext& context,
                                          ArrayRef<ResourceInstance>      resources) override final;
        virtual void      DestroyResources(ArrayRef<ResourceInstance> resources) override final;
        virtual RpsResult CreateCommandResources(const RenderGraphUpdateContext& context) override final;
        virtual void      OnDestroy() override final;

    private:
        RPS_NO_DISCARD
        RpsResult CreateBufferViews(const RenderGraphUpdateContext& context, ConstArrayRef<uint32_t> accessIndices);
        RPS_NO_DISCARD
        RpsResult CreateImageViews(const RenderGraphUpdateContext& context, ConstArrayRef<uint32_t> accessIndices);

        RpsResult CreateRenderPasses(const RenderGraphUpdateContext& context, ConstArrayRef<uint32_t> cmdIndices);

        void ProcessBarrierBatch(const RenderGraphUpdateContext& context, Span<RuntimeCmdInfo>& transitionRange);

        void AppendImageBarrier(VkImage                      hImage,
                                VKBarrierBatch&              barrierBatch,
                                const RpsAccessAttr&         prevAccess,
                                const RpsAccessAttr&         currAccess,
                                const ResourceInstance&      resInfo,
                                const SubresourceRangePacked range);

        void AppendBufferBarrier(VkBuffer                hBuffer,
                                 VKBarrierBatch&         barrierBatch,
                                 const RpsAccessAttr&    prevAccess,
                                 const RpsAccessAttr&    currAccess,
                                 const ResourceInstance& resInfo);

        RpsResult TrackImageLayoutInfo(RpsResourceId           resourceId,
                                       const ResourceInstance& resInfo,
                                       const CmdAccessInfo&    accessInfo);

        VkImageLayout GetTrackedImageLayoutInfo(const ResourceInstance& resInfo, const CmdAccessInfo& accessInfo) const;

    private:
        void RecordBarrierBatch(VkCommandBuffer hCmdBuf, uint32_t barrierBatch) const;

        template <typename ViewHandleType>
        RpsResult GetCmdArgViews(const RuntimeCmdCallbackContext& context,
                                 ConstArrayRef<ViewHandleType>    views,
                                 uint32_t                         argIndex,
                                 uint32_t                         srcArrayOffset,
                                 ViewHandleType*                  pViews,
                                 uint32_t                         count) const;

    private:
        VKRuntimeDevice& m_device;
        Arena            m_persistentPool;

        ArenaVector<VKRuntimeCmd>          m_runtimeCmds;
        ArenaVector<VKBarrierBatch>        m_barrierBatches;
        ArenaVector<VkImageMemoryBarrier>  m_imageBarriers;
        ArenaVector<VkBufferMemoryBarrier> m_bufferBarriers;
        ArenaVector<VkMemoryBarrier>       m_memoryBarriers;

        ArenaVector<uint32_t>      m_resourceLayoutOffsets;
        ArenaVector<VkImageLayout> m_subResLayouts;
        ArenaVector<VkImageLayout> m_imageViewLayouts;

        struct FrameResources
        {
            ArenaVector<VkImageView>   imageViews;
            ArenaVector<VkBufferView>  bufferViews;
            ArenaVector<VkRenderPass>  renderPasses;
            ArenaVector<VkFramebuffer> frameBuffers;
            ArenaVector<VkImage>       pendingImages;
            ArenaVector<VkBuffer>      pendingBuffers;

            void Reset(Arena& arena)
            {
                imageViews.reset(&arena);
                bufferViews.reset(&arena);
                renderPasses.reset(&arena);
                frameBuffers.reset(&arena);
                pendingImages.reset(&arena);
                pendingBuffers.reset(&arena);
            }

            void DestroyDeviceResources(VKRuntimeDevice& device)
            {   
                VkDevice hDevice = device.GetVkDevice();
                RPS_USE_VK_FUNCTIONS(device.GetVkFunctions());

                for (VkFramebuffer fb : frameBuffers)
                {
                    RPS_VK_API_CALL(vkDestroyFramebuffer(hDevice, fb, nullptr));
                }

                for (VkRenderPass rp : renderPasses)
                {
                    RPS_VK_API_CALL(vkDestroyRenderPass(hDevice, rp, nullptr));
                }

                for (VkBufferView bufView : bufferViews)
                {
                    RPS_VK_API_CALL(vkDestroyBufferView(hDevice, bufView, nullptr));
                }

                for (VkImageView imgView : imageViews)
                {
                    RPS_VK_API_CALL(vkDestroyImageView(hDevice, imgView, nullptr));
                }

                for (VkBuffer buf : pendingBuffers)
                {
                    RPS_VK_API_CALL(vkDestroyBuffer(hDevice, buf, nullptr));
                }

                for (VkImage img : pendingImages)
                {
                    RPS_VK_API_CALL(vkDestroyImage(hDevice, img, nullptr));
                }

                pendingImages.clear();
                pendingBuffers.clear();
                imageViews.clear();
                bufferViews.clear();
                renderPasses.clear();
                frameBuffers.clear();
            }
        };

        ArenaVector<VkImage>        m_pendingReleaseImages;
        ArenaVector<VkBuffer>       m_pendingReleaseBuffers;

        ArenaVector<FrameResources> m_frameResources;
        uint32_t                    m_currentResourceFrame = 0;

        ArenaVector<uint32_t> m_accessToDescriptorMap;
    };
}  // namespace rps

#endif  //RPS_VK_RUNTIME_BACKEND_HPP
