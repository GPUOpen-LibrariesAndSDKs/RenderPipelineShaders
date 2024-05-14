// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_D3D12_RUNTIME_H
#define RPS_D3D12_RUNTIME_H

#include "rps/core/rps_api.h"
#include "rps/runtime/common/rps_runtime.h"
#include "rps/runtime/d3d_common/rps_d3d_common.h"

#include <d3d12.h>

#if D3D12_SDK_VERSION >= 606
#define RPS_D3D12_MSAA_UAV_SUPPORT 1
#define RPS_D3D12_ENHANCED_BARRIER_SUPPORT 1
#endif

#if D3D12_SDK_VERSION >= 600
#define RPS_D3D12_FEATURE_D3D12_OPTIONS12_DEFINED 1
#endif

/// @addtogroup RpsD3D12RuntimeDevice
/// @{

/// @brief Bitflags for D3D12 runtime behavior.
typedef enum RpsD3D12RuntimeFlagBits
{
    RPS_D3D12_RUNTIME_FLAG_NONE                      = 0,       ///< No flags.
    RPS_D3D12_RUNTIME_FLAG_FORCE_RESOURCE_HEAP_TIER1 = 1 << 0,  ///< Forces the runtime to behave as if the D3D12 device
                                                                ///  only supports D3D12_RESOURCE_HEAP_TIER_1.
    RPS_D3D12_RUNTIME_FLAG_PREFER_RENDER_PASS       = 1 << 1,   ///< Prefers using render passes.
    RPS_D3D12_RUNTIME_FLAG_PREFER_ENHANCED_BARRIERS = 1 << 2,   ///< Prefers using enhanced barriers.
} RpsD3D12RuntimeFlagBits;

/// @brief Bitmask type for <c><i>RpsD3D12RuntimeFlagBits</i></c>
typedef uint32_t RpsD3D12RuntimeFlags;

/// @brief Creation parameters for an RPS device with a d3d12 backend.
typedef struct RpsD3D12RuntimeDeviceCreateInfo
{
    const RpsDeviceCreateInfo* pDeviceCreateInfo;          ///< Pointer to general RPS device creation parameters.
                                                           ///  Passing NULL uses default parameters instead.
    const RpsRuntimeDeviceCreateInfo* pRuntimeCreateInfo;  ///< Pointer to general RPS runtime creation info. Passing
                                                           ///  NULL uses default parameters instead.
    ID3D12Device* pD3D12Device;                            ///< Pointer to the D3D12 device to use for the runtime.
                                                           ///  Must not be NULL.
    RpsD3D12RuntimeFlags flags;                            ///< D3D13 runtime flags.
} RpsD3D12RuntimeDeviceCreateInfo;

/// @brief Indices for D3D12 heap/memory types.
typedef enum RpsD3D12HeapTypeIndex
{
    RPS_D3D12_HEAP_TYPE_INDEX_UPLOAD = 0,    ///< Maps to D3D12_HEAP_TYPE_UPLOAD.
    RPS_D3D12_HEAP_TYPE_INDEX_READBACK,      ///< Maps to D3D12_HEAP_TYPE_READBACK.
    RPS_D3D12_HEAP_TYPE_INDEX_DEFAULT,       ///< Maps to D3D12_HEAP_TYPE_DEFAULT.
    RPS_D3D12_HEAP_TYPE_INDEX_DEFAULT_MSAA,  ///< Maps to D3D12_HEAP_TYPE_DEFAULT with MSAA support.
    RPS_D3D12_HEAP_TYPE_COUNT_TIER_2,        ///< Number of heap types for D3D12_RESOURCE_HEAP_TIER_2.

    /// Maps to D3D12_HEAP_TYPE_DEFAULT with the D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES flag set.
    RPS_D3D12_HEAP_TYPE_INDEX_DEFAULT_TIER_1_RT_DS_TEXTURE = RPS_D3D12_HEAP_TYPE_INDEX_DEFAULT,

    /// Maps to D3D12_HEAP_TYPE_DEFAULT with the D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES flag set and MSAA support.
    RPS_D3D12_HEAP_TYPE_INDEX_DEFAULT_TIER_1_RT_DS_TEXTURE_MSAA = RPS_D3D12_HEAP_TYPE_INDEX_DEFAULT_MSAA,

    /// Maps to D3D12_HEAP_TYPE_DEFAULT with the D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS flag set.
    RPS_D3D12_HEAP_TYPE_INDEX_DEFAULT_TIER_1_BUFFER,

    /// Maps to D3D12_HEAP_TYPE_DEFAULT with the D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES flag set.
    RPS_D3D12_HEAP_TYPE_INDEX_DEFAULT_TIER_1_NON_RT_DS_TEXTURE,

    /// Number of heap types for D3D12_RESOURCE_HEAP_TIER_1.
    RPS_D3D12_HEAP_TYPE_COUNT_TIER_1,

    ///Total number of heap types.
    RPS_D3D12_HEAP_TYPE_COUNT_MAX = RPS_D3D12_HEAP_TYPE_COUNT_TIER_1,
} RpsD3D12HeapTypeIndex;

