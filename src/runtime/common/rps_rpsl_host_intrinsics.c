// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include <setjmp.h>
#include <math.h>

#include "rps/core/rps_api.h"

#include "runtime/common/rps_rpsl_host.h"

extern RpsResult RpslHostBlockMarker(uint32_t markerType,
                                     uint32_t blockIndex,
                                     uint32_t resourceCount,
                                     uint32_t nodeCount,
                                     uint32_t localLoopIndex,
                                     uint32_t numChildren,
                                     uint32_t parentId);

extern RpsResult RpslHostCallNode(uint32_t  nodeDeclId,
                                  uint32_t  numArgs,
                                  void**    ppArgs,
                                  uint32_t  nodeCallFlags,
                                  uint32_t  localNodeId,
                                  uint32_t* pCmdIdOut);

extern RpsResult RpslHostNodeDependencies(uint32_t numDeps, const uint32_t* pDeps, uint32_t dstNodeId);

extern RpsResult RpslHostDescribeHandle(void*           pOutData,
                                        uint32_t        dataSize,
                                        const uint32_t* inHandle,
                                        uint32_t        describeOp);

extern RpsResult RpslHostCreateResource(uint32_t  type,
                                        uint32_t  flags,
                                        uint32_t  format,
                                        uint32_t  width,
                                        uint32_t  height,
                                        uint32_t  depthOrArraySize,
                                        uint32_t  mipLevels,
                                        uint32_t  sampleCount,
                                        uint32_t  sampleQuality,
                                        uint32_t  temporalLayers,
                                        uint32_t  id,
                                        uint32_t* pOutResourceId);

RpsResult RpslHostNameResource(uint32_t resourceHdl, const char* name, uint32_t nameLength);

RpsResult RpslNotifyOutParamResources(uint32_t paramId, const void* pViews);

RpsResult RpslSchedulerMarker(uint32_t opCode, uint32_t flags, const char* name, uint32_t nameLength);

void RpslNotifyAbort(RpsResult result);

// Thread local
#ifdef __cplusplus
#define RPS_THREAD_LOCAL thread_local
#else  // #ifdef __cplusplus
#ifdef _MSC_VER
#define RPS_THREAD_LOCAL __declspec(thread)
#else
#define RPS_THREAD_LOCAL __thread
#endif  // #ifdef _MSC_VER
#endif  // #ifdef __cplusplus

RPS_THREAD_LOCAL jmp_buf* tls_pJmpBuf = NULL;

static inline void RpslAbortIfFail(RpsResult result)
{
    if (RPS_FAILED(result))
    {
        RpslNotifyAbort(result);
        longjmp(*tls_pJmpBuf, result);
    }
}

RpsResult RpslHostCallEntry(PFN_RpslEntry pfnEntry, uint32_t numArgs, const void* const* ppArgs)
{
    RpsResult result = RPS_OK;

    jmp_buf jmpBuf;

    jmp_buf* pPrevJmpBuf = tls_pJmpBuf;
    tls_pJmpBuf          = &jmpBuf;

    if ((result = setjmp(jmpBuf)) == 0)
    {
        pfnEntry(numArgs, ppArgs, RPSL_ENTRY_CALL_DEFAULT);
    }

    tls_pJmpBuf = pPrevJmpBuf;

    return result;
}

void ___rpsl_abort(uint32_t errorCode)
{
    RpslAbortIfFail(errorCode);
}

uint32_t ___rpsl_node_call(
    uint32_t nodeDeclId, uint32_t numArgs, uint8_t** ppArgs, uint32_t nodeCallFlags, uint32_t nodeId)
{
    uint32_t cmdId;
    RpslAbortIfFail(RpslHostCallNode(nodeDeclId, numArgs, (void**)ppArgs, nodeCallFlags, nodeId, &cmdId));

    return cmdId;
}

void ___rpsl_node_dependencies(uint32_t numDeps, uint32_t* pDeps, uint32_t dstNodeId)
{
    RpslAbortIfFail(RpslHostNodeDependencies(numDeps, pDeps, dstNodeId));
}

void ___rpsl_block_marker(uint32_t markerType,
                          uint32_t blockIndex,
                          uint32_t resourceCount,
                          uint32_t nodeCount,
                          uint32_t localLoopIndex,
                          uint32_t numChildren,
                          uint32_t parentId)
{
    RpslAbortIfFail(
        RpslHostBlockMarker(markerType, blockIndex, resourceCount, nodeCount, localLoopIndex, numChildren, parentId));
}

