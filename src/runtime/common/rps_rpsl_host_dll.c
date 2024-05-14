// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#if defined(RPS_SHADER_GUEST)

typedef unsigned int  uint32_t;
typedef unsigned char uint8_t;

#endif  //RPS_SHADER_GUEST

#define RPS_RPSL_INTERFACE_DECL \
    RPSL_RETURN() 

#if defined(RPS_SHADER_GUEST) || defined(RPS_SHADER_HOST)

#ifdef _WIN32
#define RPS_EXPORT __declspec(dllexport)
#else
#define RPS_EXPORT
#endif

typedef void     (*PFN_rpsl_abort)                      (uint32_t result);
typedef uint32_t (*PFN_rpsl_node_call)                  (uint32_t nodeDeclId, uint32_t numArgs, uint8_t** ppArgs, uint32_t nodeCallFlags, uint32_t nodeId);
typedef void     (*PFN_rpsl_node_dependencies)          (uint32_t numDeps, uint32_t* pDeps, uint32_t dstNodeId);
typedef void     (*PFN_rpsl_block_marker)               (uint32_t markerType,
                                                         uint32_t blockIndex,
                                                         uint32_t resourceCount,
                                                         uint32_t nodeCount,
                                                         uint32_t localLoopIndex,
                                                         uint32_t numChildren,
                                                         uint32_t parentId);
typedef void     (*PFN_rpsl_scheduler_marker)           (uint32_t opCode, uint32_t flags, unsigned char* name, uint32_t nameLength);
typedef void     (*PFN_rpsl_describe_handle)            (uint8_t* pDstDesc, uint32_t bufferSize, uint32_t* pHandle, uint32_t describeOp);
typedef uint32_t (*PFN_rpsl_create_resource)            (uint32_t type,
                                                         uint32_t flags,
                                                         uint32_t format,
                                                         uint32_t width,
                                                         uint32_t height,
                                                         uint32_t depthOrArraySize,
                                                         uint32_t mipLevels,
                                                         uint32_t sampleCount,
                                                         uint32_t sampleQuality,
                                                         uint32_t temporalLayers,
                                                         uint32_t id);
typedef void     (*PFN_rpsl_name_resource)              (uint32_t resourceHdl, unsigned char* name, uint32_t nameLength);
typedef void     (*PFN_rpsl_notify_out_param_resources) (uint32_t paramId, uint8_t* pViews);

typedef uint32_t (*PFN_rpsl_dxop_unary_i32)             (uint32_t op, uint32_t a);
typedef uint32_t (*PFN_rpsl_dxop_binary_i32)            (uint32_t op, uint32_t a, uint32_t b);
typedef uint32_t (*PFN_rpsl_dxop_tertiary_i32)          (uint32_t op, uint32_t a, uint32_t b, uint32_t c);
typedef float    (*PFN_rpsl_dxop_unary_f32)             (uint32_t op, float a);
typedef float    (*PFN_rpsl_dxop_binary_f32)            (uint32_t op, float a, float b);
typedef float    (*PFN_rpsl_dxop_tertiary_f32)          (uint32_t op, float a, float b, float c);
typedef uint8_t  (*PFN_rpsl_dxop_isSpecialFloat_f32)    (uint32_t op, float a);

typedef struct ___rpsl_runtime_procs
{
    PFN_rpsl_abort                      pfn_rpsl_abort;
    PFN_rpsl_node_call                  pfn_rpsl_node_call;
    PFN_rpsl_node_dependencies          pfn_rpsl_node_dependencies;
    PFN_rpsl_block_marker               pfn_rpsl_block_marker;
    PFN_rpsl_scheduler_marker           pfn_rpsl_scheduler_marker;
    PFN_rpsl_describe_handle            pfn_rpsl_describe_handle;
    PFN_rpsl_create_resource            pfn_rpsl_create_resource;
    PFN_rpsl_name_resource              pfn_rpsl_name_resource;
    PFN_rpsl_notify_out_param_resources pfn_rpsl_notify_out_param_resources;
    PFN_rpsl_dxop_unary_i32             pfn_rpsl_dxop_unary_i32;
    PFN_rpsl_dxop_binary_i32            pfn_rpsl_dxop_binary_i32;
    PFN_rpsl_dxop_tertiary_i32          pfn_rpsl_dxop_tertiary_i32;
    PFN_rpsl_dxop_unary_f32             pfn_rpsl_dxop_unary_f32;
    PFN_rpsl_dxop_binary_f32            pfn_rpsl_dxop_binary_f32;
    PFN_rpsl_dxop_tertiary_f32          pfn_rpsl_dxop_tertiary_f32;
    PFN_rpsl_dxop_isSpecialFloat_f32    pfn_rpsl_dxop_isSpecialFloat_f32;
} ___rpsl_runtime_procs;

