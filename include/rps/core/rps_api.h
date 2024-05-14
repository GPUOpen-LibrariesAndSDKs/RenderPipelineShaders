// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_API_H
#define RPS_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdarg.h>
#include <string.h>

/// @defgroup Core Core Components
/// @{

#include "rps/core/rps_result.h"

//
// Definitions
//

/// @defgroup Basic Basic Types and Definitions
/// @{

/// @brief Macro for defining a handle type to an internal implementation structure.
///
/// A handle is a pointer to an internal struct type with a name postfixed with _T. e.g: a handle called
/// <c><i>RpsDevice</i></c> should have a matching structure available called <c><i>RpsDevice_T</i></c>.
#define RPS_DEFINE_HANDLE(TypeName) typedef struct TypeName##_T* TypeName;

//defines a bool() operator for checking if an opaque handle is NULL.
#ifdef __cplusplus
#define RPS_OPAQUE_HANDLE_METHODS() \
    operator bool() const           \
    {                               \
        return ptr != nullptr;      \
    }
#else
#define RPS_OPAQUE_HANDLE_METHODS()
#endif  //__cplusplus

/// @brief Macro for declaring an opaque handle type.
///
/// This macro is useful for forward declarations of such handle types in headers and not strictly required. May only
/// be used in conjunction with <c><i>RPS_DEFINE_OPAQUE_HANDLE</i></c>.
#define RPS_DECLARE_OPAQUE_HANDLE(TypeName) typedef struct TypeName##_T TypeName;

/// @brief Macro to define an opaque handle type.
///
/// An opaque handle is a pointer to an unspecified type that may vary depending on the runtime implementation. This
/// type can be different for e.g. a Vulkan and a DX12 backend. In addition to the pointer itself, the handle provides
/// additional methods for C++ compilation units.
#define RPS_DEFINE_OPAQUE_HANDLE(TypeName) \
    typedef struct TypeName##_T            \
    {                                      \
        void* ptr;                         \
        RPS_OPAQUE_HANDLE_METHODS()        \
    } TypeName;

/// @brief A macro for defining a mapping between an opaque handle type and an underlying implementation type.
///
/// Includes conversions from and to the given object type.
#define RPS_IMPL_OPAQUE_HANDLE(Name, HandleType, ObjectType)        \
    static inline ObjectType* rps##Name##FromHandle(HandleType hdl) \
    {                                                               \
        return (ObjectType*)(hdl.ptr);                              \
    }                                                               \
    static inline HandleType rps##Name##ToHandle(ObjectType* ptr)   \
    {                                                               \
        return {ptr};                                               \
    }

/// @brief Constant for an invalid RPS object handle value.
#define RPS_NULL_HANDLE 0

/// @brief Boolean value type.
typedef int32_t RpsBool;

/// @brief Constant for a boolean value holding a value of true. For use with <c><i>RpsBool</i></c>.
#define RPS_TRUE (1)

/// @brief Constant for a boolean value holding a value of false. For use with <c><i>RpsBool</i></c>.
#define RPS_FALSE (0)

/// @brief Type for holding up to 16 bitflags.
typedef uint16_t RpsFlags16;

/// @brief Type for holding up to 32 bitflags.
typedef uint32_t RpsFlags32;

/// @brief Type for holding up to 64 bitflags.
typedef uint64_t RpsFlags64;

/// @brief Type for general 32-bit index values.
typedef uint32_t RpsIndex32;

/// @brief Constant for an invalid unsigned 32-bit index value.
#define RPS_INDEX_NONE_U32 (UINT32_MAX)

/// @brief Constant for an invalid signed 32-bit index value.
#define RPS_INDEX_NONE_I32 (-1)

/// @brief Constant for the maximum length of names supported by RPS APIs (including the terminating '\0').
#define RPS_NAME_MAX_LEN (256)

/// @} end defgroup Basic

//
// Types
//

/// @defgroup RpsAllocator RpsAllocator
/// @{

/// @brief Signature of functions for allocating memory.
///
/// @param pContext                 Context for memory allocation.
/// @param size                     Size of the desired allocation in bytes.
/// @param alignment                Minimum alignment requirement of the desired allocation in bytes.
///
/// @returns                        Pointer to the allocated memory.
typedef void* (*PFN_rpsAlloc)(void* pContext, size_t size, size_t alignment);