void ___rpsl_scheduler_marker(uint32_t opCode, uint32_t flags, unsigned char* name, uint32_t nameLength)
{
    RpslAbortIfFail(RpslSchedulerMarker(opCode, flags, (char*)name, nameLength));
}

void ___rpsl_describe_handle(uint8_t* pOutData, uint32_t dataSize, uint32_t* inHandle, uint32_t describeOp)
{
    RpslAbortIfFail(RpslHostDescribeHandle((void*)pOutData, dataSize, inHandle, describeOp));
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
    uint32_t resourceId;
    RpslAbortIfFail(RpslHostCreateResource(type,
                                           flags,
                                           format,
                                           width,
                                           height,
                                           depthOrArraySize,
                                           mipLevels,
                                           sampleCount,
                                           sampleQuality,
                                           temporalLayers,
                                           id,
                                           &resourceId));
    return resourceId;
}

void ___rpsl_name_resource(uint32_t resourceHdl, unsigned char* name, uint32_t nameLength)
{
    RpslAbortIfFail(RpslHostNameResource(resourceHdl, (char*)name, nameLength));
}

void ___rpsl_notify_out_param_resources(uint32_t paramId, uint8_t* pViews)
{
    RpslAbortIfFail(RpslNotifyOutParamResources(paramId, (void*)pViews));
}

#define RPS_SHADER_HOST 1

#include "rps_rpsl_host_dll.c"

// DXIL Intrinsics
// TODO: Generate from hctdb
enum DXILOpCode
{
    // Binary float
    FMax = 35,  // returns a if a >= b, else b
    FMin = 36,  // returns a if a < b, else b

    // Binary int with two outputs
    IMul = 41,  // multiply of 32-bit operands to produce the correct full 64-bit result.

    // Binary int
    IMax = 37,  // IMax(a,b) returns a if a > b, else b
    IMin = 38,  // IMin(a,b) returns a if a < b, else b

    // Binary uint with carry or borrow
    UAddc = 44,  // unsigned add of 32-bit operand with the carry
    USubb = 45,  // unsigned subtract of 32-bit operands with the borrow

    // Binary uint with two outputs
    UDiv = 43,  // unsigned divide of the 32-bit operand src0 by the 32-bit operand src1.
    UMul = 42,  // multiply of 32-bit operands to produce the correct full 64-bit result.

    // Binary uint
    UMax = 39,  // unsigned integer maximum. UMax(a,b) = a > b ? a : b
    UMin = 40,  // unsigned integer minimum. UMin(a,b) = a < b ? a : b

    // Bitcasts with different sizes
    BitcastF16toI16 = 125,  // bitcast between different sizes
    BitcastF32toI32 = 127,  // bitcast between different sizes
    BitcastF64toI64 = 129,  // bitcast between different sizes
    BitcastI16toF16 = 124,  // bitcast between different sizes
    BitcastI32toF32 = 126,  // bitcast between different sizes
    BitcastI64toF64 = 128,  // bitcast between different sizes

    // Dot product with accumulate
    Dot2AddHalf     = 162,  // 2D half dot product with accumulate to float
    Dot4AddI8Packed = 163,  // signed dot product of 4 x i8 vectors packed into i32, with accumulate to i32
    Dot4AddU8Packed = 164,  // unsigned dot product of 4 x u8 vectors packed into i32, with accumulate to i32

    // Dot
    Dot2 = 54,  // Two-dimensional vector dot-product
    Dot3 = 55,  // Three-dimensional vector dot-product
    Dot4 = 56,  // Four-dimensional vector dot-product

    // Double precision
    LegacyDoubleToFloat  = 132,  // legacy fuction to convert double to float
    LegacyDoubleToSInt32 = 133,  // legacy fuction to convert double to int32
    LegacyDoubleToUInt32 = 134,  // legacy fuction to convert double to uint32
    MakeDouble           = 101,  // creates a double value
    SplitDouble          = 102,  // splits a double into low and high parts

    // Legacy floating-point
    LegacyF16ToF32 = 131,  // legacy fuction to convert half (f16) to float (f32) (this is not related to min-precision)
    LegacyF32ToF16 = 130,  // legacy fuction to convert float (f32) to half (f16) (this is not related to min-precision)

    // Packing intrinsics
    Pack4x8 = 220,  // packs vector of 4 signed or unsigned values into a packed datatype, drops or clamps unused bits

    // Quaternary
    Bfi = 53,  // Given a bit range from the LSB of a number, places that number of bits in another number at any offset

