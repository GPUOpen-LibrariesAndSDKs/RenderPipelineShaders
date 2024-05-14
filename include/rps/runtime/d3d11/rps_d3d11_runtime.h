// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_D3D11_RUNTIME_H
#define RPS_D3D11_RUNTIME_H

#include "rps/core/rps_api.h"
#include "rps/runtime/common/rps_runtime.h"
#include "rps/runtime/d3d_common/rps_d3d_common.h"

#include <d3d11_1.h>

/// @addtogroup RpsD3D11RuntimeDevice
/// @{

/// @brief Bitflags for D3D11 runtime behavior.
///
/// For future use.
typedef enum RpsD3D11RuntimeFlagBits
{
    RPS_D3D11_RUNTIME_FLAG_NONE = 0,  ///< No runtime flags.
} RpsD3D11RuntimeFlagBits;

/// @brief Bitmask type for <c><i>RpsD3D11RuntimeFlagBits</i></c>.
typedef uint32_t RpsD3D11RuntimeFlags;

/// @brief Creation parameters for an RPS device with D3D11 backend.
typedef struct RpsD3D11RuntimeDeviceCreateInfo
{
    const RpsDeviceCreateInfo* pDeviceCreateInfo;          ///< Pointer to general RPS device creation parameters.
                                                           ///  Passing NULL uses default parameters instead.
    const RpsRuntimeDeviceCreateInfo* pRuntimeCreateInfo;  ///< Pointer to general RPS runtime creation info. Passing
                                                           ///  NULL uses default parameters instead.
    ID3D11Device* pD3D11Device;                            ///< Pointer to the D3D11 device to use for the runtime.
                                                           ///  Must not be NULL.
    RpsD3D11RuntimeFlags flags;                            ///< D3D11 runtime flags.
} RpsD3D11RuntimeDeviceCreateInfo;

/// @} end addtogroup RpsD3D11RuntimeDevice

#ifdef __cplusplus
extern "C" {
#endif  //__cplusplus

/// @brief Creates a D3D11 runtime device.
///
/// @param pCreateInfo                  Pointer to creation parameters. Must not be NULL.
/// @param phDevice                     Pointer to a handle in which the device is returned. Must not be NULL.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
/// @ingroup RpsD3D11RuntimeDevice
RpsResult rpsD3D11RuntimeDeviceCreate(const RpsD3D11RuntimeDeviceCreateInfo* pCreateInfo, RpsDevice* phDevice);

/// @addtogroup RpsRenderGraphCommandRecordingD3D11
/// @{

/// @brief Mapping between <c><i>RpsRuntimeCommandBuffer</i></c> and <c><i>ID3D11DeviceContext*</i></c>.
RPS_IMPL_OPAQUE_HANDLE(D3D11DeviceContext, RpsRuntimeCommandBuffer, ID3D11DeviceContext);

/// @brief Mapping between <c><i>RpsRuntimeResource</i></c> and <c><i>ID3D11Resource*</i></c>.
RPS_IMPL_OPAQUE_HANDLE(D3D11Resource, RpsRuntimeResource, ID3D11Resource);

/// @brief Gets an array of resource pointers from a resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the resource pointers from. Must be a resource
///                                     array argument if numResources > 1.
/// @param srcArrayOffset               Offset to the first resource pointer to get.
/// @param ppResources                  Pointer to an array of <c><i>ID3D11Resource*</i></c> in which the numResources
///                                     resource pointers are returned. Must not be NULL if numResources != 0.
/// @param numResources                 Number of resource pointers to get. Requires srcArrayOffset + numResources to be
///                                     less than the number of elements in the node argument.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsD3D11GetCmdArgResourceArray(const RpsCmdCallbackContext* pContext,
                                         uint32_t                     argIndex,
                                         uint32_t                     srcArrayOffset,
                                         ID3D11Resource**             ppResources,
                                         uint32_t                     numResources);

/// @brief Get a resource pointer from a resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the resource pointer from. Must be a resource
///                                     argument.
/// @param ppResource                   Pointer in which the resource pointer is returned. Must not be NULL.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsD3D11GetCmdArgResource(const RpsCmdCallbackContext* pContext,
                                    uint32_t                     argIndex,
                                    ID3D11Resource**             ppResource);

