// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "rps/runtime/common/rps_render_states.h"

#include "runtime/vk/rps_vk_runtime_device.hpp"
#include "runtime/vk/rps_vk_runtime_backend.hpp"
#include "runtime/vk/rps_vk_util.hpp"

#include "runtime/common/rps_runtime_util.hpp"

namespace rps
{

    // template<uint MaxRects>
    // graphics node clear_color_regions( [writeonly(clear)] texture t, float4 data, uint numRects, int4 rects[MaxRects] );
    // template<uint MaxRects>
    // graphics node clear_depth_stencil_regions( [writeonly(clear)] texture t, RPS_CLEAR_FLAGS option, float d, uint s, uint numRects, int4 rects[MaxRects] );
    // template<uint MaxRects>
    // compute  node clear_texture_regions( [writeonly(clear)] texture t, uint4 data, uint numRects, int4 rects[MaxRects] );

    // graphics node clear_color            ( [writeonly(clear)] texture t, float4 data );
    // graphics node clear_depth_stencil    ( [writeonly(clear)] texture t, RPS_CLEAR_FLAGS option, float d, uint s );
    // compute  node clear_texture          ( [writeonly(clear)] texture t, uint4 data );
    // copy     node clear_buffer           ( [writeonly(clear)] buffer b, uint4 data );
    // copy     node copy_texture           ( [writeonly(copy)] texture dst, uint3 dstOffset, [readonly(copy)] texture src, uint3 srcOffset, uint3 extent );
    // copy     node copy_buffer            ( [writeonly(copy)] buffer dst, uint64_t dstOffset, [readonly(copy)] buffer src, uint64_t srcOffset, uint64_t size );
    // copy     node copy_texture_to_buffer ( [writeonly(copy)] buffer dst, uint64_t dstByteOffset, uint rowPitch, uint3 bufferImageSize, uint3 dstOffset, [readonly(copy)] texture src, uint3 srcOffset, uint3 extent );
    // copy     node copy_buffer_to_texture ( [writeonly(copy)] texture dst, uint3 dstOffset, [readonly(copy)] buffer src, uint64_t srcByteOffset, uint rowPitch, uint3 bufferImageSize, uint3 srcOffset, uint3 extent );
    // graphics node resolve                ( [writeonly(resolve)] texture dst, uint2 dstOffset, [readonly(resolve)] texture src, uint2 srcOffset, uint2 extent, RPS_RESOLVE_MODE resolveMode );

    static constexpr bool NoRegions   = false;
    static constexpr bool WithRegions = true;

    template <bool HasRegions>
    void VKBuiltInClearColorImpl(const RpsCmdCallbackContext* pContext)
    {
        VkCommandBuffer hCmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);
        RPS_USE_VK_FUNCTIONS(rps::VKRuntimeBackend::Get(pContext)->GetVkRuntimeDevice().GetVkFunctions());

        RPS_ASSERT(pContext->numArgs > 1);

        static_assert(sizeof(RpsClearValue) == sizeof(VkClearColorValue),
                      "Assumption 'sizeof(RpsClearValue) == sizeof(VkClearColorValue)' is no longer true.");

        auto           pImageView  = rpsCmdGetArg<RpsImageView, 0>(pContext);
        auto           pClearValue = rpsCmdGetArg<VkClearColorValue, 1>(pContext);
        uint32_t       numRects    = 0;
        const RpsRect* pRects      = nullptr;

        if (HasRegions)
        {
            RPS_TODO();
        }

        VkImage hImg = {};
        rpsVKGetCmdArgImage(pContext, 0, &hImg);

        VkImageSubresourceRange vkRange = {};
        vkRange.aspectMask              = VK_IMAGE_ASPECT_COLOR_BIT;
        vkRange.baseMipLevel            = pImageView->subresourceRange.baseMipLevel;
        vkRange.levelCount              = pImageView->subresourceRange.mipLevels;
        vkRange.baseArrayLayer          = pImageView->subresourceRange.baseArrayLayer;
        vkRange.layerCount              = pImageView->subresourceRange.arrayLayers;