/// @brief Signature of functions for reallocating memory.
///
/// @param pUserContext             Context for memory allocation.
/// @param oldBuffer                Address of the memory allocation to be reallocated.
///                                 Must not be NULL for oldSize != 0.
/// @param oldSize                  Size of the old memory allocation in bytes.
/// @param newSize                  Desired size of the allocation in bytes.
/// @param alignment                Minimum alignment requirement of the desired allocation in bytes.
///
/// @returns                        Pointer to the (re-)allocated memory.
typedef void* (*PFN_rpsRealloc)(void* pUserContext, void* oldBuffer, size_t oldSize, size_t newSize, size_t alignment);

/// @brief Signature of functions for freeing allocated memory.
///
/// @param pUserContext             Context for memory allocation.
/// @param buffer                   Address of the memory allocation to be freed.
typedef void (*PFN_rpsFree)(void* pUserContext, void* buffer);

/// @brief Memory allocator interface.
typedef struct RpsAllocator
{
    PFN_rpsAlloc   pfnAlloc;    ///< Pointer to a function for allocating memory.
    PFN_rpsFree    pfnFree;     ///< Pointer to a function for releasing memory.
    PFN_rpsRealloc pfnRealloc;  ///< Pointer to a function for reallocating memory.
    void*          pContext;    ///< Context to be passed to the allocator functions.
} RpsAllocator;

/// @brief Requirements for a single memory allocation.
typedef struct RpsAllocInfo
{
    size_t size;       ///< Size of the allocation in bytes.
    size_t alignment;  ///< Minimum alignment requirement of the allocation in bytes.
} RpsAllocInfo;

/// @} end defgroup RpsAllocator

/// @defgroup RpsPrinter RpsPrinter
/// @{

/// @brief Signature of functions for printing with variadic arguments.
///
/// @param pContext                 Context for printing. See <c><i>RpsPrinter</i></c>.
/// @param format                   Format string for the print operation matching the C99 specification for printf.
/// @param ...                      List of arguments matching the requirements of the contents of <c><i>format</i></c>.
typedef void (*PFN_rpsPrintf)(void* pContext, const char* format, ...);

/// @brief Signature of functions for printing with a variable argument list.
///
/// @param pContext                 Context for printing. See <c><i>RpsPrinter</i></c>.
/// @param format                   Format string for the print operation matching the C99 specification for printf.
/// @param vl                       Variable argument list matching the requirements of the contents of
///                                 <c><i>format</i></c>.
typedef void (*PFN_rpsVPrintf)(void* pContext, const char* format, va_list vl);

/// @brief Printer interface.
typedef struct RpsPrinter
{
    PFN_rpsPrintf  pfnPrintf;   ///< Pointer to a function for printing with variadic arguments.
    PFN_rpsVPrintf pfnVPrintf;  ///< Pointer to a function for printing with a variable argument list.
    void*          pContext;    ///< Context to be passed to the print functions.
} RpsPrinter;

/// @} end defgroup RpsPrinter

/// @brief Signature of functions for generating random integers uniformly distributed on the closed interval
/// [minValue, maxValue].
///
/// @param pContext                 Context for generating random numbers.
/// @param minValue                 Minimum output value.
/// @param maxValue                 Maximum output value.
///
/// @returns                        Generated uniform random value.
typedef int32_t (*PFN_rpsRandomUniformInt)(void* pContext, int32_t minValue, int32_t maxValue);

/// @brief Random number generator interface.
typedef struct RpsRandomNumberGenerator
{
    PFN_rpsRandomUniformInt pfnRandomUniformInt;  ///< Pointer to a function for generating random uniform integers.
    void*                   pContext;             ///< Context to be passed to the generator function.
} RpsRandomNumberGenerator;

//
// RpsDevice
//

/// @defgroup RpsDevice RpsDevice
/// @{

/// @brief Handle type for RPS device objects.
///
/// The RPS device is used as the main state object for the RPS runtime API. It provides a central location for data
/// and callbacks of the rest of the software stack.
RPS_DEFINE_HANDLE(RpsDevice);

