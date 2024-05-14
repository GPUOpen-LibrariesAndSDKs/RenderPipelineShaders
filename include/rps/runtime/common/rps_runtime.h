// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_RUNTIME_H
#define RPS_RUNTIME_H

#include "rps/runtime/common/rps_format.h"
#include "rps/runtime/common/rps_resource.h"
#include "rps/runtime/common/rps_access.h"
#include "rps/runtime/common/rps_render_states.h"

/// @defgroup Runtime Runtime
/// @{

/// @defgroup RpsRuntimeDevice               RpsRuntimeDevice
/// @defgroup RpsVKRuntimeDevice             RpsVKRuntimeDevice
/// @defgroup RpsD3D12RuntimeDevice          RpsD3D12RuntimeDevice
/// @defgroup RpsD3D11RuntimeDevice          RpsD3D11RuntimeDevice
/// @defgroup RpsRenderGraphRuntime          RpsRenderGraph Runtime
/// @defgroup RpsRenderGraphRuntimeResources RpsRenderGraph Runtime Resources
/// @defgroup RpsRenderGraphCommandRecording RpsRenderGraph Command Recording
/// @defgroup RpsSubprogram                  RpsSubprogram

#ifdef __cplusplus
extern "C" {
#endif  //__cplusplus

/// @brief Handle type for RPS runtime device objects.
///
/// @ingroup RpsRuntimeDevice
RPS_DEFINE_HANDLE(RpsRuntimeDevice);

/// @addtogroup RpsRenderGraphRuntime
/// @{

/// @brief Handle type for RPS render graph objects.
RPS_DEFINE_HANDLE(RpsRenderGraph);

/// @brief Handle type for the render graph builder objects.
RPS_DEFINE_HANDLE(RpsRenderGraphBuilder);

/// @brief Handle type for RPS render graph phase objects.
RPS_DEFINE_HANDLE(RpsRenderGraphPhase);

/// @brief Handle type for RPS subprogram objects.
///
/// Can be used as either main entry or a node implementation in a render graph.
RPS_DEFINE_HANDLE(RpsSubprogram);

/// @} end addtogroup RpsRenderGraphRuntime

/// @addtogroup RpsRenderGraphRuntimeResources
/// @{

/// @brief Opaque handle type for RPS runtime heap objects.
RPS_DEFINE_OPAQUE_HANDLE(RpsRuntimeHeap);

/// @brief Opaque handle type for RPS runtime resource objects.
RPS_DEFINE_OPAQUE_HANDLE(RpsRuntimeResource);

/// @brief Opaque handle type for runtime command buffer objects.
RPS_DEFINE_OPAQUE_HANDLE(RpsRuntimeCommandBuffer);

/// @} end addtogroup RpsRenderGraphRuntimeResources

/// @addtogroup RpsRenderGraphRuntime
/// @{

/// @defgroup RpsParamAttr RpsParamAttr
/// @{

/// @brief Node parameter attribute.
typedef struct RpsParamAttr
{
    RpsAccessAttr   access;    ///< Access attribute of the parameter.
    RpsSemanticAttr semantic;  ///< Semantic attribute of the parameter.
} RpsParamAttr;

/// @brief Handle type for an object describing a number of render graph node parameter attributes.
RPS_DEFINE_HANDLE(RpsParamAttrList);

/// @} end defgroup RpsParamAttr

/// @brief Handle type for an object describing a number of render graph node attributes.
RPS_DEFINE_HANDLE(RpsNodeAttrList);

/// @brief Bitflags for scheduling behavior.
typedef enum RpsScheduleFlagBits
{
    /// No schedule flag bits are specified. Default options are used. When used as
    /// RpsRenderGraphUpdateInfo::scheduleFlags, the RpsRenderGraphCreateInfo::scheduleInfo::scheduleFlags specified
    /// at render graph creation time are used instead.
    RPS_SCHEDULE_UNSPECIFIED = (0),

    /// Command nodes are kept in the program order.
    RPS_SCHEDULE_KEEP_PROGRAM_ORDER_BIT = (1 << 0),

    /// Schedules in favor of reducing total GPU memory usage. Possible strategies include minimizing transient resource
    /// lifetimes and aggressive aliasing. This may increase the number of barriers generated.
    RPS_SCHEDULE_PREFER_MEMORY_SAVING_BIT = (1 << 1),

    /// Schedules commands randomly (without changing program logic). Mostly useful for testing purposes. Applications
    /// should normally avoid using this flag for end-user scenarios. If RPS_SCHEDULE_KEEP_PROGRAM_ORDER_BIT is set,
    /// this flag will have no effect.
    RPS_SCHEDULE_RANDOM_ORDER_BIT = (1 << 2),

    /// Avoids alternating between graphics and compute work on the same queue. This can help for some architectures
    /// where switching between graphics and compute produces extra overhead.
    RPS_SCHEDULE_MINIMIZE_COMPUTE_GFX_SWITCH_BIT = (1 << 3),

    /// Disables dead code elimination optimization. By default, RPS removes nodes that have no visible effect (Not
    /// contributing to modification of external, temporal, persistent or CPU resources). This flag disables this
    /// optimization.
    RPS_SCHEDULE_DISABLE_DEAD_CODE_ELIMINATION_BIT = (1 << 4),

    /// Disables work pipelining based on the workload type.
    RPS_SCHEDULE_WORKLOAD_TYPE_PIPELINING_DISABLE_BIT = (1 << 5),

    /// Performs aggressive work pipelining based on the workload type. If
    /// RPS_SCHEDULE_WORKLOAD_TYPE_PIPELINING_DISABLE_BIT is set, this flag will have no effect.
    RPS_SCHEDULE_WORKLOAD_TYPE_PIPELINING_AGGRESSIVE_BIT = (1 << 6),

    // Reserved for future use:

    /// Reserved for future use. Includes split barriers where appropriate.
    RPS_SCHEDULE_ALLOW_SPLIT_BARRIERS_BIT = (1 << 16),

    /// Reserved for future use. Avoids rescheduling if possible and uses the existing schedule instead.
    RPS_SCHEDULE_AVOID_RESCHEDULE_BIT = (1 << 17),

    /// Reserved for future use. Allows work to overlap between multiple frames.
    RPS_SCHEDULE_ALLOW_FRAME_OVERLAP_BIT = (1 << 21),

    /// Reserved for future use. Tries to use render pass transitions instead of standalone transition nodes when
    /// possible. If RPS_SCHEDULE_DISABLE_RENDERPASS_TRANSITIONS_BIT is set, this flag will have no effect.
    RPS_SCHEDULE_PREFER_RENDERPASS_TRANSITIONS_BIT = (1 << 22),

    /// Reserved for future use. Uses standalone transition nodes instead of render pass transitions.
    RPS_SCHEDULE_DISABLE_RENDERPASS_TRANSITIONS_BIT = (1 << 23),

    // End reserved for future use. 

    /// Uses default options. This is identical to RPS_SCHEDULE_UNSPECIFIED in most cases, except when used as
    /// RpsRenderGraphUpdateInfo::scheduleFlags, instead using the default options regardless of
    /// RpsRenderGraphCreateInfo::scheduleInfo::scheduleFlags. This default behavior is a baseline set of criteria used
    /// for scheduling to which these flags can add additional ones.
    RPS_SCHEDULE_DEFAULT = (1 << 30),

    /// Prioritizes application performance over a lower memory footprint.
    RPS_SCHEDULE_DEFAULT_PERFORMANCE = RPS_SCHEDULE_DEFAULT,

    /// Prioritizes a lower memory footprint over performance.
    RPS_SCHEDULE_DEFAULT_MEMORY = RPS_SCHEDULE_PREFER_MEMORY_SAVING_BIT,
} RpsScheduleFlagBits;

/// @brief Bitmask type for <c><i>RpsScheduleFlagBits</i></c>.
typedef RpsFlags32 RpsScheduleFlags;

/// @brief Bitflags for enabling diagnostic systems.
typedef enum RpsDiagnosticFlagBits
{
    RPS_DIAGNOSTIC_NONE                     = 0,         ///< No diagnostic mode enabled.
    RPS_DIAGNOSTIC_ENABLE_PRE_SCHEDULE_DUMP = 1 << 0,    ///< Dumps the resources and commands of a render graph
                                                         ///  before optimization through the scheduler.
    RPS_DIAGNOSTIC_ENABLE_POST_SCHEDULE_DUMP = 1 << 1,   ///< Dumps the commands of the render graph after
                                                         ///  optimization through the scheduler.
    RPS_DIAGNOSTIC_ENABLE_DAG_DUMP = 1 << 2,             ///< Dumps the directed acyclic graph of nodes defined
                                                         ///  by the render graph in graphviz format.
    RPS_DIAGNOSTIC_ENABLE_SOURCE_LOCATION = 1 << 3,      ///< Inserts source code location debug data for
                                                         ///  resource definitions and node calls.
    RPS_DIAGNOSTIC_ENABLE_RUNTIME_DEBUG_NAMES = 1 << 4,  ///< Sets resource names as debug names in the graphics
                                                         ///  API in use.

    RPS_DIAGNOSTIC_ENABLE_ALL = (1 << 5) - 1,  ///< Enable all flags.
} RpsDiagnosticFlagBits;

/// @brief Bitmask type for <c><i>RpsDiagnosticFlagBits</i></c>.
typedef RpsFlags32 RpsDiagnosticFlags;

/// @brief Bitflags for special render graph properties.
typedef enum RpsRenderGraphFlagBits
{
    RPS_RENDER_GRAPH_FLAG_NONE                  = 0,       ///< No special properties.
    RPS_RENDER_GRAPH_DISALLOW_UNBOUND_NODES_BIT = 1 << 0,  ///< Disallows unbound nodes if no default callback is set.
    RPS_RENDER_GRAPH_NO_GPU_MEMORY_ALIASING     = 1 << 1,  ///< Disables GPU memory aliasing.
    RPS_RENDER_GRAPH_NO_LIFETIME_ANALYSIS       = 1 << 2,  ///< Disables lifetime analysis unless required by other
                                                           ///  core features, e.g. memory aliasing.
} RpsRenderGraphFlagBits;

/// @brief Bitmask type for <c><i>RpsRenderGraphFlagBits</i></c>.
typedef RpsFlags32 RpsRenderGraphFlags;

/// @brief Constant for the maximum number of hardware queues in use by RPS.
#define RPS_MAX_QUEUES (8)

/// @brief Bitflags for properties of a render graph node declaration.
///
/// These are e.g. required queue type, async preferences, etc. All usages of a node will use the same set of
/// properties the node was declared with.
typedef enum RpsNodeDeclFlagBits
{
    RPS_NODE_DECL_FLAG_NONE          = 0,       ///< No node declaration properties.
    RPS_NODE_DECL_GRAPHICS_BIT       = 1 << 0,  ///< Node requires a queue with graphics capabilites.
    RPS_NODE_DECL_COMPUTE_BIT        = 1 << 1,  ///< Node requires a queue with compute capabilities.
    RPS_NODE_DECL_COPY_BIT           = 1 << 2,  ///< Node requires a queue with copy capabilities.
    RPS_NODE_DECL_PREFER_RENDER_PASS = 1 << 3,  ///< Node prefers to be executed as a render pass if the API backend
                                                ///  supports it.
    RPS_NODE_DECL_PREFER_ASYNC = 1 << 4,        ///< Node prefers to be executed asynchronously.
} RpsNodeDeclFlagBits;

/// @brief Bitmask type for <c><i>RpsNodeDeclFlagBits</i></c>.
typedef RpsFlags32 RpsNodeDeclFlags;

/// @brief Bitflags for decorating node parameters.
typedef enum RpsParameterFlagBits
{
    RPS_PARAMETER_FLAG_NONE         = 0,       ///< No bit flags.
    RPS_PARAMETER_FLAG_OUT_BIT      = 1 << 0,  ///< Node parameter is an output parameter.
    RPS_PARAMETER_FLAG_OPTIONAL_BIT = 1 << 1,  ///< Node parameter is optional.
    RPS_PARAMETER_FLAG_RESOURCE_BIT = 1 << 2,  ///< Node parameter is an RPS resource.
} RpsParameterFlagBits;

/// @brief Bitmask type for <c><i>RpsParameterFlagBits</i></c>.
typedef uint32_t RpsParameterFlags;

/// @brief Bitflags for command callback properties.
///
/// These flags control the graphics state setup and teardown behavior that occurs before entering and after
/// exiting the callback.
typedef enum RpsCmdCallbackFlagBits
{
    /// No callback properties.
    RPS_CMD_CALLBACK_FLAG_NONE = 0,

    /// Skips default render target / depth stencil buffer setup, even if any were specified in the node parameter
    /// semantics.
    RPS_CMD_CALLBACK_CUSTOM_RENDER_TARGETS_BIT = 1 << 0,

    /// Skips viewport and scissor rect setup. Used when the command callback will do the setup instead.
    RPS_CMD_CALLBACK_CUSTOM_VIEWPORT_SCISSOR_BIT = 1 << 1,

    /// Skips render state & resource binding setup other than render targets (including depth stencil buffer) and
    /// viewport (including scissor rects).
    RPS_CMD_CALLBACK_CUSTOM_STATE_SETUP_BIT = 1 << 2,

    /// Skips all setup.
    RPS_CMD_CALLBACK_CUSTOM_ALL = RPS_CMD_CALLBACK_CUSTOM_RENDER_TARGETS_BIT |
                                  RPS_CMD_CALLBACK_CUSTOM_VIEWPORT_SCISSOR_BIT | RPS_CMD_CALLBACK_CUSTOM_STATE_SETUP_BIT
} RpsCmdCallbackFlagBits;

/// @brief Bitmask type for <c><i>RpsCmdCallbackFlagBits</i></c>.
typedef RpsFlags32 RpsCmdCallbackFlags;

/// @brief Type for command callback contexts.
typedef struct RpsCmdCallbackContext RpsCmdCallbackContext;

/// @brief Signature of render graph node callbacks.
///
/// @param pContext                     Context for the command callback.
typedef void (*PFN_rpsCmdCallback)(const RpsCmdCallbackContext* pContext);

/// @brief Command callback with usage parameters.
typedef struct RpsCmdCallback
{
    PFN_rpsCmdCallback  pfnCallback;   ///< Pointer to a callback function.
    void*               pUserContext;  ///< User context to be passed to the callback.
    RpsCmdCallbackFlags flags;         ///< Flags for the callback.
} RpsCmdCallback;

/// @brief Parameters for describing a node call parameter.
typedef struct RpsParameterDesc
{
    RpsTypeInfo typeInfo;     ///< Type info of the parameter.
    uint32_t    arraySize;    ///< Number of array elements for this parameter. 0 indicates not an array (single
                              ///  element). UINT32_MAX indicates an unbounded array.
    RpsConstant       attr;   ///< Pointer to a runtime defined structure with attributes of the parameter.
    const char*       name;   ///< Null terminated string with the name of the parameter.
    RpsParameterFlags flags;  ///< Parameter type flags.
} RpsParameterDesc;

/// @brief Parameters for describing a render graph node.
typedef struct RpsNodeDesc
{
    RpsNodeDeclFlags        flags;        ///< Flags for the type of render graph node.
    uint32_t                numParams;    ///< Number of parameters used in the callback.
    const RpsParameterDesc* pParamDescs;  ///< Pointer to an array of <c><i>const RpsParameterDesc</i></c> with
                                          ///  numParams parameter descriptions. Must not be NULL if numParams != 0.
    const char* name;                     ///< Null terminated string with the name of the callback.
} RpsNodeDesc;

/// @brief Parameters for describing a render graph signature.
typedef struct RpsRenderGraphSignatureDesc
{
    uint32_t numParams;                   ///< Number of parameters in the signature.
    uint32_t numNodeDescs;                ///< Number of node descriptions in the signature.
    uint32_t maxExternalResources;        ///< Number of resources in the parameters of the signature. Array parameters
                                          ///  contribute with their size towards this number.
    const RpsParameterDesc* pParamDescs;  ///< Pointer to an array of <c><i>const RpsParameterDesc</i></c> with
                                          ///  numParams parameters for the signature. Must not be NULL if
                                          ///  numParams != 0.
    const RpsNodeDesc* pNodeDescs;        ///< Pointer to an array of <c><i>const RpsNodeDesc</i></c> with numNodeDescs
                                          ///  node descriptions for the signature. Must not be NULL if numParams != 0.
    const char* name;                     ///< Null terminated string with the name of the render graph.
} RpsRenderGraphSignatureDesc;

/// @brief Reports an error from a command callback context.
///
/// @param pContext                         Pointer to the context. Must be the primary context (passed to the command
///                                         callback as argument). This function fails if the context is a secondary
///                                         context (created via rpsCmdCloneContext). Must not be NULL.
///
/// @param errorCode                        Error code for the type of error to report. For errorCode == RPS_OK, this
///                                         function does nothing.
///
/// @returns                                Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsCmdCallbackReportError(const RpsCmdCallbackContext* pContext, RpsResult errorCode);

/// @brief Bitflags for node instance properties.
typedef enum RpsNodeFlagBits
{
    RPS_NODE_FLAG_NONE    = 0,       ///< No node instance properties.
    RPS_NODE_PREFER_ASYNC = 1 << 1,  ///< Node prefers to be executed asynchronously.
} RpsNodeFlagBits;

/// @brief Bitmask type for  <c><i>RpsNodeFlagBits</i></c> of properties for a render graph node instance.
///
/// While RpsNodeDeclFlags apply to all instances which share the same node declaration, RpsNodeFlags apply to one
/// specific node instance.
typedef RpsFlags32 RpsNodeFlags;

/// @brief Signature of functions for render graph building.
///
/// @param hBuilder                             Handle to the render graph builder to use.
/// @param pArgs                                Pointer to an array of <c><i>RpsConstant</i></c> with numArgs constant
///                                             arguments to use for building. Must not be NULL if numArgs != 0.
/// @param numArgs                              Number of constant arguments to use for building.
///
/// @returns                                    Result code of the operation. See <c><i>RpsResult</i></c> for more info.
typedef RpsResult (*PFN_rpsRenderGraphBuild)(RpsRenderGraphBuilder hBuilder,
                                             const RpsConstant*    pArgs,
                                             uint32_t              numArgs);

/// @brief Parameters for updating a render graph.
///
/// @relates RpsRenderGraph
typedef struct RpsRenderGraphUpdateInfo
{
    /// Index of the current frame to be recorded after the update.
    uint64_t frameIndex;

    /// Index of the last frame that finished executing on the GPU. Used for resource lifetime management.
    uint64_t gpuCompletedFrameIndex;

    /// Flags for scheduling behavior. Used for overriding flags specified at creation.
    RpsScheduleFlags scheduleFlags;

    /// Flags for enabling diagnostics systems during the render graph update.
    RpsDiagnosticFlags diagnosticFlags;

    /// Number of arguments to pass to the entry. Passing less than the number of values from the render graph entry
    /// updates only the first numArgs arguments and does not touch any other argument.
    uint32_t numArgs;

    /// Pointer to an array of <c><i>RpsConstant</i></c> with numArgs constant arguments to pass to the entry.
    /// Must not be NULL if numArgs != 0.
    const RpsConstant* ppArgs;

    /// Pointer to an array of <c><i>const RpsRuntimeResource* const</i></c> with pointers to externally managed
    /// resources used in the render graph. Resource arguments in ppArgs have a corresponding runtime resource (or
    /// array) in ppArgResources, at the same index. If e.g. {&backBufferResourceDescription, value} is passed for
    /// ppArgs, ppArgResources[0] would have to point to the corresponding <c><i>RpsRuntimeResource</i></c> of the
    /// backbuffer.
    const RpsRuntimeResource* const* ppArgResources;

    /// Pointer to a function for starting a user-defined render graph building process. Passing NULL uses the default
    /// one.
    PFN_rpsRenderGraphBuild pfnBuildCallback;

    /// Pointer to a random number generator. Only required if any randomized behavior is used, e.g.
    /// RPS_SCHEDULE_RANDOM_ORDER_BIT.
    const RpsRandomNumberGenerator* pRandomNumberGenerator;
} RpsRenderGraphUpdateInfo;

/// @brief Constant for the maximum number of supported frames which can be queued on the GPU simultaneously.
#define RPS_MAX_QUEUED_FRAMES (16)

/// @brief Special frame index value, when passed as RpsRenderGraphUpdateInfo::gpuCompletedFrameIndex,
///        indicates that no frames are known to have finished executing on the GPU yet.

#define RPS_GPU_COMPLETED_FRAME_INDEX_NONE (UINT64_MAX)

/// @brief Signature of functions for executing a render graph phase.
///
/// @param hRenderGraph             Handle to the render graph to execute the phase for.
/// @param pUpdateInfo              Pointer to update parameters.
/// @param phase                    Handle to the render graph phase object.
///
/// @returns                        Result code of the operation. See <c><i>RpsResult</i></c> for more info.
typedef RpsResult (*PFN_rpsRenderGraphPhaseRun)(RpsRenderGraph                  hRenderGraph,
                                                const RpsRenderGraphUpdateInfo* pUpdateInfo,
                                                RpsRenderGraphPhase             phase);

/// @brief Signature of functions for destroying a render graph phase object.
///
/// @param phase                    Handle to the render graph phase object.
///
/// @returns                        Result code of the operation. See <c><i>RpsResult</i></c> for more info.
typedef RpsResult (*PFN_rpsRenderGraphPhaseDestroy)(RpsRenderGraphPhase phase);

/// @brief Parameters of a render graph processing phase.
typedef struct RpsRenderGraphPhaseInfo
{
    RpsRenderGraphPhase            hPhase;      ///< Handle to the render graph phase object.
    PFN_rpsRenderGraphPhaseRun     pfnRun;      ///< Pointer to a function for executing the render graph phase.
    PFN_rpsRenderGraphPhaseDestroy pfnDestroy;  ///< Pointer to a function for destroying the render graph phase.
} RpsRenderGraphPhaseInfo;

/// @} end addtogroup RpsRenderGraphRuntime

/// @addtogroup RpsRenderGraphRuntimeResources
/// @{

/// @brief Parameters of a memory type.
typedef struct RpsMemoryTypeInfo
{
    uint64_t defaultHeapSize;  ///< Default size for creating a heap of this type.
    uint32_t minAlignment;     ///< Minimum alignment for heaps of this memory type in bytes.
} RpsMemoryTypeInfo;

/// @brief Required parameters for a GPU memory allocation.
typedef struct RpsGpuMemoryRequirement
{
    uint64_t   size;             ///< Size of the allocation in bytes.
    uint32_t   alignment;        ///< Minimum alignment required by the allocation in bytes.
    RpsIndex32 memoryTypeIndex;  ///< Index for the type of memory the allocation should be created from. API backend
                                 ///  specific.
} RpsGpuMemoryRequirement;

/// @brief Type for heap identifiers.
///
/// Internally, these are used as simple indices.
typedef RpsIndex32 RpsHeapId;

/// @brief Parameters of a resource placement inside a heap.
typedef struct RpsHeapPlacement
{
    RpsHeapId heapId;  ///< ID of the heap in the render graph.
    uint64_t  offset;  ///< Offset of the resource placement inside the heap in bytes.
} RpsHeapPlacement;

/// @brief Parameters of a runtime resource.
typedef struct RpsRuntimeResourceInfo
{
    RpsRuntimeResource      hResource;        ///< Handle to the resource created by the API backend.
    RpsResourceDesc         resourceDesc;     ///< Resource description.
    uint32_t                numSubresources;  ///< Number of subresources in the entire resource.
    RpsSubresourceRange     fullRange;        ///< Range spanning all subresources.
    RpsHeapId               heapId;           ///< ID of the heap its memory is placed in.
    RpsGpuMemoryRequirement allocInfo;        ///< Allocation parameters of the resource.
} RpsRuntimeResourceInfo;

/// @brief Bitflags for queue capabilities.
typedef enum RpsQueueFlagBits
{
    RPS_QUEUE_FLAG_NONE     = 0,       ///< No capabilities.
    RPS_QUEUE_FLAG_GRAPHICS = 1 << 0,  ///< Graphics capabilities.
    RPS_QUEUE_FLAG_COMPUTE  = 1 << 1,  ///< Compute capabilities.
    RPS_QUEUE_FLAG_COPY     = 1 << 2,  ///< Copy capabilities.
} RpsQueueFlagBits;

/// @brief Bitmask type for <c><i>RpsQueueFlagBits</i></c>.
typedef RpsFlags32 RpsQueueFlags;

/// @} end addtogroup RpsRenderGraphRuntimeResources

#include "rps/runtime/common/rps_runtime_callbacks.h"

/// @addtogroup RpsRuntimeDevice
/// @{

/// @brief Parameters for creating a runtime device.
typedef struct RpsRuntimeDeviceCreateInfo
{
    void*               pUserContext;  ///< User defined context to be passed to the callback functions.
    RpsRuntimeCallbacks callbacks;     ///< Callback functions.
} RpsRuntimeDeviceCreateInfo;

/// @brief Parameters for creating a dummy runtime device.
///
/// A <c><i>NullRuntimeDevice</i></c> is a default implementation of the RuntimeDevice interface without any real GPU
/// device associated.
typedef struct RpsNullRuntimeDeviceCreateInfo
{
    const RpsDeviceCreateInfo* pDeviceCreateInfo;          ///< Pointer to parameters for creating the core device
                                                           ///  associated with the runtime device. Passing NULL uses
                                                           ///  default parameters for creation instead.
    const RpsRuntimeDeviceCreateInfo* pRuntimeCreateInfo;  ///< Pointer to parameters for creating the runtime device.
                                                           ///  Passing NULL uses default parameters for creation
                                                           ///  instead.
} RpsNullRuntimeDeviceCreateInfo;

/// @brief Creates a dummy runtime.
///
/// For more info see <c><i>RpsNullRuntimeDeviceCreateInfo</i></c>.
///
/// @param pCreateInfo                                      Pointer to parameters for creating a dummy runtime. Passing
///                                                         NULL uses default parameters for creation instead.
/// @param phDevice                                         Pointer a handle in which the device is returned. Must
///                                                         not be NULL.
///
/// @returns                                                Result code of the operation. See <c><i>RpsResult</i></c>
///                                                         for more info.
RpsResult rpsNullRuntimeDeviceCreate(const RpsNullRuntimeDeviceCreateInfo* pCreateInfo, RpsDevice* phDevice);

/// @} end addtogroup RpsRuntimeDevice

/// @addtogroup RpsSubprogram
/// @{

/// Parameters for creating an RPS program.
typedef struct RpsProgramCreateInfo
{
    /// Pointer to signature parameters for the program entry. If hRpslEntryPoint is specified, this parameter will be
    /// ignored and the signature will be taken from the RpslEntry definition.
    /// Must not be NULL if hRpslEntryPoint == RPS_NULL_HANDLE.
    const RpsRenderGraphSignatureDesc* pSignatureDesc;

    /// Handle to the program entry point.
    RpsRpslEntry hRpslEntryPoint;

    /// Default node callback. Used when a node is called for which no implementation is bound.
    RpsCmdCallback defaultNodeCallback;
} RpsProgramCreateInfo;

/// @brief Gets the signature description of an RPSL entry point.
///
/// @param hRpslEntry               Handle to the RPSL entry point.
/// @param pDesc                    Pointer in which the signature description is returned. Must not be NULL.
///
/// @returns                        Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsRpslEntryGetSignatureDesc(RpsRpslEntry hRpslEntry, RpsRenderGraphSignatureDesc* pDesc);

/// @brief Creates a subprogram.
///
/// @param hDevice                  Handle to the device to use for creation.
/// @param pCreateInfo              Pointer to the creation parameters. Must not be NULL.
/// @param phProgram                Pointer to a handle in which the subprogram is returned. Must not be NULL.
///
/// @returns                        Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsProgramCreate(RpsDevice hDevice, const RpsProgramCreateInfo* pCreateInfo, RpsSubprogram* phProgram);

/// @brief Destroys a subprogram instance.
///
/// @param hProgram                 Handle to the subprogram object.
void rpsProgramDestroy(RpsSubprogram hProgram);

/// @brief Binds a command node callback to a node declaration specified by name.
///
/// Node instances generated from the program with the specified node declaration will call the same callback during
/// render graph command recording.
///
/// @param hProgram                 Handle to the program to bind a node for.
/// @param name                     Null terminated string with the name of the node.
/// @param pCallback                Pointer to callback parameters. Passing NULL uses default callback parameters.
///
/// @returns                        Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsProgramBindNodeCallback(RpsSubprogram hProgram, const char* name, const RpsCmdCallback* pCallback);

/// @brief Binds a subprogram to a node declaration specified by name.
///
/// The subprogram will be executed during render graph update as if inlined into the parent program.
/// During render graph command recording, node instances generated from the subprogram will call the subprogram node
/// callbacks bindings. Subprograms can be nested recursively.
///
/// @param hProgram                 Handle to the program to bind the subprogram to.
/// @param name                     Null terminated string with the name of the node.
/// @param hSubprogram              Handle to the subprogram to bind.
///
/// @returns                        Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsProgramBindNodeSubprogram(RpsSubprogram hProgram, const char* name, RpsSubprogram hSubprogram);

/// @} end addtogroup RpsSubprogram

/// @addtogroup RpsRenderGraphRuntime
/// @{

/// @brief Parameters for creating a render graph.
typedef struct RpsRenderGraphCreateInfo
{
    struct
    {
        RpsScheduleFlags scheduleFlags;    ///< Flags for scheduling behavior.
        uint32_t         numQueues;        ///< Number of queues available to the render graph. If 0, RPS assumes there
                                           ///  is 1 graphics queue.
        const RpsQueueFlags* pQueueInfos;  ///< Pointer to an array of <c><i>RpsQueueFlags</i></c> with numQueues queue
                                           ///  flags. Must not be NULL if numQueues != 0.
    } scheduleInfo;

    struct
    {
        uint32_t        numHeaps;        ///< Number of memory heaps available to the render graph.
        const uint32_t* heapBudgetMiBs;  ///< Pointer to an array of <c><i>uint32_t</i></c> numHeaps memory sizes as
                                         ///  limits on the amount of memory to be used. Must not be NULL if
                                         ///  numHeaps != 0.
    } memoryInfo;

    RpsProgramCreateInfo mainEntryCreateInfo;  ///< Creation parameters for the main entry RPS program.
    RpsRenderGraphFlags  renderGraphFlags;     ///< Flags for render graph properties.

    /// Number of render graph phase objects used by the render graph.
    uint32_t numPhases;

    /// Pointer to an array of <c><i>const RpsRenderGraphPhaseInfo</i></c> with numPhases render graph phase objects
    /// used by the render graph. If null, RPS uses the runtime specified default pipeline to process the render graph.
    const RpsRenderGraphPhaseInfo* pPhases;

} RpsRenderGraphCreateInfo;

/// @brief Creates a render graph.
///
/// @param hDevice                              Handle to the device to use for creation. Must not be RPS_NULL_HANDLE.
/// @param pCreateInfo                          Pointer to creation parameters. Must not be NULL.
/// @param phRenderGraph                        Pointer a handle in which the render graph is returned.
///                                             Must not be NULL.
///
/// @returns                                    Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsRenderGraphCreate(RpsDevice                       hDevice,
                               const RpsRenderGraphCreateInfo* pCreateInfo,
                               RpsRenderGraph*                 phRenderGraph);

