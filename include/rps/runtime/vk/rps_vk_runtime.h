// Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the AMD INTERNAL EVALUATION LICENSE.
//
// See file LICENSE.RTF for full license details.

#ifndef RPS_VK_RUNTIME_H
#define RPS_VK_RUNTIME_H

#include "rps/core/rps_api.h"
#include "rps/runtime/common/rps_runtime.h"

#if defined(RPS_VK_NO_PROTOTYPES) || defined(RPS_VK_DYNAMIC_VULKAN_FUNCTIONS)
#define VK_NO_PROTOTYPES
#endif

#include <vulkan/vulkan.h>

/// @addtogroup RpsVKRuntimeDevice
/// @{

/// @brief Bitflags for VK runtime behavior.
typedef enum RpsVKRuntimeFlagBits
{
    RPS_VK_RUNTIME_FLAG_NONE               = 0,       ///< No special behavior.
    RPS_VK_RUNTIME_FLAG_PREFER_RENDER_PASS = 1 << 1,  ///< Prefers using render passes.
    RPS_VK_RUNTIME_FLAG_DONT_FLIP_VIEWPORT = 1 << 2,  ///< Disables viewport flipping. By default RPS flips the viewport
                                                      ///  when the automatic viewport setup is enabled for a command
                                                      ///  node to match the D3D12 backend behavior.
} RpsVKRuntimeFlagBits;

/// @brief Bitmask type for <c><i>RpsVKRuntimeFlagBits</i></c>
typedef uint32_t RpsVKRuntimeFlags;

/// @brief Pointers to some Vulkan functions - a subset used by the library.
typedef struct RpsVKFunctions {
    PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
    PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
    PFN_vkCreateImage vkCreateImage;
    PFN_vkDestroyImage vkDestroyImage;
    PFN_vkBindImageMemory vkBindImageMemory;
    PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
    PFN_vkCreateBuffer vkCreateBuffer;
    PFN_vkDestroyBuffer vkDestroyBuffer;
    PFN_vkBindBufferMemory vkBindBufferMemory;
    PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
    PFN_vkCreateFramebuffer vkCreateFramebuffer;
    PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
    PFN_vkCreateRenderPass vkCreateRenderPass;
    PFN_vkDestroyRenderPass vkDestroyRenderPass;
    PFN_vkCreateBufferView vkCreateBufferView;
    PFN_vkDestroyBufferView vkDestroyBufferView;
    PFN_vkCreateImageView vkCreateImageView;
    PFN_vkDestroyImageView vkDestroyImageView;
    PFN_vkAllocateMemory vkAllocateMemory;
    PFN_vkFreeMemory vkFreeMemory;
    PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass;
    PFN_vkCmdEndRenderPass vkCmdEndRenderPass;
    PFN_vkCmdSetViewport vkCmdSetViewport;
    PFN_vkCmdSetScissor vkCmdSetScissor;
    PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
    PFN_vkCmdClearColorImage vkCmdClearColorImage;
    PFN_vkCmdClearDepthStencilImage vkCmdClearDepthStencilImage;
    PFN_vkCmdCopyImage vkCmdCopyImage;
    PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
    PFN_vkCmdCopyImageToBuffer vkCmdCopyImageToBuffer;
    PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage;
    PFN_vkCmdResolveImage vkCmdResolveImage;
} RpsVKFunctions;

/// @brief Creation parameters for an RPS device with a Vulkan backend.
typedef struct RpsVKRuntimeDeviceCreateInfo
{
    const RpsDeviceCreateInfo* pDeviceCreateInfo;          ///< Pointer to general RPS device creation parameters.
                                                           ///  Passing NULL uses default parameters instead.
    const RpsRuntimeDeviceCreateInfo* pRuntimeCreateInfo;  ///< Pointer to general RPS runtime creation info. Passing
                                                           ///  NULL uses default parameters instead.
    const RpsVKFunctions* pVulkanFunctions;                ///< Pointer to the Vulkan functions used by the library.
                                                           ///  If RPS_VK_STATIC_VULKAN_FUNCTIONS is not defined, pFunctions must not be NULL.
    VkDevice hVkDevice;                                    ///< Handle to the VK device to use for the runtime. Must not
                                                           ///  be VK_NULL_HANDLE.
    VkPhysicalDevice hVkPhysicalDevice;                    ///< Handle to the VK physical device to use for the runtime.
                                                           ///  Must not be VK_NULL_HANDLE.
    RpsVKRuntimeFlags flags;                               ///< VK runtime flags.
} RpsVKRuntimeDeviceCreateInfo;

/// @} end addtogroup RpsVKRuntimeDevice

