// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "runtime/d3d11/rps_d3d11_runtime_backend.hpp"

#include "runtime/common/rps_runtime_util.hpp"

namespace rps
{

    RpsFormat GetD3D11SRVFormat(const CmdAccessInfo& accessInfo)
    {
        RpsFormat viewFormat = accessInfo.viewFormat;

        switch (viewFormat)
        {
        case RPS_FORMAT_D32_FLOAT:
            viewFormat = RPS_FORMAT_R32_FLOAT;
            break;
        case RPS_FORMAT_D16_UNORM:
            viewFormat = RPS_FORMAT_R16_UNORM;
            break;
        case RPS_FORMAT_D24_UNORM_S8_UINT:
            viewFormat = RPS_FORMAT_R24_UNORM_X8_TYPELESS;
            break;
        case RPS_FORMAT_D32_FLOAT_S8X24_UINT:
            viewFormat = RPS_FORMAT_R32_FLOAT_X8X24_TYPELESS;
            break;
        default:
            break;
        }

        return viewFormat;
    }

    RpsResult InitD3D11RTVDesc(D3D11_RENDER_TARGET_VIEW_DESC* pRTVDesc,
                               const CmdAccessInfo&           accessInfo,
                               const ResourceInstance&        resource)
    {
        const auto& resDesc = resource.desc;

        RPS_ASSERT(resDesc.IsImage() || (accessInfo.viewFormat != RPS_FORMAT_UNKNOWN));

        pRTVDesc->Format = rpsFormatToDXGI(accessInfo.viewFormat);

        RPS_ASSERT(rpsCountBits(accessInfo.range.aspectMask) == 1);

        if (resource.desc.type == RPS_RESOURCE_TYPE_IMAGE_2D)
        {
            if (resource.desc.image.arrayLayers <= 1)
            {
                if (resource.desc.image.sampleCount <= 1)
                {
                    pRTVDesc->ViewDimension      = D3D11_RTV_DIMENSION_TEXTURE2D;
                    pRTVDesc->Texture2D.MipSlice = accessInfo.range.baseMipLevel;
                }
                else
                {
                    pRTVDesc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
                }
            }
            else
            {
                if (resDesc.image.sampleCount <= 1)
                {
                    pRTVDesc->ViewDimension                  = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                    pRTVDesc->Texture2DArray.MipSlice        = accessInfo.range.baseMipLevel;
                    pRTVDesc->Texture2DArray.FirstArraySlice = accessInfo.range.baseArrayLayer;
                    pRTVDesc->Texture2DArray.ArraySize       = accessInfo.range.GetArrayLayerCount();
                }
                else
                {
                    pRTVDesc->ViewDimension                    = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
                    pRTVDesc->Texture2DMSArray.FirstArraySlice = accessInfo.range.baseArrayLayer;
                    pRTVDesc->Texture2DMSArray.ArraySize       = accessInfo.range.GetArrayLayerCount();
                }
            }
        }
        else if (resDesc.type == RPS_RESOURCE_TYPE_IMAGE_3D)
        {
            //TODO: No WSlice info here. Using full-resource for now

            pRTVDesc->ViewDimension         = D3D11_RTV_DIMENSION_TEXTURE3D;
            pRTVDesc->Texture3D.MipSlice    = accessInfo.range.baseMipLevel;
            pRTVDesc->Texture3D.FirstWSlice = 0;
            pRTVDesc->Texture3D.WSize       = resDesc.image.depth;
        }
        else if (resDesc.type == RPS_RESOURCE_TYPE_IMAGE_1D)
        {
            if (resDesc.image.arrayLayers <= 1)
            {
                pRTVDesc->ViewDimension      = D3D11_RTV_DIMENSION_TEXTURE1D;
                pRTVDesc->Texture1D.MipSlice = accessInfo.range.baseMipLevel;
            }
            else
            {
                pRTVDesc->ViewDimension                  = D3D11_RTV_DIMENSION_TEXTURE1DARRAY;
                pRTVDesc->Texture1DArray.MipSlice        = accessInfo.range.baseMipLevel;
                pRTVDesc->Texture1DArray.FirstArraySlice = accessInfo.range.baseArrayLayer;
                pRTVDesc->Texture1DArray.ArraySize       = accessInfo.range.GetArrayLayerCount();
            }
        }
        else if (resDesc.type == RPS_RESOURCE_TYPE_BUFFER)
        {
            auto           pBufView    = reinterpret_cast<const RpsBufferView*>(accessInfo.pViewInfo);
            const uint32_t elementSize = rpsGetFormatElementBytes(accessInfo.viewFormat);
            RPS_RETURN_ERROR_IF(elementSize == 0, RPS_ERROR_INVALID_ARGUMENTS);

            const uint64_t bufViewBytes = GetBufferViewBytes(pBufView, resource.desc);
            const uint64_t numElements  = bufViewBytes / elementSize;
            const uint64_t firstElement = pBufView->offset / elementSize;
            RPS_RETURN_ERROR_IF(numElements > UINT32_MAX, RPS_ERROR_INTEGER_OVERFLOW);
            RPS_RETURN_ERROR_IF(firstElement > UINT32_MAX, RPS_ERROR_INTEGER_OVERFLOW);

            pRTVDesc->Buffer.FirstElement = uint32_t(firstElement);
            pRTVDesc->Buffer.NumElements  = uint32_t(numElements);
        }
        else
        {
            return RPS_ERROR_INVALID_OPERATION;
        }

        return RPS_OK;
    }

