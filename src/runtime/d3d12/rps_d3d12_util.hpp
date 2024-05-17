// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_D3D12_UTIL_HPP
#define RPS_D3D12_UTIL_HPP

#include "rps/runtime/d3d12/rps_d3d12_runtime.h"
#include "runtime/d3d_common/rps_d3d_common_util.hpp"

namespace rps
{
    constexpr uint32_t D3D12CalcSubresource(
        uint32_t MipSlice, uint32_t ArraySlice, uint32_t PlaneSlice, uint32_t MipLevels, uint32_t ArraySize) noexcept
    {
        return MipSlice + ArraySlice * MipLevels + PlaneSlice * MipLevels * ArraySize;
    }

    static inline D3D12_RESOURCE_DIMENSION GetD3D12ResourceDimension(RpsResourceType type)
    {
        switch (type)
        {
        case RPS_RESOURCE_TYPE_BUFFER:
            return D3D12_RESOURCE_DIMENSION_BUFFER;
        case RPS_RESOURCE_TYPE_IMAGE_2D:
            return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        case RPS_RESOURCE_TYPE_IMAGE_3D:
            return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        case RPS_RESOURCE_TYPE_IMAGE_1D:
            return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
        default:
            break;
        }
        return D3D12_RESOURCE_DIMENSION_UNKNOWN;
    }

    static inline RpsResourceType D3D12ResourceDimensionToRps(D3D12_RESOURCE_DIMENSION type)
    {
        switch (type)
        {
        case D3D12_RESOURCE_DIMENSION_BUFFER:
            return RPS_RESOURCE_TYPE_BUFFER;
        case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
            return RPS_RESOURCE_TYPE_IMAGE_2D;
        case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
            return RPS_RESOURCE_TYPE_IMAGE_3D;
        case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
            return RPS_RESOURCE_TYPE_IMAGE_1D;
        default:
            break;
        }
        return RPS_RESOURCE_TYPE_UNKNOWN;
    }

    static inline D3D12_RESOURCE_FLAGS GetD3D12ResourceFlags(const ResourceInstance& resInfo)
    {
        D3D12_RESOURCE_FLAGS result = D3D12_RESOURCE_FLAG_NONE;

        if (resInfo.allAccesses.accessFlags & RPS_ACCESS_UNORDERED_ACCESS_BIT)
            result |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        if (resInfo.allAccesses.accessFlags & RPS_ACCESS_RENDER_TARGET_BIT)
            result |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        if (resInfo.allAccesses.accessFlags & RPS_ACCESS_DEPTH_STENCIL)
        {
            result |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            if (!(resInfo.allAccesses.accessFlags & RPS_ACCESS_SHADER_RESOURCE_BIT))
            {
                result |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
            }
        }

#if RPS_D3D12_ENHANCED_BARRIER_SUPPORT
        if (resInfo.allAccesses.accessFlags & (RPS_ACCESS_RAYTRACING_AS_BUILD_BIT | RPS_ACCESS_RAYTRACING_AS_READ_BIT))
        {
            result |= D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE;
        }
#endif  //RPS_D3D12_ENHANCED_BARRIER_SUPPORT

        return result;
    }

    static inline RpsResourceFlags D3D12ResourceFlagsToRps(D3D12_RESOURCE_FLAGS flags)
    {
        RpsResourceFlags result = RPS_RESOURCE_FLAG_NONE;
        return result;
    }