typedef int (*PFN_rps_dyn_lib_init)(const ___rpsl_runtime_procs* pProcs, uint32_t sizeofProcs);

#endif  // defined(RPS_SHADER_GUEST) || defined(RPS_SHADER_HOST)

#if defined(RPS_SHADER_GUEST)

static ___rpsl_runtime_procs s_rpslRuntimeProcs;

#define RPS_TRAMPOLINE_IMPL_RET(RetType, FuncName, ParamsList, Args) \
    RetType FuncName ParamsList                                      \
    {                                                                \
        return (*s_rpslRuntimeProcs.pfn##FuncName)Args;              \
    }

#define RPS_TRAMPOLINE_IMPL(FuncName, ParamsList, Args) \
    void FuncName ParamsList                            \
    {                                                   \
        (*s_rpslRuntimeProcs.pfn##FuncName) Args;       \
    }

void ___rpsl_abort(uint32_t result)
{
    (*s_rpslRuntimeProcs.pfn_rpsl_abort)(result);
}

uint32_t ___rpsl_node_call(
    uint32_t nodeDeclId, uint32_t numArgs, uint8_t** ppArgs, uint32_t nodeCallFlags, uint32_t nodeId)
{
    return (*s_rpslRuntimeProcs.pfn_rpsl_node_call)(nodeDeclId, numArgs, ppArgs, nodeCallFlags, nodeId);
}

void ___rpsl_node_dependencies(uint32_t numDeps, uint32_t* pDeps, uint32_t dstNodeId)
{
    (*s_rpslRuntimeProcs.pfn_rpsl_node_dependencies)(numDeps, pDeps, dstNodeId);
}

void ___rpsl_block_marker(uint32_t markerType,
                          uint32_t blockIndex,
                          uint32_t resourceCount,
                          uint32_t nodeCount,
                          uint32_t localLoopIndex,
                          uint32_t numChildren,
                          uint32_t parentId)
{
    (*s_rpslRuntimeProcs.pfn_rpsl_block_marker)(markerType, blockIndex, resourceCount, nodeCount, localLoopIndex, numChildren, parentId);
}

void ___rpsl_scheduler_marker(uint32_t opCode, uint32_t flags, unsigned char* name, uint32_t nameLength)
{
    (*s_rpslRuntimeProcs.pfn_rpsl_scheduler_marker)(opCode, flags, name, nameLength);
}

void ___rpsl_describe_handle(uint8_t* pOutData, uint32_t dataSize, uint32_t* inHandle, uint32_t describeOp)
{
    (*s_rpslRuntimeProcs.pfn_rpsl_describe_handle)(pOutData, dataSize, inHandle, describeOp);
}

uint32_t ___rpsl_create_resource(uint32_t type,
                                 uint32_t flags,
                                 uint32_t format,
                                 uint32_t width,
                                 uint32_t height,
                                 uint32_t depthOrArraySize,
                                 uint32_t mipLevels,
                                 uint32_t sampleCount,
                                 uint32_t sampleQuality,
                                 uint32_t temporalLayers,
                                 uint32_t id)
{
    return (*s_rpslRuntimeProcs.pfn_rpsl_create_resource)(
        type, flags, format, width, height, depthOrArraySize, mipLevels, sampleCount, sampleQuality, temporalLayers, id);
}

void ___rpsl_name_resource(uint32_t resourceHdl, unsigned char* name, uint32_t nameLength)
{
    (*s_rpslRuntimeProcs.pfn_rpsl_name_resource)(resourceHdl, name, nameLength);
}

void ___rpsl_notify_out_param_resources(uint32_t paramId, uint8_t* pViews)
{
    (*s_rpslRuntimeProcs.pfn_rpsl_notify_out_param_resources)(paramId, pViews);
}

uint32_t ___rpsl_dxop_unary_i32(uint32_t op, uint32_t a)
{
    return (*s_rpslRuntimeProcs.pfn_rpsl_dxop_unary_i32)(op, a);
}

uint32_t ___rpsl_dxop_binary_i32(uint32_t op, uint32_t a, uint32_t b)
{
    return (*s_rpslRuntimeProcs.pfn_rpsl_dxop_binary_i32)(op, a, b);
}

uint32_t ___rpsl_dxop_tertiary_i32(uint32_t op, uint32_t a, uint32_t b, uint32_t c)
{
    return (*s_rpslRuntimeProcs.pfn_rpsl_dxop_tertiary_i32)(op, a, b, c);
}

float ___rpsl_dxop_unary_f32(uint32_t op, float a)
{
    return (*s_rpslRuntimeProcs.pfn_rpsl_dxop_unary_f32)(op, a);
}

float ___rpsl_dxop_binary_f32(uint32_t op, float a, float b)
{
    return (*s_rpslRuntimeProcs.pfn_rpsl_dxop_binary_f32)(op, a, b);
}

float ___rpsl_dxop_tertiary_f32(uint32_t op, float a, float b, float c)
{
    return (*s_rpslRuntimeProcs.pfn_rpsl_dxop_tertiary_f32)(op, a, b, c);
}

uint8_t ___rpsl_dxop_isSpecialFloat_f32(uint32_t op, float a)
{
    return (*s_rpslRuntimeProcs.pfn_rpsl_dxop_isSpecialFloat_f32)(op, a);
}

int RPS_EXPORT ___rps_dyn_lib_init(const ___rpsl_runtime_procs* pProcs, uint32_t sizeofProcs)
{
    if (sizeof(___rpsl_runtime_procs) != sizeofProcs)
    {
        return -1;
    }

    s_rpslRuntimeProcs.pfn_rpsl_abort                   = pProcs->pfn_rpsl_abort;
    s_rpslRuntimeProcs.pfn_rpsl_node_call               = pProcs->pfn_rpsl_node_call;
    s_rpslRuntimeProcs.pfn_rpsl_node_dependencies       = pProcs->pfn_rpsl_node_dependencies;
    s_rpslRuntimeProcs.pfn_rpsl_block_marker            = pProcs->pfn_rpsl_block_marker;
    s_rpslRuntimeProcs.pfn_rpsl_scheduler_marker        = pProcs->pfn_rpsl_scheduler_marker;
    s_rpslRuntimeProcs.pfn_rpsl_describe_handle         = pProcs->pfn_rpsl_describe_handle;
    s_rpslRuntimeProcs.pfn_rpsl_create_resource         = pProcs->pfn_rpsl_create_resource;
    s_rpslRuntimeProcs.pfn_rpsl_name_resource           = pProcs->pfn_rpsl_name_resource;
    s_rpslRuntimeProcs.pfn_rpsl_notify_out_param_resources = pProcs->pfn_rpsl_notify_out_param_resources;
    s_rpslRuntimeProcs.pfn_rpsl_dxop_unary_i32          = pProcs->pfn_rpsl_dxop_unary_i32 ;
    s_rpslRuntimeProcs.pfn_rpsl_dxop_binary_i32         = pProcs->pfn_rpsl_dxop_binary_i32;
    s_rpslRuntimeProcs.pfn_rpsl_dxop_tertiary_i32       = pProcs->pfn_rpsl_dxop_tertiary_i32;
    s_rpslRuntimeProcs.pfn_rpsl_dxop_unary_f32          = pProcs->pfn_rpsl_dxop_unary_f32 ;
    s_rpslRuntimeProcs.pfn_rpsl_dxop_binary_f32         = pProcs->pfn_rpsl_dxop_binary_f32;
    s_rpslRuntimeProcs.pfn_rpsl_dxop_tertiary_f32       = pProcs->pfn_rpsl_dxop_tertiary_f32;
    s_rpslRuntimeProcs.pfn_rpsl_dxop_isSpecialFloat_f32 = pProcs->pfn_rpsl_dxop_isSpecialFloat_f32;

    return 0;
}

#endif  //RPS_SHADER_GUEST

// clang-format off