        RPS_VK_API_CALL(
            vkCmdClearColorImage(hCmdBuf, hImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, pClearValue, 1, &vkRange));
    }

    void VKBuiltInClearColorRegions(const RpsCmdCallbackContext* pContext)
    {
        VKBuiltInClearColorImpl<WithRegions>(pContext);
    }

    void VKBuiltInClearColor(const RpsCmdCallbackContext* pContext)
    {
        VKBuiltInClearColorImpl<NoRegions>(pContext);
    }

    void VKBuiltInClearDepthStencil(const RpsCmdCallbackContext* pContext)
    {
        VkCommandBuffer hCmdBuf        = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);
        RPS_USE_VK_FUNCTIONS(rps::VKRuntimeBackend::Get(pContext)->GetVkRuntimeDevice().GetVkFunctions());

        RPS_ASSERT(pContext->numArgs > 1);

        static_assert(sizeof(RpsClearValue) == sizeof(VkClearColorValue),
                      "Assumption 'sizeof(RpsClearValue) == sizeof(VkClearColorValue)' is no longer true.");

        VkClearDepthStencilValue clearValue = {};

        auto pImageView    = rpsCmdGetArg<RpsImageView, 0>(pContext);
        auto clearFlags    = *rpsCmdGetArg<RpsClearFlags, 1>(pContext);
        clearValue.depth   = *rpsCmdGetArg<float, 2>(pContext);
        clearValue.stencil = *rpsCmdGetArg<uint32_t, 2>(pContext);

        VkImage hImg = {};
        rpsVKGetCmdArgImage(pContext, 0, &hImg);

        const VkImageAspectFlags aspectMask = ((clearFlags & RPS_CLEAR_FLAG_DEPTH) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) |
                                              ((clearFlags & RPS_CLEAR_FLAG_STENCIL) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

        VkImageSubresourceRange vkRange = {};
        vkRange.aspectMask              = aspectMask;
        vkRange.baseMipLevel            = pImageView->subresourceRange.baseMipLevel;
        vkRange.levelCount              = pImageView->subresourceRange.mipLevels;
        vkRange.baseArrayLayer          = pImageView->subresourceRange.baseArrayLayer;
        vkRange.layerCount              = pImageView->subresourceRange.arrayLayers;

        RPS_VK_API_CALL(
            vkCmdClearDepthStencilImage(hCmdBuf, hImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &vkRange));
    }

    void VKBuiltInClearDepthStencilRegions(const RpsCmdCallbackContext* pContext)
    {
        RPS_TODO();
    }

    void VKBuiltInClearTextureUAV(const RpsCmdCallbackContext* pContext)
    {
        RPS_TODO();
    }
    void VKBuiltInClearTextureUAVRegions(const RpsCmdCallbackContext* pContext)
    {
        RPS_TODO();
    }
    void VKBuiltInClearBufferUAV(const RpsCmdCallbackContext* pContext)
    {
        RPS_TODO();
    }
    void VKBuiltInCopyTexture(const RpsCmdCallbackContext* pContext)
    {
        VkCommandBuffer hCmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);
        RPS_USE_VK_FUNCTIONS(rps::VKRuntimeBackend::Get(pContext)->GetVkRuntimeDevice().GetVkFunctions());

        RPS_ASSERT(pContext->numArgs == 5);

        const ResourceInstance *pDstResource, *pSrcResource;
        RPS_V_REPORT_AND_RETURN(pContext,
                                rps::VKRuntimeBackend::GetCmdArgResourceInfos(pContext, 0, 0, &pDstResource, 1));
        RPS_V_REPORT_AND_RETURN(pContext,
                                rps::VKRuntimeBackend::GetCmdArgResourceInfos(pContext, 2, 0, &pSrcResource, 1));

        auto pDstView  = rpsCmdGetArg<RpsImageView, 0>(pContext);
        auto dstOffset = *rpsCmdGetArg<VkOffset3D, 1>(pContext);
        auto pSrcView  = rpsCmdGetArg<RpsImageView, 2>(pContext);
        auto srcOffset = *rpsCmdGetArg<VkOffset3D, 3>(pContext);
        auto extent    = *rpsCmdGetArg<VkExtent3D, 4>(pContext);

        uint32_t srcMipDim[3] = {
            GetMipLevelDimension(pSrcResource->desc.image.width, pSrcView->subresourceRange.baseMipLevel),
            GetMipLevelDimension(pSrcResource->desc.image.height, pSrcView->subresourceRange.baseMipLevel),
            GetMipLevelDimension(pSrcResource->desc.GetImageDepth(), pSrcView->subresourceRange.baseMipLevel),
        };

        extent.width  = (extent.width != UINT32_MAX) ? extent.width : (srcMipDim[0] - srcOffset.x);
        extent.height = (extent.height != UINT32_MAX) ? extent.height : (srcMipDim[1] - srcOffset.y);
        extent.depth  = (extent.depth != UINT32_MAX) ? extent.depth : (srcMipDim[2] - srcOffset.z);

        // TODO: Handle BCn format reinterpret cast copy
        RpsFormat srcFmt = (pSrcView->base.viewFormat != RPS_FORMAT_UNKNOWN) ? pSrcView->base.viewFormat
                                                                             : pSrcResource->desc.image.format;
        RpsFormat dstFmt = (pDstView->base.viewFormat != RPS_FORMAT_UNKNOWN) ? pDstView->base.viewFormat
                                                                             : pDstResource->desc.image.format;
        if (rpsFormatIsBlockCompressed(srcFmt) != rpsFormatIsBlockCompressed(dstFmt))
        {
            RPS_V_REPORT_AND_RETURN(pContext, RPS_ERROR_NOT_IMPLEMENTED);
        }

        RPS_ASSERT(pSrcResource->desc.GetSampleCount() == pDstResource->desc.GetSampleCount());