    // Tertiary float
    FMad = 46,  // floating point multiply & add
    Fma  = 47,  // fused multiply-add

    // Tertiary int
    IMad = 48,  // Signed integer multiply & add
    Ibfe = 51,  // Integer bitfield extract
    Msad = 50,  // masked Sum of Absolute Differences.

    // Tertiary uint
    UMad = 49,  // Unsigned integer multiply & add
    Ubfe = 52,  // Unsigned integer bitfield extract

    // Unary float - rounding
    Round_ne = 26,  // floating-point round to integral float.
    Round_ni = 27,  // floating-point round to integral float.
    Round_pi = 28,  // floating-point round to integral float.
    Round_z  = 29,  // floating-point round to integral float.

    // Unary float
    Acos =
        15,  // Returns the arccosine of the specified value. Input should be a floating-point value within the range of -1 to 1.
    Asin =
        16,  // Returns the arccosine of the specified value. Input should be a floating-point value within the range of -1 to 1
    Atan = 17,  // Returns the arctangent of the specified value. The return value is within the range of -PI/2 to PI/2.
    Cos  = 12,  // returns cosine(theta) for theta in radians.
    Exp  = 21,  // returns 2^exponent
    FAbs = 6,   // returns the absolute value of the input value.
    Frc  = 22,  // extract fracitonal component.
    Hcos = 18,  // returns the hyperbolic cosine of the specified value.
    Hsin = 19,  // returns the hyperbolic sine of the specified value.
    Htan = 20,  // returns the hyperbolic tangent of the specified value.
    IsFinite = 10,  // Returns true if x is finite, false otherwise.
    IsInf    = 9,   // Returns true if x is +INF or -INF, false otherwise.
    IsNaN    = 8,   // Returns true if x is NAN or QNAN, false otherwise.
    IsNormal = 11,  // returns IsNormal
    Log      = 23,  // returns log base 2.
    Rsqrt    = 25,  // returns reciprocal square root (1 / sqrt(src)
    Saturate = 7,   // clamps the result of a single or double precision floating point value to [0.0f...1.0f]
    Sin      = 13,  // returns sine(theta) for theta in radians.
    Sqrt     = 24,  // returns square root
    Tan      = 14,  // returns tan(theta) for theta in radians.

    // Unary int
    Bfrev     = 30,  // Reverses the order of the bits.
    Countbits = 31,  // Counts the number of bits in the input integer.
    FirstbitLo =
        32,  // Returns the location of the first set bit starting from the lowest order bit and working upward.
    FirstbitSHi = 34,  // Returns the location of the first set bit from the highest order bit based on the sign.

    // Unary uint
    FirstbitHi =
        33,  // Returns the location of the first set bit starting from the highest order bit and working downward.

    // Unpacking intrinsics
    Unpack4x8 = 219,  // unpacks 4 8-bit signed or unsigned values into int32 or int16 vector
};

uint32_t ___rpsl_dxop_binary_i32(uint32_t op, uint32_t a, uint32_t b)
{
    switch (op)
    {
    case IMax:
        return (int32_t)a > (int32_t)b ? a : b;
    case IMin:
        return (int32_t)a < (int32_t)b ? a : b;
    case UMax:
        return a > b ? a : b;
    case UMin:
        return a < b ? a : b;

    default:
        break;
    }

    ___rpsl_abort(RPS_ERROR_NOT_IMPLEMENTED);
    return 0;
}

uint32_t RpslHostReverseBits32(uint32_t value);
uint32_t RpslHostCountBits(uint32_t value);
uint32_t RpslHostFirstBitLow(uint32_t value);
uint32_t RpslHostFirstBitHigh(uint32_t value);

uint32_t ___rpsl_dxop_unary_i32(uint32_t op, uint32_t a)
{
    switch (op)
    {
    case Bfrev:
        return RpslHostReverseBits32(a);
    case Countbits:
        return RpslHostCountBits(a);
    case FirstbitLo:
        return RpslHostFirstBitLow(a);
    case FirstbitSHi:
        return 31 - RpslHostFirstBitHigh(((int32_t)a < 0) ? ~a : a);
    case FirstbitHi:
    {
        uint32_t fbh = RpslHostFirstBitHigh(a);
        return fbh == 32 ? 0 : (31 - fbh);
    }

    default:
        break;
    }

    ___rpsl_abort(RPS_ERROR_NOT_IMPLEMENTED);
    return 0;
}

