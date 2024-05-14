// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_D3D11_UTIL_HPP
#define RPS_D3D11_UTIL_HPP

#include "rps/runtime/d3d_common/rps_d3d_common.h"
#include "rps/runtime/d3d11/rps_d3d11_runtime.h"

namespace rps
{
    static inline D3D11_BIND_FLAG GetD3D11BindFlags(const RpsAccessAttr& access)
    {
        uint32_t result = 0;

        if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_UNORDERED_ACCESS_BIT))
            result |= D3D11_BIND_UNORDERED_ACCESS;

        if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_RENDER_TARGET_BIT))
            result |= D3D11_BIND_RENDER_TARGET;

        if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_DEPTH_STENCIL))
            result |= D3D11_BIND_DEPTH_STENCIL;

        if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_SHADER_RESOURCE_BIT))
            result |= D3D11_BIND_SHADER_RESOURCE;

        if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_VERTEX_BUFFER_BIT))
            result |= D3D11_BIND_VERTEX_BUFFER;

        if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_INDEX_BUFFER_BIT))
            result |= D3D11_BIND_INDEX_BUFFER;

        if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_CONSTANT_BUFFER_BIT))
            result |= D3D11_BIND_CONSTANT_BUFFER;

        if (rpsAnyBitsSet(access.accessFlags, RPS_ACCESS_STREAM_OUT_BIT))
            result |= D3D11_BIND_STREAM_OUTPUT;

        return D3D11_BIND_FLAG(result);
    }

    static inline D3D11_CPU_ACCESS_FLAG GetD3D11CpuAccessFlags(const RpsAccessAttr& access)
    {
        uint32_t flags = 0;

        if (access.accessFlags & RPS_ACCESS_CPU_READ_BIT)
            flags |= D3D11_CPU_ACCESS_READ;

        if (access.accessFlags & RPS_ACCESS_CPU_WRITE_BIT)
            flags |= D3D11_CPU_ACCESS_WRITE;

        return D3D11_CPU_ACCESS_FLAG(flags);
    }

    static inline D3D11_RESOURCE_MISC_FLAG GetD3D11ResourceMiscFlags(const ResourceInstance& resInfo)
    {
        uint32_t flags = 0;

        //D3D11_RESOURCE_MISC_GENERATE_MIPS	= 0x1L,
        //D3D11_RESOURCE_MISC_SHARED	= 0x2L,

        if (resInfo.desc.flags & RPS_RESOURCE_FLAG_CUBEMAP_COMPATIBLE_BIT)
            flags |= D3D11_RESOURCE_MISC_TEXTURECUBE;

        if (resInfo.allAccesses.accessFlags & RPS_ACCESS_INDIRECT_ARGS_BIT)
            flags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;

        //D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS	= 0x20L,
        //D3D11_RESOURCE_MISC_BUFFER_STRUCTURED	= 0x40L,

        return D3D11_RESOURCE_MISC_FLAG(flags);
    }

    static inline D3D11_USAGE GetD3D11Usage(const ResourceInstance& resourceInstance)
    {
        if (rpsAnyBitsSet(resourceInstance.allAccesses.accessFlags, RPS_ACCESS_CPU_READ_BIT))
        {
            return D3D11_USAGE_STAGING;
        }
        else if (rpsAnyBitsSet(resourceInstance.allAccesses.accessFlags, RPS_ACCESS_CPU_WRITE_BIT))
        {
            return D3D11_USAGE_STAGING;
        }

        return D3D11_USAGE_DEFAULT;
    }

    static inline RpsResult CreateD3D11ResourceDesc(ID3D11Device*           pDevice,
                                                    const ResourceInstance& resInfo,
                                                    ID3D11Resource**        ppResource)
    {
        auto& rpsDesc = resInfo.desc;

        const auto d3dUsage          = GetD3D11Usage(resInfo);
        const auto d3dBindFlags      = GetD3D11BindFlags(resInfo.allAccesses);
        const auto d3dCpuAccessFlags = GetD3D11CpuAccessFlags(resInfo.allAccesses);
        const auto d3dMiscFlags      = GetD3D11ResourceMiscFlags(resInfo);

        HRESULT hr = E_NOTIMPL;

        switch (resInfo.desc.type)
        {
        case RPS_RESOURCE_TYPE_BUFFER:
        {
            ID3D11Buffer*     pBuffer;
            D3D11_BUFFER_DESC bufDesc;

            uint64_t bufSize = rpsDesc.GetBufferSize();
            RPS_RETURN_ERROR_IF(bufSize > UINT32_MAX, RPS_ERROR_NOT_SUPPORTED);

            bufDesc.ByteWidth           = UINT(bufSize);
            bufDesc.Usage               = d3dUsage;
            bufDesc.BindFlags           = d3dBindFlags;
            bufDesc.CPUAccessFlags      = d3dCpuAccessFlags;
            bufDesc.MiscFlags           = d3dMiscFlags;
            bufDesc.StructureByteStride = 0;  // TODO

            hr = pDevice->CreateBuffer(&bufDesc, nullptr, &pBuffer);

            *ppResource = pBuffer;
        }
        break;
        case RPS_RESOURCE_TYPE_IMAGE_2D:
        {
            ID3D11Texture2D*     pTex2D;
            D3D11_TEXTURE2D_DESC tex2DDesc;

            tex2DDesc.Width              = rpsDesc.image.width;
            tex2DDesc.Height             = rpsDesc.image.height;
            tex2DDesc.MipLevels          = rpsDesc.image.mipLevels;
            tex2DDesc.ArraySize          = rpsDesc.GetImageArrayLayers();
            tex2DDesc.Format             = rpsFormatToDXGI(rpsDesc.GetFormat());
            tex2DDesc.SampleDesc.Count   = rpsDesc.image.sampleCount;
            tex2DDesc.SampleDesc.Quality = 0;  // TODO
            tex2DDesc.Usage              = d3dUsage;
            tex2DDesc.BindFlags          = d3dBindFlags;
            tex2DDesc.CPUAccessFlags     = d3dCpuAccessFlags;
            tex2DDesc.MiscFlags          = d3dMiscFlags;

            hr = pDevice->CreateTexture2D(&tex2DDesc, nullptr, &pTex2D);

            *ppResource = pTex2D;
        }
        break;
        case RPS_RESOURCE_TYPE_IMAGE_3D:
        {
            ID3D11Texture3D*     pTex3D;
            D3D11_TEXTURE3D_DESC tex3DDesc;

            tex3DDesc.Width          = rpsDesc.image.width;
            tex3DDesc.Height         = rpsDesc.image.height;
            tex3DDesc.Depth          = rpsDesc.GetImageDepth();
            tex3DDesc.MipLevels      = rpsDesc.image.mipLevels;
            tex3DDesc.Format         = rpsFormatToDXGI(rpsDesc.GetFormat());
            tex3DDesc.Usage          = d3dUsage;
            tex3DDesc.BindFlags      = d3dBindFlags;
            tex3DDesc.CPUAccessFlags = d3dCpuAccessFlags;
            tex3DDesc.MiscFlags      = d3dMiscFlags;

            hr = pDevice->CreateTexture3D(&tex3DDesc, nullptr, &pTex3D);

            *ppResource = pTex3D;
        }
        break;
        case RPS_RESOURCE_TYPE_IMAGE_1D:
        {
            ID3D11Texture1D*     pTex1D;
            D3D11_TEXTURE1D_DESC tex1DDesc;

            tex1DDesc.Width          = rpsDesc.image.width;
            tex1DDesc.MipLevels      = rpsDesc.image.mipLevels;
            tex1DDesc.ArraySize      = rpsDesc.GetImageArrayLayers();
            tex1DDesc.Format         = rpsFormatToDXGI(rpsDesc.GetFormat());
            tex1DDesc.Usage          = d3dUsage;
            tex1DDesc.BindFlags      = d3dBindFlags;
            tex1DDesc.CPUAccessFlags = d3dCpuAccessFlags;
            tex1DDesc.MiscFlags      = d3dMiscFlags;

            hr = pDevice->CreateTexture1D(&tex1DDesc, nullptr, &pTex1D);

            *ppResource = pTex1D;
        }
        break;
        default:
            break;
        }

        if (FAILED(hr))
        {
            *ppResource = nullptr;
        }

        return HRESULTToRps(hr);
    }

}  // namespace rps

#endif  //RPS_D3D11_UTIL_HPP