/// @brief Signature of functions for destroying device objects.
///
/// @param hDevice      Handle to the <c><i>RpsDevice</i></c> object to destroy.
typedef void (*PFN_rpsDeviceOnDestroy)(RpsDevice hDevice);

/// @brief Creation parameters for an <c><i>RpsDevice</i></c>.
typedef struct RpsDeviceCreateInfo
{
    RpsAllocator allocator;                     ///< Default allocator to be usable for all memory allocations which do
                                                ///  not specify a separate allocator.
    RpsPrinter printer;                         ///< Default printer to be usable for all printing operations which do
                                                ///  not specify a separate printer.
    RpsAllocInfo privateDataAllocInfo;          ///< Allocation info for user controlled data which is part of the
                                                ///  device.
    PFN_rpsDeviceOnDestroy pfnDeviceOnDestroy;  ///< Pointer to a function for eventual destruction of the device.
} RpsDeviceCreateInfo;

/// @brief Creates a device object.
///
/// @param pCreateInfo                  Pointer to creation parameters. Passing NULL initializes the device with default
///                                     parameters.
/// @param pHDevice                     Pointer in which the device object is returned. Must not be NULL.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsDeviceCreate(const RpsDeviceCreateInfo* pCreateInfo, RpsDevice* pHDevice);

/// @brief Signature of functions for creating device objects.
///
/// @param pCreateInfo                  Pointer to creation parameters. Passing NULL initializes the device with
///                                     default parameters.
/// @param pHDevice                     Pointer in which the device object is returned. Must not be NULL.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
typedef RpsResult (*PFN_rpsDeviceCreate)(const RpsDeviceCreateInfo* pCreateInfo, RpsDevice* pHDevice);

/// @brief Destroys an RPS device object.
///
/// @param hDevice                      Handle to the <c><i>RpsDevice</i></c>.
void rpsDeviceDestroy(RpsDevice hDevice);

/// @brief Accesses user data of an RPS device.
///
/// This user is normally not the API user but RPS itself. The device private data is created at device allocation,
/// based on privateDataAllocInfo and the device itself will never touch the data until it is destroyed.
///
/// @param hDevice                      Handle to the <c><i>RpsDevice</i></c>.
///
/// @returns                            Pointer to the user controlled data if a handle != RPS_NULL_HANDLE is passed,
///                                     NULL otherwise.
void* rpsDeviceGetPrivateData(RpsDevice hDevice);

/// @brief Log levels for diagnostics.
typedef enum RpsDiagLogLevel
{
    RPS_DIAG_INFO = 0,
    RPS_DIAG_WARNING,
    RPS_DIAG_ERROR,
    RPS_DIAG_FATAL,
    RPS_DIAG_COUNT
} RpsDiagLogLevel;

/// @brief Sets the global debug printer which is used for diagnostic purposes when no device context is available.
///
/// @param pPrinter                     Pointer to the printer to set.
void rpsSetGlobalDebugPrinter(const RpsPrinter* pPrinter);

/// @brief Gets the global debug printer.
///
/// @returns                            Pointer to the current debug printer.
const RpsPrinter* rpsGetGlobalDebugPrinter();

/// @brief Sets the minimum diagnostic log level used by the global debug printer.
///
/// @param minLogLevel                  Minimum log level to set.
void rpsSetGlobalDebugPrinterLogLevel(RpsDiagLogLevel minLogLevel);

/// @} end defgroup RpsDevice

/// @brief Parameters of a type used in the RPS library.
typedef struct RpsTypeInfo
{
    uint16_t size;  ///< Size of a single instance in bytes.
    uint16_t id;    ///< Unique identifier of the type.
} RpsTypeInfo;