/// @brief Gets an array of render target view pointers from a resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the render target view pointers from. Must be a
///                                     resource array argument if numRTVs > 1.
/// @param srcArrayOffset               Offset to the first render target view pointer to get.
/// @param ppRTVs                       Pointer to an array of <c><i>ID3D11RenderTargetView*</i></c> in which the
///                                     numRTVs render target view pointers are returned. Must not be NULL if
///                                     numRTVs != 0.
/// @param numRTVs                      Number of render target view pointers to get. Requires srcArrayOffset + numRTVs
///                                     to be less than the number of elements in the node argument.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsD3D11GetCmdArgRTVArray(const RpsCmdCallbackContext* pContext,
                                    uint32_t                     argIndex,
                                    uint32_t                     srcArrayOffset,
                                    ID3D11RenderTargetView**     ppRTVs,
                                    uint32_t                     numRTVs);

/// @brief Get a render target view pointer from a resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the render target view from. Must be a
///                                     resource argument.
/// @param ppRTV                        Pointer in which the render target view pointer is returned. Must not be NULL.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsD3D11GetCmdArgRTV(const RpsCmdCallbackContext* pContext,
                               uint32_t                     argIndex,
                               ID3D11RenderTargetView**     ppRTV);

/// @brief Gets an array of depth stencil view pointers from a depth stencil image node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the depth stencil views from. Must be a depth
///                                     stencil image array argument if numDSVs > 1.
/// @param srcArrayOffset               Offset to the first depth stencil view pointer to get.
/// @param ppDSVs                       Pointer to an array of <c><i>ID3D11DepthStencilView*</i></c> in which the
///                                     numDSVs depth stencil view pointers are returned. Must not be NULL if
///                                     numDSVs != 0.
/// @param numDSVs                      Number of depth stencil view pointers to get. Requires
///                                     srcArrayOffset + numDSVs to be less than the number of elements in the node
///                                     argument.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsD3D11GetCmdArgDSVArray(const RpsCmdCallbackContext* pContext,
                                    uint32_t                     argIndex,
                                    uint32_t                     srcArrayOffset,
                                    ID3D11DepthStencilView**     ppDSVs,
                                    uint32_t                     numDSVs);

/// @brief Get a depth stencil view pointer from a depth stencil image node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the depth stencil view from. Must be a depth
///                                     stencil image argument.
/// @param ppDSV                        Pointer in which the depth stencil view pointer is returned. Must not be NULL.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsD3D11GetCmdArgDSV(const RpsCmdCallbackContext* pContext,
                               uint32_t                     argIndex,
                               ID3D11DepthStencilView**     ppDSV);

/// @brief Gets an array of shader resource view pointers from a resource array node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the shader resource views from. Must be a resource
///                                     array argument if numSRVs > 1.
/// @param srcArrayOffset               Offset to the first shader resource view to get.
/// @param ppSRVs                       Pointer in which array of <c><i>ID3D11ShaderResourceView*</i></c> in which the
///                                     numSRVs shader resource view pointers are returned. Must not be NULL if
///                                     numSRVs != 0.
/// @param numSRVs                      Number of shader resource view pointers to get. Requires
///                                     srcArrayOffset + numSRVs to be less than the number of elements in the node
///                                     argument.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsD3D11GetCmdArgSRVArray(const RpsCmdCallbackContext* pContext,
                                    uint32_t                     argIndex,
                                    uint32_t                     srcArrayOffset,
                                    ID3D11ShaderResourceView**   ppSRVs,
                                    uint32_t                     numSRVs);

/// @brief Get a shader resource view pointer from a resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the shader resource view from. Must be a resource
///                                     argument.
/// @param ppSRV                        Pointer in which the shader resource view pointer is returned. Must not be NULL.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsD3D11GetCmdArgSRV(const RpsCmdCallbackContext* pContext,
                               uint32_t                     argIndex,
                               ID3D11ShaderResourceView**   ppSRV);

/// @brief Gets an array of unordered access view pointers from a resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the unordered access views from. Must be a resource
///                                     array argument if numUAVs > 1.
/// @param srcArrayOffset               Offset to the first unordered access view pointer to get.
/// @param ppUAVs                       Pointer to an array of <c><i>ID3D11UnorderedAccessView*</i></c> in which the
///                                     numUAVs unordered access view pointers are returned. Must not be NULL if
///                                     numUAVs != 0.
/// @param numUAVs                      Number of unordered access view pointers to get. Requires
///                                     srcArrayOffset + numUAVs to be less than the number of elements in the node
///                                     argument.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsD3D11GetCmdArgUAVArray(const RpsCmdCallbackContext* pContext,
                                    uint32_t                     argIndex,
                                    uint32_t                     srcArrayOffset,
                                    ID3D11UnorderedAccessView**  ppUAVs,
                                    uint32_t                     numUAVs);