#define RPS_VK_MAX_IMAGE_COPY_INFO (32)
        VkImageCopy copyInfos[RPS_VK_MAX_IMAGE_COPY_INFO];
#undef RPS_VK_MAX_IMAGE_COPY_INFO

        VkImageSubresourceRange srcRange, dstRange;
        GetVkSubresourceRange(srcRange,
                              SubresourceRangePacked(GetFormatAspectMask(srcFmt, pSrcResource->desc.GetFormat()),
                                                     pSrcView->subresourceRange));
        GetVkSubresourceRange(dstRange,
                              SubresourceRangePacked(GetFormatAspectMask(dstFmt, pDstResource->desc.GetFormat()),
                                                     pDstView->subresourceRange));

        const uint32_t numMipLevels =
            rpsMin(pSrcView->subresourceRange.mipLevels, pDstView->subresourceRange.mipLevels);
        const uint32_t numArrayLayers =
            rpsMin(pSrcView->subresourceRange.arrayLayers, pDstView->subresourceRange.arrayLayers);

        RPS_ASSERT(numMipLevels < RPS_COUNTOF(copyInfos));

        for (uint32_t iMip = 0; iMip < numMipLevels; iMip++)
        {
            const uint32_t srcMip = srcRange.baseMipLevel + iMip;
            const uint32_t dstMip = dstRange.baseMipLevel + iMip;

            VkImageCopy* pCopyInfo = &copyInfos[iMip];

            pCopyInfo->srcSubresource.aspectMask     = srcRange.aspectMask;
            pCopyInfo->srcSubresource.mipLevel       = srcMip;
            pCopyInfo->srcSubresource.baseArrayLayer = srcRange.baseArrayLayer;
            pCopyInfo->srcSubresource.layerCount     = numArrayLayers;
            pCopyInfo->srcOffset                     = srcOffset;

            pCopyInfo->dstSubresource.aspectMask     = dstRange.aspectMask;
            pCopyInfo->dstSubresource.mipLevel       = dstMip;
            pCopyInfo->dstSubresource.baseArrayLayer = dstRange.baseArrayLayer;
            pCopyInfo->dstSubresource.layerCount     = numArrayLayers;
            pCopyInfo->dstOffset                     = dstOffset;

            pCopyInfo->extent = extent;

            extent.width  = rpsMax(1u, extent.width >> 1);
            extent.height = rpsMax(1u, extent.height >> 1);
            extent.depth  = rpsMax(1u, extent.depth >> 1);

            srcOffset.x = srcOffset.x >> 1;
            srcOffset.y = srcOffset.y >> 1;
            srcOffset.z = srcOffset.z >> 1;

            dstOffset.x = dstOffset.x >> 1;
            dstOffset.y = dstOffset.y >> 1;
            dstOffset.z = dstOffset.z >> 1;
        }

        const VkImage hDstResource = rpsVKImageFromHandle(pDstResource->hRuntimeResource);
        const VkImage hSrcResource = rpsVKImageFromHandle(pSrcResource->hRuntimeResource);

        RPS_VK_API_CALL(vkCmdCopyImage(hCmdBuf,
                                       hSrcResource,
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       hDstResource,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       numMipLevels,
                                       copyInfos));
    }

    void VKBuiltInCopyBuffer(const RpsCmdCallbackContext* pContext)
    {
        VkCommandBuffer hCmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);
        RPS_USE_VK_FUNCTIONS(rps::VKRuntimeBackend::Get(pContext)->GetVkRuntimeDevice().GetVkFunctions());

        const ResourceInstance *pDstResource, *pSrcResource;
        RPS_V_REPORT_AND_RETURN(pContext,
                                rps::RuntimeBackend::GetCmdArgResourceInfos(pContext, 0, 0, &pDstResource, 1));
        RPS_V_REPORT_AND_RETURN(pContext,
                                rps::RuntimeBackend::GetCmdArgResourceInfos(pContext, 2, 0, &pSrcResource, 1));

        const auto* pDstView  = rpsCmdGetArg<RpsBufferView, 0>(pContext);
        uint64_t    dstOffset = *rpsCmdGetArg<uint64_t, 1>(pContext);
        const auto* pSrcView  = rpsCmdGetArg<RpsBufferView, 2>(pContext);
        uint64_t    srcOffset = *rpsCmdGetArg<uint64_t, 3>(pContext);
        uint64_t    copySize  = *rpsCmdGetArg<uint64_t, 4>(pContext);

        const uint64_t dstTotalSize = pDstResource->desc.GetBufferSize();
        const uint64_t srcTotalSize = pSrcResource->desc.GetBufferSize();

        const VkBuffer dstBuffer = rpsVKBufferFromHandle(pDstResource->hRuntimeResource);
        const VkBuffer srcBuffer = rpsVKBufferFromHandle(pSrcResource->hRuntimeResource);

        VkBufferCopy copyInfo;
        copyInfo.srcOffset = srcOffset;
        copyInfo.dstOffset = dstOffset;
        copyInfo.size      = (copySize != UINT64_MAX) ? copySize : srcTotalSize;

        RPS_VK_API_CALL(vkCmdCopyBuffer(hCmdBuf, srcBuffer, dstBuffer, 1, &copyInfo));
    }

    static constexpr bool TextureToBuffer = true;
    static constexpr bool BufferToTexture = false;

    template <bool     SourceIsTexture,
              uint32_t BufferArgIdx,
              uint32_t TextureArgIdx,
              uint32_t BufferByteOffsetArgIdx,
              uint32_t BufferRowPitchArgIdx,
              uint32_t BufferImageSizeArgIdx,
              uint32_t BufferOffsetArgIdx,
              uint32_t TextureOffsetArgIdx,
              uint32_t ExtentArgIdx>
    void VKBuiltInCopyTextureBufferCommon(const RpsCmdCallbackContext* pContext)
    {
        VkCommandBuffer hCmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);
        RPS_USE_VK_FUNCTIONS(rps::VKRuntimeBackend::Get(pContext)->GetVkRuntimeDevice().GetVkFunctions());

        const ResourceInstance *pTextureResource, *pBufferResource;
        RPS_V_REPORT_AND_RETURN(
            pContext, rps::VKRuntimeBackend::GetCmdArgResourceInfos(pContext, TextureArgIdx, 0, &pTextureResource, 1));
        RPS_V_REPORT_AND_RETURN(
            pContext, rps::VKRuntimeBackend::GetCmdArgResourceInfos(pContext, BufferArgIdx, 0, &pBufferResource, 1));

        auto* pBufferView      = rpsCmdGetArg<RpsBufferView, BufferArgIdx>(pContext);
        auto* pTextureView     = rpsCmdGetArg<RpsImageView, TextureArgIdx>(pContext);
        auto  bufferByteOffset = *rpsCmdGetArg<uint64_t, BufferByteOffsetArgIdx>(pContext);
        auto  bufferRowPitch   = *rpsCmdGetArg<uint32_t, BufferRowPitchArgIdx>(pContext);
        auto  bufferImageSize  = *rpsCmdGetArg<VkExtent3D, BufferImageSizeArgIdx>(pContext);
        auto  bufferImgOffset  = *rpsCmdGetArg<VkOffset3D, BufferOffsetArgIdx>(pContext);
        auto  textureOffset    = *rpsCmdGetArg<VkOffset3D, TextureOffsetArgIdx>(pContext);
        auto  extent           = *rpsCmdGetArg<VkExtent3D, ExtentArgIdx>(pContext);

        uint32_t texMipDim[3] = {
            GetMipLevelDimension(pTextureResource->desc.image.width, pTextureView->subresourceRange.baseMipLevel),
            GetMipLevelDimension(pTextureResource->desc.image.height, pTextureView->subresourceRange.baseMipLevel),
            GetMipLevelDimension(pTextureResource->desc.GetImageDepth(), pTextureView->subresourceRange.baseMipLevel),
        };

        extent.width  = (extent.width != UINT32_MAX) ? extent.width : texMipDim[0];
        extent.height = (extent.height != UINT32_MAX) ? extent.height : texMipDim[1];
        extent.depth  = (extent.depth != UINT32_MAX) ? extent.depth : texMipDim[2];

        const RpsFormat imgFormat = (pTextureView->base.viewFormat != RPS_FORMAT_UNKNOWN)
                                        ? pTextureView->base.viewFormat
                                        : pTextureResource->desc.image.format;

        const uint32_t texelElementSize = rpsGetFormatElementBytes(imgFormat);

        const uint32_t bufferOffsetToByteOffset =
            (bufferImgOffset.z * bufferImageSize.height + bufferImgOffset.y) * bufferRowPitch +
            bufferImgOffset.x * texelElementSize;

        VkBufferImageCopy copyInfos;
        copyInfos.bufferOffset      = bufferByteOffset + bufferOffsetToByteOffset;
        copyInfos.bufferRowLength   = bufferRowPitch / texelElementSize;
        copyInfos.bufferImageHeight = bufferImageSize.height;
        copyInfos.imageOffset       = textureOffset;

        copyInfos.imageSubresource.aspectMask     = GetFormatAspectMask(imgFormat, pTextureResource->desc.GetFormat());
        copyInfos.imageSubresource.mipLevel       = pTextureView->subresourceRange.baseMipLevel;
        copyInfos.imageSubresource.baseArrayLayer = pTextureView->subresourceRange.baseArrayLayer;
        copyInfos.imageSubresource.layerCount     = pTextureView->subresourceRange.arrayLayers;
        copyInfos.imageExtent                     = extent;

        const VkBuffer bufferHdl = rpsVKBufferFromHandle(pBufferResource->hRuntimeResource);
        const VkImage  imageHdl  = rpsVKImageFromHandle(pTextureResource->hRuntimeResource);

        if (SourceIsTexture)
        {
            RPS_VK_API_CALL(vkCmdCopyImageToBuffer(
                hCmdBuf, imageHdl, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, bufferHdl, 1, &copyInfos));
        }
        else
        {
            RPS_VK_API_CALL(vkCmdCopyBufferToImage(
                hCmdBuf, bufferHdl, imageHdl, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyInfos));
        }
    }

    void VKBuiltInCopyTextureToBuffer(const RpsCmdCallbackContext* pContext)
    {
        enum
        {
            BUFFER_DST = 0,
            BUFFER_BYTE_OFFSET,
            ROW_PITCH,
            BUFFER_IMAGE_SIZE,
            BUFFER_IMAGE_OFFSET,
            TEXTURE_SRC,
            TEXTURE_OFFSET,
            EXTENT,
        };

        VKBuiltInCopyTextureBufferCommon<TextureToBuffer,
                                         BUFFER_DST,
                                         TEXTURE_SRC,
                                         BUFFER_BYTE_OFFSET,
                                         ROW_PITCH,
                                         BUFFER_IMAGE_SIZE,
                                         BUFFER_IMAGE_OFFSET,
                                         TEXTURE_OFFSET,
                                         EXTENT>(pContext);
    }
    void VKBuiltInCopyBufferToTexture(const RpsCmdCallbackContext* pContext)
    {
        enum
        {
            TEXTURE_DST = 0,
            TEXTURE_OFFSET,
            BUFFER_SRC,
            BUFFER_BYTE_OFFSET,
            ROW_PITCH,
            BUFFER_IMAGE_SIZE,
            BUFFER_IMAGE_OFFSET,
            EXTENT,
        };

        VKBuiltInCopyTextureBufferCommon<BufferToTexture,
                                         BUFFER_SRC,
                                         TEXTURE_DST,
                                         BUFFER_BYTE_OFFSET,
                                         ROW_PITCH,
                                         BUFFER_IMAGE_SIZE,
                                         BUFFER_IMAGE_OFFSET,
                                         TEXTURE_OFFSET,
                                         EXTENT>(pContext);
    }

    void VKBuiltInResolve(const RpsCmdCallbackContext* pContext)
    {
        VkCommandBuffer hCmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);
        RPS_USE_VK_FUNCTIONS(rps::VKRuntimeBackend::Get(pContext)->GetVkRuntimeDevice().GetVkFunctions());

        RPS_ASSERT(pContext->numArgs == 6);

        const ResourceInstance *pDstResource, *pSrcResource;
        RPS_V_REPORT_AND_RETURN(pContext,
                                rps::RuntimeBackend::GetCmdArgResourceInfos(pContext, 0, 0, &pDstResource, 1));
        RPS_V_REPORT_AND_RETURN(pContext,
                                rps::RuntimeBackend::GetCmdArgResourceInfos(pContext, 2, 0, &pSrcResource, 1));

        auto pDstView    = rpsCmdGetArg<RpsImageView, 0>(pContext);
        auto dstOffset   = *rpsCmdGetArg<VkOffset2D, 1>(pContext);
        auto pSrcView    = rpsCmdGetArg<RpsImageView, 2>(pContext);
        auto srcOffset   = *rpsCmdGetArg<VkOffset2D, 3>(pContext);
        auto extent      = *rpsCmdGetArg<VkExtent2D, 4>(pContext);
        auto resolveMode = *rpsCmdGetArg<RpsResolveMode, 5>(pContext);

        RPS_ASSERT(!pDstResource->desc.IsBuffer());
        RPS_ASSERT(!pSrcResource->desc.IsBuffer());

        // The builtin resolve command only supports avg for vulkan, as that is usually used for vkCmdResolveImage.
        if (resolveMode != RPS_RESOLVE_MODE_AVERAGE)
        {
            RPS_V_REPORT_AND_RETURN(pContext, RPS_ERROR_NOT_IMPLEMENTED);
        }

        if (pSrcResource->desc.GetSampleCount() < pDstResource->desc.GetSampleCount())
        {
            RPS_V_REPORT_AND_RETURN(pContext, RPS_ERROR_INVALID_OPERATION);
        }

        uint32_t texMipDim[3] = {
            GetMipLevelDimension(pSrcResource->desc.image.width, pSrcView->subresourceRange.baseMipLevel),
            GetMipLevelDimension(pSrcResource->desc.image.height, pSrcView->subresourceRange.baseMipLevel),
            GetMipLevelDimension(pSrcResource->desc.GetImageDepth(), pSrcView->subresourceRange.baseMipLevel),
        };

        extent.width  = (extent.width != UINT32_MAX) ? extent.width : (texMipDim[0] - srcOffset.x);
        extent.height = (extent.height != UINT32_MAX) ? extent.height : (texMipDim[1] - srcOffset.y);

        RpsFormat srcFmt = (pSrcView->base.viewFormat != RPS_FORMAT_UNKNOWN) ? pSrcView->base.viewFormat
                                                                             : pSrcResource->desc.image.format;
        RpsFormat dstFmt = (pDstView->base.viewFormat != RPS_FORMAT_UNKNOWN) ? pDstView->base.viewFormat
                                                                             : pDstResource->desc.image.format;

        uint32_t srcAspectMask = GetFormatAspectMask(srcFmt, pSrcResource->desc.GetFormat());
        uint32_t dstAspectMask = GetFormatAspectMask(dstFmt, pDstResource->desc.GetFormat());

        uint32_t mipLevelCount = rpsMin(pSrcView->subresourceRange.mipLevels, pDstView->subresourceRange.mipLevels);
        uint32_t arrayLayerCount =
            rpsMin(pSrcView->subresourceRange.arrayLayers, pDstView->subresourceRange.arrayLayers);