/// @} end addtogroup RpsD3D12RuntimeDevice

#ifdef __cplusplus
extern "C" {
#endif  //__cplusplus

/// @brief Creates a D3D12 runtime device.
///
/// @param pCreateInfo                  Pointer to creation parameters. Must not be NULL.
/// @param phDevice                     Pointer to a handle in which the device is returned. Must not be NULL.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
/// @ingroup RpsD3D12RuntimeDevice
RpsResult rpsD3D12RuntimeDeviceCreate(const RpsD3D12RuntimeDeviceCreateInfo* pCreateInfo, RpsDevice* phDevice);

/// @addtogroup RpsRenderGraphCommandRecordingD3D12
/// @{

/// @brief Mapping between <c><i>RpsRuntimeCommandBuffer</i></c> and <c><i>ID3D12GraphicsCommandList*</i></c>.
RPS_IMPL_OPAQUE_HANDLE(D3D12CommandList, RpsRuntimeCommandBuffer, ID3D12GraphicsCommandList);

/// @brief Mapping between <c><i>RpsRuntimeCommandBuffer</i></c> and <c><i>ID3D12GraphicsCommandList1*</i></c>.
RPS_IMPL_OPAQUE_HANDLE(D3D12CommandList1, RpsRuntimeCommandBuffer, ID3D12GraphicsCommandList1);

/// @brief Mapping between <c><i>RpsRuntimeResource</i></c> and <c><i>ID3D12Resource*</i></c>.
RPS_IMPL_OPAQUE_HANDLE(D3D12Resource, RpsRuntimeResource, ID3D12Resource);

/// @brief Mapping between <c><i>RpsRuntimeHeap</i></c> and <c><i>ID3D12Heap*</i></c>.
RPS_IMPL_OPAQUE_HANDLE(D3D12Heap, RpsRuntimeHeap, ID3D12Heap);

/// @brief Helper function for converting a D3D12_RESOURCE_DESC structure to an RpsResourceDesc structure.
///
/// @param pD3D12Desc                   A pointer to the input D3D12_RESOURCE_DESC structure.
/// @param pRpsDesc                     A pointer to the output RpsResourceDesc structure.
///
/// @return                             Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsD3D12ResourceDescToRps(const D3D12_RESOURCE_DESC* pD3D12Desc, RpsResourceDesc* pRpsDesc);

/// @brief Gets an array of resource pointers from a resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the resource pointers from. Must be a resource
///                                     array argument if numResources > 1.
/// @param srcArrayOffset               Offset to the first resource pointer to get.
/// @param ppResources                  Pointer to an array of <c><i>ID3D12Resource*</i></c> in which the numResources
///                                     resource pointers are returned. Must not be NULL if numResources != 0.
/// @param numResources                 Number of resources to get. Requires srcArrayOffset + numResources to be less
///                                     than the number of elements in the node argument.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsD3D12GetCmdArgResourceArray(const RpsCmdCallbackContext* pContext,
                                         uint32_t                     argIndex,
                                         uint32_t                     srcArrayOffset,
                                         ID3D12Resource**             ppResources,
                                         uint32_t                     numResources);