    RpsResult InitD3D11DSVDesc(D3D11_DEPTH_STENCIL_VIEW_DESC* pDSVDesc,
                               const CmdAccessInfo&           accessInfo,
                               const ResourceInstance&        resource)
    {
        RPS_RETURN_ERROR_IF(!resource.desc.IsImage(), RPS_ERROR_INVALID_OPERATION);

        // TODO: Add actual view Format info to Access.
        const RpsFormat viewFormat = accessInfo.viewFormat;
        pDSVDesc->Format           = rpsFormatToDXGI(viewFormat);

        pDSVDesc->Flags = 0;

        if ((accessInfo.access.accessFlags & RPS_ACCESS_DEPTH_READ_BIT) &&
            !(accessInfo.access.accessFlags & RPS_ACCESS_DEPTH_WRITE_BIT))
        {
            pDSVDesc->Flags |= D3D11_DSV_READ_ONLY_DEPTH;
        }

        if (rpsFormatHasStencil(viewFormat) && (accessInfo.access.accessFlags & RPS_ACCESS_STENCIL_READ_BIT) &&
            !(accessInfo.access.accessFlags & RPS_ACCESS_STENCIL_WRITE_BIT))
        {
            pDSVDesc->Flags |= D3D11_DSV_READ_ONLY_STENCIL;
        }

        if (resource.desc.type == RPS_RESOURCE_TYPE_IMAGE_2D)
        {
            if (resource.desc.image.arrayLayers <= 1)
            {
                if (resource.desc.image.sampleCount <= 1)
                {
                    pDSVDesc->ViewDimension      = D3D11_DSV_DIMENSION_TEXTURE2D;
                    pDSVDesc->Texture2D.MipSlice = accessInfo.range.baseMipLevel;
                }
                else
                {
                    pDSVDesc->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
                }
            }
            else
            {
                if (resource.desc.image.sampleCount <= 1)
                {
                    pDSVDesc->ViewDimension                  = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
                    pDSVDesc->Texture2DArray.MipSlice        = accessInfo.range.baseMipLevel;
                    pDSVDesc->Texture2DArray.FirstArraySlice = accessInfo.range.baseArrayLayer;
                    pDSVDesc->Texture2DArray.ArraySize       = accessInfo.range.GetArrayLayerCount();
                }
                else
                {
                    pDSVDesc->ViewDimension                    = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
                    pDSVDesc->Texture2DMSArray.FirstArraySlice = accessInfo.range.baseArrayLayer;
                    pDSVDesc->Texture2DMSArray.ArraySize       = accessInfo.range.GetArrayLayerCount();
                }
            }
        }
        else if (resource.desc.type == RPS_RESOURCE_TYPE_IMAGE_1D)
        {
            if (resource.desc.image.arrayLayers <= 1)
            {
                pDSVDesc->ViewDimension      = D3D11_DSV_DIMENSION_TEXTURE1D;
                pDSVDesc->Texture1D.MipSlice = accessInfo.range.baseMipLevel;
            }
            else
            {
                pDSVDesc->ViewDimension                  = D3D11_DSV_DIMENSION_TEXTURE1DARRAY;
                pDSVDesc->Texture1DArray.MipSlice        = accessInfo.range.baseMipLevel;
                pDSVDesc->Texture1DArray.FirstArraySlice = accessInfo.range.baseArrayLayer;
                pDSVDesc->Texture1DArray.ArraySize       = accessInfo.range.GetArrayLayerCount();
            }
        }
        else
        {
            return RPS_ERROR_INVALID_OPERATION;
        }

        return RPS_OK;
    }