/// @brief Integer type ids for all built-in types, e.g. integers and floating point types.
typedef enum RpsBuiltInTypeIds
{
    RPS_TYPE_OPAQUE = 0,                   ///< General type with specified size.
    RPS_TYPE_BUILT_IN_BOOL,                ///< 32-bit boolean type.
    RPS_TYPE_BUILT_IN_INT8,                ///< 8-bit signed integer type.
    RPS_TYPE_BUILT_IN_UINT8,               ///< 8-bit unsigned integer type.
    RPS_TYPE_BUILT_IN_INT16,               ///< 16-bit signed integer type.
    RPS_TYPE_BUILT_IN_UINT16,              ///< 16-bit unsigned integer type.
    RPS_TYPE_BUILT_IN_INT32,               ///< 32-bit signed integer type.
    RPS_TYPE_BUILT_IN_UINT32,              ///< 32-bit unsigned integer type.
    RPS_TYPE_BUILT_IN_INT64,               ///< 64-bit signed integer type.
    RPS_TYPE_BUILT_IN_UINT64,              ///< 64-bit unsigned integer type.
    RPS_TYPE_BUILT_IN_FLOAT32,             ///< 32-bit floating point type.
    RPS_TYPE_BUILT_IN_FLOAT64,             ///< 64-bit floating point type.
    RPS_TYPE_BUILT_IN_MAX_VALUE,           ///< Number of built-in types.
    RPS_TYPE_RUNTIME_DEFINED_BEGIN = 64,   ///< Starting value of the type id range reserved for runtime defined types.
    RPS_TYPE_USER_DEFINED_BEGIN    = 256,  ///< Starting value of the type id range reserved for user defined types.
} RpsBuiltInTypeIds;

/// @brief Integer type ids for any kind of type.
typedef uint32_t RpsTypeId;

/// @brief Creates an RpsTypeInfo structure from only a size parameter.
///
/// @param size         Size in bytes.
///
/// @returns            <c><i>RpsTypeInfo</i></c> created from the size.
static inline RpsTypeInfo rpsTypeInfoInitFromSize(size_t size)
{
    const RpsTypeInfo result = {(uint16_t)size, (uint16_t)RPS_TYPE_OPAQUE};
    return result;
}

/// @brief Creates a RpsTypeInfo structure with size and type ID.
///
/// @param size         Size of the type in bytes.
/// @param typeId       Identifier of the type. Can be one of the @ref RpsBuiltInTypeIds values or a value defined by the runtime or user.
///
/// @returns             <c><i>RpsTypeInfo</i></c> created from the size.
static inline RpsTypeInfo rpsTypeInfoInitFromSizeAndTypeID(size_t size, RpsTypeId typeId)
{
    const RpsTypeInfo result = {(uint16_t)size, (uint16_t)typeId};
    return result;
}

/// @brief Initializes a general type info structure from a type.
///
/// @param TypeName                     Name of the type.
///
/// @returns                            <c><i>RpsTypeInfo</i></c> of the type name with RPS_TYPE_OPAQUE as ID.
#define rpsTypeInfoInitFromType(TypeName) rpsTypeInfoInitFromSize(sizeof(TypeName))

/// @brief Initializes a general type info structure from the type and a type ID.
///
/// @param TypeName                     Name of the type to generate an info structure for.
/// @param TypeID                       ID of the type.
///
/// @returns                            <c><i>RpsTypeInfo</i></c> of the type name and ID.
#define rpsTypeInfoInitFromTypeAndID(TypeName, TypeID) rpsTypeInfoInitFromSizeAndTypeID(sizeof(TypeName), TypeID)

/// @brief Type for render graph node declaration identifiers.
typedef uint32_t RpsNodeDeclId;

/// @brief Type for function parameter identifiers.
typedef uint32_t RpsParamId;

/// @brief Type for render graph node identifiers.
typedef uint32_t RpsNodeId;

/// @brief Constant for an invalid render graph node ID.
#define RPS_NODEDECL_ID_INVALID RPS_INDEX_NONE_U32

/// @brief Constant for an invalid render graph node parameter ID.
#define RPS_PARAM_ID_INVALID RPS_INDEX_NONE_U32

/// @brief Transparent handle type for a general render graph variable.
typedef void* RpsVariable;

/// @brief Transparent handle type for a general, immutable render graph variable.
typedef const void* RpsConstant;

/// @brief Bitflags for subgraph properties.
typedef enum RpsSubgraphFlagBits
{
    RPS_SUBGRAPH_FLAG_NONE   = 0,           ///< No subgraph properties.
    RPS_SUBGRAPH_FLAG_ATOMIC = 1 << 0,      ///< The subgraph is atomic, so external nodes may not be reorderd
                                            ///  in-between nodes belonging to this subgraph.
    RPS_SUBGRAPH_FLAG_SEQUENTIAL = 1 << 1,  ///< The subgraph is sequential, the relative order of its nodes should be
                                            ///  preserved.
} RpsSubgraphFlagBits;

