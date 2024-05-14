// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "rps/runtime/d3d_common/rps_d3d_common.h"
#include "rps/runtime/common/rps_render_states.h"

#include "runtime/common/rps_runtime_util.hpp"

#include "runtime/d3d11/rps_d3d11_runtime_device.hpp"
#include "runtime/d3d11/rps_d3d11_runtime_backend.hpp"
#include "runtime/d3d11/rps_d3d11_util.hpp"

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
    void D3D11BuiltInClearColorImpl(const RpsCmdCallbackContext* pContext)
    {
    }

    void D3D11BuiltInClearColorRegions(const RpsCmdCallbackContext* pContext)
    {
        ID3D11DeviceContext*               pD3DDC = rpsD3D11DeviceContextFromHandle(pContext->hCommandBuffer);
        ScopedComPtr<ID3D11DeviceContext1> pD3DDC1;

        // TODO
        HRESULT hr = pD3DDC->QueryInterface(pD3DDC1.ReleaseAndGetAddressOf());

        RPS_ASSERT(pContext->numArgs == 4);

        ID3D11RenderTargetView* pRTV;
        RPS_V_REPORT_AND_RETURN(pContext, rpsD3D11GetCmdArgRTV(pContext, 0, &pRTV));

        auto              pClearValue = rpsCmdGetArg<RpsClearValue, 1>(pContext);
        uint32_t          numRects    = *rpsCmdGetArg<uint32_t, 2>(pContext);
        const D3D11_RECT* pRects      = rpsCmdGetArg<D3D11_RECT, 3>(pContext);

        static_assert(sizeof(RpsRect) == sizeof(D3D11_RECT),
                      "Assumption 'sizeof(RpsRect) == sizeof(D3D11_RECT)' is no longer true.");

        pD3DDC1->ClearView(pRTV, pClearValue->color.float32, pRects, numRects);
    }

    void D3D11BuiltInClearColor(const RpsCmdCallbackContext* pContext)
    {
        ID3D11DeviceContext* pD3DDC = rpsD3D11DeviceContextFromHandle(pContext->hCommandBuffer);

        RPS_ASSERT(pContext->numArgs == 2);

        ID3D11RenderTargetView* pRTV;
        rpsD3D11GetCmdArgRTV(pContext, 0, &pRTV);

        auto pClearValue = rpsCmdGetArg<RpsClearValue, 1>(pContext);

        static_assert(sizeof(RpsRect) == sizeof(D3D11_RECT),
                      "Assumption 'sizeof(RpsRect) == sizeof(D3D11_RECT)' is no longer true.");

        pD3DDC->ClearRenderTargetView(pRTV, pClearValue->color.float32);
    }

    void D3D11BuiltInClearDepthStencil(const RpsCmdCallbackContext* pContext)
    {
        ID3D11DeviceContext* pD3DDC = rpsD3D11DeviceContextFromHandle(pContext->hCommandBuffer);

        RPS_ASSERT(pContext->numArgs == 4);

        auto pClearFlags   = rpsCmdGetArg<RpsClearFlags, 1>(pContext);
        auto pDepthValue   = rpsCmdGetArg<float, 2>(pContext);
        auto pStencilValue = rpsCmdGetArg<uint32_t, 3>(pContext);

        uint32_t          numRects = 0;
        const D3D11_RECT* pRects   = nullptr;

        static_assert(sizeof(RpsRect) == sizeof(D3D11_RECT),
                      "Assumption 'sizeof(RpsRect) == sizeof(D3D11_RECT)' is no longer true.");

        ID3D11DepthStencilView* pDSV;
        RPS_V_REPORT_AND_RETURN(pContext, rpsD3D11GetCmdArgDSV(pContext, 0, &pDSV));

        const uint32_t d3dClearFlags = (((*pClearFlags) & RPS_CLEAR_FLAG_DEPTH) ? D3D11_CLEAR_DEPTH : 0) |
                                       (((*pClearFlags) & RPS_CLEAR_FLAG_STENCIL) ? D3D11_CLEAR_STENCIL : 0);

        pD3DDC->ClearDepthStencilView(pDSV, d3dClearFlags, *pDepthValue, *pStencilValue);
    }

    void D3D11BuiltInClearDepthStencilRegions(const RpsCmdCallbackContext* pContext)
    {
        ID3D11DeviceContext*               pD3DDC = rpsD3D11DeviceContextFromHandle(pContext->hCommandBuffer);
        ScopedComPtr<ID3D11DeviceContext1> pD3DDC1;

        // TODO
        HRESULT hr = pD3DDC->QueryInterface(pD3DDC1.ReleaseAndGetAddressOf());

        RPS_ASSERT(pContext->numArgs == 6);

        auto              pClearFlags   = rpsCmdGetArg<RpsClearFlags, 1>(pContext);
        auto              pDepthValue   = rpsCmdGetArg<float, 2>(pContext);
        auto              pStencilValue = rpsCmdGetArg<uint32_t, 3>(pContext);
        uint32_t          numRects      = *rpsCmdGetArg<uint32_t, 5>(pContext);
        const D3D11_RECT* pRects        = rpsCmdGetArg<D3D11_RECT, 6>(pContext);

        static_assert(sizeof(RpsRect) == sizeof(D3D11_RECT),
                      "Assumption 'sizeof(RpsRect) == sizeof(D3D11_RECT)' is no longer true.");

        ID3D11DepthStencilView* pDSV;
        RPS_V_REPORT_AND_RETURN(pContext, rpsD3D11GetCmdArgDSV(pContext, 0, &pDSV));

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        pDSV->GetDesc(&dsvDesc);

        if (((*pClearFlags) & RPS_CLEAR_FLAG_STENCIL) || (dsvDesc.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT) ||
            (dsvDesc.Format == DXGI_FORMAT_D24_UNORM_S8_UINT))
        {
            RPS_V_REPORT_AND_RETURN(pContext, RPS_ERROR_NOT_SUPPORTED);
        }

        float depth          = *pDepthValue;
        float clearValues[4] = {depth, depth, depth, depth};

        pD3DDC1->ClearView(pDSV, clearValues, pRects, numRects);
    }

    void D3D11BuiltInClearTextureUAV(const RpsCmdCallbackContext* pContext)
    {
        RPS_TODO();
    }
    void D3D11BuiltInClearTextureUAVRegions(const RpsCmdCallbackContext* pContext)
    {
        RPS_TODO();
    }
    void D3D11BuiltInClearBufferUAV(const RpsCmdCallbackContext* pContext)
    {
        RPS_TODO();
    }

    // copy     node copy_texture           ( [writeonly(copy)] texture dst, uint3 dstOffset, [readonly(copy)] texture src, uint3 srcOffset, uint3 extent );
    void D3D11BuiltInCopyTexture(const RpsCmdCallbackContext* pContext)
    {
        ID3D11DeviceContext* pD3DDC = rpsD3D11DeviceContextFromHandle(pContext->hCommandBuffer);

        auto* pBackend = rps::D3D11RuntimeBackend::Get(pContext);

        auto* pRuntimeDevice = D3D11RuntimeDevice::Get<D3D11RuntimeDevice>(pBackend->GetRenderGraph().GetDevice());

        RPS_ASSERT(pContext->numArgs == 5);

        const ResourceInstance *pDstResource, *pSrcResource;
        RPS_V_REPORT_AND_RETURN(pContext,
                                rps::D3D11RuntimeBackend::GetCmdArgResourceInfos(pContext, 0, 0, &pDstResource, 1));
        RPS_V_REPORT_AND_RETURN(pContext,
                                rps::D3D11RuntimeBackend::GetCmdArgResourceInfos(pContext, 2, 0, &pSrcResource, 1));

        auto pDstView   = rpsCmdGetArg<RpsImageView, 0>(pContext);
        auto pDstOffset = *rpsCmdGetArg<uint32_t[3], 1>(pContext);
        auto pSrcView   = rpsCmdGetArg<RpsImageView, 2>(pContext);
        auto pSrcOffset = *rpsCmdGetArg<uint32_t[3], 3>(pContext);
        auto pExtent    = *rpsCmdGetArg<uint32_t[3], 4>(pContext);

        ID3D11Resource* pDstD3DResource = rpsD3D11ResourceFromHandle(pDstResource->hRuntimeResource);
        ID3D11Resource* pSrcD3DResource = rpsD3D11ResourceFromHandle(pSrcResource->hRuntimeResource);

        RpsFormat srcFmt = (pSrcView->base.viewFormat != RPS_FORMAT_UNKNOWN) ? pSrcView->base.viewFormat
                                                                             : pSrcResource->desc.image.format;
        RpsFormat dstFmt = (pDstView->base.viewFormat != RPS_FORMAT_UNKNOWN) ? pDstView->base.viewFormat
                                                                             : pDstResource->desc.image.format;

        uint32_t srcMipDim[3] = {
            GetMipLevelDimension(pSrcResource->desc.image.width, pSrcView->subresourceRange.baseMipLevel),
            GetMipLevelDimension(pSrcResource->desc.image.height, pSrcView->subresourceRange.baseMipLevel),
            GetMipLevelDimension(pSrcResource->desc.GetImageDepth(), pSrcView->subresourceRange.baseMipLevel),
        };

        D3D11_BOX box;
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

        const uint32_t mipLevels = rpsMin(pSrcView->subresourceRange.mipLevels, pDstView->subresourceRange.mipLevels);

        const uint32_t arrayLayers =
            rpsMin(pSrcView->subresourceRange.arrayLayers, pDstView->subresourceRange.arrayLayers);

        for (uint32_t iMip = 0; iMip < mipLevels; iMip++)
        {
            const uint32_t srcMip = pSrcView->subresourceRange.baseMipLevel + iMip;
            const uint32_t dstMip = pDstView->subresourceRange.baseMipLevel + iMip;

            for (uint32_t iArrayLayer = 0; iArrayLayer < arrayLayers; iArrayLayer++)
            {
                uint32_t srcSubresourceIndex =
                    D3D11CalcSubresource(srcMip,
                                         pSrcView->subresourceRange.baseArrayLayer + iArrayLayer,
                                         pSrcResource->desc.image.mipLevels);
                uint32_t dstSubresourceIndex =
                    D3D11CalcSubresource(dstMip,
                                         pDstView->subresourceRange.baseArrayLayer + iArrayLayer,
                                         pDstResource->desc.image.mipLevels);

                pD3DDC->CopySubresourceRegion(pDstD3DResource,
                                              dstSubresourceIndex,
                                              dstOffset[0],
                                              dstOffset[1],
                                              dstOffset[2],
                                              pSrcD3DResource,
                                              srcSubresourceIndex,
                                              &box);
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

    static constexpr bool TextureToBuffer = true;
    static constexpr bool BufferToTexture = false;

    void D3D11BuiltInCopyBuffer(const RpsCmdCallbackContext* pContext)
    {
        ID3D11DeviceContext* pD3DDC = rpsD3D11DeviceContextFromHandle(pContext->hCommandBuffer);

        auto* pBackend       = rps::D3D11RuntimeBackend::Get(pContext);
        auto* pRuntimeDevice = D3D11RuntimeDevice::Get<D3D11RuntimeDevice>(pBackend->GetRenderGraph().GetDevice());

        const ResourceInstance *pDstResource, *pSrcResource;
        RPS_V_REPORT_AND_RETURN(pContext,
                                rps::D3D11RuntimeBackend::GetCmdArgResourceInfos(pContext, 0, 0, &pDstResource, 1));
        RPS_V_REPORT_AND_RETURN(pContext,
                                rps::D3D11RuntimeBackend::GetCmdArgResourceInfos(pContext, 2, 0, &pSrcResource, 1));

        const auto* pDstView  = rpsCmdGetArg<RpsBufferView, 0>(pContext);
        uint64_t    dstOffset = *rpsCmdGetArg<uint64_t, 1>(pContext);
        const auto* pSrcView  = rpsCmdGetArg<RpsBufferView, 2>(pContext);
        uint64_t    srcOffset = *rpsCmdGetArg<uint64_t, 3>(pContext);
        uint64_t    copySize  = *rpsCmdGetArg<uint64_t, 4>(pContext);

        if ((srcOffset > UINT32_MAX) || (dstOffset > UINT32_MAX))
        {
            RPS_V_REPORT_AND_RETURN(pContext, RPS_ERROR_NOT_SUPPORTED);
        }

        const uint64_t dstTotalSize = pDstResource->desc.GetBufferSize();
        const uint64_t srcTotalSize = pSrcResource->desc.GetBufferSize();

        if ((dstOffset == 0) && (srcOffset == 0) && (dstTotalSize == srcTotalSize) &&
            ((copySize == UINT64_MAX) || (copySize == srcTotalSize)))
        {
            pD3DDC->CopyResource(rpsD3D11ResourceFromHandle(pDstResource->hRuntimeResource),
                                 rpsD3D11ResourceFromHandle(pSrcResource->hRuntimeResource));
        }
        else
        {
            D3D11_BOX box = {};
            box.left      = UINT(srcOffset);
            box.top       = 0;
            box.front     = 0;
            box.right     = (copySize == UINT64_MAX) ? uint32_t(srcTotalSize - srcOffset) : uint32_t(copySize);
            box.bottom    = 1;
            box.back      = 1;

            pD3DDC->CopySubresourceRegion(rpsD3D11ResourceFromHandle(pDstResource->hRuntimeResource),
                                          0,
                                          UINT(dstOffset),
                                          1,
                                          1,
                                          rpsD3D11ResourceFromHandle(pSrcResource->hRuntimeResource),
                                          0,
                                          &box);
        }
    }

    // copy     node copy_texture_to_buffer ( [writeonly(copy)] buffer dst, uint64_t dstByteOffset, uint rowPitch, uint3 bufferImageSize, uint3 dstOffset, [readonly(copy)] texture src, uint3 srcOffset, uint3 extent );
    // copy     node copy_buffer_to_texture ( [writeonly(copy)] texture dst, uint3 dstOffset, [readonly(copy)] buffer src, uint64_t srcByteOffset, uint rowPitch, uint3 bufferImageSize, uint3 srcOffset, uint3 extent );
    // graphics node resolve                ( [writeonly(resolve)] texture dst, uint2 dstOffset, [readonly(resolve)] texture src, uint2 srcOffset, uint2 extent, RPS_RESOLVE_MODE resolveMode );
    void D3D11BuiltInCopyTextureToBuffer(const RpsCmdCallbackContext* pContext)
    {
        RPS_V_REPORT_AND_RETURN(pContext, RPS_ERROR_NOT_IMPLEMENTED);
    }

    void D3D11BuiltInCopyBufferToTexture(const RpsCmdCallbackContext* pContext)
    {
        RPS_V_REPORT_AND_RETURN(pContext, RPS_ERROR_NOT_IMPLEMENTED);
    }

    void D3D11BuiltInResolve(const RpsCmdCallbackContext* pContext)
    {
        ID3D11DeviceContext* pD3DDC = rpsD3D11DeviceContextFromHandle(pContext->hCommandBuffer);

        auto* pBackend = rps::D3D11RuntimeBackend::Get(pContext);

        auto* pRuntimeDevice = D3D11RuntimeDevice::Get<D3D11RuntimeDevice>(pBackend->GetRenderGraph().GetDevice());

        RPS_ASSERT(pContext->numArgs == 6);

        const ResourceInstance *pDstResource, *pSrcResource;
        RPS_V_REPORT_AND_RETURN(pContext,
                                rps::D3D11RuntimeBackend::GetCmdArgResourceInfos(pContext, 0, 0, &pDstResource, 1));
        RPS_V_REPORT_AND_RETURN(pContext,
                                rps::D3D11RuntimeBackend::GetCmdArgResourceInfos(pContext, 2, 0, &pSrcResource, 1));

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

        if (!isFullSubresource || (resolveMode != RPS_RESOLVE_MODE_AVERAGE))
        {
            RPS_V_REPORT_AND_RETURN(pContext, RPS_ERROR_NOT_SUPPORTED);
        }

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

        const uint32_t mipLevels =
            (pSrcResource->desc.GetSampleCount() > 1)
                ? 1
                : rpsMin(pDstView->subresourceRange.mipLevels, pDstView->subresourceRange.mipLevels);
        const uint32_t arrayLayers =
            rpsMin(pDstView->subresourceRange.arrayLayers, pSrcView->subresourceRange.arrayLayers);

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

                D3D11_RECT srcRect;
                srcRect.left   = pSrcOffset[0] >> iMip;
                srcRect.top    = pSrcOffset[1] >> iMip;
                srcRect.right  = rpsMax(1u, (pSrcOffset[0] + pExtent[0]) >> iMip);
                srcRect.bottom = rpsMax(1u, (pSrcOffset[1] + pExtent[1]) >> iMip);

                UINT dstOffset[2] = {
                    (pDstOffset[0] >> iMip),
                    (pDstOffset[1] >> iMip),
                };

                for (uint32_t iArrayLayer = 0; iArrayLayer < arrayLayers; iArrayLayer++)
                {
                    const uint32_t srcSubresourceIndex =
                        D3D11CalcSubresource(srcMip,
                                             pSrcView->subresourceRange.baseArrayLayer + iArrayLayer,
                                             pSrcResource->desc.image.mipLevels);

                    const uint32_t dstSubresourceIndex =
                        D3D11CalcSubresource(dstMip,
                                             pDstView->subresourceRange.baseArrayLayer + iArrayLayer,
                                             pDstResource->desc.image.mipLevels);

                    pD3DDC->ResolveSubresource(rpsD3D11ResourceFromHandle(pDstResource->hRuntimeResource),
                                               dstSubresourceIndex,
                                               rpsD3D11ResourceFromHandle(pSrcResource->hRuntimeResource),
                                               srcSubresourceIndex,
                                               rpsFormatToDXGI(dstFmt));
                }
            }
        }
    }
}  // namespace rps