    RpsResult InitD3D11SRVDesc(D3D11RuntimeDevice&              device,
                               D3D11_SHADER_RESOURCE_VIEW_DESC* pSRVDesc,
                               const CmdAccessInfo&             accessInfo,
                               const ResourceInstance&          resource)
    {
        RPS_RETURN_ERROR_IF(!IsResourceTypeValid(resource.desc.type), RPS_ERROR_INVALID_OPERATION);

        pSRVDesc->Format = rpsFormatToDXGI(GetD3D11SRVFormat(accessInfo));

        if (resource.desc.IsBuffer())
        {
            auto pBufView = reinterpret_cast<const RpsBufferView*>(accessInfo.pViewInfo);

            if (!(accessInfo.access.accessFlags & RPS_ACCESS_RAYTRACING_AS_READ_BIT))
            {
                uint32_t elementSize = rpsGetFormatElementBytes(accessInfo.viewFormat);

                if (elementSize == 0)
                {
                    elementSize = pBufView->stride;
                }
                if (elementSize == 0)
                {
                    RPS_ASSERT(accessInfo.viewFormat == RPS_FORMAT_UNKNOWN);
                    elementSize = 4;  //TODO: RAW
                }

                const uint64_t bufViewBytes = GetBufferViewBytes(pBufView, resource.desc);
                const uint64_t numElements  = bufViewBytes / elementSize;
                const uint64_t firstElement = pBufView->offset / elementSize;
                RPS_RETURN_ERROR_IF(firstElement > UINT32_MAX, RPS_ERROR_INTEGER_OVERFLOW);
                RPS_RETURN_ERROR_IF(numElements > UINT32_MAX, RPS_ERROR_INTEGER_OVERFLOW);

                pSRVDesc->ViewDimension         = D3D11_SRV_DIMENSION_BUFFEREX;
                pSRVDesc->BufferEx.FirstElement = uint32_t(firstElement);
                pSRVDesc->BufferEx.NumElements  = uint32_t(numElements);

                if (((accessInfo.viewFormat == RPS_FORMAT_UNKNOWN) ||
                     (accessInfo.viewFormat == RPS_FORMAT_R32_TYPELESS)) &&
                    (pBufView->stride == 0))
                {
                    pSRVDesc->BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
                    pSRVDesc->Format         = DXGI_FORMAT_R32_TYPELESS;
                }
            }
            else
            {
                RPS_ASSERT("NoImpl");
            }
        }
        else
        {
            auto pImageView = reinterpret_cast<const RpsImageView*>(accessInfo.pViewInfo);

            if (resource.desc.type == RPS_RESOURCE_TYPE_IMAGE_2D)
            {
                if (resource.desc.image.sampleCount > 1)
                {
                    if (resource.desc.image.arrayLayers <= 1)
                    {
                        pSRVDesc->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
                    }
                    else
                    {
                        RPS_ASSERT(!(accessInfo.pViewInfo->flags & RPS_RESOURCE_VIEW_FLAG_CUBEMAP_BIT));
                        {
                            pSRVDesc->ViewDimension                    = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
                            pSRVDesc->Texture2DMSArray.FirstArraySlice = accessInfo.range.baseArrayLayer;
                            pSRVDesc->Texture2DMSArray.ArraySize       = accessInfo.range.GetArrayLayerCount();
                        }
                    }
                }
                else if (resource.desc.image.arrayLayers <= 1)
                {
                    pSRVDesc->ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
                    pSRVDesc->Texture2D.MostDetailedMip = accessInfo.range.baseMipLevel;
                    pSRVDesc->Texture2D.MipLevels       = accessInfo.range.GetMipLevelCount();
                }
                else
                {
                    if (!(accessInfo.pViewInfo->flags & RPS_RESOURCE_VIEW_FLAG_CUBEMAP_BIT))
                    {
                        pSRVDesc->ViewDimension                  = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                        pSRVDesc->Texture2DArray.MostDetailedMip = accessInfo.range.baseMipLevel;
                        pSRVDesc->Texture2DArray.MipLevels       = accessInfo.range.GetMipLevelCount();
                        pSRVDesc->Texture2DArray.FirstArraySlice = accessInfo.range.baseArrayLayer;
                        pSRVDesc->Texture2DArray.ArraySize       = accessInfo.range.GetArrayLayerCount();
                    }
                    else if ((accessInfo.range.GetArrayLayerCount() > 6) || (accessInfo.range.baseArrayLayer > 0))
                    {
                        pSRVDesc->ViewDimension                     = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
                        pSRVDesc->TextureCubeArray.MostDetailedMip  = accessInfo.range.baseMipLevel;
                        pSRVDesc->TextureCubeArray.MipLevels        = accessInfo.range.GetMipLevelCount();
                        pSRVDesc->TextureCubeArray.First2DArrayFace = accessInfo.range.baseArrayLayer;
                        pSRVDesc->TextureCubeArray.NumCubes         = accessInfo.range.GetArrayLayerCount() / 6;
                    }
                    else
                    {
                        RPS_ASSERT(accessInfo.range.GetArrayLayerCount() == 6);
                        pSRVDesc->ViewDimension               = D3D11_SRV_DIMENSION_TEXTURECUBE;
                        pSRVDesc->TextureCube.MostDetailedMip = accessInfo.range.baseMipLevel;
                        pSRVDesc->TextureCube.MipLevels       = accessInfo.range.GetMipLevelCount();
                    }
                }
            }
            else if (resource.desc.type == RPS_RESOURCE_TYPE_IMAGE_3D)
            {
                pSRVDesc->ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE3D;
                pSRVDesc->Texture3D.MostDetailedMip = accessInfo.range.baseMipLevel;
                pSRVDesc->Texture3D.MipLevels       = accessInfo.range.GetMipLevelCount();
            }
            else if (resource.desc.type == RPS_RESOURCE_TYPE_IMAGE_1D)
            {
                if (resource.desc.image.arrayLayers <= 1)
                {
                    pSRVDesc->ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE1D;
                    pSRVDesc->Texture1D.MostDetailedMip = accessInfo.range.baseMipLevel;
                    pSRVDesc->Texture1D.MipLevels       = accessInfo.range.GetMipLevelCount();
                }
                else
                {
                    pSRVDesc->ViewDimension                  = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
                    pSRVDesc->Texture1DArray.MostDetailedMip = accessInfo.range.baseMipLevel;
                    pSRVDesc->Texture1DArray.MipLevels       = accessInfo.range.GetMipLevelCount();
                    pSRVDesc->Texture1DArray.FirstArraySlice = accessInfo.range.baseArrayLayer;
                    pSRVDesc->Texture1DArray.ArraySize       = accessInfo.range.GetArrayLayerCount();
                }
            }
            else
            {
                return RPS_ERROR_INVALID_OPERATION;
            }
        }

        return RPS_OK;
    }

