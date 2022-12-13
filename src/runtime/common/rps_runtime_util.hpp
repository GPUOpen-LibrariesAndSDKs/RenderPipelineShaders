// Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the AMD INTERNAL EVALUATION LICENSE.
//
// See file LICENSE.RTF for full license details.

#ifndef _RPS_RUNTIME_UTILS_H_
#define _RPS_RUNTIME_UTILS_H_

#include "rps/runtime/common/rps_runtime.h"
#include "runtime/common/rps_render_graph_resource.hpp"

namespace rps
{
#define RPS_V_REPORT_AND_RETURN(Context, Expr)                      \
    do                                                              \
    {                                                               \
        RpsResult _RPS_RESULT_TEMP__ = Expr;                        \
        if (_RPS_RESULT_TEMP__ != RPS_OK)                           \
        {                                                           \
            RPS_DIAG_RESULT_CODE((#Expr), _RPS_RESULT_TEMP__);      \
            rpsCmdCallbackReportError(Context, _RPS_RESULT_TEMP__); \
        }                                                           \
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

#endif  //_RPS_RUNTIME_UTILS_H_