    template <typename TD3D12ResourceDesc>
    static void CalcD3D12ResourceDesc(TD3D12ResourceDesc* pD3D12Desc, const ResourceInstance& resInfo)
    {
        pD3D12Desc->Dimension = GetD3D12ResourceDimension(resInfo.desc.type);

        pD3D12Desc->Alignment = 0;
        pD3D12Desc->Format    = rpsFormatToDXGI(resInfo.desc.GetFormat());
        pD3D12Desc->Flags     = GetD3D12ResourceFlags(resInfo);

        if (resInfo.desc.IsImage())
        {
            pD3D12Desc->Width              = resInfo.desc.image.width;
            pD3D12Desc->Height             = resInfo.desc.image.height;
            pD3D12Desc->DepthOrArraySize   = (pD3D12Desc->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
                                                 ? resInfo.desc.image.depth
                                                 : resInfo.desc.image.arrayLayers;
            pD3D12Desc->MipLevels          = resInfo.desc.image.mipLevels;
            pD3D12Desc->SampleDesc.Count   = resInfo.desc.image.sampleCount;
            pD3D12Desc->SampleDesc.Quality = 0;
            const bool bRowMajor           = !!(resInfo.desc.flags & RPS_RESOURCE_FLAG_ROWMAJOR_IMAGE_BIT);
            pD3D12Desc->Layout             = bRowMajor ? D3D12_TEXTURE_LAYOUT_ROW_MAJOR : D3D12_TEXTURE_LAYOUT_UNKNOWN;
        }
        else if (resInfo.desc.IsBuffer())
        {
            pD3D12Desc->Width              = resInfo.desc.GetBufferSize();
            pD3D12Desc->Height             = 1;
            pD3D12Desc->DepthOrArraySize   = 1;
            pD3D12Desc->MipLevels          = 1;
            pD3D12Desc->SampleDesc.Count   = 1;
            pD3D12Desc->SampleDesc.Quality = 0;
            pD3D12Desc->Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        }
    }

    template <typename TD3D12ResourceDesc = D3D12_RESOURCE_DESC>
    static void D3D12ResourceDescToRps(RpsResourceDesc* pRpsDesc, const TD3D12ResourceDesc* pD3D12Desc)
    {
        pRpsDesc->type  = D3D12ResourceDimensionToRps(pD3D12Desc->Dimension);
        pRpsDesc->flags = D3D12ResourceFlagsToRps(pD3D12Desc->Flags);

        if (ResourceDesc::IsImage(pRpsDesc->type))
        {
            pRpsDesc->image.format = rpsFormatFromDXGI(pD3D12Desc->Format);
            pRpsDesc->image.width  = uint32_t(pD3D12Desc->Width);
            pRpsDesc->image.height = pD3D12Desc->Height;
            if (pD3D12Desc->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
            {
                pRpsDesc->image.depth = pD3D12Desc->DepthOrArraySize;
            }
            else
            {
                pRpsDesc->image.arrayLayers = pD3D12Desc->DepthOrArraySize;
            }
            pRpsDesc->image.mipLevels   = pD3D12Desc->MipLevels;
            pRpsDesc->image.sampleCount = pD3D12Desc->SampleDesc.Count;
            if (pD3D12Desc->Layout == D3D12_TEXTURE_LAYOUT_ROW_MAJOR)
            {
                pRpsDesc->flags |= RPS_RESOURCE_FLAG_ROWMAJOR_IMAGE_BIT;
            }
        }
        else if (ResourceDesc::IsBuffer(pRpsDesc->type))
        {
            pRpsDesc->buffer.sizeInBytesHi = uint32_t(pD3D12Desc->Width >> 32u);
            pRpsDesc->buffer.sizeInBytesLo = uint32_t(pD3D12Desc->Width & UINT32_MAX);
        }
    }

    static inline uint32_t GetD3D12HeapTypeIndex(D3D12_RESOURCE_HEAP_TIER heapTier,
                                                 const ResourceInstance&  resourceInstance)
    {
        if (rpsAnyBitsSet(resourceInstance.allAccesses.accessFlags, RPS_ACCESS_CPU_READ_BIT))
        {
            return RPS_D3D12_HEAP_TYPE_INDEX_READBACK;
        }
        else if (rpsAnyBitsSet(resourceInstance.allAccesses.accessFlags, RPS_ACCESS_CPU_WRITE_BIT))
        {
            return RPS_D3D12_HEAP_TYPE_INDEX_UPLOAD;
        }

        if (heapTier == D3D12_RESOURCE_HEAP_TIER_2)
        {
            return (resourceInstance.desc.IsImage() && (resourceInstance.desc.image.sampleCount > 1))
                       ? RPS_D3D12_HEAP_TYPE_INDEX_DEFAULT_MSAA
                       : RPS_D3D12_HEAP_TYPE_INDEX_DEFAULT;
        }
        else if (resourceInstance.desc.IsBuffer())
        {
            return RPS_D3D12_HEAP_TYPE_INDEX_DEFAULT_TIER_1_BUFFER;
        }
        else if (resourceInstance.desc.IsImage())
        {
            if (rpsAnyBitsSet(resourceInstance.allAccesses.accessFlags,
                              (RPS_ACCESS_RENDER_TARGET_BIT | RPS_ACCESS_DEPTH_STENCIL)))
            {
                return (resourceInstance.desc.image.sampleCount > 1)
                           ? RPS_D3D12_HEAP_TYPE_INDEX_DEFAULT_TIER_1_RT_DS_TEXTURE_MSAA
                           : RPS_D3D12_HEAP_TYPE_INDEX_DEFAULT_TIER_1_RT_DS_TEXTURE;
            }
            else
            {
                return RPS_D3D12_HEAP_TYPE_INDEX_DEFAULT_TIER_1_NON_RT_DS_TEXTURE;
            }
        }
        return RPS_D3D12_HEAP_TYPE_INDEX_DEFAULT;
    }

    static inline D3D12_RESOLVE_MODE D3D12GetResolveMode(RpsResolveMode mode)
    {
        switch (mode)
        {
        case RPS_RESOLVE_MODE_AVERAGE:
            return D3D12_RESOLVE_MODE_AVERAGE;
        case RPS_RESOLVE_MODE_MIN:
            return D3D12_RESOLVE_MODE_MIN;
        case RPS_RESOLVE_MODE_MAX:
            return D3D12_RESOLVE_MODE_MAX;
        case RPS_RESOLVE_MODE_ENCODE_SAMPLER_FEEDBACK:
            return D3D12_RESOLVE_MODE_ENCODE_SAMPLER_FEEDBACK;
        case RPS_RESOLVE_MODE_DECODE_SAMPLER_FEEDBACK:
            return D3D12_RESOLVE_MODE_DECODE_SAMPLER_FEEDBACK;
        default:
            break;
        }

        RPS_ASSERT(RPS_FALSE);
        return D3D12_RESOLVE_MODE_AVERAGE;
    }

}  // namespace rps

#endif  //RPS_D3D12_UTIL_HPP