/// @brief Updates a render graph.
///
/// @param hRenderGraph                         Handle to the render graph to update. Must not be RPS_NULL_HANDLE.
/// @param pUpdateInfo                          Pointer to update parameters. Must not be NULL.
///
/// @returns                                    RpsResult indicating potential errors during the execution. See
///                                             <c><i>RpsResult</i></c> for more info.
RpsResult rpsRenderGraphUpdate(RpsRenderGraph hRenderGraph, const RpsRenderGraphUpdateInfo* pUpdateInfo);

/// @brief Destroys a render graph.
///
/// @param hRenderGraph                         Handle to the render graph object to destroy.
void rpsRenderGraphDestroy(RpsRenderGraph hRenderGraph);

/// @brief Allocates memory from a render graph builder.
///
/// @param hRenderGraphBuilder      Handle to the render graph builder. Must not be RPS_NULL_HANDLE.
/// @param size                     Required size of the allocation.
///
/// @returns                        Pointer to the allocated memory if the allocation was successful, NULL otherwise.
///                                 Only valid until the next render graph update.
void* rpsRenderGraphAllocateData(RpsRenderGraphBuilder hRenderGraphBuilder, size_t size);

/// @brief Allocates memory from a render graph builder with alignment requirements.
///
/// @param hRenderGraphBuilder      Handle to the render graph builder. Must not be RPS_NULL_HANDLE.
/// @param size                     Required size of the allocation.
/// @param alignment                Minimum alignment requirement of the allocation in bytes.
///
/// @returns                        Pointer to the allocated memory if allocation was successful, NULL otherwise.
///                                 Only valid until the next render graph update.
void* rpsRenderGraphAllocateDataAligned(RpsRenderGraphBuilder hRenderGraphBuilder, size_t size, size_t alignment);