#ifdef __cplusplus
extern "C" {
#endif  //__cplusplus

/// @brief Creates a VK runtime device.
///
/// @param pCreateInfo                  Pointer to creation parameters. Must not be NULL.
/// @param phDevice                     Pointer to a handle in which the device is returned. Must not be NULL.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
/// @ingroup RpsVKRuntimeDevice
RpsResult rpsVKRuntimeDeviceCreate(const RpsVKRuntimeDeviceCreateInfo* pCreateInfo, RpsDevice* phDevice);

/// @addtogroup RpsRenderGraphCommandRecordingVK
/// @{

/// @brief Mapping between <c><i>RpsRuntimeCommandBuffer</i></c> and <c><i>VkCommandBuffer</i></c>.
RPS_IMPL_OPAQUE_HANDLE(VKCommandBuffer, RpsRuntimeCommandBuffer, VkCommandBuffer_T);

/// @brief Mapping between <c><i>RpsRuntimeResource</i></c> and <c><i>VkImage</i></c>.
RPS_IMPL_OPAQUE_HANDLE(VKImage, RpsRuntimeResource, VkImage_T);

/// @brief Mapping between <c><i>RpsRuntimeResource</i></c> and <c><i>VkBuffer</i></c>.
RPS_IMPL_OPAQUE_HANDLE(VKBuffer, RpsRuntimeResource, VkBuffer_T);

/// @brief Mapping between <c><i>RpsRuntimeHeap</i></c> and <c><i>VkDeviceMemory</i></c>.
RPS_IMPL_OPAQUE_HANDLE(VKMemory, RpsRuntimeHeap, VkDeviceMemory_T);

/// @brief Gets an array of VK image view handles from an image resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the image view handles from. Must be an image
///                                     resource array argument if numImageViews > 1.
/// @param srcArrayOffset               Offset to the first image view handle to get.
/// @param pImageViews                  Pointer to an array of <c><i>VkImageView</i></c> in which the numImageViews
///                                     image view handles are returned. Must not be NULL if numImageViews != 0.
/// @param numImageViews                Number of image view handles to get. Requires srcArrayOffset + numImageViews to
///                                     be less than the number of elements in the node argument.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsVKGetCmdArgImageViewArray(const RpsCmdCallbackContext* pContext,
                                       uint32_t                     argIndex,
                                       uint32_t                     srcArrayOffset,
                                       VkImageView*                 pImageViews,
                                       uint32_t                     numImageViews);

/// @brief Gets a VK image view handle from an image resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the image view from. Must be an image resource
///                                     argument.
/// @param pImageView                   Pointer in which the image view handle is returned. Must not be NULL.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsVKGetCmdArgImageView(const RpsCmdCallbackContext* pContext, uint32_t argIndex, VkImageView* pImageView);

/// @brief Parameters of a VK image view info.
typedef struct RpsVkImageViewInfo
{
    VkImageView   hImageView;  ///< Handle to the image view.
    VkImageLayout layout;      ///< Layout of the viwed image.
} RpsVkImageViewInfo;

/// @brief Gets an array of VK image view infos from an image resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the image view infos from. Must be an image
///                                     resource array argument if numImageViewInfos > 1.
/// @param srcArrayOffset               Offset to the first image view info to get.
/// @param pImageViewInfos              Pointer to an array of <c><i>RpsVkImageViewInfo</i></c> in which the
///                                     numImageViewInfos image view infos are returned. Must not be NULL if
///                                     numImageViewInfos != 0.
/// @param numImageViewInfos            Number of image view infos to get. Requires srcArrayOffset + numImageViewInfos
///                                     to be less than the number of elements in the node argument.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsVKGetCmdArgImageViewInfoArray(const RpsCmdCallbackContext* pContext,
                                           uint32_t                     argIndex,
                                           uint32_t                     srcArrayOffset,
                                           RpsVkImageViewInfo*          pImageViewInfos,
                                           uint32_t                     numImageViewInfos);

/// @brief Gets a VK image view info from an image view node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the image view info from. Must be an image resource
///                                     argument.
/// @param pImageViewInfo               Pointer in which the image view info is returned. Must not be NULL.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsVKGetCmdArgImageViewInfo(const RpsCmdCallbackContext* pContext,
                                      uint32_t                     argIndex,
                                      RpsVkImageViewInfo*          pImageViewInfo);