///  @brief Bitmask type for <c><i>RpsSubgraphFlagBits</i></c>.
typedef RpsFlags32 RpsSubgraphFlags;

//
// Debug
//

/// @defgroup Debug Debug
/// @{

/// @brief Type for files represented by an RPSL internal integer identifier.
typedef RpsIndex32 RpsSourceFileId;

/// @brief Parameters of a source code location.
typedef struct RpsSourceLocation
{
    RpsSourceFileId file;  ///< Identifier for a file.
    uint32_t        line;  ///< Line number within the file.
} RpsSourceLocation;

/// @brief Type for RPSL debug information.
typedef struct RpsDebugInfo RpsDebugInfo;

/// @} end defgroup Debug

/// @} end addtogroup Core

//
// RPSL
//

/// @defgroup RPSLRuntime RPSL Runtime
/// @{

/// Bitmask type for an internal entry flags type.
typedef RpsFlags32 RpslEntryCallFlags;

/// @brief Signature of functions for executing RPSL callbacks.
///
/// @param numArgs                      Number of arguments used for the callback.
/// @param ppArgs                       Pointer to an array of <c><i>const void* const</i></c> with numArgs elements to use for the callback.
/// @param flags                        Flags for the type of entry.
typedef void (*PFN_RpslEntry)(uint32_t numArgs, const void* const* ppArgs, RpslEntryCallFlags flags);

/// @brief Type for RPSL entry point declarations.
///
/// An RpslEntry is defined by an export function entry in RPSL and usually statically linked or dynamically loaded into
/// the application. It contains the signature info and the function entry point. Users can use the subsequent macros to
/// define such an entry point for usage in their application when binding nodes.
typedef const struct RpsRpslEntry_T* RpsRpslEntry;

/// @brief Macro for creating a variable name matching the name of the entry point of an RPSL module.
///
/// @param ModuleName                   Name of the module.
/// @param EntryName                    Name of the entry point.
///
/// @returns                            Concatenation of a prefix with these two names as a unique identifier.
#define RPS_ENTRY_REF(ModuleName, EntryName) rpsl_M_##ModuleName##_E_##EntryName

/// @brief Macro for defining a unique string identifier to an entry point of an RPSL module.
///
/// @param ModuleName                   Name of the module.
/// @param EntryName                    Name of the entry point.
///
/// @returns                            Concatenation of a prefix with these two names as a unique identifier into a
///                                     string.
#define RPS_ENTRY_NAME(ModuleName, EntryName) "rpsl_M_" #ModuleName "_E_" #EntryName

/// @brief String constant for the name of the entry table.
#define RPS_ENTRY_TABLE_NAME "rpsl_M_entry_tbl"

/// @brief String constant for the name of the module ID.
#define RPS_MODULE_ID_NAME "rpsl_M_module_id"

/// @brief Macro for defining a C++ declaration of an RPSL entry point.
///
/// @param ModuleName                   Name of the module.
/// @param EntryName                    Name of the entry point.
///
/// @returns                            C++ variable declaration of the module with external C linkage.
#define RPS_DECLARE_RPSL_ENTRY(ModuleName, EntryName) extern "C" RpsRpslEntry RPS_ENTRY_REF(ModuleName, EntryName);

/// @brief Type for a dispatch table of an RPSL process.
typedef struct ___rpsl_runtime_procs ___rpsl_runtime_procs;

/// @brief Signature of functions for initializing RPSL processes from a DLL.
///
/// @param pProcs
/// @param sizeofProcs
///
/// returns
typedef int32_t (*PFN_rpslDynLibInit)(const ___rpsl_runtime_procs* pProcs, uint32_t sizeofProcs);

/// @brief Initializes an RPSL DLL module.
///
/// The user can create an RPSL DLL module by linking rpsl code with rps_rpsl_host_dll.c. After this DLL is loaded, the
/// user must get the address of the `___rps_dyn_lib_init` entry point and call rpsRpslDynamicLibraryInit with this
/// entry point address as the parameter. This initializes the RPSL runtime callbacks for the DLL.
///
/// @param pfn_dynLibInit               Address of "___rps_dyn_lib_init" entry point of the RPSL DLL module.
///
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsRpslDynamicLibraryInit(PFN_rpslDynLibInit pfn_dynLibInit);