/// @brief Declare an on-demand node type during the render graph construction.
///
/// Normally, node declarations are specified in the RenderGraphSignature ahead of time. This function allows
/// additional node declarations to be added. Note: The lifetime of the dynamic node declaration is temporary
/// and it is only valid until the next render graph update.
///
/// @param hRenderGraphBuilder      Handle to the render graph builder. Must not be RPS_NULL_HANDLE.
/// @param pNodeDesc                Pointer to a node description. Passing NULL for the name of the description
///                                 registers the node as a fallback for calling unknown nodes.
///
/// @returns                        ID of the node declaration if successful, RPS_NODEDECL_ID_INVALID otherwise.
RpsNodeDeclId rpsRenderGraphDeclareDynamicNode(RpsRenderGraphBuilder hRenderGraphBuilder, const RpsNodeDesc* pNodeDesc);

/// @brief Gets a variable from the render graph builder by its ID.
///
/// @param hRenderGraphBuilder      Handle to the render graph builder to get the variable from.
/// @param paramId                  Index of the parameter.
///
/// @returns                        Variable identified by its ID.
RpsVariable rpsRenderGraphGetParamVariable(RpsRenderGraphBuilder hRenderGraphBuilder, RpsParamId paramId);

/// @brief Gets the resource ID of a resource parameter by the parameter ID.
///
/// Resource parameters have RPS_PARAMETER_FLAG_RESOURCE_BIT set and Resources are expected to be provided externally
/// to the render graph.
///
/// @param hRenderGraphBuilder      Handle to the render graph builder. Must not be RPS_NULL_HANDLE.
/// @param paramId                  Index of the parameter.
///
/// @returns                        ID of the resource identified by its parameter ID.
RpsResourceId rpsRenderGraphGetParamResourceId(RpsRenderGraphBuilder hRenderGraphBuilder, RpsParamId paramId);