/// @brief Get a resource from a resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the resource pointer from.
/// @param ppResource                   Pointer in which the resource pointer is returned. Must not be NULL.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsD3D12GetCmdArgResource(const RpsCmdCallbackContext* pContext,
                                    uint32_t                     argIndex,
                                    ID3D12Resource**             ppResource);

/// @brief Gets an array of D3D12 CPU descriptor handles from a resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the descriptor handle from. Must be a resource
///                                     array argument if numHandles > 1.
/// @param srcArrayOffset               Offset to the first descriptor handle to get.
/// @param pHandles                     Pointer to an array of <c><i>ID3D12_CPU_DESCRIPTOR_HANDLE</i></c> in which the
///                                     descriptor handles are returned. Must not be NULL if numHandles != 0.
/// @param numHandles                   Number of descriptor handles to get. Requires srcArrayOffset + numHandles to be
///                                     less than the number of elements in the node argument.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsD3D12GetCmdArgDescriptorArray(const RpsCmdCallbackContext* pContext,
                                           uint32_t                     argIndex,
                                           uint32_t                     srcArrayOffset,
                                           D3D12_CPU_DESCRIPTOR_HANDLE* pHandles,
                                           uint32_t                     numHandles);

/// @brief Get a CPU descriptor handle from a resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the descriptor handle from. Must be a resource
///                                     argument.
/// @param pHandle                      Pointer in which the descriptor handle is returned. Must not be NULL.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsD3D12GetCmdArgDescriptor(const RpsCmdCallbackContext* pContext,
                                      uint32_t                     argIndex,
                                      D3D12_CPU_DESCRIPTOR_HANDLE* pHandle);

/// @brief Copies an array of D3D12 CPU descriptor handles from a resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to copy the descriptor content from. Must be a resource
///                                     array argument if numHandles > 1.
/// @param srcArrayOffset               Offset to the first descriptor handle to copy.
/// @param numHandles                   Number of descriptor handles to copy. Requires srcArrayOffset + numHandles to be
///                                     less than the number of elements in the node argument.
/// @param singleHandleToArray          If true, pDstHandles points only to a single destination handle which refers to a
///                                     continuous range of descriptors in a descriptor heap. If false, pDstHandles
///                                     points to an array of descriptor handles possibly referring to independent
///                                     locations or heaps.
/// @param pDstHandles                  Pointer to an array of descriptors to copy to.
///                                     Must not be NULL if numHandles != 0.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsD3D12CopyCmdArgDescriptors(const RpsCmdCallbackContext* pContext,
                                        uint32_t                     argIndex,
                                        uint32_t                     srcArrayOffset,
                                        uint32_t                     numHandles,
                                        RpsBool                      singleHandleToArray,
                                        D3D12_CPU_DESCRIPTOR_HANDLE* pDstHandles);

/// @} end addtogroup RpsRenderGraphCommandRecordingD3D12

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
        struct CommandArgUnwrapper<Index, ID3D12Resource*>
        {
            ID3D12Resource* operator()(const RpsCmdCallbackContext* pContext)
            {
                ID3D12Resource* resource = nullptr;
                RpsResult result = rpsD3D12GetCmdArgResource(pContext, Index, &resource);
                if (RPS_FAILED(result))
                {
                    rpsCmdCallbackReportError(pContext, result);
                }
                return resource;
            }
        };

        template <int32_t Index>
        struct CommandArgUnwrapper<Index, D3D12_CPU_DESCRIPTOR_HANDLE>
        {
            D3D12_CPU_DESCRIPTOR_HANDLE operator()(const RpsCmdCallbackContext* pContext)
            {
                D3D12_CPU_DESCRIPTOR_HANDLE resourceView;
                RpsResult result = rpsD3D12GetCmdArgDescriptor(pContext, Index, &resourceView);
                if (RPS_FAILED(result))
                {
                    rpsCmdCallbackReportError(pContext, result);
                }
                return resourceView;
            }
        };
    }  // namespace details
}  // namespace rps

#endif  //__cplusplus

#endif  //RPS_D3D12_RUNTIME_H