/// @brief Generates an RPSL entry name.
///
/// Generates the name from the given module and entry names, so that it matches the symbol name generated by the RPSL
/// compiler for this entry. This name can be used to retrieve the RpslEntry address from a dynamically loaded library
/// using e.g. GetProcAddress or dlsym.
///
/// @param pBuf                         Pointer in which the name is returned. Must not be NULL.
/// @param bufSize                      Size of the buffer in bytes.
/// @param moduleName                   Null terminated string with the name of the module.
/// @param entryName                    Null terminated string with the name of the entry point.
///
/// @returns                            Pointer to the buffer if its size is large enough, otherwise NULL.
const char* rpsMakeRpslEntryName(char* pBuf, size_t bufSize, const char* moduleName, const char* entryName);

//
// RPSL-JIT
//

/// @defgroup RPSLJIT RPSL JIT
/// @{

/// Handle type for JIT loaded modules.
RPS_DEFINE_HANDLE(RpsJITModule);

// Currently RPSL-JIT module is provided as a separated DLL on Windows only, mainly for tooling purpose.
// Below function prototypes and proc names are provided to enable using the DLL.

/// @brief Signature of functions for initializing the JIT compiler.
///
/// @param argc                         Number of program arguments.
/// @param args                         Pointer to an array of <c><i>const char*</i></c> with argc elements. Must not be NULL for argc != 0.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
typedef int32_t (*PFN_RpsJITStartup)(int32_t argc, const char** args);

/// @brief Signature of functions for shutting down the JIT compiler.
typedef void (*PFN_RpsJITShutdown)(void);

/// @brief Signature of functions for loading RPSL modules with the JIT compiler.
///
/// @param name                         Null terminated string with the name of the module.
/// @param pJITModule                   Pointer in which a handle to the loaded module is returned.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
typedef int32_t (*PFN_RpsJITLoad)(const char* name, RpsJITModule* pJITModule);

/// @brief Signature of functions for unloading a JIT loaded module.
///
/// @param hJITModule                   Handle to the JIT module.
typedef void (*PFN_RpsJITUnload)(RpsJITModule hJITModule);

/// @brief Signature of functions for getting an RPSL entry point from a module.
///
/// @param hJITModule                   Handle to the RPSL module. Must not be RPS_NULL_HANDLE.
/// @param symbolName                   Null terminated string with the name of the entry point.
/// @param pEntryPoint                  Pointer to the entry point as a 64-bit unsigned integer.
///
/// @returns                            Result code of the operation. See <c><i>RpsResult</i></c> for more info.
typedef int32_t (*PFN_RpsJITGetEntryPoint)(RpsJITModule hJITModule, const char* symbolName, uint64_t* pEntryPoint);

#define RPS_JIT_PROC_NAME_STARTUP       "RpsJITStartup"
#define RPS_JIT_PROC_NAME_SHUTDOWN      "RpsJITShutdown"
#define RPS_JIT_PROC_NAME_LOAD          "RpsJITLoad"
#define RPS_JIT_PROC_NAME_UNLOAD        "RpsJITUnload"
#define RPS_JIT_PROC_NAME_GETENTRYPOINT "RpsJITGetEntryPoint"

/// @} end defgroup RPSLJIT

/// @} end defgroup RPSLRuntime

#ifdef __cplusplus
}
#endif  // __cplusplus

#ifdef __cplusplus

#define RPS_CLASS_NO_COPY(ClassName)                 \
private:                                             \
    ClassName(const ClassName&)            = delete; \
    ClassName& operator=(const ClassName&) = delete;

#define RPS_CLASS_NO_MOVE(ClassName)            \
private:                                        \
    ClassName(ClassName&&)            = delete; \
    ClassName& operator=(ClassName&&) = delete;

#define RPS_CLASS_NO_COPY_MOVE(ClassName) \
    RPS_CLASS_NO_COPY(ClassName)          \
    RPS_CLASS_NO_MOVE(ClassName)

#endif  //__cplusplus

#endif  //RPS_API_H