/// @brief Gets an array of VK image handles from an image resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the image view infos from. Must be an image
///                                     resource array argument if numImages > 1.
/// @param srcArrayOffset               Offset to the first image handle to get.
/// @param pImages                      Pointer to an array of <c><i>VkImage</i></c> in which the numImages image
///                                     handles are returned. Must not be NULL if numImages != 0.
/// @param numImages                    Number of image handles to get. Requires srcArrayOffset + numImages to be
///                                     less than the number of elements in the node argument.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsVKGetCmdArgImageArray(const RpsCmdCallbackContext* pContext,
                                   uint32_t                     argIndex,
                                   uint32_t                     srcArrayOffset,
                                   VkImage*                     pImages,
                                   uint32_t                     numImages);

/// @brief Gets a VK image handle from an image resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the image handle info from. Must be an image
///                                     resource argument.
/// @param pImage                       Pointer in which the image handle is returned. Must not be NULL.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsVKGetCmdArgImage(const RpsCmdCallbackContext* pContext, uint32_t argIndex, VkImage* pImage);

/// @brief Gets an array of VK buffer view handles from a buffer resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the buffer view handle from. Must be a buffer
///                                     resource array argument if numBufferViews > 1.
/// @param srcArrayOffset               Offset to the first buffer view handle to get.
/// @param pBufferViews                 Pointer to an array of <c><i>VkBufferView</i></c> in which the numBufferViews
///                                     buffer view handles are returned. Must not be NULL if numBufferViews != 0.
/// @param numBufferViews               Number of buffer view handles to get. Requires srcArrayOffset + numBufferViews
///                                     to be less than the number of elements in the node argument.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsVKGetCmdArgBufferViewArray(const RpsCmdCallbackContext* pContext,
                                        uint32_t                     argIndex,
                                        uint32_t                     srcArrayOffset,
                                        VkBufferView*                pBufferViews,
                                        uint32_t                     numBufferViews);

/// @brief Gets a VK buffer view handle from an buffer resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the buffer view handle from. Must be a buffer
///                                     resource argument.
/// @param pBufferViews                 Pointer in which the buffer view handle is returned. Must not be NULL.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsVKGetCmdArgBufferView(const RpsCmdCallbackContext* pContext,
                                   uint32_t                     argIndex,
                                   VkBufferView*                pBufferViews);

/// @brief Gets an array of VK buffer handles from a buffer resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the image view infos from. Must be a buffer
///                                     resource array argument if numBuffers > 1.
/// @param srcArrayOffset               Offset to the first buffer handle to get.
/// @param pBuffers                     Pointer to an array of <c><i>VkBuffer</i></c> in which the numBuffers buffer
///                                     handles are returned. Must not be NULL if numBuffers != 0.
/// @param numBuffers                   Number of buffer handles to get. Requires srcArrayOffset + numBuffers to be
///                                     less than the number of elements in the node argument.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsVKGetCmdArgBufferArray(const RpsCmdCallbackContext* pContext,
                                    uint32_t                     argIndex,
                                    uint32_t                     srcArrayOffset,
                                    VkBuffer*                    pBuffers,
                                    uint32_t                     numBuffers);

/// @brief Gets a VK buffer handle from a buffer resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the buffer handle from. Must be a buffer resource
///                                     argument.
/// @param pBuffer                      Pointer in which the buffer handle is returned. Must not be NULL.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsVKGetCmdArgBuffer(const RpsCmdCallbackContext* pContext, uint32_t argIndex, VkBuffer* pBuffer);

/// @brief Parameters of a VK memory range.
typedef struct RpsVkDeviceMemoryRange
{
    VkDeviceMemory hMemory;  ///< Handle to the device memory.
    size_t         offset;   ///< Offset into the device memory in bytes.
    size_t         size;     ///< Size of the range in bytes.
} RpsVkDeviceMemoryRange;

/// @brief Gets an array of VK memory ranges from a resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the memory ranges from. Must be a resource array
///                                     argument if numRanges > 1.
/// @param srcArrayOffset               Offset to the first memory range to get. Pointer to an array of
///                                     <c><i>RpsVkDeviceMemoryRange</i></c> in which the numRanges memory ranges are
///                                     returned. Must not be NULL if numRanges != 0.
/// @param pMemoryRanges                Pointer to an array of <c><i>RpsVkDeviceMemoryRange</i></c> in which the
///                                     numRanges memory ranges are returned. Must not be NULL if numRanges != 0.
/// @param numRanges                    Number of memory ranges to get. Requires srcArrayOffset + numRanges to be
///                                     less than the number of elements in the node argument.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.

RpsResult rpsVKGetCmdArgGpuMemoryArray(const RpsCmdCallbackContext* pContext,
                                       uint32_t                     argIndex,
                                       uint32_t                     srcArrayOffset,
                                       RpsVkDeviceMemoryRange*      pMemoryRanges,
                                       uint32_t                     numRanges);

