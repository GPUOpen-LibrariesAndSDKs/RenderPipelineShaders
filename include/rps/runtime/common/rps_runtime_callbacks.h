// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_RUNTIME_CALLBACKS_H
#define RPS_RUNTIME_CALLBACKS_H

#include "rps/runtime/common/rps_format.h"
#include "rps/runtime/common/rps_resource.h"
#include "rps/runtime/common/rps_access.h"
#include "rps/runtime/common/rps_render_states.h"

#ifdef __cplusplus
extern "C" {
#endif  //__cplusplus

RPS_DECLARE_OPAQUE_HANDLE(RpsRuntimeHeap);
RPS_DECLARE_OPAQUE_HANDLE(RpsRuntimeResource);

/// @addtogroup RpsRuntimeDevice
/// @{

/// @brief Parameters for creating a custom GPU memory heap.
typedef struct RpsRuntimeOpCreateHeapArgs
{
    uint32_t memoryTypeIndex;   ///< Index of the type of memory the allocation should be created from. API
                                ///  backend specific.
    size_t          size;       ///< Size of the heap in bytes.
    size_t          alignment;  ///< Alignment of the heap in bytes.
    const char*     debugName;  ///< Null terminated string with the debug name of the heap. Passing NULL sets no name.
    RpsRuntimeHeap* phHeap;     ///< Pointer to a handle in which the heap is returned. Must not be NULL.
} RpsRuntimeOpCreateHeapArgs;

/// @brief Parameters for destroying an array of runtime GPU memory heaps.
typedef struct RpsRuntimeOpDestroyHeapArgs
{
    uint32_t        numHeaps;   ///< Number of heaps to destroy.
    RpsRuntimeHeap* phRtHeaps;  ///< Pointer to an array of <c><i>RpsRuntimeHeap</i></c> with numHeaps heap handles to
                                ///  destroy. Must not be NULL if numHeaps != 0.
} RpsRuntimeOpDestroyHeapArgs;

/// @brief Parameters for creating a runtime resource.
typedef struct RpsRuntimeOpCreateResourceArgs
{
    RpsResourceId           resourceId;         ///< ID of the resource declaration.
    RpsResourceDesc         desc;               ///< Resource description.
    RpsVariable             originalDesc;       ///< Unmodified resource description as originally defined by the user.
    RpsClearValue           clearValue;         ///< Default value for clearing the resource.
    RpsGpuMemoryRequirement allocRequirement;   ///< GPU memory allocation requirements.
    RpsHeapPlacement        allocPlacement;     ///< Allocation placement parameters.
    RpsAccessAttr           allAccesses;        ///< Combined accesses of the resource.
    RpsAccessAttr           initialAccess;      ///< Inital access of the resource in a frame.
    uint32_t                numMutableFormats;  ///< Number of mutable formats the resource can be used with.
    RpsFormat*              mutableFormats;     ///< Pointer to an array of <c><i>RpsFormat</i></c> with
                                                ///  numMutableFormats formats the resource can be used with. Must not
                                                ///  be NULL if numMutableFormats != 0.
    RpsBool bBufferFormattedWrite;              ///< Indicator for a formatted texel buffer (maps to
                                                ///  VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT).
    RpsBool bBufferFormattedRead;               ///< Indicator for a formatted texel buffer (maps to
                                                ///  VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT)
    RpsRuntimeResource* phRuntimeResource;      ///< Pointer to a handle to the runtime resource to be returned.
} RpsRuntimeOpCreateResourceArgs;

/// @brief Parameters for destroying an array of runtime resources.
typedef struct RpsRuntimeOpDestroyResourceArgs
{
    RpsResourceType           type;                ///< Type of the resources.
    uint32_t                  numResources;        ///< Number of resources.
    const RpsRuntimeResource* phRuntimeResources;  ///< Pointer to an array of <c><i>RpsRuntimeResource</i></c> with
                                                   ///  numResources resource handles.
                                                   ///  Must not be NULL if numResource != 0.
} RpsRuntimeOpDestroyResourceArgs;

/// @brief Parameters for creating the used defined resources associated with a node.
///
/// These can be e.g. descriptor set allocations, PSOs or custom viewports.
typedef struct RpsRuntimeOpCreateNodeUserResourcesArgs
{
    void*        pUserContext;  ///< User defined context to pass to the node.
    void* const* ppArgs;        ///< Pointer to an array of <c><i>void*</i></c> with numArgs arguments to be passed to
                                ///  the node.
    uint32_t numArgs;           ///< Number of arguments of the node.
    uint32_t nodeTag;           ///< User defined node tag to be used for node associations.
} RpsRuntimeOpCreateNodeUserResourcesArgs;

/// @brief Debug marker modes.
typedef enum RpsRuntimeDebugMarkerMode
{
    RPS_RUNTIME_DEBUG_MARKER_BEGIN,  ///< Beginning of a marker region.
    RPS_RUNTIME_DEBUG_MARKER_LABEL,  ///< Standalone label.
    RPS_RUNTIME_DEBUG_MARKER_END,    ///< End of a marker region.
} RpsRuntimeDebugMarkerMode;

/// @brief Bitflags for render pass behavior.
typedef enum RpsRuntimeRenderPassFlagBits
{
    RPS_RUNTIME_RENDER_PASS_FLAG_NONE                         = 0,       ///< No special behavior.
    RPS_RUNTIME_RENDER_PASS_SUSPENDING                        = 1 << 0,  ///< Suspending render pass in D3D12.
    RPS_RUNTIME_RENDER_PASS_RESUMING                          = 1 << 1,  ///< Resuming render pass in D3D12.
    RPS_RUNTIME_RENDER_PASS_EXECUTE_SECONDARY_COMMAND_BUFFERS = 1 << 2,  ///< Render pass executes secondary command
                                                                         ///  buffers.
    RPS_RUNTIME_RENDER_PASS_SECONDARY_COMMAND_BUFFER = 1 << 3,           ///< Current render pass is on a secondary
                                                                         ///  command buffer.
} RpsRuntimeRenderPassFlagBits;

/// @brief Bitmask type for  <c><i>RpsRuntimeRenderPassFlagBits</i></c>.
typedef RpsFlags32 RpsRuntimeRenderPassFlags;

/// @brief Parameters for recording a debug marker command.
typedef struct RpsRuntimeOpRecordDebugMarkerArgs
{
    RpsRuntimeCommandBuffer hCommandBuffer;  ///< Handle to the runtime command buffer to record the command for.
                                             ///  Must not be RPS_NULL_HANDLE.
    void* pUserRecordContext;                ///< User context passed as RpsRenderGraphRecordCommandInfo::pUserContext.
    RpsRuntimeDebugMarkerMode mode;          ///< Marker mode.
    const char*               text;          ///< String payload of the marker. Ignored for
                                             ///  <c><i>RPS_RUNTIME_DEBUG_MARKER_END</i></c>.
} RpsRuntimeOpRecordDebugMarkerArgs;

/// @brief Parameters for setting a resource debug name.
typedef struct RpsRuntimeOpSetDebugNameArgs
{
    RpsRuntimeResource hResource;  ///< Handle to the runtime resource. Only support resource objects at the moment.
                                   ///  Must not be RPS_NULL_HANDLE.
    RpsResourceType resourceType;  ///< Resource type.
    const char*     name;          ///< Null terminated string with the resource name.
} RpsRuntimeOpSetDebugNameArgs;

/// @brief Signature of functions for defining an array of render graph phases.
///
/// @param pUserContext                     User defined context.
/// @param hRenderGraph                     Handle to the render graph to build the phases for.
///                                         Must not be RPS_NULL_HANDLE.
/// @param ppPhaseInfo                      Pointer to an array of <c><i>const RpsRenderGraphPhaseInfo*</i></c> in which
///                                         *pNumPhases render graph phase objects are returned. Must not be NULL.
/// @param pNumPhases                       Pointer to write the number of created phases to. Must not be NULL.
///
/// @returns                                Result code of the operation. See <c><i>RpsResult</i></c> for more info.
typedef RpsResult (*PFN_rpsRuntimeDeviceBuildRenderGraphPhases)(void*                           pUserContext,
                                                                RpsRenderGraph                  hRenderGraph,
                                                                const RpsRenderGraphPhaseInfo** ppPhaseInfo,
                                                                uint32_t*                       pNumPhases);

/// @brief Signature of functions for destroying a runtime device.
///
/// @param pUserContext                     User defined context.
typedef void (*PFN_rpsRuntimeDeviceDestroy)(void* pUserContext);

/// @brief Signature of functions for creating a runtime heap.
///
/// @param pUserContext                     User defined context.
/// @param pArgs                            Pointer to heap creation parameters. Must not be NULL.
///
/// @returns                                Result code of the operation. See <c><i>RpsResult</i></c> for more info.
typedef RpsResult (*PFN_rpsRuntimeCreateHeap)(void* pUserContext, const RpsRuntimeOpCreateHeapArgs* pArgs);

/// @brief Signature of functions for destroying runtime heaps.
///
/// @param pUserContext                     User defined context.
/// @param pArgs                            Pointer to heap destruction parameters. Must not be NULL.
typedef void (*PFN_rpsRuntimeDestroyHeap)(void* pUserContext, const RpsRuntimeOpDestroyHeapArgs* pArgs);

/// @brief Signature of functions for creating a runtime resource.
///
/// @param pUserContext                     User defined context.
/// @param pArgs                            Pointer to resource creation parameters. Must not be NULL.
///
/// @returns                                Result code of the operation. See <c><i>RpsResult</i></c> for more info.
typedef RpsResult (*PFN_rpsRuntimeCreateResource)(void* pUserContext, const RpsRuntimeOpCreateResourceArgs* pArgs);

/// @brief Signature of functions for destroying runtime resources.
///
/// @param pUserContext                     User defined context.
/// @param pArgs                            Pointer to heap destruction parameters. Must not be NULL.
typedef void (*PFN_rpsRuntimeDestroyResource)(void* pUserContext, const RpsRuntimeOpDestroyResourceArgs* pArgs);

/// @brief Creates the user defined resources associated with a node.
///
/// These can be e.g. descriptor set allocations, PSOs or custom viewports.
///
/// @param pUserContext                     User defined context.
/// @param pArgs                            Pointer to node arg creation parameters. Must not be NULL.
///
/// @returns                                Result code of the operation. See <c><i>RpsResult</i></c> for more info.
typedef RpsResult (*PFN_rpsRuntimeOpCreateNodeUserResources)(void* pUserContext,
                                                             const RpsRuntimeOpCreateNodeUserResourcesArgs* pArgs);

/// @brief Destroys the user defined resources associated with a node.
///
/// These can be e.g. descriptor set allocations, PSOs or custom viewports.
///
/// @param pUserContext                     User defined context.
typedef void (*PFN_rpsRuntimeOpDestroyNodeUserResources)(void* pUserContext);

/// @brief Signature of functions for recording runtime debug markers.
///
/// @param pUserContext                     User defined context.
/// @param pArgs                            Pointer to debug marker parameters. Must not be NULL.
typedef void (*PFN_rpsRuntimeOpRecordDebugMarker)(void* pUserContext, const RpsRuntimeOpRecordDebugMarkerArgs* pArgs);

/// @brief Signature of functions for setting runtime debug names.
///
/// @param pUserContext                     User defined context.
/// @param pArgs                            Pointer to debug name parameters. Must not be NULL.
typedef void (*PFN_rpsRuntimeOpSetDebugName)(void* pUserContext, const RpsRuntimeOpSetDebugNameArgs* pArgs);

/// @brief Callback functions of a runtime.
typedef struct RpsRuntimeCallbacks
{
    PFN_rpsRuntimeDeviceBuildRenderGraphPhases pfnBuildRenderGraphPhases;  ///< Render graph phase build callback.
    PFN_rpsRuntimeDeviceDestroy                pfnDestroyRuntime;          ///< Runtime destruction callback.
    PFN_rpsRuntimeCreateHeap                   pfnCreateHeap;              ///< Heap creation callback.
    PFN_rpsRuntimeDestroyHeap                  pfnDestroyHeap;             ///< Heap destruction callback.
    PFN_rpsRuntimeCreateResource               pfnCreateResource;          ///< Resource creation callback.
    PFN_rpsRuntimeDestroyResource              pfnDestroyResource;         ///< Resource destruction callback.
    PFN_rpsRuntimeOpCreateNodeUserResources    pfnCreateNodeResources;     ///< Node resource creation callback.
    PFN_rpsRuntimeOpDestroyNodeUserResources   pfnDestroyNodeResources;    ///< Node argument destruction callback.
    PFN_rpsRuntimeOpRecordDebugMarker          pfnRecordDebugMarker;       ///< Debug marker recording callback.
    PFN_rpsRuntimeOpSetDebugName               pfnSetDebugName;            ///< Debug name setting callback.
} RpsRuntimeCallbacks;

/// @} end addtogroup RpsRuntimeDevice

#ifdef __cplusplus
}
#endif  //__cplusplus

#endif  //RPS_RUNTIME_CALLBACKS_H