/// @brief Get an unordered access view pointer from a resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the unordered access view from. Must be a resource
///                                     argument.
/// @param ppUAV                        Pointer in which the unordered access view pointer is returned. Must not be NULL.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsD3D11GetCmdArgUAV(const RpsCmdCallbackContext* pContext,
                               uint32_t                     argIndex,
                               ID3D11UnorderedAccessView**  ppUAV);

/// @} end addtogroup RpsRenderGraphCommandRecordingD3D11

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
        struct CommandArgUnwrapper<Index, ID3D11Resource*>
        {
            ID3D11Resource* operator()(const RpsCmdCallbackContext* pContext)
            {
                ID3D11Resource* resource = nullptr;
                RpsResult result = rpsD3D11GetCmdArgResource(pContext, Index, &resource);
                if (RPS_FAILED(result))
                {
                    rpsCmdCallbackReportError(pContext, result);
                }
                return resource;
            }
        };

        template <int32_t Index>
        struct CommandArgUnwrapper<Index, ID3D11Texture1D*>
        {
            ID3D11Texture1D* operator()(const RpsCmdCallbackContext* pContext)
            {
                ID3D11Resource* resource = nullptr;
                RpsResult result = rpsD3D11GetCmdArgResource(pContext, Index, &resource);
                if (RPS_FAILED(result))
                {
                    rpsCmdCallbackReportError(pContext, result);
                }
                return static_cast<ID3D11Texture1D*>(resource);
            }
        };

        template <int32_t Index>
        struct CommandArgUnwrapper<Index, ID3D11Texture2D*>
        {
            ID3D11Texture2D* operator()(const RpsCmdCallbackContext* pContext)
            {
                ID3D11Resource* resource = nullptr;
                RpsResult       result   = rpsD3D11GetCmdArgResource(pContext, Index, &resource);
                if (RPS_FAILED(result))
                {
                    rpsCmdCallbackReportError(pContext, result);
                }
                return static_cast<ID3D11Texture2D*>(resource);
            }
        };

        template <int32_t Index>
        struct CommandArgUnwrapper<Index, ID3D11Texture3D*>
        {
            ID3D11Texture3D* operator()(const RpsCmdCallbackContext* pContext)
            {
                ID3D11Resource* resource = nullptr;
                RpsResult       result   = rpsD3D11GetCmdArgResource(pContext, Index, &resource);
                if (RPS_FAILED(result))
                {
                    rpsCmdCallbackReportError(pContext, result);
                }
                return static_cast<ID3D11Texture3D*>(resource);
            }
        };

        template <int32_t Index>
        struct CommandArgUnwrapper<Index, ID3D11Buffer*>
        {
            ID3D11Buffer* operator()(const RpsCmdCallbackContext* pContext)
            {
                ID3D11Resource* resource = nullptr;
                RpsResult       result   = rpsD3D11GetCmdArgResource(pContext, Index, &resource);
                if (RPS_FAILED(result))
                {
                    rpsCmdCallbackReportError(pContext, result);
                }
                return static_cast<ID3D11Buffer*>(resource);
            }
        };

        template <int32_t Index>
        struct CommandArgUnwrapper<Index, ID3D11ShaderResourceView*>
        {
            ID3D11ShaderResourceView* operator()(const RpsCmdCallbackContext* pContext)
            {
                ID3D11ShaderResourceView* pSRV;
                RpsResult                 result = rpsD3D11GetCmdArgSRV(pContext, Index, &pSRV);
                if (RPS_FAILED(result))
                {
                    rpsCmdCallbackReportError(pContext, result);
                }
                return pSRV;
            }
        };

        template <int32_t Index>
        struct CommandArgUnwrapper<Index, ID3D11UnorderedAccessView*>
        {
            ID3D11UnorderedAccessView* operator()(const RpsCmdCallbackContext* pContext)
            {
                ID3D11UnorderedAccessView* pUAV;
                RpsResult                  result = rpsD3D11GetCmdArgUAV(pContext, Index, &pUAV);
                if (RPS_FAILED(result))
                {
                    rpsCmdCallbackReportError(pContext, result);
                }
                return pUAV;
            }
        };
    }  // namespace details
}  // namespace rps

#endif  //__cplusplus

#endif  //RPS_D3D11_RUNTIME_H
