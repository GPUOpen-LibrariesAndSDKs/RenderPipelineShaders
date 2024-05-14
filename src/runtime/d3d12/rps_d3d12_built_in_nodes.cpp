// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "rps/runtime/common/rps_render_states.h"
#include "rps/runtime/d3d_common/rps_d3d_common.h"

#include "runtime/common/rps_runtime_util.hpp"
#include "runtime/d3d12/rps_d3d12_runtime_device.hpp"
#include "runtime/d3d12/rps_d3d12_runtime_backend.hpp"
#include "runtime/d3d12/rps_d3d12_util.hpp"

namespace rps
{

    static constexpr bool NoRegions   = false;
    static constexpr bool WithRegions = true;

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

    template <bool HasRegions>
    void D3D12BuiltInClearColorImpl(const RpsCmdCallbackContext* pContext)
    {
        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);

        RPS_ASSERT(pContext->numArgs > 1);

        auto              pClearValue = rpsCmdGetArg<RpsClearValue, 1>(pContext);
        uint32_t          numRects    = 0;
        const D3D12_RECT* pRects      = nullptr;

        static_assert(sizeof(RpsRect) == sizeof(D3D12_RECT),
                      "Assumption 'sizeof(RpsRect) == sizeof(D3D12_RECT)' is no longer true.");

        if (HasRegions)
        {
            RPS_ASSERT(pContext->numArgs == 4);
            numRects = *rpsCmdGetArg<uint32_t, 2>(pContext);
            pRects   = rpsCmdGetArg<D3D12_RECT, 3>(pContext);
        }

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHdl;
        rpsD3D12GetCmdArgDescriptor(pContext, 0, &rtvHdl);