/// @brief Gets a VK memory range from a resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the memory range from. Must be a resource argument.
/// @param pMemoryRange                 Pointer in which the memory range is returned. Must not be NULL.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsVKGetCmdArgGpuMemory(const RpsCmdCallbackContext* pContext,
                                  uint32_t                     argIndex,
                                  RpsVkDeviceMemoryRange*      pMemoryRange);

/// @brief Gets the render pass handle of the current node.
///
/// @param pContext                     Callback context of the current command.
/// @param pRenderPass                  Pointer in which the render pass handle is returned. Must not be NULL.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsVKGetCmdRenderPass(const RpsCmdCallbackContext* pContext, VkRenderPass* pRenderPass);

/// @} end addtogroup RpsRenderGraphCommandRecordingVK

/// @addtogroup RpsFormat
/// @{

/// @brief Converts an RPS format to a VK format.
///
/// @param rpsFormat                RPS format to convert.
///
/// @returns                        Converted VK format.
VkFormat rpsFormatToVK(RpsFormat rpsFormat);

/// @brief Converts a VK format to an RPS format.
///
/// @param vkFormat                 VK format to convert.
///
/// @returns                        Converted RPS format.
RpsFormat rpsFormatFromVK(VkFormat vkFormat);

/// @} end addtogroup RpsFormat

#ifdef __cplusplus
}
#endif  //__cplusplus

#ifdef __cplusplus

#include "rps/core/rps_cmd_callback_wrapper.hpp"

namespace rps
{
    namespace details
    {
        template <int32_t Index>
        struct CommandArgUnwrapper<Index, VkImage>
        {
            VkImage operator()(const RpsCmdCallbackContext* pContext)
            {
                VkImage   image  = VK_NULL_HANDLE;
                RpsResult result = rpsVKGetCmdArgImage(pContext, Index, &image);
                if (RPS_FAILED(result))
                {
                    rpsCmdCallbackReportError(pContext, result);
                }
                return image;
            }
        };

        template <int32_t Index>
        struct CommandArgUnwrapper<Index, VkBuffer>
        {
            VkBuffer operator()(const RpsCmdCallbackContext* pContext)
            {
                VkBuffer  buffer = VK_NULL_HANDLE;
                RpsResult result = rpsVKGetCmdArgBuffer(pContext, Index, &buffer);
                if (RPS_FAILED(result))
                {
                    rpsCmdCallbackReportError(pContext, result);
                }
                return buffer;
            }
        };

        template <int32_t Index>
        struct CommandArgUnwrapper<Index, VkImageView>
        {
            VkImageView operator()(const RpsCmdCallbackContext* pContext)
            {
                VkImageView imageView;
                RpsResult   result = rpsVKGetCmdArgImageView(pContext, Index, &imageView);
                if (RPS_FAILED(result))
                {
                    rpsCmdCallbackReportError(pContext, result);
                }
                return imageView;
            }
        };

        template <int32_t Index>
        struct CommandArgUnwrapper<Index, VkBufferView>
        {
            VkBufferView operator()(const RpsCmdCallbackContext* pContext)
            {
                VkBufferView bufferView;
                RpsResult    result = rpsVKGetCmdArgBufferView(pContext, Index, &bufferView);
                if (RPS_FAILED(result))
                {
                    rpsCmdCallbackReportError(pContext, result);
                }
                return bufferView;
            }
        };

        template <int32_t Index>
        struct CommandArgUnwrapper<Index, RpsVkDeviceMemoryRange>
        {
            RpsVkDeviceMemoryRange operator()(const RpsCmdCallbackContext* pContext)
            {
                RpsVkDeviceMemoryRange memoryRange;
                RpsResult              result = rpsVKGetCmdArgGpuMemory(pContext, Index, &memoryRange);

                if (RPS_FAILED(result))
                {
                    rpsCmdCallbackReportError(pContext, result);
                }

                return memoryRange;
            }
        };

        template <int32_t Index>
        struct CommandArgUnwrapper<Index, RpsVkImageViewInfo>
        {
            RpsVkImageViewInfo operator()(const RpsCmdCallbackContext* pContext)
            {
                RpsVkImageViewInfo imageViewInfo;
                RpsResult          result = rpsVKGetCmdArgImageViewInfo(pContext, Index, &imageViewInfo);

                if (RPS_FAILED(result))
                {
                    rpsCmdCallbackReportError(pContext, result);
                }

                return imageViewInfo;
            }
        };

    }  // namespace details
}  // namespace rps

#endif  //__cplusplus

#endif  //RPS_VK_RUNTIME_H