    RpsResult InitD3D11UAVDesc(D3D11RuntimeDevice&               device,
                               D3D11_UNORDERED_ACCESS_VIEW_DESC* pUAVDesc,
                               const CmdAccessInfo&              accessInfo,
                               const ResourceInstance&           resource)

    {
        const RpsFormat viewFormat = accessInfo.viewFormat;

        pUAVDesc->Format = rpsFormatToDXGI(viewFormat);

        if (resource.desc.IsBuffer())
        {
            auto pBufView = reinterpret_cast<const RpsBufferView*>(accessInfo.pViewInfo);

            uint32_t elementSize = rpsGetFormatElementBytes(viewFormat);

            if (elementSize == 0)
            {
                elementSize = pBufView->stride;
            }
            if (elementSize == 0)
            {
                elementSize = 4;  //TODO: RAW
            }

            const uint64_t bufViewBytes = GetBufferViewBytes(pBufView, resource.desc);
            const uint64_t numElements  = bufViewBytes / elementSize;
            const uint64_t firstElement = pBufView->offset / elementSize;
            RPS_RETURN_ERROR_IF(firstElement > UINT32_MAX, RPS_ERROR_INTEGER_OVERFLOW);
            RPS_RETURN_ERROR_IF(numElements > UINT32_MAX, RPS_ERROR_INTEGER_OVERFLOW);

            pUAVDesc->ViewDimension       = D3D11_UAV_DIMENSION_BUFFER;
            pUAVDesc->Buffer.FirstElement = uint32_t(firstElement);
            pUAVDesc->Buffer.NumElements  = uint32_t(numElements);
            pUAVDesc->Buffer.Flags        = 0;

            if (((pUAVDesc->Format == DXGI_FORMAT_UNKNOWN) || (pUAVDesc->Format == DXGI_FORMAT_R32_TYPELESS)) &&
                (pBufView->stride == 0))
            {
                pUAVDesc->Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
                pUAVDesc->Format       = DXGI_FORMAT_R32_TYPELESS;
            }

            // TODO: Append/Counter
        }
        else
        {
            auto pImageView = reinterpret_cast<const RpsImageView*>(accessInfo.pViewInfo);

            RPS_ASSERT(resource.desc.image.sampleCount == 1);

            if (resource.desc.type == RPS_RESOURCE_TYPE_IMAGE_2D)
            {
                if (resource.desc.image.arrayLayers <= 1)
                {
                    pUAVDesc->ViewDimension      = D3D11_UAV_DIMENSION_TEXTURE2D;
                    pUAVDesc->Texture2D.MipSlice = accessInfo.range.baseMipLevel;
                }
                else
                {
                    pUAVDesc->ViewDimension                  = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
                    pUAVDesc->Texture2DArray.MipSlice        = accessInfo.range.baseMipLevel;
                    pUAVDesc->Texture2DArray.FirstArraySlice = accessInfo.range.baseArrayLayer;
                    pUAVDesc->Texture2DArray.ArraySize       = accessInfo.range.GetArrayLayerCount();
                }
            }
            else if (resource.desc.type == RPS_RESOURCE_TYPE_IMAGE_3D)
            {
                pUAVDesc->ViewDimension         = D3D11_UAV_DIMENSION_TEXTURE3D;
                pUAVDesc->Texture3D.MipSlice    = accessInfo.range.baseMipLevel;
                pUAVDesc->Texture3D.FirstWSlice = 0;
                pUAVDesc->Texture3D.WSize       = resource.desc.image.depth;  // TODO - W range
            }
            else if (resource.desc.type == RPS_RESOURCE_TYPE_IMAGE_1D)
            {
                if (resource.desc.image.arrayLayers <= 1)
                {
                    pUAVDesc->ViewDimension      = D3D11_UAV_DIMENSION_TEXTURE1D;
                    pUAVDesc->Texture1D.MipSlice = accessInfo.range.baseMipLevel;
                }
                else
                {
                    pUAVDesc->ViewDimension                  = D3D11_UAV_DIMENSION_TEXTURE1DARRAY;
                    pUAVDesc->Texture1DArray.MipSlice        = accessInfo.range.baseMipLevel;
                    pUAVDesc->Texture1DArray.FirstArraySlice = accessInfo.range.baseArrayLayer;
                    pUAVDesc->Texture1DArray.ArraySize       = accessInfo.range.GetArrayLayerCount();
                }
            }
            else
            {
                return RPS_ERROR_INVALID_OPERATION;
            }
        }
        return RPS_OK;
    }