/// @brief Declare a render graph managed resource.
///
/// @param hRenderGraphBuilder      Handle to the render graph builder. Must not be RPS_NULL_HANDLE.
/// @param name                     Null terminated string with the name of the resource.
/// @param localId                  Subprogram local ID of the resource.
/// @param hDesc                    Handle to the description of the resource.
///
/// @returns                        ID of the declared resource.
RpsResourceId rpsRenderGraphDeclareResource(RpsRenderGraphBuilder hRenderGraphBuilder,
                                            const char*           name,
                                            RpsResourceId         localId,
                                            RpsVariable           hDesc);

// Nodes

/// @brief Adds a render graph node to a render graph.
///
/// @param hRenderGraphBuilder              Handle to the render graph builder. Must not be RPS_NULL_HANDLE.
/// @param nodeDeclId                       Node declaration ID.
/// @param userTag                          User controlled tag for associations with a node call. Is accessible through
///                                         <c><i>RpsCmdCallbackContext</i></c>.
/// @param callback                         Pointer to the callback function.
/// @param pCallbackUserContext             Pointer to a user controlled structure to be passed to the callback.
/// @param callbackFlags                    Flags controlling the callback behavior.
/// @param pArgs                            Pointer to the parameters used for the callback.
///                                         Must not be NULL if numArgs != 0.
/// @param numArgs                          Number of parameters used for the callback.
///
/// @returns                                ID of the command node.
RpsNodeId rpsRenderGraphAddNode(RpsRenderGraphBuilder hRenderGraphBuilder,
                                RpsNodeDeclId         nodeDeclId,
                                uint32_t              userTag,
                                PFN_rpsCmdCallback    callback,
                                void*                 pCallbackUserContext,
                                RpsCmdCallbackFlags   callbackFlags,
                                const RpsVariable*    pArgs,
                                uint32_t              numArgs);

/// @brief Gets the runtime resource info from a resource ID.
///
/// Can be used to retrieve information such as the API resource handle, resource description and subresource info.
///
/// @param hRenderGraph                         Handle to the render graph to get the resource info from. Must not be
///                                             RPS_NULL_HANDLE.
/// @param resourceId                           The index to the resource to get. This can be the index returned by
///                                             rpsRenderGraphDeclareResource or rpsRenderGraphGetParamResourceId.
/// @param temporalLayerIndex                   The temporal layer index. Ignored for non-temporal resource.
/// @param pResourceInfo                        Pointer in which the runtime resource info is returned. Must not be NULL.
///
/// @returns                                    Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsRenderGraphGetResourceInfo(RpsRenderGraph          hRenderGraph,
                                        RpsResourceId           resourceId,
                                        uint32_t                temporalLayerIndex,
                                        RpsRuntimeResourceInfo* pResourceInfo);

/// @brief Gets the runtime resource info of an output parameter.
///
/// When updating a render graph, any resource handle returned through pResourceInfos will have updated if there was
/// an update to the corresponding output resource description. Make sure to call this function after updating a render
/// graph to get any updated handles.
///
/// @param hRenderGraph                         Handle to the render graph to get the resource info from.
/// @param paramId                              Index of the resource parameter. Must be an output resource parameter
///                                             of a render graph entry (Declared as 'out [...] texture / buffer' in
///                                             RPSL or with
///                                             (RPS_PARAMETER_FLAG_OUT_BIT | RPS_PARAMETER_FLAG_RESOURCE_BIT) set.
/// @param arrayOffset                          Offset of the first parameters for array parameters. Must be 0
///                                             otherwise.
/// @param numResources                         Number of resources to get infos for.
/// @param pResourceInfos                       Pointer to an array of <c><i>RpsRuntimeResourceInfo</i></c> in which the
///                                             numResources resource infos are returned. Must not be NULL if
///                                             numResources != 0.
///
/// @returns                                    Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsRenderGraphGetOutputParameterResourceInfos(RpsRenderGraph          hRenderGraph,
                                                        RpsParamId              paramId,
                                                        uint32_t                arrayOffset,
                                                        uint32_t                numResources,
                                                        RpsRuntimeResourceInfo* pResourceInfos);

/// @brief Gets the main entry of a render graph.
///
/// @param hRenderGraph             Handle to the render graph. Must not be RPS_NULL_HANDLE.
///
/// @returns                        Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsSubprogram rpsRenderGraphGetMainEntry(RpsRenderGraph hRenderGraph);

/// @} end addtogroup RpsRenderGraphRuntime

/// @addtogroup RpsRenderGraphCommandRecording
/// @{

/// @defgroup RpsRenderGraphCommandRecordingVK    Vulkan
/// @defgroup RpsRenderGraphCommandRecordingD3D11 D3D11
/// @defgroup RpsRenderGraphCommandRecordingD3D12 D3D12

/// @brief Parameters of a batch of commands to be recorded by the graphics API in use.
///
/// These commands are the result of scheduling and have to be executed on the same queue.
typedef struct RpsCommandBatch
{
    uint32_t queueIndex;        ///< Index of the queue to submit the current batch to.
    uint32_t waitFencesBegin;   ///< Offset of the range of fence IDs into the
                                ///  RpsRenderGraphBatchLayout::pWaitFenceIds array to wait for before submitting.
    uint32_t numWaitFences;     ///< Number of fence IDs to wait for before submitting.
    uint32_t signalFenceIndex;  ///< Index of the fence to signal after submitting.
    uint32_t cmdBegin;          ///< Index of the first runtime command in the batch.
    uint32_t numCmds;           ///< Number of runtime commands in the batch.
} RpsCommandBatch;