#define RPS_VK_MAX_IMAGE_RESOLVE_INFO (32)
        VkImageResolve resolveInfo[RPS_VK_MAX_IMAGE_RESOLVE_INFO];
        RPS_ASSERT(mipLevelCount < RPS_VK_MAX_IMAGE_RESOLVE_INFO);
#undef RPS_VK_MAX_IMAGE_RESOLVE_INFO

        for (uint32_t iMip = 0; iMip < mipLevelCount; iMip++)
        {
            const uint32_t srcMip = pSrcView->subresourceRange.baseMipLevel + iMip;
            const uint32_t dstMip = pDstView->subresourceRange.baseMipLevel + iMip;

            resolveInfo[iMip].srcSubresource.aspectMask     = srcAspectMask;
            resolveInfo[iMip].srcSubresource.baseArrayLayer = pSrcView->subresourceRange.baseArrayLayer;
            resolveInfo[iMip].srcSubresource.layerCount     = pSrcView->subresourceRange.arrayLayers;
            resolveInfo[iMip].srcSubresource.mipLevel       = srcMip;
            resolveInfo[iMip].srcOffset.x                   = srcOffset.x >> iMip;
            resolveInfo[iMip].srcOffset.y                   = srcOffset.y >> iMip;
            resolveInfo[iMip].srcOffset.z                   = 0;
            resolveInfo[iMip].dstSubresource.aspectMask     = dstAspectMask;
            resolveInfo[iMip].dstSubresource.baseArrayLayer = pDstView->subresourceRange.baseArrayLayer;
            resolveInfo[iMip].dstSubresource.layerCount     = pDstView->subresourceRange.arrayLayers;
            resolveInfo[iMip].dstSubresource.mipLevel       = dstMip;
            resolveInfo[iMip].dstOffset.x                   = dstOffset.x >> iMip;
            resolveInfo[iMip].dstOffset.y                   = dstOffset.y >> iMip;
            resolveInfo[iMip].dstOffset.z                   = 0;
            resolveInfo[iMip].extent.width                  = rpsMax(1u, extent.width >> iMip);
            resolveInfo[iMip].extent.height                 = rpsMax(1u, extent.height >> iMip);
            resolveInfo[iMip].extent.depth                  = 1;
        }

        const VkImage hDstResource = rpsVKImageFromHandle(pDstResource->hRuntimeResource);
        const VkImage hSrcResource = rpsVKImageFromHandle(pSrcResource->hRuntimeResource);

        RPS_VK_API_CALL(vkCmdResolveImage(hCmdBuf,
                                          hSrcResource,
                                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                          hDstResource,
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                          mipLevelCount,
                                          resolveInfo));
    }
}  // namespace rps