    RpsResult D3D11RuntimeBackend::CreateResourceViews(const RenderGraphUpdateContext& context,
                                                       ViewType                        type,
                                                       ConstArrayRef<uint32_t>         accessIndices)
    {
        RPS_RETURN_OK_IF(accessIndices.empty());

        auto& cmdAccesses = context.renderGraph.GetCmdAccessInfos();

        auto resourceInstances = context.renderGraph.GetResourceInstances().range_all();
        auto pD3DDevice        = m_device.GetD3DDevice();

        if (type == ViewType::RTV)
        {
            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
            ID3D11RenderTargetView*       pD3DRTV = {};

            for (auto accessIndex : accessIndices)
            {
                auto& access = cmdAccesses[accessIndex];

                const auto& resource = resourceInstances[access.resourceId];
                auto*       pD3DRes  = D3D11RuntimeDevice::FromHandle(resource.hRuntimeResource);

                RPS_V_RETURN(InitD3D11RTVDesc(&rtvDesc, access, resource));
                RPS_V_RETURN(HRESULTToRps(pD3DDevice->CreateRenderTargetView(pD3DRes, &rtvDesc, &pD3DRTV)));

                m_views[accessIndex] = pD3DRTV;
            }
        }
        else if (type == ViewType::DSV)
        {
            D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
            ID3D11DepthStencilView*       pD3DDSV = {};

            for (auto accessIndex : accessIndices)
            {
                auto& access = cmdAccesses[accessIndex];

                const auto& resource = resourceInstances[access.resourceId];
                auto*       pD3DRes  = D3D11RuntimeDevice::FromHandle(resource.hRuntimeResource);

                RPS_V_RETURN(InitD3D11DSVDesc(&dsvDesc, access, resource));
                RPS_V_RETURN(HRESULTToRps(pD3DDevice->CreateDepthStencilView(pD3DRes, &dsvDesc, &pD3DDSV)));

                m_views[accessIndex] = pD3DDSV;
            }
        }
        else if (type == ViewType::SRV)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            ID3D11ShaderResourceView*       pD3DSRV = {};

            for (auto accessIndex : accessIndices)
            {
                auto& access = cmdAccesses[accessIndex];

                const auto& resource = resourceInstances[access.resourceId];
                auto*       pD3DRes  = D3D11RuntimeDevice::FromHandle(resource.hRuntimeResource);

                RPS_V_RETURN(InitD3D11SRVDesc(m_device, &srvDesc, access, resource));
                RPS_V_RETURN(HRESULTToRps(pD3DDevice->CreateShaderResourceView(pD3DRes, &srvDesc, &pD3DSRV)));

                m_views[accessIndex] = pD3DSRV;
            }
        }
        else if (type == ViewType::UAV)
        {
            D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            ID3D11UnorderedAccessView*       pD3DUAV = {};

            for (auto accessIndex : accessIndices)
            {
                auto& access = cmdAccesses[accessIndex];

                const auto& resource = resourceInstances[access.resourceId];
                auto*       pD3DRes  = D3D11RuntimeDevice::FromHandle(resource.hRuntimeResource);

                RPS_V_RETURN(InitD3D11UAVDesc(m_device, &uavDesc, access, resource));
                pD3DDevice->CreateUnorderedAccessView(pD3DRes, &uavDesc, &pD3DUAV);

                m_views[accessIndex] = pD3DUAV;
            }
        }
        else
        {
            RPS_TODO_RETURN_NOT_IMPLEMENTED();
        }

        return RPS_OK;
    }
}  // namespace rps
