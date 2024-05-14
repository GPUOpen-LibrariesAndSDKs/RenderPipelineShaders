// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_RESULT_H
#define RPS_RESULT_H

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Result and error codes used by operations of the RPS library.
///
/// @ingroup Basic
typedef enum RpsResult
{
    /// Successful completion.
    RPS_OK = 0,

    /// Failure due to an unspecified error.
    RPS_ERROR_UNSPECIFIED = -1,

    /// Failure due to an unrecognized command.
    RPS_ERROR_UNRECOGNIZED_COMMAND = -2,

    /// Failure due to invalid arguments.
    RPS_ERROR_INVALID_ARGUMENTS = -3,

    /// Failure due to invalid data.
    RPS_ERROR_INVALID_DATA = -4,

    /// Failure due to an invalid operation.
    RPS_ERROR_INVALID_OPERATION = -5,

    /// Failure due to running out of memory.
    RPS_ERROR_OUT_OF_MEMORY = -6,

    /// Failure due to not being able to find the specified file.
    RPS_ERROR_FILE_NOT_FOUND = -7,

    /// Failure due to an invalid file format.
    RPS_ERROR_INVALID_FILE_FORMAT = -8,

    /// Failure due to the file format version being too old.
    RPS_ERROR_UNSUPPORTED_VERSION_TOO_OLD = -9,

    /// Failure due to the file format version being too new.
    RPS_ERROR_UNSUPPORTED_VERSION_TOO_NEW = -10,

    /// Failure due to an unknown node.
    RPS_ERROR_UNKNOWN_NODE = -11,

    /// Failure due to an index being out of its valid bounds.
    RPS_ERROR_INDEX_OUT_OF_BOUNDS = -12,

    /// Failure due to a command being already finalized.
    RPS_ERROR_COMMAND_ALREADY_FINAL = -13,

    /// Failure due to a data layout mismatch between runtime and shader.
    RPS_ERROR_INTEROP_DATA_LAYOUT_MISMATCH = -14,

    /// Failure due to a key not being found.
    RPS_ERROR_KEY_NOT_FOUND = -15,

    /// Failure due to a key value being duplicated where it is required to be unique.
    RPS_ERROR_KEY_DUPLICATED = -16,

    /// Failure due to a feature not being implemented yet.
    RPS_ERROR_NOT_IMPLEMENTED = -17,

    /// Failure due to an integer overflow.
    RPS_ERROR_INTEGER_OVERFLOW = -18,

    /// Failure due to exclusive ranges overlapping.
    RPS_ERROR_RANGE_OVERLAPPING = -19,

    /// Failure due to rpsRenderPipelineValidate finding an invalid pipeline configuration. More details are provided
    /// via output of the device print function.
    RPS_ERROR_VALIDATION_FAILED = -20,

    /// Failure due to a compiled RPSL shader program being ill formed. Normally indicates a compiler error.
    RPS_ERROR_INVALID_PROGRAM = -21,

    /// Failure due to an RPSL module being incompatible with the current runtime.
    RPS_ERROR_UNSUPPORTED_MODULE_VERSION = -22,

    /// Failure due to a failed type safety check.
    RPS_ERROR_TYPE_MISMATCH = -23,

    /// Failure due to a feature not being supported.
    RPS_ERROR_NOT_SUPPORTED = -24,

    /// Failure due to failed a runtime API without direct mapping of the API error code.
    RPS_ERROR_RUNTIME_API_ERROR = -25,

    /// Failure due to an RPS library internal error.
    RPS_ERROR_INTERNAL_ERROR = -26,

    /// Number of unique RPS result codes.
    RPS_RESULT_CODE_COUNT = 27,
} RpsResult;

/// @brief Checks if the RpsResult code indicates a success.
/// 
/// @param R                            Result code.
/// 
/// @returns                            true if RPS_OK, false otherwise.
///
/// @ingroup Basic
#define RPS_SUCCEEDED(R) ((R) >= 0)

/// @brief Checks if the RpsResult code indicates a failure.
/// 
/// @param R                            Result code.
/// 
/// @returns                            false if RPS_OK, true otherwise.
///
/// @ingroup Basic
#define RPS_FAILED(R) ((R) < 0)

/// @brief Gets the name string of a result code.
///
/// @param result       Result code.
///
/// @returns            Null terminated string with the result name.
const char* rpsResultGetName(RpsResult result);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // #ifndef RPS_RESULT_H