uint32_t ___rpsl_dxop_tertiary_i32(uint32_t op, uint32_t a, uint32_t b, uint32_t c)
{
    switch (op)
    {
    case IMad:
        return (int32_t)a * (int32_t)b + (int32_t)c;
    case UMad:
        return a * b + c;
    case Ubfe:
    case Ibfe:
    case Msad:
    default:
        break;
    }

    ___rpsl_abort(RPS_ERROR_NOT_IMPLEMENTED);
    return 0;
}

float ___rpsl_dxop_binary_f32(uint32_t op, float a, float b)
{
    switch (op)
    {
    case FMax:
        return a >= b ? a : b;
    case FMin:
        return a < b ? a : b;
    default:
        break;
    }

    ___rpsl_abort(RPS_ERROR_NOT_IMPLEMENTED);
    return 0;
}

float ___rpsl_dxop_unary_f32(uint32_t op, float a)
{
    switch (op)
    {
    case Acos:
        return acosf(a);
    case Asin:
        return asinf(a);
    case Atan:
        return atanf(a);
    case Cos:
        return cosf(a);
    case Exp:
        return exp2f(a);
    case FAbs:
        return fabsf(a);
    case Frc:
        return a - floorf(a);
    case Hcos:
        return coshf(a);
    case Hsin:
        return sinhf(a);
    case Htan:
        return tanhf(a);
    case Log:
        return logf(a);
    case Rsqrt:
        return 1 / sqrtf(a);
    case Saturate:
        return fminf(fmaxf(a, 0.0f), 1.0f);
    case Sin:
        return sinf(a);
    case Sqrt:
        return sqrtf(a);
    case Tan:
        return tanf(a);

    case Round_ne:
        return roundf(a * 0.5f) * 2.0f;
    case Round_ni:
        return floorf(a);
    case Round_pi:
        return ceilf(a);
    case Round_z:
        return truncf(a);
    }

    ___rpsl_abort(RPS_ERROR_NOT_IMPLEMENTED);
    return 0;
}

uint8_t ___rpsl_dxop_isSpecialFloat_f32(uint32_t op, float a)
{
    switch (op)
    {
    case IsFinite:
        return isfinite(a);
    case IsInf:
        return isinf(a);
    case IsNaN:
        return isnan(a);
    case IsNormal:
        return isnormal(a);
    };

    ___rpsl_abort(RPS_ERROR_NOT_IMPLEMENTED);
    return 0;
}

float ___rpsl_dxop_tertiary_f32(uint32_t op, float a, float b, float c)
{
    switch (op)
    {
    case FMad:
        return a * b + c;
    case Fma:
        return fmaf(a, b, c);
    default:
        break;
    }

    ___rpsl_abort(RPS_ERROR_NOT_IMPLEMENTED);
    return 0;
}

RpsResult rpsRpslDynamicLibraryInit(PFN_rpslDynLibInit pfn_dynLibInit)
{
    ___rpsl_runtime_procs procs;
    memset(&procs, 0, sizeof(procs));

    procs.pfn_rpsl_abort                      = &___rpsl_abort;
    procs.pfn_rpsl_node_call                  = &___rpsl_node_call;
    procs.pfn_rpsl_node_dependencies          = &___rpsl_node_dependencies;
    procs.pfn_rpsl_block_marker               = &___rpsl_block_marker;
    procs.pfn_rpsl_scheduler_marker           = &___rpsl_scheduler_marker;
    procs.pfn_rpsl_describe_handle            = &___rpsl_describe_handle;
    procs.pfn_rpsl_create_resource            = &___rpsl_create_resource;
    procs.pfn_rpsl_name_resource              = &___rpsl_name_resource;
    procs.pfn_rpsl_notify_out_param_resources = &___rpsl_notify_out_param_resources;
    procs.pfn_rpsl_dxop_unary_i32             = &___rpsl_dxop_unary_i32;
    procs.pfn_rpsl_dxop_binary_i32            = &___rpsl_dxop_binary_i32;
    procs.pfn_rpsl_dxop_tertiary_i32          = &___rpsl_dxop_tertiary_i32;
    procs.pfn_rpsl_dxop_unary_f32             = &___rpsl_dxop_unary_f32;
    procs.pfn_rpsl_dxop_binary_f32            = &___rpsl_dxop_binary_f32;
    procs.pfn_rpsl_dxop_tertiary_f32          = &___rpsl_dxop_tertiary_f32;
    procs.pfn_rpsl_dxop_isSpecialFloat_f32    = &___rpsl_dxop_isSpecialFloat_f32;

    pfn_dynLibInit(&procs, sizeof(procs));

    return RPS_OK;
}