        pCmdList->ClearRenderTargetView(rtvHdl, pClearValue->color.float32, numRects, pRects);
    }

    void D3D12BuiltInClearColorRegions(const RpsCmdCallbackContext* pContext)
    {
        D3D12BuiltInClearColorImpl<WithRegions>(pContext);
    }

    void D3D12BuiltInClearColor(const RpsCmdCallbackContext* pContext)
    {
        D3D12BuiltInClearColorImpl<NoRegions>(pContext);
    }

    template <bool HasRegions>
    void D3D12BuiltInClearDepthStencilImpl(const RpsCmdCallbackContext* pContext)
    {
        ID3D12GraphicsCommandList* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);

        RPS_ASSERT(pContext->numArgs > 1);

        auto pClearFlags   = rpsCmdGetArg<RpsClearFlags, 1>(pContext);
        auto pDepthValue   = rpsCmdGetArg<float, 2>(pContext);
        auto pStencilValue = rpsCmdGetArg<uint32_t, 3>(pContext);

        uint32_t          numRects = 0;
        const D3D12_RECT* pRects   = nullptr;

        static_assert(sizeof(RpsRect) == sizeof(D3D12_RECT),
                      "Assumption 'sizeof(RpsRect) == sizeof(D3D12_RECT)' is no longer true.");

        if (HasRegions)
        {
            RPS_ASSERT(pContext->numArgs == 6);
            numRects = *rpsCmdGetArg<uint32_t, 5>(pContext);
            pRects   = rpsCmdGetArg<D3D12_RECT, 6>(pContext);
        }

        D3D12_CPU_DESCRIPTOR_HANDLE dsvHdl;
        RPS_V_REPORT_AND_RETURN(pContext, rpsD3D12GetCmdArgDescriptor(pContext, 0, &dsvHdl));

        const D3D12_CLEAR_FLAGS d3dClearFlags =
            (((*pClearFlags) & RPS_CLEAR_FLAG_DEPTH) ? D3D12_CLEAR_FLAG_DEPTH : D3D12_CLEAR_FLAGS(0)) |
            (((*pClearFlags) & RPS_CLEAR_FLAG_STENCIL) ? D3D12_CLEAR_FLAG_STENCIL : D3D12_CLEAR_FLAGS(0));

        pCmdList->ClearDepthStencilView(dsvHdl, d3dClearFlags, *pDepthValue, *pStencilValue, numRects, pRects);
    }

    void D3D12BuiltInClearDepthStencil(const RpsCmdCallbackContext* pContext)
    {
        D3D12BuiltInClearDepthStencilImpl<NoRegions>(pContext);
    }

    void D3D12BuiltInClearDepthStencilRegions(const RpsCmdCallbackContext* pContext)
    {
        D3D12BuiltInClearDepthStencilImpl<WithRegions>(pContext);
    }

    void D3D12BuiltInClearTextureUAV(const RpsCmdCallbackContext* pContext)
    {
        RPS_TODO();
    }
    void D3D12BuiltInClearTextureUAVRegions(const RpsCmdCallbackContext* pContext)
    {
        RPS_TODO();
    }
    void D3D12BuiltInClearBufferUAV(const RpsCmdCallbackContext* pContext)
    {
        RPS_TODO();
    }

    // copy     node copy_texture           ( [writeonly(copy)] texture dst, uint3 dstOffset, [readonly(copy)] texture src, uint3 srcOffset, uint3 extent );
    void D3D12BuiltInCopyTexture(const RpsCmdCallbackContext* pContext)
    {
        auto* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);
        auto* pBackend = rps::D3D12RuntimeBackend::Get(pContext);

        auto* pRuntimeDevice = D3D12RuntimeDevice::Get<D3D12RuntimeDevice>(pBackend->GetRenderGraph().GetDevice());

        RPS_ASSERT(pContext->numArgs == 5);

        const ResourceInstance *pDstResource, *pSrcResource;
        RPS_V_REPORT_AND_RETURN(pContext,
                                rps::D3D12RuntimeBackend::GetCmdArgResourceInfos(pContext, 0, 0, &pDstResource, 1));
        RPS_V_REPORT_AND_RETURN(pContext,
                                rps::D3D12RuntimeBackend::GetCmdArgResourceInfos(pContext, 2, 0, &pSrcResource, 1));

        auto pDstView   = rpsCmdGetArg<RpsImageView, 0>(pContext);
        auto pDstOffset = *rpsCmdGetArg<uint32_t[3], 1>(pContext);
        auto pSrcView   = rpsCmdGetArg<RpsImageView, 2>(pContext);
        auto pSrcOffset = *rpsCmdGetArg<uint32_t[3], 3>(pContext);
        auto pExtent    = *rpsCmdGetArg<uint32_t[3], 4>(pContext);

        D3D12_TEXTURE_COPY_LOCATION dstLocation;
        dstLocation.Type      = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLocation.pResource = rpsD3D12ResourceFromHandle(pDstResource->hRuntimeResource);

        D3D12_TEXTURE_COPY_LOCATION srcLocation;
        srcLocation.Type      = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcLocation.pResource = rpsD3D12ResourceFromHandle(pSrcResource->hRuntimeResource);

        RpsFormat srcFmt = (pSrcView->base.viewFormat != RPS_FORMAT_UNKNOWN) ? pSrcView->base.viewFormat
                                                                             : pSrcResource->desc.image.format;
        RpsFormat dstFmt = (pDstView->base.viewFormat != RPS_FORMAT_UNKNOWN) ? pDstView->base.viewFormat
                                                                             : pDstResource->desc.image.format;

        uint32_t srcMipDim[3] = {
            GetMipLevelDimension(pSrcResource->desc.image.width, pSrcView->subresourceRange.baseMipLevel),
            GetMipLevelDimension(pSrcResource->desc.image.height, pSrcView->subresourceRange.baseMipLevel),
            GetMipLevelDimension(pSrcResource->desc.GetImageDepth(), pSrcView->subresourceRange.baseMipLevel),
        };

        D3D12_BOX box;
        box.left   = pSrcOffset[0];
        box.top    = pSrcOffset[1];
        box.front  = pSrcOffset[2];
        box.right  = (pExtent[0] == UINT32_MAX) ? srcMipDim[0] : (pSrcOffset[0] + pExtent[0]);
        box.bottom = (pExtent[1] == UINT32_MAX) ? srcMipDim[1] : (pSrcOffset[1] + pExtent[1]);
        box.back   = (pExtent[2] == UINT32_MAX) ? srcMipDim[2] : (pSrcOffset[2] + pExtent[2]);

        uint32_t dstOffset[3] = {pDstOffset[0], pDstOffset[1], pDstOffset[2]};

        const bool isFullSubresource =
            (pSrcOffset[0] == 0) && (pSrcOffset[1] == 0) && (pSrcOffset[2] == 0) &&
            ((box.right == srcMipDim[0]) && (box.bottom == srcMipDim[1]) && (box.back == srcMipDim[2]));

        uint32_t srcAspectMask = pRuntimeDevice->GetFormatPlaneMask(srcFmt);
        uint32_t dstAspectMask = pRuntimeDevice->GetFormatPlaneMask(dstFmt);

        const uint32_t mipLevels = rpsMin(pSrcView->subresourceRange.mipLevels, pDstView->subresourceRange.mipLevels);

        const uint32_t numArrayLayers =
            rpsMin(pSrcView->subresourceRange.arrayLayers, pDstView->subresourceRange.arrayLayers);

        while ((srcAspectMask != 0) && (dstAspectMask != 0))
        {
            const uint32_t srcPlane = (srcAspectMask & 1u) ? 0 : 1;
            srcAspectMask &= ~(1 << srcPlane);
            const uint32_t dstPlane = (dstAspectMask & 1u) ? 0 : 1;
            dstAspectMask &= ~(1 << dstPlane);

            for (uint32_t iMip = 0; iMip < mipLevels; iMip++)
            {
                const uint32_t srcMip = pSrcView->subresourceRange.baseMipLevel + iMip;
                const uint32_t dstMip = pDstView->subresourceRange.baseMipLevel + iMip;

                for (uint32_t iArrayLayer = 0; iArrayLayer < numArrayLayers; iArrayLayer++)
                {
                    srcLocation.SubresourceIndex =
                        D3D12CalcSubresource(srcMip,
                                             pSrcView->subresourceRange.baseArrayLayer + iArrayLayer,
                                             srcPlane,
                                             pSrcResource->desc.image.mipLevels,
                                             pSrcResource->desc.GetImageArrayLayers());
                    dstLocation.SubresourceIndex =
                        D3D12CalcSubresource(dstMip,
                                             pDstView->subresourceRange.baseArrayLayer + iArrayLayer,
                                             dstPlane,
                                             pDstResource->desc.image.mipLevels,
                                             pDstResource->desc.GetImageArrayLayers());

                    pCmdList->CopyTextureRegion(&dstLocation,
                                                dstOffset[0],
                                                dstOffset[1],
                                                dstOffset[2],
                                                &srcLocation,
                                                isFullSubresource ? nullptr : &box);
                }

                box.left   = box.left >> 1;
                box.right  = box.right >> 1;
                box.front  = box.front >> 1;
                box.right  = box.right >> 1;
                box.bottom = box.bottom >> 1;
                box.back   = box.back >> 1;

                dstOffset[0] = dstOffset[0] >> 1;
                dstOffset[1] = dstOffset[1] >> 1;
                dstOffset[2] = dstOffset[2] >> 1;
            }
        }
    }

    static constexpr bool TextureToBuffer = true;
    static constexpr bool BufferToTexture = false;

    void D3D12BuiltInCopyBuffer(const RpsCmdCallbackContext* pContext)
    {
        auto* pCmdList = rpsD3D12CommandListFromHandle(pContext->hCommandBuffer);
        auto* pBackend = rps::D3D12RuntimeBackend::Get(pContext);

        auto* pRuntimeDevice = D3D12RuntimeDevice::Get<D3D12RuntimeDevice>(pBackend->GetRenderGraph().GetDevice());

        const ResourceInstance *pDstResource, *pSrcResource;
        RPS_V_REPORT_AND_RETURN(pContext,
                                rps::D3D12RuntimeBackend::GetCmdArgResourceInfos(pContext, 0, 0, &pDstResource, 1));
        RPS_V_REPORT_AND_RETURN(pContext,
                                rps::D3D12RuntimeBackend::GetCmdArgResourceInfos(pContext, 2, 0, &pSrcResource, 1));

        const auto* pDstView  = rpsCmdGetArg<RpsBufferView, 0>(pContext);
        uint64_t    dstOffset = *rpsCmdGetArg<uint64_t, 1>(pContext);
        const auto* pSrcView  = rpsCmdGetArg<RpsBufferView, 2>(pContext);
        uint64_t    srcOffset = *rpsCmdGetArg<uint64_t, 3>(pContext);
        uint64_t    copySize  = *rpsCmdGetArg<uint64_t, 4>(pContext);

        const uint64_t dstTotalSize = pDstResource->desc.GetBufferSize();
        const uint64_t srcTotalSize = pSrcResource->desc.GetBufferSize();

        if ((dstOffset == 0) && (srcOffset == 0) && (dstTotalSize == srcTotalSize) &&
            ((copySize == UINT64_MAX) || (copySize == srcTotalSize)))
        {
            pCmdList->CopyResource(rpsD3D12ResourceFromHandle(pDstResource->hRuntimeResource),
                                   rpsD3D12ResourceFromHandle(pSrcResource->hRuntimeResource));
        }
        else
        {
            pCmdList->CopyBufferRegion(rpsD3D12ResourceFromHandle(pDstResource->hRuntimeResource),
                                       dstOffset,
                                       rpsD3D12ResourceFromHandle(pSrcResource->hRuntimeResource),
                                       srcOffset,
                                       copySize);
        }
    }

    template <bool     SourceIsTexture,
              uint32_t BufferArgIdx,
              uint32_t TextureArgIdx,
              uint32_t BufferByteOffsetArgIdx,
              uint32_t BufferRowPitchArgIdx,
              uint32_t BufferImageSizeArgIdx,
              uint32_t BufferOffsetArgIdx,
              uint32_t TextureOffsetArgIdx,
              uint32_t ExtentArgIdx>
    void D3D12BuiltInCopyTextureBufferCommon(const RpsCmdCallbackContext* pContext)
    {
        auto* pCmdList = rpsD3D12CommandList1FromHandle(pContext->hCommandBuffer);
        auto* pBackend = rps::D3D12RuntimeBackend::Get(pContext);

        auto* pRuntimeDevice = D3D12RuntimeDevice::Get<D3D12RuntimeDevice>(pBackend->GetRenderGraph().GetDevice());

        const ResourceInstance *pTextureResource, *pBufferResource;
        RPS_V_REPORT_AND_RETURN(
            pContext,
            rps::D3D12RuntimeBackend::GetCmdArgResourceInfos(pContext, TextureArgIdx, 0, &pTextureResource, 1));
        RPS_V_REPORT_AND_RETURN(
            pContext, rps::D3D12RuntimeBackend::GetCmdArgResourceInfos(pContext, BufferArgIdx, 0, &pBufferResource, 1));

        auto* pBufferView      = rpsCmdGetArg<RpsBufferView, BufferArgIdx>(pContext);
        auto* pTextureView     = rpsCmdGetArg<RpsImageView, TextureArgIdx>(pContext);
        auto  bufferByteOffset = *rpsCmdGetArg<uint64_t, BufferByteOffsetArgIdx>(pContext);
        auto  bufferRowPitch   = *rpsCmdGetArg<uint32_t, BufferRowPitchArgIdx>(pContext);
        auto  pBufferImageSize = *rpsCmdGetArg<uint32_t[3], BufferImageSizeArgIdx>(pContext);
        auto  pBufferImgOffset = *rpsCmdGetArg<uint32_t[3], BufferOffsetArgIdx>(pContext);
        auto  pTextureOffset   = *rpsCmdGetArg<uint32_t[3], TextureOffsetArgIdx>(pContext);
        auto  pExtent          = *rpsCmdGetArg<uint32_t[3], ExtentArgIdx>(pContext);

        D3D12_TEXTURE_COPY_LOCATION textureLocation;
        textureLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

        D3D12_TEXTURE_COPY_LOCATION bufferLocation;
        bufferLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

        uint32_t texMipDim[3] = {
            GetMipLevelDimension(pTextureResource->desc.image.width, pTextureView->subresourceRange.baseMipLevel),
            GetMipLevelDimension(pTextureResource->desc.image.height, pTextureView->subresourceRange.baseMipLevel),
            GetMipLevelDimension(pTextureResource->desc.GetImageDepth(), pTextureView->subresourceRange.baseMipLevel),
        };

        const uint32_t* pSrcOffset = SourceIsTexture ? pTextureOffset : pBufferImgOffset;

        uint32_t boxEnd[3] = {
            (pExtent[0] == UINT32_MAX) ? texMipDim[0] : (pSrcOffset[0] + pExtent[0]),
            (pExtent[1] == UINT32_MAX) ? texMipDim[1] : (pSrcOffset[1] + pExtent[1]),
            (pExtent[2] == UINT32_MAX) ? texMipDim[2] : (pSrcOffset[2] + pExtent[2]),
        };

        const bool isFullSubresource =
            ((boxEnd[0] == texMipDim[0]) && (boxEnd[1] == texMipDim[1]) && (boxEnd[2] == texMipDim[2]));
        const bool onlyAllowFullSubresource = (pTextureResource->allAccesses.accessFlags & RPS_ACCESS_DEPTH_STENCIL) ||
                                              (pTextureResource->desc.GetSampleCount() > 1);

        const RpsFormat viewFormat = (pTextureView->base.viewFormat != RPS_FORMAT_UNKNOWN)
                                         ? pTextureView->base.viewFormat
                                         : pTextureResource->desc.image.format;
        const uint32_t  planeMask  = pRuntimeDevice->GetFormatPlaneMask(viewFormat);

        RPS_ASSERT((planeMask == 1) || (planeMask == 2));

        textureLocation.pResource        = rpsD3D12ResourceFromHandle(pTextureResource->hRuntimeResource);
        textureLocation.SubresourceIndex = D3D12CalcSubresource(pTextureView->subresourceRange.baseMipLevel,
                                                                pTextureView->subresourceRange.baseArrayLayer,
                                                                ((planeMask == 0x1) ? 0 : 1),
                                                                pTextureResource->desc.image.mipLevels,
                                                                pTextureResource->desc.GetImageArrayLayers());

        bufferLocation.pResource                        = rpsD3D12ResourceFromHandle(pBufferResource->hRuntimeResource);
        bufferLocation.PlacedFootprint.Offset           = bufferByteOffset;
        bufferLocation.PlacedFootprint.Footprint.Format = rpsFormatToDXGI(viewFormat);
        bufferLocation.PlacedFootprint.Footprint.Width  = pBufferImageSize[0];
        bufferLocation.PlacedFootprint.Footprint.Height = pBufferImageSize[1];
        bufferLocation.PlacedFootprint.Footprint.Depth  = pBufferImageSize[2];
        bufferLocation.PlacedFootprint.Footprint.RowPitch = bufferRowPitch;

        if (SourceIsTexture)
        {
            D3D12_BOX box = {pTextureOffset[0], pTextureOffset[1], pTextureOffset[2], boxEnd[0], boxEnd[1], boxEnd[2]};

            pCmdList->CopyTextureRegion(&bufferLocation,
                                        pBufferImgOffset[0],
                                        pBufferImgOffset[1],
                                        pBufferImgOffset[2],
                                        &textureLocation,
                                        isFullSubresource ? nullptr : &box);
        }
        else
        {
            D3D12_BOX box = {
                pBufferImgOffset[0], pBufferImgOffset[1], pBufferImgOffset[2], boxEnd[0], boxEnd[1], boxEnd[2]};

            pCmdList->CopyTextureRegion(&textureLocation,
                                        pTextureOffset[0],
                                        pTextureOffset[1],
                                        pTextureOffset[2],
                                        &bufferLocation,
                                        isFullSubresource ? nullptr : &box);
        }
    }

    // copy     node copy_texture_to_buffer ( [writeonly(copy)] buffer dst, uint64_t dstByteOffset, uint rowPitch, uint3 bufferImageSize, uint3 dstOffset, [readonly(copy)] texture src, uint3 srcOffset, uint3 extent );
    // copy     node copy_buffer_to_texture ( [writeonly(copy)] texture dst, uint3 dstOffset, [readonly(copy)] buffer src, uint64_t srcByteOffset, uint rowPitch, uint3 bufferImageSize, uint3 srcOffset, uint3 extent );
    // graphics node resolve                ( [writeonly(resolve)] texture dst, uint2 dstOffset, [readonly(resolve)] texture src, uint2 srcOffset, uint2 extent, RPS_RESOLVE_MODE resolveMode );
    void D3D12BuiltInCopyTextureToBuffer(const RpsCmdCallbackContext* pContext)
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

        D3D12BuiltInCopyTextureBufferCommon<TextureToBuffer,
                                            BUFFER_DST,
                                            TEXTURE_SRC,
                                            BUFFER_BYTE_OFFSET,
                                            ROW_PITCH,
                                            BUFFER_IMAGE_SIZE,
                                            BUFFER_IMAGE_OFFSET,
                                            TEXTURE_OFFSET,
                                            EXTENT>(pContext);
    }
    void D3D12BuiltInCopyBufferToTexture(const RpsCmdCallbackContext* pContext)
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

        D3D12BuiltInCopyTextureBufferCommon<BufferToTexture,
                                            BUFFER_SRC,
                                            TEXTURE_DST,
                                            BUFFER_BYTE_OFFSET,
                                            ROW_PITCH,
                                            BUFFER_IMAGE_SIZE,
                                            BUFFER_IMAGE_OFFSET,
                                            TEXTURE_OFFSET,
                                            EXTENT>(pContext);
    }

    void D3D12BuiltInResolve(const RpsCmdCallbackContext* pContext)
    {
        auto* pCmdList = rpsD3D12CommandList1FromHandle(pContext->hCommandBuffer);

        auto* pBackend = rps::D3D12RuntimeBackend::Get(pContext);

        auto* pRuntimeDevice = D3D12RuntimeDevice::Get<D3D12RuntimeDevice>(pBackend->GetRenderGraph().GetDevice());

        RPS_ASSERT(pContext->numArgs == 6);

        const ResourceInstance *pDstResource, *pSrcResource;
        RPS_V_REPORT_AND_RETURN(pContext,
                                rps::D3D12RuntimeBackend::GetCmdArgResourceInfos(pContext, 0, 0, &pDstResource, 1));
        RPS_V_REPORT_AND_RETURN(pContext,
                                rps::D3D12RuntimeBackend::GetCmdArgResourceInfos(pContext, 2, 0, &pSrcResource, 1));

        auto pDstView    = rpsCmdGetArg<RpsImageView, 0>(pContext);
        auto pDstOffset  = *rpsCmdGetArg<uint32_t[2], 1>(pContext);
        auto pSrcView    = rpsCmdGetArg<RpsImageView, 2>(pContext);
        auto pSrcOffset  = *rpsCmdGetArg<uint32_t[2], 3>(pContext);
        auto pExtent     = *rpsCmdGetArg<uint32_t[2], 4>(pContext);
        auto resolveMode = *rpsCmdGetArg<RpsResolveMode, 5>(pContext);

        RPS_ASSERT(!pDstResource->desc.IsBuffer());
        RPS_ASSERT(!pSrcResource->desc.IsBuffer());

        // TODO: Precalculate these flags at shader loading time.
        const bool isFullSubresource = ((pDstOffset[0] == 0) && (pDstOffset[1] == 0)) &&
                                       ((pSrcOffset[0] == 0) && (pSrcOffset[1] == 0)) &&
                                       ((pExtent[0] == UINT32_MAX) && (pExtent[1] == UINT32_MAX));

        if (pSrcResource->desc.GetSampleCount() < pDstResource->desc.GetSampleCount())
        {
            rpsCmdCallbackReportError(pContext, RPS_ERROR_INVALID_OPERATION);
            return;
        }

        RpsFormat srcFmt = (pSrcView->base.viewFormat != RPS_FORMAT_UNKNOWN) ? pSrcView->base.viewFormat
                                                                             : pSrcResource->desc.image.format;
        RpsFormat dstFmt = (pDstView->base.viewFormat != RPS_FORMAT_UNKNOWN) ? pDstView->base.viewFormat
                                                                             : pDstResource->desc.image.format;

        uint32_t srcAspectMask = pRuntimeDevice->GetFormatPlaneMask(srcFmt);
        uint32_t dstAspectMask = pRuntimeDevice->GetFormatPlaneMask(dstFmt);

        const uint32_t mipLevelCount =
            (pSrcResource->desc.GetSampleCount() > 1)
                ? 1
                : rpsMin(pDstView->subresourceRange.mipLevels, pDstView->subresourceRange.mipLevels);
        const uint32_t arrayLayerCount =
            rpsMin(pDstView->subresourceRange.arrayLayers, pSrcView->subresourceRange.arrayLayers);

        while ((srcAspectMask != 0) && (dstAspectMask != 0))
        {
            const uint32_t srcPlane = (srcAspectMask & 1u) ? 0 : 1;
            srcAspectMask &= ~(1 << srcPlane);
            const uint32_t dstPlane = (dstAspectMask & 1u) ? 0 : 1;
            dstAspectMask &= ~(1 << dstPlane);

            for (uint32_t iMip = 0; iMip < mipLevelCount; iMip++)
            {
                const uint32_t srcMip = pSrcView->subresourceRange.baseMipLevel + iMip;
                const uint32_t dstMip = pDstView->subresourceRange.baseMipLevel + iMip;

                D3D12_RECT srcRect;
                srcRect.left   = pSrcOffset[0] >> iMip;
                srcRect.top    = pSrcOffset[1] >> iMip;
                srcRect.right  = rpsMax(1u, (pSrcOffset[0] + pExtent[0]) >> iMip);
                srcRect.bottom = rpsMax(1u, (pSrcOffset[1] + pExtent[1]) >> iMip);

                UINT dstOffset[2] = {
                    (pDstOffset[0] >> iMip),
                    (pDstOffset[1] >> iMip),
                };

                for (uint32_t iArrayLayer = 0; iArrayLayer < arrayLayerCount; iArrayLayer++)
                {
                    const uint32_t srcSubresourceIndex =
                        D3D12CalcSubresource(srcMip,
                                             pSrcView->subresourceRange.baseArrayLayer + iArrayLayer,
                                             srcPlane,
                                             pSrcResource->desc.image.mipLevels,
                                             pSrcResource->desc.GetImageArrayLayers());

                    const uint32_t dstSubresourceIndex =
                        D3D12CalcSubresource(dstMip,
                                             pDstView->subresourceRange.baseArrayLayer + iArrayLayer,
                                             dstPlane,
                                             pDstResource->desc.image.mipLevels,
                                             pDstResource->desc.GetImageArrayLayers());

                    pCmdList->ResolveSubresourceRegion(rpsD3D12ResourceFromHandle(pDstResource->hRuntimeResource),
                                                       dstSubresourceIndex,
                                                       dstOffset[0],
                                                       dstOffset[1],
                                                       rpsD3D12ResourceFromHandle(pSrcResource->hRuntimeResource),
                                                       srcSubresourceIndex,
                                                       isFullSubresource ? NULL : &srcRect,
                                                       rpsFormatToDXGI(dstFmt),
                                                       D3D12GetResolveMode(resolveMode));
                }
            }
        }
    }
}  // namespace rps