/// @brief Parameters of the command batch layout of a render graph.
typedef struct RpsRenderGraphBatchLayout
{
    uint32_t               numCmdBatches;    ///< Number of command batches.
    uint32_t               numFenceSignals;  ///< Number of fence signals in the pipeline.
    const RpsCommandBatch* pCmdBatches;      ///< Pointer to an array of <c><i>const RpsCommandBatch</i></c> with
                                             ///  numCmdBatches command batch parameters. Must not be NULL.
    const uint32_t* pWaitFenceIndices;       ///< Pointer to an array of <c><i>const uint32_t</i></c> numFenceSignals
                                             ///  wait fence indices. Must not be NULL if numFenceSignals != 0. Each
                                             ///  batch can wait for a range of fence IDs in this array, defined by its
                                             ///  waitFencesBeginIndex and numWaitFences.
} RpsRenderGraphBatchLayout;

/// @brief Bitflags for recording commands.
typedef enum RpsRecordCommandFlagBits
{
    RPS_RECORD_COMMAND_FLAG_NONE                         = 0,       ///< No recording options.
    RPS_RECORD_COMMAND_FLAG_ENABLE_COMMAND_DEBUG_MARKERS = 1 << 0,  ///< Enables per-command debug markers during
                                                                    ///  command recording.
} RpsRecordCommandFlagBits;

/// @brief Bitmask type for <c><i>RpsRecordCommandFlagBits</i></c>.
typedef RpsFlags32 RpsRecordCommandFlags;

/// @brief Parameters for recording commands using a processed render graph.
typedef struct RpsRenderGraphRecordCommandInfo
{
    RpsRuntimeCommandBuffer hCmdBuffer;    ///< Handle to the runtime command buffer object.
    void*                   pUserContext;  ///< User defined context to be passed to the callbacks during recording.
                                           ///  Passing NULL uses a default context instead.
    uint64_t              frameIndex;      ///< Index of the frame to record commands for.
    uint32_t              cmdBeginIndex;   ///< Index of the first command to be recorded.
    uint32_t              numCmds;         ///< Number of commands to record.
    RpsRecordCommandFlags flags;           ///< Flags for specifying recording behavior.
} RpsRenderGraphRecordCommandInfo;

/// @brief Gets the command batch layout of a render graph.
///
/// @param hRenderGraph                         Handle to the render graph. Must not be RPS_NULL_HANDLE.
/// @param pBatchLayout                         Pointer to return the batch layout in. Must not be NULL.
///
/// @returns                                    Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsRenderGraphGetBatchLayout(RpsRenderGraph hRenderGraph, RpsRenderGraphBatchLayout* pBatchLayout);

/// @brief Records graphics API commands from a processed render graph.
///
/// @param hRenderGraph                         Handle to the render graph. Must not be RPS_NULL_HANDLE.
/// @param pRecordInfo                          Pointer to recording parameters. Must not be NULL.
///
/// @returns                                    Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsRenderGraphRecordCommands(RpsRenderGraph hRenderGraph, const RpsRenderGraphRecordCommandInfo* pRecordInfo);

/// @brief Constant for an invalid command ID.
#define RPS_CMD_ID_INVALID RPS_INDEX_NONE_U32

/// @brief Diagnostic information for a command of the render graph command stream.
typedef struct RpsCmdDiagnosticInfo
{
    uint32_t cmdIndex;     ///< Index of the command in the scheduled command stream. Also used for
                           ///  resource lifetimes.
    RpsBool isTransition;  ///< Indicator for transition commands.
    union
    {
        //TODO Add more struct members if required by a tool.
        struct
        {
            uint32_t dummy;  ///< Dummy value to avoid an empty struct.
        } cmd;

        struct
        {
            RpsAccessAttr       prevAccess;     ///< Access before the current transition.
            RpsAccessAttr       nextAccess;     ///< Access after the current transition.
            RpsSubresourceRange range;          ///< Access range for the transition.
            uint32_t            resourceIndex;  ///< Index of the resource to transition.
        } transition;
    };

} RpsCmdDiagnosticInfo;

/// @brief Diagnostic information for a resource.
typedef struct RpsResourceDiagnosticInfo
{
    const char* name;                          ///< Null terminated string with the name of the resource.
    uint32_t    temporalChildIndex;            ///< Index to the first temporal child of the temporal parent. Only for
                                               ///  use in temporal parent resources.
    RpsBool                 isExternal;        ///< Indicator for external resources.
    RpsResourceDesc         desc;              ///< Description of the resource.
    RpsClearValue           clearValue;        ///< Clear value of the resource.
    RpsAccessAttr           allAccesses;       ///< Combination of all accesses of the resource throughout the frame.
    RpsAccessAttr           initialAccess;     ///< Initial access of the resource.
    uint32_t                lifetimeBegin;     ///< Index of the first command to which the runtime resource is active
                                               ///  in its backing heap.
    uint32_t lifetimeEnd;                      ///< Index of the last command to which the runtime resource is active
                                               ///  in its backing heap.
    RpsGpuMemoryRequirement allocRequirement;  ///< Allocation requirements for the memory of the resource.
    RpsHeapPlacement        allocPlacement;    ///< Allocation placement for the memory of the resource.
    RpsRuntimeResource      hRuntimeResource;  ///< Handle to the backend specific resource.
} RpsResourceDiagnosticInfo;

/// @brief Diagnostic information for a heap.
typedef struct RpsHeapDiagnosticInfo
{
    uint64_t size;                   ///< Total size of the heap. May be 0 if the heap is not created in the backend
                                     ///  yet.
    uint64_t       usedSize;         ///< Amount of memory allocated from the heap.
    uint64_t       maxUsedSize;      ///< Maximum amount of memory ever allocated from the heap.
    uint32_t       alignment;        ///< Alignment of the heap in bytes.
    uint32_t       memoryTypeIndex;  ///< Index of the backend specific memory type of the heap.
    RpsRuntimeHeap hRuntimeHeap;     ///< Handle to the backend specific heap implementation.
} RpsHeapDiagnosticInfo;

/// @brief Diagnostic information for parts of a render graph.
typedef struct RpsRenderGraphDiagnosticInfo
{
    uint32_t numResourceInfos;  ///< Number of resource infos.
    uint32_t numCommandInfos;   ///< Number of command infos.
    uint32_t numHeapInfos;      ///< Number of heap infos.

    /// Pointer to an array of <c><i>RpsResourceDiagnosticInfo</i></c> with numResourceInfos resource infos.
    const RpsResourceDiagnosticInfo* pResourceDiagInfos;

    /// Pointer to an array of <c><i>RpsCmdDiagnosticInfo</i></c> with numCommandInfos command infos.
    const RpsCmdDiagnosticInfo* pCmdDiagInfos;

    /// Pointer to an array of <c><i>RpsHeapDiagnosticInfo</i></c> with numHeapInfos heap infos.
    const RpsHeapDiagnosticInfo* pHeapDiagInfos;
} RpsRenderGraphDiagnosticInfo;

/// @brief Bitflags for diagnostic info modes.
typedef enum RpsRenderGraphDiagnosticInfoFlagBits
{
    RPS_RENDER_GRAPH_DIAGNOSTIC_INFO_DEFAULT        = 0,          ///< Diagnostic info is taken from the latest frame.
    RPS_RENDER_GRAPH_DIAGNOSTIC_INFO_USE_CACHED_BIT = (1u << 0),  ///< The previously cached diagnostic info is returned
                                                                  ///< if not called for the first time.
} RpsRenderGraphDiagnosticInfoFlagBits;

/// @brief Bitmask type for <c><i>RpsRenderGraphDiagnosticInfoFlagBits</i></c>.
typedef RpsFlags32 RpsRenderGraphDiagnosticInfoFlags;

/// @brief Gets diagnostic information from a render graph.
///
/// Diagnostic information is intended to be consumed by tools related to RPS, e.g. the visualizer tool set.
///
/// @param hRenderGraph                      Handle to the render graph. Must not be RPS_NULL_HANDLE.
/// @param pDiagInfo                         Pointer in which the diagnostic information is returned. Must not be NULL.
/// @param diagnosticFlags                   Flags for the diagnostic mode.
///
/// @returns                                 Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsRenderGraphGetDiagnosticInfo(RpsRenderGraph                    hRenderGraph,
                                          RpsRenderGraphDiagnosticInfo*     pDiagInfo,
                                          RpsRenderGraphDiagnosticInfoFlags diagnosticFlags);

/// @brief Parameters of a command callback context.
typedef struct RpsCmdCallbackContext
{
    /// Handle to the command buffer for command recording.
    RpsRuntimeCommandBuffer hCommandBuffer;

    /// User context passed as RpsRenderGraphRecordCommandInfo::pUserContext. Can vary per rpsRenderGraphRecordCommands
    /// call and can e.g. be used as per-thread context if doing multi-threaded recording.
    void* pUserRecordContext;

    /// User context specified with the command node callback function, for example via a rpsProgramBindNode call. Can
    /// vary per callback.
    void* pCmdCallbackContext;

    /// Pointer to an array of <c><i>void* const</i></c> with numArgs pointers to arguments to use for the callback.
    /// Must not be NULL if numArgs != 0.
    void* const* ppArgs;

    /// Number of arguments defined for the callback.
    uint32_t numArgs;

    /// User defined tag for associations with a specific node. Can be set by passing a value to
    /// <c><i>rpsCmdCallNode</i></c>.
    uint32_t userTag;
} RpsCmdCallbackContext;

/// @brief Parameters for accessing a resource.
typedef struct RpsResourceAccessInfo
{
    RpsResourceId       resourceId;  ///< ID of the resource to access.
    RpsSubresourceRange range;       ///< Subresource range to access.
    RpsAccessAttr       access;      ///< Attributes for access type and shader stages.
    RpsFormat           viewFormat;  ///< Format to use for accessing.
} RpsResourceAccessInfo;

/// @brief Parameters of a graphics node render target.
typedef struct RpsCmdRenderTargetInfo RpsCmdRenderTargetInfo;

