// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_RUNTIME_UTIL_HPP
#define RPS_RUNTIME_UTIL_HPP

#include "rps/runtime/common/rps_runtime.h"
#include "runtime/common/rps_render_graph_resource.hpp"

namespace rps
{
#define RPS_V_REPORT_AND_RETURN(Context, Expr)                              \
    do                                                                      \
    {                                                                       \
        RpsResult RPS_RESULT_TEMP = Expr;                                   \
        if (RPS_RESULT_TEMP != RPS_OK)                                      \
        {                                                                   \
            RPS_DIAG_RESULT_CODE(RPS_DIAG_ERROR, (#Expr), RPS_RESULT_TEMP); \
            rpsCmdCallbackReportError(Context, RPS_RESULT_TEMP);            \
        }                                                                   \
    } while (0)

    static inline void CanonicalizeMipLevels(ResourceDescPacked& resDesc)
    {
        if (!resDesc.IsImage())
        {
            return;
        }

        if (resDesc.image.sampleCount > 1)
        {
            resDesc.image.mipLevels = 1;
        }

        if (resDesc.image.mipLevels == 0)
        {
            uint32_t w    = resDesc.image.width;
            uint32_t h    = resDesc.image.height;
            uint32_t d    = (resDesc.type == RPS_RESOURCE_TYPE_IMAGE_3D) ? resDesc.image.depth : 1;
            uint32_t mips = 1;

            while ((w > 1) || (h > 1) || (d > 1))
            {
                mips++;
                w = w >> 1;
                h = h >> 1;
                d = d >> 1;
            }

            resDesc.image.mipLevels = mips;
        }
    }

    static inline uint32_t GetMipLevelDimension(uint32_t mostDetailedMipDim, uint32_t mipLevel)
    {
        return rpsMax(1u, mostDetailedMipDim >> mipLevel);
    }

    static inline void GetFullSubresourceRange(SubresourceRangePacked&   subResRange,
                                               const ResourceDescPacked& resDesc,
                                               uint32_t                  aspectMask)
    {
        subResRange.aspectMask     = 1;
        subResRange.baseArrayLayer = 0;
        subResRange.arrayLayerEnd  = 1;
        subResRange.baseMipLevel   = 0;
        subResRange.mipLevelEnd    = 1;

        if (resDesc.IsImage())
        {
            subResRange.mipLevelEnd = resDesc.image.mipLevels;

            if (resDesc.type != RPS_RESOURCE_TYPE_IMAGE_3D)
            {
                subResRange.arrayLayerEnd = resDesc.image.arrayLayers;
            }

            subResRange.aspectMask = aspectMask;
        }
    }

    static inline bool IsDepthStencilReadWriteTransition(RpsAccessFlags before, RpsAccessFlags after)
    {
        RpsAccessFlags beforeDepth   = (before & RPS_ACCESS_DEPTH);
        RpsAccessFlags beforeStencil = (before & RPS_ACCESS_STENCIL);
        RpsAccessFlags afterDepth    = (after & RPS_ACCESS_DEPTH);
        RpsAccessFlags afterStencil  = (after & RPS_ACCESS_STENCIL);

        return (beforeDepth && afterDepth &&
                ((beforeDepth & RPS_ACCESS_DEPTH_WRITE_BIT) != (afterDepth & RPS_ACCESS_DEPTH_WRITE_BIT))) ||
               (beforeStencil && afterStencil &&
                ((beforeStencil & RPS_ACCESS_STENCIL_WRITE_BIT) != (afterStencil & RPS_ACCESS_STENCIL_WRITE_BIT)));
    }

    static inline bool IsResourceTypeValid(RpsResourceType type)
    {
        return (type != RPS_RESOURCE_TYPE_UNKNOWN) && (int32_t(type) < RPS_RESOURCE_TYPE_COUNT);
    }

    static inline uint64_t GetBufferViewBytes(const RpsBufferView* pBufView, const ResourceDescPacked& resourceDesc)
    {
        const uint64_t bufferViewBytes = (pBufView->sizeInBytes != RPS_BUFFER_WHOLE_SIZE)
                                             ? pBufView->sizeInBytes
                                             : (resourceDesc.GetBufferSize() - pBufView->offset);

        return bufferViewBytes;
    }

}  // namespace rps

#endif  //RPS_RUNTIME_UTIL_HPP
