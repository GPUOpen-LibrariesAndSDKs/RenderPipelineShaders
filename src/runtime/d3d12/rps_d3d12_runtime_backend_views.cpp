// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "runtime/d3d12/rps_d3d12_runtime_backend.hpp"

#include "runtime/common/rps_render_graph.hpp"
#include "runtime/common/rps_runtime_util.hpp"

namespace rps
{

    RpsFormat GetD3D12SRVFormat(const CmdAccessInfo& accessInfo)
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
        case RPS_FORMAT_R24G8_TYPELESS:
            viewFormat = RPS_FORMAT_R24_UNORM_X8_TYPELESS;
            break;
        case RPS_FORMAT_D32_FLOAT_S8X24_UINT:
        case RPS_FORMAT_R32G8X24_TYPELESS:
            viewFormat = RPS_FORMAT_R32_FLOAT_X8X24_TYPELESS;
            break;
        default:
            break;
        }

        return viewFormat;
    }

    RpsFormat GetD3D12DSVFormat(RpsFormat viewFormat)
    {
        switch (viewFormat)
        {
        case RPS_FORMAT_R32G8X24_TYPELESS:
            viewFormat = RPS_FORMAT_D32_FLOAT_S8X24_UINT;
            break;
        case RPS_FORMAT_R24G8_TYPELESS:
            viewFormat = RPS_FORMAT_D24_UNORM_S8_UINT;
            break;
        case RPS_FORMAT_R32_TYPELESS:
            viewFormat = RPS_FORMAT_D32_FLOAT;
            break;
        case RPS_FORMAT_R16_TYPELESS:
            viewFormat = RPS_FORMAT_D16_UNORM;
            break;
        default:
            break;
        }

        return viewFormat;
    }

    static inline uint32_t GetD3D12ComponentMapping(uint32_t rpsMapping)
    {
        static_assert((uint32_t(RPS_RESOURCE_VIEW_COMPONENT_MAPPING_R) ==
                       uint32_t(D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0)) &&
                          (uint32_t(RPS_RESOURCE_VIEW_COMPONENT_MAPPING_G) ==
                           uint32_t(D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1)) &&
                          (uint32_t(RPS_RESOURCE_VIEW_COMPONENT_MAPPING_B) ==
                           uint32_t(D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2)) &&
                          (uint32_t(RPS_RESOURCE_VIEW_COMPONENT_MAPPING_A) ==
                           uint32_t(D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3)) &&
                          (uint32_t(RPS_RESOURCE_VIEW_COMPONENT_MAPPING_ZERO) ==
                           uint32_t(D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_0)) &&
                          (uint32_t(RPS_RESOURCE_VIEW_COMPONENT_MAPPING_ONE) ==
                           uint32_t(D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1)),
                      "Unexpected D3D12_SHADER_COMPONENT_MAPPING value");

        return D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING((RPS_IMAGE_VIEW_GET_COMPONENT_MAPPING_CHANNEL_R(rpsMapping)),
                                                       (RPS_IMAGE_VIEW_GET_COMPONENT_MAPPING_CHANNEL_G(rpsMapping)),
                                                       (RPS_IMAGE_VIEW_GET_COMPONENT_MAPPING_CHANNEL_B(rpsMapping)),
                                                       (RPS_IMAGE_VIEW_GET_COMPONENT_MAPPING_CHANNEL_A(rpsMapping)));
    }

    RpsResult InitD3D12RTVDesc(D3D12_RENDER_TARGET_VIEW_DESC* pRTVDesc,
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
                    pRTVDesc->ViewDimension        = D3D12_RTV_DIMENSION_TEXTURE2D;
                    pRTVDesc->Texture2D.MipSlice   = accessInfo.range.baseMipLevel;
                    pRTVDesc->Texture2D.PlaneSlice = 0;
                }
                else
                {
                    pRTVDesc->ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
                }
            }
            else
            {
                if (resDesc.image.sampleCount <= 1)
                {
                    pRTVDesc->ViewDimension                  = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                    pRTVDesc->Texture2DArray.MipSlice        = accessInfo.range.baseMipLevel;
                    pRTVDesc->Texture2DArray.FirstArraySlice = accessInfo.range.baseArrayLayer;
                    pRTVDesc->Texture2DArray.ArraySize       = accessInfo.range.GetArrayLayerCount();
                    pRTVDesc->Texture2DArray.PlaneSlice      = 0;
                }
                else
                {
                    pRTVDesc->ViewDimension                    = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
                    pRTVDesc->Texture2DMSArray.FirstArraySlice = accessInfo.range.baseArrayLayer;
                    pRTVDesc->Texture2DMSArray.ArraySize       = accessInfo.range.GetArrayLayerCount();
                }
            }
        }
        else if (resDesc.type == RPS_RESOURCE_TYPE_IMAGE_3D)
        {
            //TODO: No WSlice info here. Using full-resource for now

            pRTVDesc->ViewDimension         = D3D12_RTV_DIMENSION_TEXTURE3D;
            pRTVDesc->Texture3D.MipSlice    = accessInfo.range.baseMipLevel;
            pRTVDesc->Texture3D.FirstWSlice = 0;
            pRTVDesc->Texture3D.WSize       = resDesc.image.depth;
        }
        else if (resDesc.type == RPS_RESOURCE_TYPE_IMAGE_1D)
        {
            if (resDesc.image.arrayLayers <= 1)
            {
                pRTVDesc->ViewDimension      = D3D12_RTV_DIMENSION_TEXTURE1D;
                pRTVDesc->Texture1D.MipSlice = accessInfo.range.baseMipLevel;
            }
            else
            {
                pRTVDesc->ViewDimension                  = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
                pRTVDesc->Texture1DArray.MipSlice        = accessInfo.range.baseMipLevel;
                pRTVDesc->Texture1DArray.FirstArraySlice = accessInfo.range.baseArrayLayer;
                pRTVDesc->Texture1DArray.ArraySize       = accessInfo.range.GetArrayLayerCount();
            }
        }
        else if (resDesc.type == RPS_RESOURCE_TYPE_BUFFER)
        {
            auto           pBufView     = reinterpret_cast<const RpsBufferView*>(accessInfo.pViewInfo);
            const uint32_t elementSize  = rpsGetFormatElementBytes(accessInfo.viewFormat);
            const uint64_t bufViewBytes = GetBufferViewBytes(pBufView, resource.desc);

            RPS_RETURN_ERROR_IF(elementSize == 0, RPS_ERROR_INVALID_ARGUMENTS);

            const uint64_t numElements = bufViewBytes / elementSize;
            RPS_RETURN_ERROR_IF(numElements > UINT32_MAX, RPS_ERROR_INTEGER_OVERFLOW);
            RPS_RETURN_ERROR_IF(numElements > D3D12_REQ_RENDER_TO_BUFFER_WINDOW_WIDTH, RPS_ERROR_INVALID_ARGUMENTS);

            pRTVDesc->ViewDimension       = D3D12_RTV_DIMENSION_BUFFER;
            pRTVDesc->Buffer.FirstElement = pBufView->offset / elementSize;
            pRTVDesc->Buffer.NumElements  = uint32_t(numElements);
        }
        else
        {
            return RPS_ERROR_INVALID_OPERATION;
        }

        return RPS_OK;
    }

    RpsResult InitD3D12DSVDesc(D3D12_DEPTH_STENCIL_VIEW_DESC* pDSVDesc,
                               const CmdAccessInfo&           accessInfo,
                               const ResourceInstance&        resource)
    {
        RPS_RETURN_ERROR_IF(!resource.desc.IsImage(), RPS_ERROR_INVALID_OPERATION);

        // TODO: Add actual view Format info to Access.
        const RpsFormat dsvFormat = GetD3D12DSVFormat(accessInfo.viewFormat);
        pDSVDesc->Format          = rpsFormatToDXGI(dsvFormat);

        pDSVDesc->Flags = D3D12_DSV_FLAG_NONE;

        // Check depth plane is READONLY:
        if ((accessInfo.access.accessFlags & RPS_ACCESS_DEPTH_READ_BIT) &&
            !(accessInfo.access.accessFlags & RPS_ACCESS_DEPTH_WRITE_BIT))
        {
            pDSVDesc->Flags |= D3D12_DSV_FLAG_READ_ONLY_DEPTH;
        }

        // Check stencil plane is READONLY:
        if (rpsFormatHasStencil(dsvFormat) && (accessInfo.access.accessFlags & RPS_ACCESS_STENCIL_READ_BIT) &&
            !(accessInfo.access.accessFlags & RPS_ACCESS_STENCIL_WRITE_BIT))
        {
            pDSVDesc->Flags |= D3D12_DSV_FLAG_READ_ONLY_STENCIL;
        }

        if (resource.desc.type == RPS_RESOURCE_TYPE_IMAGE_2D)
        {
            if (resource.desc.image.arrayLayers <= 1)
            {
                if (resource.desc.image.sampleCount <= 1)
                {
                    pDSVDesc->ViewDimension      = D3D12_DSV_DIMENSION_TEXTURE2D;
                    pDSVDesc->Texture2D.MipSlice = accessInfo.range.baseMipLevel;
                }
                else
                {
                    pDSVDesc->ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
                }
            }
            else
            {
                if (resource.desc.image.sampleCount <= 1)
                {
                    pDSVDesc->ViewDimension                  = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                    pDSVDesc->Texture2DArray.MipSlice        = accessInfo.range.baseMipLevel;
                    pDSVDesc->Texture2DArray.FirstArraySlice = accessInfo.range.baseArrayLayer;
                    pDSVDesc->Texture2DArray.ArraySize       = accessInfo.range.GetArrayLayerCount();
                }
                else
                {
                    pDSVDesc->ViewDimension                    = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
                    pDSVDesc->Texture2DMSArray.FirstArraySlice = accessInfo.range.baseArrayLayer;
                    pDSVDesc->Texture2DMSArray.ArraySize       = accessInfo.range.GetArrayLayerCount();
                }
            }
        }
        else if (resource.desc.type == RPS_RESOURCE_TYPE_IMAGE_1D)
        {
            if (resource.desc.image.arrayLayers <= 1)
            {
                pDSVDesc->ViewDimension      = D3D12_DSV_DIMENSION_TEXTURE1D;
                pDSVDesc->Texture1D.MipSlice = accessInfo.range.baseMipLevel;
            }
            else
            {
                pDSVDesc->ViewDimension                  = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
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

    RpsResult InitD3D12SRVDesc(D3D12RuntimeDevice&              device,
                               D3D12_SHADER_RESOURCE_VIEW_DESC* pSRVDesc,
                               const CmdAccessInfo&             accessInfo,
                               const ResourceInstance&          resource)
    {
        RPS_RETURN_ERROR_IF(!IsResourceTypeValid(resource.desc.type), RPS_ERROR_INVALID_OPERATION);

        pSRVDesc->Format = rpsFormatToDXGI(GetD3D12SRVFormat(accessInfo));

        if (resource.desc.IsBuffer())
        {
            auto pBufView = reinterpret_cast<const RpsBufferView*>(accessInfo.pViewInfo);

            pSRVDesc->Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;  //TODO

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

                RPS_RETURN_ERROR_IF(numElements > UINT32_MAX, RPS_ERROR_INTEGER_OVERFLOW);

                pSRVDesc->ViewDimension              = D3D12_SRV_DIMENSION_BUFFER;
                pSRVDesc->Buffer.FirstElement        = firstElement;
                pSRVDesc->Buffer.NumElements         = uint32_t(numElements);
                pSRVDesc->Buffer.StructureByteStride = pBufView->stride;
                pSRVDesc->Buffer.Flags               = D3D12_BUFFER_SRV_FLAG_NONE;

                if (((accessInfo.viewFormat == RPS_FORMAT_UNKNOWN) ||
                     (accessInfo.viewFormat == RPS_FORMAT_R32_TYPELESS)) &&
                    (pBufView->stride == 0))
                {
                    pSRVDesc->Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
                    pSRVDesc->Format       = DXGI_FORMAT_R32_TYPELESS;
                }
            }
            else
            {
                RPS_ASSERT("NoImpl");
            }
        }
        else
        {
            auto pImageView                   = reinterpret_cast<const RpsImageView*>(accessInfo.pViewInfo);
            pSRVDesc->Shader4ComponentMapping = GetD3D12ComponentMapping(pImageView->componentMapping);

            if (resource.desc.type == RPS_RESOURCE_TYPE_IMAGE_2D)
            {
                if (resource.desc.image.sampleCount > 1)
                {
                    if (resource.desc.image.arrayLayers <= 1)
                    {
                        pSRVDesc->ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
                    }
                    else
                    {
                        RPS_ASSERT(!(accessInfo.pViewInfo->flags & RPS_RESOURCE_VIEW_FLAG_CUBEMAP_BIT));
                        {
                            pSRVDesc->ViewDimension                    = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
                            pSRVDesc->Texture2DMSArray.FirstArraySlice = accessInfo.range.baseArrayLayer;
                            pSRVDesc->Texture2DMSArray.ArraySize       = accessInfo.range.GetArrayLayerCount();
                        }
                    }
                }
                else if (resource.desc.image.arrayLayers <= 1)
                {
                    pSRVDesc->ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
                    pSRVDesc->Texture2D.MostDetailedMip     = accessInfo.range.baseMipLevel;
                    pSRVDesc->Texture2D.MipLevels           = accessInfo.range.GetMipLevelCount();
                    pSRVDesc->Texture2D.PlaneSlice          = device.GetFormatPlaneIndex(accessInfo.viewFormat);
                    pSRVDesc->Texture2D.ResourceMinLODClamp = pImageView->minLodClamp;
                }
                else
                {
                    if (!(accessInfo.pViewInfo->flags & RPS_RESOURCE_VIEW_FLAG_CUBEMAP_BIT))
                    {
                        pSRVDesc->ViewDimension                  = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                        pSRVDesc->Texture2DArray.MostDetailedMip = accessInfo.range.baseMipLevel;
                        pSRVDesc->Texture2DArray.MipLevels       = accessInfo.range.GetMipLevelCount();
                        pSRVDesc->Texture2DArray.FirstArraySlice = accessInfo.range.baseArrayLayer;
                        pSRVDesc->Texture2DArray.ArraySize       = accessInfo.range.GetArrayLayerCount();
                        pSRVDesc->Texture2DArray.PlaneSlice      = device.GetFormatPlaneIndex(accessInfo.viewFormat);
                        pSRVDesc->Texture2DArray.ResourceMinLODClamp = pImageView->minLodClamp;
                    }
                    else if ((accessInfo.range.GetArrayLayerCount() > 6) || (accessInfo.range.baseArrayLayer > 0))
                    {
                        pSRVDesc->ViewDimension                        = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
                        pSRVDesc->TextureCubeArray.MostDetailedMip     = accessInfo.range.baseMipLevel;
                        pSRVDesc->TextureCubeArray.MipLevels           = accessInfo.range.GetMipLevelCount();
                        pSRVDesc->TextureCubeArray.First2DArrayFace    = accessInfo.range.baseArrayLayer;
                        pSRVDesc->TextureCubeArray.NumCubes            = accessInfo.range.GetArrayLayerCount() / 6;
                        pSRVDesc->TextureCubeArray.ResourceMinLODClamp = pImageView->minLodClamp;
                    }
                    else
                    {
                        RPS_ASSERT(accessInfo.range.GetArrayLayerCount() == 6);
                        pSRVDesc->ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURECUBE;
                        pSRVDesc->TextureCube.MostDetailedMip     = accessInfo.range.baseMipLevel;
                        pSRVDesc->TextureCube.MipLevels           = accessInfo.range.GetMipLevelCount();
                        pSRVDesc->TextureCube.ResourceMinLODClamp = pImageView->minLodClamp;
                    }
                }
            }
            else if (resource.desc.type == RPS_RESOURCE_TYPE_IMAGE_3D)
            {
                pSRVDesc->ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE3D;
                pSRVDesc->Texture3D.MostDetailedMip     = accessInfo.range.baseMipLevel;
                pSRVDesc->Texture3D.MipLevels           = accessInfo.range.GetMipLevelCount();
                pSRVDesc->Texture3D.ResourceMinLODClamp = pImageView->minLodClamp;
            }
            else if (resource.desc.type == RPS_RESOURCE_TYPE_IMAGE_1D)
            {
                if (resource.desc.image.arrayLayers <= 1)
                {
                    pSRVDesc->ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE1D;
                    pSRVDesc->Texture1D.MostDetailedMip     = accessInfo.range.baseMipLevel;
                    pSRVDesc->Texture1D.MipLevels           = accessInfo.range.GetMipLevelCount();
                    pSRVDesc->Texture1D.ResourceMinLODClamp = pImageView->minLodClamp;
                }
                else
                {
                    pSRVDesc->ViewDimension                      = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
                    pSRVDesc->Texture1DArray.MostDetailedMip     = accessInfo.range.baseMipLevel;
                    pSRVDesc->Texture1DArray.MipLevels           = accessInfo.range.GetMipLevelCount();
                    pSRVDesc->Texture1DArray.FirstArraySlice     = accessInfo.range.baseArrayLayer;
                    pSRVDesc->Texture1DArray.ArraySize           = accessInfo.range.GetArrayLayerCount();
                    pSRVDesc->Texture1DArray.ResourceMinLODClamp = pImageView->minLodClamp;
                }
            }
            else
            {
                return RPS_ERROR_INVALID_OPERATION;
            }
        }

        return RPS_OK;
    }

    RpsResult InitD3D12CBVDesc(D3D12_CONSTANT_BUFFER_VIEW_DESC* pCBVDesc,
                               const CmdAccessInfo&             accessInfo,
                               const ResourceInstance&          resource)
    {
        RPS_RETURN_ERROR_IF(!resource.desc.IsBuffer(), RPS_ERROR_INVALID_OPERATION);

        auto  pBufView = reinterpret_cast<const RpsBufferView*>(accessInfo.pViewInfo);
        auto* pD3DRes  = D3D12RuntimeDevice::FromHandle(resource.hRuntimeResource);

        // NOTE: We allow DX12 debug layer to complain if CBV > D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT
        // and/or if defy D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT by pBufView->offset.

        RPS_RETURN_ERROR_IF((pBufView->sizeInBytes == RPS_BUFFER_WHOLE_SIZE) &&
            ((resource.desc.GetBufferSize() % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) != 0),
            RPS_ERROR_INVALID_ARGUMENTS);

        const uint64_t bufViewBytes = rpsAlignUp(GetBufferViewBytes(pBufView, resource.desc),
                                                 uint64_t(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));

        RPS_RETURN_ERROR_IF(bufViewBytes > UINT32_MAX, RPS_ERROR_INTEGER_OVERFLOW);
        RPS_RETURN_ERROR_IF((bufViewBytes + pBufView->offset) > resource.desc.GetBufferSize(),
            RPS_ERROR_INVALID_ARGUMENTS);

        pCBVDesc->BufferLocation = pD3DRes->GetGPUVirtualAddress() + pBufView->offset;
        pCBVDesc->SizeInBytes    = uint32_t(bufViewBytes);

        return RPS_OK;
    }

    RpsResult InitD3D12UAVDesc(D3D12RuntimeDevice&               device,
                               D3D12_UNORDERED_ACCESS_VIEW_DESC* pUAVDesc,
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

            RPS_RETURN_ERROR_IF(numElements > UINT32_MAX, RPS_ERROR_INTEGER_OVERFLOW);

            pUAVDesc->ViewDimension               = D3D12_UAV_DIMENSION_BUFFER;
            pUAVDesc->Buffer.FirstElement         = pBufView->offset / elementSize;
            pUAVDesc->Buffer.NumElements          = uint32_t(numElements);
            pUAVDesc->Buffer.CounterOffsetInBytes = 0;
            pUAVDesc->Buffer.StructureByteStride  = pBufView->stride;
            pUAVDesc->Buffer.Flags                = D3D12_BUFFER_UAV_FLAG_NONE;

            if (((pUAVDesc->Format == DXGI_FORMAT_UNKNOWN) || (pUAVDesc->Format == DXGI_FORMAT_R32_TYPELESS)) &&
                (pBufView->stride == 0))
            {
                pUAVDesc->Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
                pUAVDesc->Format       = DXGI_FORMAT_R32_TYPELESS;
            }
        }
        else
        {
            auto pImageView = reinterpret_cast<const RpsImageView*>(accessInfo.pViewInfo);

#if !RPS_D3D12_MSAA_UAV_SUPPORT
            RPS_ASSERT(resource.desc.image.sampleCount == 1);
#endif

            if (resource.desc.type == RPS_RESOURCE_TYPE_IMAGE_2D)
            {
#if RPS_D3D12_MSAA_UAV_SUPPORT
                if (resource.desc.image.sampleCount == 1)
#endif  //RPS_D3D12_MSAA_UAV_SUPPORT
                {
                    if (resource.desc.image.arrayLayers <= 1)
                    {
                        pUAVDesc->ViewDimension        = D3D12_UAV_DIMENSION_TEXTURE2D;
                        pUAVDesc->Texture2D.MipSlice   = accessInfo.range.baseMipLevel;
                        pUAVDesc->Texture2D.PlaneSlice = device.GetFormatPlaneIndex(viewFormat);
                    }
                    else
                    {
                        pUAVDesc->ViewDimension                  = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                        pUAVDesc->Texture2DArray.MipSlice        = accessInfo.range.baseMipLevel;
                        pUAVDesc->Texture2DArray.FirstArraySlice = accessInfo.range.baseArrayLayer;
                        pUAVDesc->Texture2DArray.ArraySize       = accessInfo.range.GetArrayLayerCount();
                        pUAVDesc->Texture2DArray.PlaneSlice      = device.GetFormatPlaneIndex(viewFormat);
                    }
                }
#if RPS_D3D12_MSAA_UAV_SUPPORT
                else
                {
                    if (resource.desc.image.arrayLayers <= 1)
                    {
                        pUAVDesc->ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DMS;
                    }
                    else
                    {
                        pUAVDesc->ViewDimension                    = D3D12_UAV_DIMENSION_TEXTURE2DMSARRAY;
                        pUAVDesc->Texture2DMSArray.FirstArraySlice = accessInfo.range.baseArrayLayer;
                        pUAVDesc->Texture2DMSArray.ArraySize       = accessInfo.range.GetArrayLayerCount();
                    }
                }
#endif  //RPS_D3D12_MSAA_UAV_SUPPORT
            }
            else if (resource.desc.type == RPS_RESOURCE_TYPE_IMAGE_3D)
            {
                pUAVDesc->ViewDimension         = D3D12_UAV_DIMENSION_TEXTURE3D;
                pUAVDesc->Texture3D.MipSlice    = accessInfo.range.baseMipLevel;
                pUAVDesc->Texture3D.FirstWSlice = 0;
                pUAVDesc->Texture3D.WSize       = resource.desc.image.depth;  // TODO - W range
            }
            else if (resource.desc.type == RPS_RESOURCE_TYPE_IMAGE_1D)
            {
                if (resource.desc.image.arrayLayers <= 1)
                {
                    pUAVDesc->ViewDimension      = D3D12_UAV_DIMENSION_TEXTURE1D;
                    pUAVDesc->Texture1D.MipSlice = accessInfo.range.baseMipLevel;
                }
                else
                {
                    pUAVDesc->ViewDimension                  = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
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

    RpsResult D3D12RuntimeBackend::CreateResourceViews(const RenderGraphUpdateContext& context,
                                                       D3D12_DESCRIPTOR_HEAP_TYPE      type,
                                                       ConstArrayRef<uint32_t>         accessIndices)
    {
        RPS_RETURN_OK_IF(accessIndices.empty());

        auto& cmdAccesses = context.renderGraph.GetCmdAccessInfos();

        auto       resourceInstances = context.renderGraph.GetResourceInstances().range_all();
        auto       pD3DDevice        = m_device.GetD3DDevice();
        const auto descriptorSize    = m_device.GetDescriptorSize(type);

        RPS_V_RETURN(
            m_cpuDescriptorHeaps[type].Reserve(context, m_device.GetD3DDevice(), type, uint32_t(accessIndices.size())));

        D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHdl = m_cpuDescriptorHeaps[type].pHeap->GetCPUDescriptorHandleForHeapStart();

        uint32_t heapOffset = 0;

        if (type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
        {
            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};

            for (auto accessIndex : accessIndices)
            {
                auto& access = cmdAccesses[accessIndex];

                const auto& resource = resourceInstances[access.resourceId];
                auto*       pD3DRes  = D3D12RuntimeDevice::FromHandle(resource.hRuntimeResource);

                RPS_V_RETURN(InitD3D12RTVDesc(&rtvDesc, access, resource));

                pD3DDevice->CreateRenderTargetView(pD3DRes, &rtvDesc, cpuDescHdl);

                m_accessToDescriptorMap[accessIndex] = heapOffset;

                heapOffset++;
                cpuDescHdl.ptr += descriptorSize;
            }
        }
        else if (type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
        {
            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};

            for (auto accessIndex : accessIndices)
            {
                auto& access = cmdAccesses[accessIndex];

                const auto& resource = resourceInstances[access.resourceId];
                auto*       pD3DRes  = D3D12RuntimeDevice::FromHandle(resource.hRuntimeResource);

                RPS_V_RETURN(InitD3D12DSVDesc(&dsvDesc, access, resource));

                pD3DDevice->CreateDepthStencilView(pD3DRes, &dsvDesc, cpuDescHdl);

                m_accessToDescriptorMap[accessIndex] = heapOffset;

                heapOffset++;
                cpuDescHdl.ptr += descriptorSize;
            }
        }
        else if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC  srvDesc = {};
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            D3D12_CONSTANT_BUFFER_VIEW_DESC  cbvDesc = {};

            for (auto accessIndex : accessIndices)
            {
                auto& access = cmdAccesses[accessIndex];

                const auto& resource = resourceInstances[access.resourceId];
                auto*       pD3DRes  = D3D12RuntimeDevice::FromHandle(resource.hRuntimeResource);

                // TODO: Do we want to support single access with multiple views?
                if (access.access.accessFlags & RPS_ACCESS_SHADER_RESOURCE_BIT)
                {
                    RPS_V_RETURN(InitD3D12SRVDesc(m_device, &srvDesc, access, resource));
                    pD3DDevice->CreateShaderResourceView(pD3DRes, &srvDesc, cpuDescHdl);
                }
                else if (access.access.accessFlags & RPS_ACCESS_CONSTANT_BUFFER_BIT)
                {
                    RPS_V_RETURN(InitD3D12CBVDesc(&cbvDesc, access, resource));
                    pD3DDevice->CreateConstantBufferView(&cbvDesc, cpuDescHdl);
                }
                else if (access.access.accessFlags & RPS_ACCESS_UNORDERED_ACCESS_BIT)
                {
                    RPS_V_RETURN(InitD3D12UAVDesc(m_device, &uavDesc, access, resource));
                    // TODO: Counter from another access
                    pD3DDevice->CreateUnorderedAccessView(pD3DRes, nullptr, &uavDesc, cpuDescHdl);
                }

                m_accessToDescriptorMap[accessIndex] = heapOffset;

                heapOffset++;
                cpuDescHdl.ptr += descriptorSize;
            }
        }
        else
        {
            RPS_TODO_RETURN_NOT_IMPLEMENTED();
        }

        return RPS_OK;
    }
}  // namespace rps