/// @brief Parameters of a graphics node viewport.
typedef struct RpsCmdViewportInfo RpsCmdViewportInfo;

/// @brief Gets the render targets parameters from the current recording context.
///
/// Must only be called from a graphics node callback.
///
/// @param pContext                             Pointer to the current recording context. Must not be NULL.
/// @param pRenderTargetInfo                    Pointer in which the render target parameters are returned.
///                                             Must not be NULL.
///
/// @returns                                    Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsCmdGetRenderTargetsInfo(const RpsCmdCallbackContext* pContext, RpsCmdRenderTargetInfo* pRenderTargetInfo);

/// @brief Gets the viewport info from the current recording context.
///
/// Must only be called from a graphics node callback.
///
/// @param pContext                             Pointer to the current recording context. Must not be NULL
/// @param pViewportInfo                        Pointer in which the viewport parameters are returned to.
///                                             Must not be NULL.
///
/// @returns                                    Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsCmdGetViewportInfo(const RpsCmdCallbackContext* pContext, RpsCmdViewportInfo* pViewportInfo);

/// @brief Parameters for explicitly beginning a render pass from a command callback.
///
/// Intended for controlling RenderPass suspend / resume & secondary command buffer behaviors.
typedef struct RpsCmdRenderPassBeginInfo
{
    RpsRuntimeRenderPassFlags flags;  ///< Flags for render pass properties.
} RpsCmdRenderPassBeginInfo;

/// @brief Clones a command callback context to create a secondary context and assigns it a new command buffer.
///
/// The cloned context inherits states from the context being cloned, such as current command info and command
/// arguments. The typical use case is multi-threaded command recording from within a node callback.
/// Must be synchronized by the caller if called from multiple threads. The created context pointer is valid until the
/// next render graph update.
///
/// @param pContext                             Pointer to the current command callback context.
/// @param hCmdBufferForDerivedContext          Handle to the command buffer to be associated with the new context.
/// @param ppDerivedContext                     Pointer in which a pointer to the cloned command callback context is
///                                             returned. Must not be NULL.
///
/// @returns                                    Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsCmdCloneContext(const RpsCmdCallbackContext*  pContext,
                             RpsRuntimeCommandBuffer       hCmdBufferForDerivedContext,
                             const RpsCmdCallbackContext** ppDerivedContext);

/// @brief Begins a rasterization rendering pass.
///
/// This may begin e.g. a VkRenderPass or set up render targets and viewport/scissor rect states for APIs that do not
/// support a render pass objects. Usually used for multi-threaded rendering from within a command callback.
///
/// @param pContext                             Pointer to the current command callback context. Must not be NULL.
/// @param pBeginInfo                           Pointer to render pass parameters. Must not be NULL.
///
/// @returns                                    Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsCmdBeginRenderPass(const RpsCmdCallbackContext* pContext, const RpsCmdRenderPassBeginInfo* pBeginInfo);

/// @brief Ends a rasterization rendering pass.
///
/// Must be paired with rpsCmdBeginRenderPass. Usually used for multi-threaded rendering from within a command callback.
///
/// @param pContext                             Pointer to the current command callback context. Must not be NULL.
///
/// @returns                                    Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsCmdEndRenderPass(const RpsCmdCallbackContext* pContext);

/// @brief Sets a new command buffer to be used for command recording.
///
/// RPS does not track previously used command buffers. It is the responsibility of the application to track and
/// submit them in order accordingly. Usually used for multi-threaded rendering.
///
/// @param pContext                             Pointer to the current command callback context. Must not be NULL.
/// @param hCmdBuffer                           Handle to the new command buffer.
///
/// @returns                                    Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsCmdSetCommandBuffer(const RpsCmdCallbackContext* pContext, RpsRuntimeCommandBuffer hCmdBuffer);

/// @brief Gets the description of the current cmd node.
///
/// @param pContext                             Pointer to the current command callback context. Must not be NULL.
/// @param ppNodeName                           Pointer in which a null terminated string with the name of the node
///                                             is returned. Must not be NULL.
/// @param pNodeNameLength                      Pointer in which the node name length is returned. Can be NULL.
///
/// @return                                     Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsCmdGetNodeName(const RpsCmdCallbackContext* pContext, const char** ppNodeName, size_t* pNodeNameLength);

/// @brief Gets the description of a node argument.
///
/// @param pContext                             Pointer to the current command callback context. Must not be NULL.
/// @param paramID                              Index of the parameter in the current command node callback.
/// @param pDesc                                Pointer in which the parameter description is returned.
///                                             Must not be NULL.
///
/// @returns                                    Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsCmdGetParamDesc(const RpsCmdCallbackContext* pContext, RpsParamId paramID, RpsParameterDesc* pDesc);

/// @brief Gets an array of resource descriptions of a resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the resource descriptions from. Must be a resource
///                                     array argument if numDescs > 1.
/// @param srcArrayOffset               Offset to the first resource description to get.
/// @param pResourceDescs               Pointer to an array of <c><i>RpsResourceDesc</i></c> in which the numDescs
///                                     resource descriptions are returned. Must not be NULL if numDescs != 0.
/// @param numDescs                     Number of resource descriptions to get. Requires srcArrayOffset + numDescs to
///                                     be less than the number of elements in the node argument.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsCmdGetArgResourceDescArray(const RpsCmdCallbackContext* pContext,
                                        RpsParamId                   argIndex,
                                        uint32_t                     srcArrayOffset,
                                        RpsResourceDesc*             pResourceDescs,
                                        uint32_t                     numDescs);

/// @brief Gets the resource description from a node argument.
///
/// @param pContext                             Pointer to the current command callback context. Must not be NULL.
/// @param argIndex                             Index of the parameter in the current command node callback.
/// @param pResourceDesc                        Pointer in which the resource description is returned. Must not be NULL.
///
/// @returns                                    Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsCmdGetArgResourceDesc(const RpsCmdCallbackContext* pContext,
                                   RpsParamId                   argIndex,
                                   RpsResourceDesc*             pResourceDesc);

/// @brief Gets an array of runtime resources from a resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the runtime resource from. Must be a resource array
///                                     argument if numResources > 1.
/// @param srcArrayOffset               Offset to the first runtime resource to get.
/// @param pRuntimeResources            Pointer to an array of <c><i>RpsRuntimeResource</i></c> in which the
///                                     numResources runtime resources are returned.
///                                     Must not be NULL if numResources != 0.
/// @param numResources                 Number of runtime resources to get. Requires srcArrayOffset + numResources to
///                                     be less than the number of elements in the node argument.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsCmdGetArgRuntimeResourceArray(const RpsCmdCallbackContext* pContext,
                                           RpsParamId                   argIndex,
                                           uint32_t                     srcArrayOffset,
                                           RpsRuntimeResource*          pRuntimeResources,
                                           uint32_t                     numResources);

/// @brief Gets the runtime resource from a resource node argument.
///
/// @param pContext                             Pointer to the current command callback context. Must not be NULL.
/// @param argIndex                             Index of the parameter in the current command node callback.
/// @param pRuntimeResource                     Pointer in which the runtime resource is returned. Must not be NULL.
///
/// @returns                                    Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsCmdGetArgRuntimeResource(const RpsCmdCallbackContext* pContext,
                                      RpsParamId                   argIndex,
                                      RpsRuntimeResource*          pRuntimeResource);

/// @brief Gets an array of resource access infos from a resource node argument.
///
/// @param pContext                     Callback context of the current command.
/// @param argIndex                     Index of the argument to get the resource access infos from. Must be a resource
///                                     array argument if numAccessess > 1.
/// @param srcArrayOffset               Offset to the first resource access info to get.
/// @param pResourceAccessInfos         Pointer to an array of <c><i>RpsResourceAccessInfo</i></c> in which the
///                                     numAccessess resource access infos are returned.
///                                     Must not be NULL if numAccessess != 0.
/// @param numAccessess                 Number of resource access infos to get. Requires srcArrayOffset +
///                                     to be less than the number of elements in the node argument.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsCmdGetArgResourceAccessInfoArray(const RpsCmdCallbackContext* pContext,
                                              RpsParamId                   argIndex,
                                              uint32_t                     srcArrayOffset,
                                              RpsResourceAccessInfo*       pResourceAccessInfos,
                                              uint32_t                     numAccessess);

/// @brief Gets the resource access info from a resource node argument.
///
/// @param pContext                             Pointer to the current command callback context. Must not be NULL.
/// @param argIndex                             Index of the parameter in the current command node callback.
/// @param pResourceAccessInfo                  Pointer in which the resource access info is returned. Must not be NULL.
///
/// @returns                                    Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsCmdGetArgResourceAccessInfo(const RpsCmdCallbackContext* pContext,
                                         RpsParamId                   argIndex,
                                         RpsResourceAccessInfo*       pResourceAccessInfo);

/// Get the argument pointer from a node argument.
///
/// @param pContext                             Pointer to the current command callback context. Must not be NULL.
/// @param argIndex                             Index of the parameter.
///
/// @returns                                    Pointer to the argument.
static inline RpsVariable rpsCmdGetArg(const RpsCmdCallbackContext* pContext, uint32_t argIndex)
{
    return pContext->ppArgs[argIndex];
}

/// @brief Signature of functions for acquiring command buffers in a simplified execution mode.
///
/// Reserved for future use.
typedef RpsResult (*PFN_rpsAcquireRuntimeCommandBuffer)(void*                    pUserContext,
                                                        uint32_t                 queueIndex,
                                                        uint32_t                 numCmdBuffers,
                                                        RpsRuntimeCommandBuffer* pCmdBuffers,
                                                        uint32_t*                pCmdBufferIdentifiers);

/// @brief Signature of functions for submitting command buffers in a simplified execution mode.
///
/// Reserved for future use.
typedef RpsResult (*PFN_rpsSubmitRuntimeCommandBuffer)(void*                          pUserContext,
                                                       uint32_t                       queueIndex,
                                                       const RpsRuntimeCommandBuffer* pRuntimeCmdBufs,
                                                       uint32_t                       numRuntimeCmdBufs,
                                                       uint32_t                       waitId,
                                                       uint32_t                       signalId);

/// @brief Parameters for executing a render graph.
typedef struct RpsRenderGraphExecuteInfo
{
    void* pUserContext;                                            ///< Pointer to a user defined context to be passed
                                                                   ///  to the callbacks.
    PFN_rpsAcquireRuntimeCommandBuffer pfnAcquireRuntimeCmdBufCb;  ///< Pointer to a function to acquire command
                                                                   ///  buffers.
    PFN_rpsSubmitRuntimeCommandBuffer pfnSubmitRuntimeCmdBufCb;    ///< Pointer to a function to submit command
                                                                   ///  buffers.
} RpsRenderGraphExecuteInfo;

/// @brief Executes a render graph.
///
/// @param hRenderGraph                                 Handle to the render graph. Must not be RPS_NULL_HANDLE.
/// @param pExecuteInfo                                 Pointer to render graph execution parameters. Must not be NULL.
///
/// @returns                                            Result code of the operation. See <c><i>RpsResult</i></c> for
///                                                     more info.
RpsResult rpsRenderGraphExecute(RpsRenderGraph hRenderGraph, const RpsRenderGraphExecuteInfo* pExecuteInfo);

/// @} end addtogroup RpsRenderGraphCommandRecording

#ifdef __cplusplus
}
#endif  //__cplusplus

/// @addtogroup RpsParamAttr
/// @{

/// @brief Initializes a parameter attribute.
///
/// @param pAttr                Pointer to the parameter attribute. Must not be NULL.
/// @param accessFlags          Flags for accessing the parameter.
/// @param shaderStageFlags     Flags for shader stages of the parameter access.
/// @param semantic             Semantic of the parameter.
/// @param semanticIndex        Semantic index of the parameter.
///
/// @returns                    Pointer to the initialized parameter attribute.
static inline const RpsParamAttr* rpsInitParamAttr(RpsParamAttr*       pAttr,
                                                   RpsAccessFlags      accessFlags,
                                                   RpsShaderStageFlags shaderStageFlags,
                                                   RpsSemantic         semantic,
                                                   uint32_t            semanticIndex)
{
    pAttr->access.accessFlags     = accessFlags;
    pAttr->access.accessStages    = shaderStageFlags;
    pAttr->semantic.semantic      = semantic;
    pAttr->semantic.semanticIndex = semanticIndex;

    return pAttr;
}

/// @brief Initializes a parameter attribute with only the access attribute being specified.
///
/// @param pAttr                Pointer to the parameter attribute. Must not be NULL.
/// @param accessFlags          Flags for accessing the parameter.
/// @param shaderStageFlags     Flags for shader stages of the parameter access.
///
/// @returns                    Pointer to the initialized parameter attribute.
static inline const RpsParamAttr* rpsInitParamAttrAccess(RpsParamAttr*       pAttr,
                                                         RpsAccessFlags      accessFlags,
                                                         RpsShaderStageFlags shaderStageFlags)
{
    return rpsInitParamAttr(pAttr, accessFlags, shaderStageFlags, RPS_SEMANTIC_UNSPECIFIED, 0);
}

/// @brief Initializes a parameter attribute with only the semantic attribute being specified.
///
/// @param pAttr                Pointer to the parameter attribute. Must not be NULL.
/// @param semantic             Semantic of the parameter.
/// @param semanticIndex        Semantic index of the parameter.
///
/// @returns                    Pointer to the initialized parameter attribute.
static inline const RpsParamAttr* rpsInitParamAttrSemantic(RpsParamAttr* pAttr,
                                                           RpsSemantic   semantic,
                                                           uint32_t      semanticIndex)
{
    return rpsInitParamAttr(pAttr, RPS_ACCESS_UNKNOWN, RPS_SHADER_STAGE_NONE, semantic, semanticIndex);
}

/// @} end addtogroup RpsParamAttr

#ifdef __cplusplus

#include <initializer_list>

#include "rps/core/rps_cmd_callback_wrapper.hpp"

/// @addtogroup RpsRenderGraphRuntime
/// @{

/// @brief Declares an on-demand node type during the render graph construction.
///
/// Normally, node declarations are specified in the RenderGraphSignature ahead of time. This function allows additional
/// node declarations to be added. Note: The lifetime of the dynamic node declaration is temporary and it is only until
/// the next render graph update.
///
/// @param hBuilder                 Handle to the RenderGraphBuilder. Must not be RPS_NULL_HANDLE.
/// @param name                     Null terminated string with the name of the node declaration. Passing NULL
///                                 registers the node as a fallback for calling unknown nodes.
/// @param flags                    Flags of the node declaration.
/// @param pParamDescs              Pointer to an array of <c><i>const RpsParameterDesc</i></c> with numParams parameter
///                                 descriptions. Must not be NULL.
/// @param numParams                Number of parameters the node has.
///
/// @returns                        ID of the node declaration if successful, RPS_NODEDECL_ID_INVALID otherwise.
static inline RpsNodeDeclId rpsRenderGraphDeclareDynamicNode(RpsRenderGraphBuilder   hBuilder,
                                                             const char*             name,
                                                             RpsNodeDeclFlags        flags,
                                                             const RpsParameterDesc* pParamDescs,
                                                             uint32_t                numParams)
{
    RpsNodeDesc nodeDesc = {};
    nodeDesc.flags       = flags;
    nodeDesc.numParams   = numParams;
    nodeDesc.pParamDescs = pParamDescs;
    nodeDesc.name        = name;

    return rpsRenderGraphDeclareDynamicNode(hBuilder, &nodeDesc);
}

/// @brief Declares an on-demand node type during the render graph construction.
///
/// Normally, node declarations are specified in the RenderGraphSignature ahead of time. This function allows additional
/// node declarations to be added. Note: The lifetime of the dynamic node declaration is temporary and it is only until
/// the next render graph update.
///
/// @param hRenderGraphBuilder      Handle to the RenderGraphBuilder. Must not be RPS_NULL_HANDLE.
/// @param name                     Null terminated string with the name of the node declaration. Passing NULL
///                                 registers the node as a fallback for calling unknown nodes.
/// @param flags                    Flags of the node declaration.
/// @param paramDescs               Initializer list of parameter descriptions for the node parameters.
///
/// @returns                        ID of the node declaration if successful, RPS_NODEDECL_ID_INVALID otherwise.
static inline RpsNodeDeclId rpsRenderGraphDeclareDynamicNode(RpsRenderGraphBuilder hRenderGraphBuilder,
                                                             const char*           name,
                                                             RpsNodeDeclFlags      flags,
                                                             std::initializer_list<RpsParameterDesc> paramDescs)
{
    return rpsRenderGraphDeclareDynamicNode(
        hRenderGraphBuilder, name, flags, paramDescs.begin(), uint32_t(paramDescs.size()));
}

/// @brief Adds a render graph node to a render graph.
///
/// @param hRenderGraphBuilder              Handle to the render graph builder. Must not be RPS_NULL_HANDLE.
/// @param nodeDeclId                       Node declaration ID.
/// @param userTag                          User controlled tag for associations with a node call. Is accessible through
///                                         <c><i>RpsCmdCallbackContext</i></c>.
/// @param callback                         Pointer to the callback function. Passing NULL uses the default one
///                                         registered for the empty string name instead.
/// @param pCallbackUserContext             Pointer to a user controlled structure to be passed to the callback. May be
///                                         NULL.
/// @param callbackFlags                    Flags controlling the callback behavior.
/// @param args                             Initializer list of RPS variables for the node parameters.
///
/// @returns                                ID of the command node.
static inline RpsNodeId rpsRenderGraphAddNode(RpsRenderGraphBuilder              hRenderGraphBuilder,
                                              RpsNodeDeclId                      nodeDeclId,
                                              uint32_t                           userTag,
                                              PFN_rpsCmdCallback                 callback,
                                              void*                              pCallbackUserContext,
                                              RpsCmdCallbackFlags                callbackFlags,
                                              std::initializer_list<RpsVariable> args)
{
    return rpsRenderGraphAddNode(hRenderGraphBuilder,
                                 nodeDeclId,
                                 userTag,
                                 callback,
                                 pCallbackUserContext,
                                 callbackFlags,
                                 args.begin(),
                                 uint32_t(args.size()));
}

/// @brief Allocates memory for an object from a render graph.
///
/// The lifetime of the memory lasts until the next call to rpsRenderGraphUpdate.
///
/// @tparam T                       Type of object. Note: This function only allocates memory. It is the application's
///                                 responsibility to call constructors and destructors when needed.
/// @param hRenderGraphBuilder      Handle to the render graph builder. Must not be RPS_NULL_HANDLE.
///
/// @returns                        Pointer to the allocated object if successful, NULL otherwise. Only valid until the
///                                 next render graph update.
template <typename T>
static inline T* rpsRenderGraphAllocateData(RpsRenderGraphBuilder hRenderGraphBuilder)
{
    return static_cast<T*>(rpsRenderGraphAllocateDataAligned(hRenderGraphBuilder, sizeof(T), alignof(T)));
}

/// @} end addtogroup RpsRenderGraphRuntime

/// @brief Gets a node argument by the argument index.
///
/// Must only be called from a command node callback.
/// The function doesn't check if the type casting is valid. Users can call rpsCmdGetParamDesc to query the type info and
/// array size of the parameter before calling this function, to ensure the cast is safe.
///
/// @tparam T               Type to cast the node argument to. The function does not perform validation for the type
///                         cast.
/// @param pContext         Pointer to the context to get the description from. Must not be NULL.
/// @param paramId          Index of the argument.
///
/// @returns                Pointer to a const type T object. nullptr if the index is out of range.
///
/// @ingroup RpsRenderGraphCommandRecording
template <typename T>
const T* rpsCmdGetArg(const RpsCmdCallbackContext* pContext, RpsParamId paramId)
{
    return (paramId < pContext->numArgs) ? static_cast<const T*>(pContext->ppArgs[paramId]) : nullptr;
}

/// @brief Gets a node argument by the argument index.
///
/// Must only be called from a command node callback.
///
/// @tparam T               Type to cast the node argument to. The function does not perform validation for the type
///                         cast.
/// @tparam Index           Index of the argument.
/// @param pContext         Pointer to the context to get the description from. Must not be NULL.
///
/// @returns                Pointer to a const type T object. nullptr if the index is out of range.
///
/// @ingroup RpsRenderGraphCommandRecording
template <typename T, size_t Index>
const T* rpsCmdGetArg(const RpsCmdCallbackContext* pContext)
{
    return rpsCmdGetArg<T>(pContext, Index);
}

namespace rps
{
    // TODO! For now, a ParamAttrList is identical to an RpsParamAttr (containing one access & one semantic attr).
    // Need to extend ParamAttrList to be a real list of attributes to properly support features like Before/After
    // access.

    template <typename T>
    const T* GetCmdArg(const RpsCmdCallbackContext* pContext, uint32_t index)
    {
        return (index < pContext->numArgs) ? static_cast<const T*>(pContext->ppArgs[index]) : nullptr;
    }

    template <typename T, size_t Index>
    const T* GetCmdArg(const RpsCmdCallbackContext* pContext)
    {
        return GetCmdArg<T>(pContext, Index);
    }

    /// @brief A list of attributes a render graph node parameter can have.
    struct ParamAttrList : public RpsParamAttr
    {
        /// @brief Constructor with parameters only for constructing a set of access attributes.
        ///
        /// @param accessFlags              Access flags of the access attribute.
        /// @param shaderStages             Shader stages of the access attribute.
        constexpr ParamAttrList(RpsAccessFlags accessFlags, RpsShaderStageFlags shaderStages = RPS_SHADER_STAGE_NONE)
            : RpsParamAttr{{accessFlags, shaderStages}, {RPS_SEMANTIC_UNSPECIFIED, 0}}
        {
        }

        /// @brief Constructor with parameters only for constructing a set of access attributes.
        ///
        /// @param inAccess                 Access attribute to copy from.
        constexpr ParamAttrList(RpsAccessAttr inAccess)
            : RpsParamAttr{inAccess, {RPS_SEMANTIC_UNSPECIFIED, 0}}
        {
        }

        /// @brief Constructor with parameters only for constructing a semantic attribute.
        ///
        /// @param semantic                 Semantic for the semantic attribute.
        /// @param semanticIndex            Index at which to define this semantic.
        constexpr ParamAttrList(RpsSemantic semantic, uint32_t semanticIndex = 0)
            : RpsParamAttr{{RPS_ACCESS_UNKNOWN, RPS_SHADER_STAGE_NONE}, {semantic, semanticIndex}}
        {
        }

        /// @brief Constructor with parameters only for constructing a semantic attribute.
        ///
        /// @param inSemantic               Semantic attribute to copy from.
        constexpr ParamAttrList(RpsSemanticAttr inSemantic)
            : RpsParamAttr{{RPS_ACCESS_UNKNOWN, RPS_SHADER_STAGE_NONE}, {inSemantic}}
        {
        }

        /// @brief Constructor for a default attribute.
        constexpr ParamAttrList()
            : RpsParamAttr{{RPS_ACCESS_UNKNOWN, RPS_SHADER_STAGE_NONE}, {RPS_SEMANTIC_UNSPECIFIED, 0}}
        {
        }

        static RpsParamAttrList ToHandle(ParamAttrList* pAttrList);
    };

    /// @brief Creates a list of parameter attributes.
    ///
    /// @param allocator                Reference to an object for allocating objects with a New(TArgs... args)
    ///                                 call.
    /// @param args                     Parameter pack for constructing of an object.
    ///
    /// @returns                        Constructed parameters attribute list.
    template <typename TAllocator, typename... TArgs>
    RpsParamAttrList MakeParamAttrList(TAllocator& allocator, TArgs... args)
    {
        return ParamAttrList::ToHandle(allocator.template New<ParamAttrList>(args...));
    }

    namespace details
    {

        // RpsRuntimeResource handle
        template <int32_t Index>
        struct CommandArgUnwrapper<Index, RpsRuntimeResource>
        {
            RpsRuntimeResource operator()(const RpsCmdCallbackContext* pContext)
            {
                RpsRuntimeResource resource = {};
                const RpsResult    result   = rpsCmdGetArgRuntimeResource(pContext, Index, &resource);
                if (RPS_FAILED(result))
                {
                    rpsCmdCallbackReportError(pContext, result);
                }
                return resource;
            }
        };

        // RpsResourceDesc info
        template <int32_t Index>
        struct CommandArgUnwrapper<Index, RpsResourceDesc>
        {
            RpsResourceDesc operator()(const RpsCmdCallbackContext* pContext)
            {
                RpsResourceDesc desc   = {};
                const RpsResult result = rpsCmdGetArgResourceDesc(pContext, Index, &desc);
                if (RPS_FAILED(result))
                {
                    rpsCmdCallbackReportError(pContext, result);
                }
                return desc;
            }
        };

        // RpsResourceAccess info
        template <int32_t Index>
        struct CommandArgUnwrapper<Index, RpsResourceAccessInfo>
        {
            RpsResourceAccessInfo operator()(const RpsCmdCallbackContext* pContext)
            {
                RpsResourceAccessInfo access = {};
                const RpsResult       result = rpsCmdGetArgResourceAccessInfo(pContext, Index, &access);
                if (RPS_FAILED(result))
                {
                    rpsCmdCallbackReportError(pContext, result);
                }
                return access;
            }
        };

        RpsResult ProgramGetBindingSlot(RpsSubprogram    hProgram,
                                        const char*      name,
                                        size_t           size,
                                        RpsCmdCallback** ppCallback);
    }  // namespace details
}  // namespace rps

/// @addtogroup RpsSubprogram
/// @{

/// @brief Binds a command callback implementation to a node type in an rps program.
///
/// This can be used to bind a C++ member function as the command callback.
///
/// @param hProgram                 Handle to the subprogram to bind the node. Must not be RPS_NULL_HANDLE.
/// @param name                     Null terminated string with the name of the node to bind to. Passing NULL uses the
///                                 callback as default fallback. See RpsProgramCreateInfo::defaultNodeCallback for
///                                 details.
/// @param cmdCallback              Pointer to a function to be bound to the node as the command recording callback.
/// @param pThis                    Pointer to class instance containing member function callback.
/// @param flags                    Flags for the callback behavior.
///
/// @returns                        Result code of the operation. See <c><i>RpsResult</i></c> for more info.
template <typename TTarget,
          typename TFunc,
          typename TContext = rps::details::MemberNodeCallbackContext<TTarget, TFunc>,
          typename          = typename std::enable_if<std::is_trivially_destructible<TContext>::value &&
                                             std::is_member_function_pointer<TFunc>::value>::type>
RpsResult rpsProgramBindNode(RpsSubprogram       hProgram,
                             const char*         name,
                             TFunc               cmdCallback,
                             TTarget*            pThis,
                             RpsCmdCallbackFlags flags = RPS_CMD_CALLBACK_FLAG_NONE)
{
    RpsCmdCallback* pSlot  = {};
    RpsResult       result = rps::details::ProgramGetBindingSlot(hProgram, name, sizeof(TContext), &pSlot);

    if (RPS_SUCCEEDED(result))
    {
        new (pSlot->pUserContext) TContext(pThis, cmdCallback);
        pSlot->pfnCallback = TContext::Callback;
        pSlot->flags       = flags;
    }

    return result;
}

/// @brief Binds a command callback implementation to a node type in an rps program.
///
/// @param hProgram                 Handle to the subprogram to bind the node. Must not be RPS_NULL_HANDLE.
/// @param name                     Null terminated string with the name of the node to bind to. Passing NULL uses the
///                                 callback as default fallback. See RpsProgramCreateInfo::defaultNodeCallback for
///                                 details.
/// @param cmdCallback              Pointer to a function to be bound to the node as the command recording callback.
/// @param flags                    Flags for the callback behavior.
///
/// @returns                        Result code of the operation. See <c><i>RpsResult</i></c> for more info.
template <typename TFunc,
          typename TContext = rps::details::NonMemberNodeCallbackContext<TFunc>,
          typename          = typename std::enable_if<std::is_trivially_destructible<TContext>::value &&
                                             !std::is_member_function_pointer<TFunc>::value>::type>
RpsResult rpsProgramBindNode(RpsSubprogram       hProgram,
                             const char*         name,
                             TFunc               cmdCallback,
                             RpsCmdCallbackFlags flags = RPS_CMD_CALLBACK_FLAG_NONE)
{
    RpsCmdCallback* pSlot  = {};
    RpsResult       result = rps::details::ProgramGetBindingSlot(hProgram, name, sizeof(TContext), &pSlot);

    if (RPS_SUCCEEDED(result))
    {
        new (pSlot->pUserContext) TContext(cmdCallback);
        pSlot->pfnCallback = TContext::Callback;
        pSlot->flags       = flags;
    }

    return result;
}

/// @brief Binds a command callback implementation to a node type in an rps program.
///
/// @param hProgram                 Handle to the subprogram to bind the node. Must not be RPS_NULL_HANDLE.
/// @param name                     Null terminated string with the name of the node to bind to. Passing NULL uses the
///                                 callback as a fallback for nodes without a definition.
/// @param pfnCmdCallback           Function pointer of type PFN_rpsCmdCallback to be bound to the node as the command
///                                 recording callback.
/// @param pCallbackContext         User defined context to be passed back when the callback is called.
/// @param flags                    Flags for the callback behavior.
///
/// @returns                        Result code of the operation. See <c><i>RpsResult</i></c> for more info.
static inline RpsResult rpsProgramBindNode(RpsSubprogram       hProgram,
                                           const char*         name,
                                           PFN_rpsCmdCallback  pfnCmdCallback,
                                           void*               pCallbackContext = nullptr,
                                           RpsCmdCallbackFlags flags            = RPS_CMD_CALLBACK_FLAG_NONE)
{
    RpsCmdCallback callbackInfo = {};
    callbackInfo.pfnCallback    = pfnCmdCallback;
    callbackInfo.pUserContext   = pCallbackContext;
    callbackInfo.flags          = flags;

    return rpsProgramBindNodeCallback(hProgram, name, &callbackInfo);
}

/// @} end addtogroup RpsSubprogram

#endif  //__cplusplus

/// @} end defgroup Runntime

#endif  //RPS_RUNTIME_H
