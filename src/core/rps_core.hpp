// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_CORE_HPP
#define RPS_CORE_HPP

/// @defgroup Core

#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <climits>

#include <tuple>
#include <memory>
#include <algorithm>
#include <type_traits>
#include <iterator>

#ifdef RPS_HAS_INTRIN_H
#include <intrin.h>
#endif

#ifndef RPS_ASSERT
#include <cassert>
#endif

#include "rps/core/rps_api.h"

#if !defined(RPS_DEBUG) && !defined(NDEBUG)
/// A preprocessor symbol indicating that if RPS is compiled in a debug configuration.
///
/// @ingroup Core
#define RPS_DEBUG (1)
#endif

#define RPS_CONCATENATE_DIRECT(A, B)   A##B
#define RPS_CONCATENATE_INDIRECT(A, B) RPS_CONCATENATE_DIRECT(A, B)

#define RPS_STRINGIFY_DIRECT(X) #X
#define RPS_STRINGIFY(X)        RPS_STRINGIFY_DIRECT(X)

#define RPS_ALIGNOF(x) alignof(x)

// CountOf C array
#ifdef __cplusplus
extern "C++" template <typename T, size_t Count>
constexpr size_t RPS_COUNTOF(T const (&)[Count]) noexcept
{
    return Count;
}
#else
#define RPS_COUNTOF(arr) (sizeof(arr) / sizeof(arr[0]))
#endif

#define RPS_UNUSED(x)       \
    do                      \
    {                       \
        (void)sizeof(!(x)); \
    } while (0)

#if RPS_HAS_NODISCARD
#define RPS_NO_DISCARD [[nodiscard]]
#else
#define RPS_NO_DISCARD
#endif

#if RPS_HAS_MAYBE_UNUSED
#define RPS_MAYBE_UNUSED [[maybe_unused]]
#else
#define RPS_MAYBE_UNUSED
#endif

#if __cplusplus >= 202002L
#define RPS_CONSTEXPR_20 constexpr
#else
#define RPS_CONSTEXPR_20 inline
#endif

#if __cplusplus >= 201703L
#define RPS_CONSTEXPR_17 constexpr
#else
#define RPS_CONSTEXPR_17 inline
#endif

#if __cplusplus >= 201402L
#define RPS_CONSTEXPR_14 constexpr
#else
#define RPS_CONSTEXPR_14
#endif

/// An assertion macro that is used through RPS codebase.
///
/// @ingroup Core
#ifndef RPS_ASSERT
#if RPS_DEBUG
#define RPS_ASSERT(x) assert(x)
#else
#define RPS_ASSERT(x) RPS_UNUSED(x)
#endif
#endif  // #ifndef RPS_ASSERT

#ifdef __cplusplus
#define RPS_STATIC_ASSERT(Expr, Msg)                    static_assert(Expr, Msg)
#define RPS_STATIC_ASSERT_STANDALONE(Expr, Msg, Prefix) static_assert(Expr, Msg)
#else
#define RPS_STATIC_ASSERT(Expr, Msg)                                                         \
    do                                                                                       \
    {                                                                                        \
        typedef char RPS_CONCATENATE_INDIRECT(RPS_STATIC_ASSERT, __LINE__)[(Expr) ? 1 : -1]; \
        (void)sizeof(RPS_CONCATENATE_INDIRECT(RPS_STATIC_ASSERT, __LINE__));                 \
    } while (0)
#define RPS_STATIC_ASSERT_STANDALONE(Expr, Msg, Postfix)                                         \
    static void RPS_CONCATENATE_INDIRECT(RPS_STATIC_ASSERT_SCOPE_FUNC_##Postfix, __LINE__)(void) \
    {                                                                                            \
        RPS_STATIC_ASSERT(Expr, Msg);                                                            \
    }
#endif

// Helper to get struct pointer from field pointer
#define RPS_STRUCT_PTR_FROM_FIELD(StructType, FieldName, FieldPtr) \
    ((StructType*)(((uint8_t*)(FieldPtr)) - offsetof(StructType, FieldName)))

/// A helper function to assert an error code does not indicate an error.
///
/// @param errCode          The error code to check, if this is != RPS_OK the assertion will fail.
///
/// @ingroup Core
static inline void RPS_ASSERT_OK(RpsResult result)
{
    RPS_ASSERT(result == RPS_OK);
}

#define RPS_TODO(...) RPS_ASSERT(RPS_FALSE && "RPS TODO!" __VA_ARGS__)

#define RPS_TODO_RETURN(Result, ...) \
    do                               \
    {                                \
        RPS_TODO(__VA_ARGS__);       \
        return (Result);             \
    } while (0)

#define RPS_TODO_RETURN_NOT_IMPLEMENTED() RPS_TODO_RETURN(RPS_ERROR_NOT_IMPLEMENTED, "Not Implemented!")

void rpsDiagLog(RpsDiagLogLevel logLevel, const char* fmt, ...);

#ifndef RPS_DIAG_LOG
#define RPS_DIAG_LOG(Level, Expr, Fmt, ...) \
    rpsDiagLog(Level, "\n[" #Level "] : '%s' " Fmt " @%s line %d.\n", Expr, __VA_ARGS__, __FILE__, __LINE__)
#endif  //RPS_DIAG_LOG

#ifndef RPS_DIAG
/// A macro to output debug info about an expression to stderr.
///
/// The expression itself, it's file and line of occurrence will be printed to stderr
/// output. Can be overwritten to any behaviour by specifying RPS_DIAG before including this header.
///
/// @param Level            The <c><i>RpsDiagLogLevel</i></c> to print with.
/// @param Expr             The expression to print.
///
/// @ingroup Core
#define RPS_DIAG(Level, Expr) \
    rpsDiagLog(Level, "\n[" #Level "] : '%s' @%s line %d.\n", Expr, __FILE__, __LINE__)
#endif  //RPS_DIAG

#ifndef RPS_DIAG_MSG
#define RPS_DIAG_MSG(Level, Expr, Fmt, ...) \
    RPS_DIAG_LOG(Level, Expr, ":\n          '" Fmt "',", __VA_ARGS__)
#endif  //RPS_DIAG_MSG

#ifndef RPS_DIAG_RESULT_CODE
#define RPS_DIAG_RESULT_CODE(Level, Expr, Err) \
    RPS_DIAG_LOG(Level, Expr, "Result = %s(%d)", rpsResultGetName(Err), Err)
#endif  //RPS_DIAG_RESULT_CODE

/// A macro to return an error from the current function if an expression indicates this error.
///
/// If the result of the expression is != RPS_OK debug info will be printed for it before returning the result of the expression.
///
/// @param Expr             The expression to check.
///
/// @ingroup Core
#define RPS_V_RETURN(Expr)                                                  \
    do                                                                      \
    {                                                                       \
        RpsResult RPS_RESULT_TEMP = Expr;                                   \
        if (RPS_RESULT_TEMP != RPS_OK)                                      \
        {                                                                   \
            RPS_DIAG_RESULT_CODE(RPS_DIAG_ERROR, (#Expr), RPS_RESULT_TEMP); \
            return RPS_RESULT_TEMP;                                         \
        }                                                                   \
    } while (0)

/// A macro to assign the result of an expression to a variable if that result indicates an error.
///
/// If the result of the expression is != RPS_OK the assignee will be assigned this result and debug info will be printed for the expression.
///
/// @param Assignee         The object to assign the result to in case of an error.
/// @param Expr             The expression to check
///
/// @ingroup Core
#define RPS_ASSIGN_IF_ERROR(Assignee, Expr)                                 \
    do                                                                      \
    {                                                                       \
        RpsResult RPS_RESULT_TEMP = Expr;                                   \
        if (RPS_RESULT_TEMP != RPS_OK)                                      \
        {                                                                   \
            Assignee = RPS_RESULT_TEMP;                                     \
            RPS_DIAG_RESULT_CODE(RPS_DIAG_ERROR, (#Expr), RPS_RESULT_TEMP); \
        }                                                                   \
    } while (0)

/// A macro to return a specific error code if a given condition is satisfied.
///
/// If the given condition is RPS_TRUE debug info will be printed and the specified error code will be returned from the current function.
///
/// @param Cond             The condition to check.
/// @param ErrCode          The error code to return in case the condition is satisfied.
///
/// @ingroup Core
#define RPS_RETURN_ERROR_IF(Cond, ErrorRet)    \
    do                                         \
    {                                          \
        if (Cond)                              \
        {                                      \
            RPS_DIAG(RPS_DIAG_ERROR, (#Cond)); \
            return ErrorRet;                   \
        }                                      \
    } while (0)

// Workaround legacy MSVC preprocessor rescanning replacement list behavior.
#define RPS_MACRO_FORWARD(X) X

#define RPS_RETURN_ERROR_IF_MSG(Cond, ErrorRet, ...)                               \
    do                                                                             \
    {                                                                              \
        if (Cond)                                                                  \
        {                                                                          \
            RPS_MACRO_FORWARD(RPS_DIAG_MSG(RPS_DIAG_ERROR, (#Cond), __VA_ARGS__)); \
            return ErrorRet;                                                       \
        }                                                                          \
    } while (0)

/// A macro to return a specific error code if a given condition is satisfied.
///
/// If the given condition is RPS_TRUE debug info will be printed and the specified error code will be returned from the current function.
///
/// @param Cond             The condition to check.
/// @param ErrCode          The error code to return in case the condition is satisfied.
///
/// @ingroup Core
#define RPS_SET_ERROR_IF(Assignee, Cond, ErrCode) \
    do                                            \
    {                                             \
        if (Cond)                                 \
        {                                         \
            RPS_DIAG(RPS_DIAG_ERROR, (#Cond));    \
            (Assignee) = (ErrCode);               \
        }                                         \
    } while (0)

/// A macro to return RPS_OK if a given condition is satisfied.
///
/// @param Cond             The condition to check.
///
/// @ingroup Core
#define RPS_RETURN_OK_IF(Cond) \
    do                         \
    {                          \
        if (Cond)              \
        {                      \
            return RPS_OK;     \
        }                      \
    } while (0)

/// A macro to return RPS_ERROR_INVALID_ARGUMENTS if the arguments of a function call do not satisfy a condition.
///
/// @param Cond             The condition to check.
///
/// @ingroup Core
#define RPS_CHECK_ARGS(Cond)                    \
    do                                          \
    {                                           \
        if (!(Cond))                            \
        {                                       \
            RPS_DIAG(RPS_DIAG_ERROR, (#Cond));  \
            return RPS_ERROR_INVALID_ARGUMENTS; \
        }                                       \
    } while (0)

/// A macro to return RPS_ERROR_OUT_OF_MEMORY if a pointer returned from a memory allocation is NULL.
///
/// @param Ptr              The pointer to check.
///
/// @ingroup Core
#define RPS_CHECK_ALLOC(Ptr)                  \
    do                                        \
    {                                         \
        if (!(Ptr))                           \
        {                                     \
            RPS_DIAG(RPS_DIAG_ERROR, (#Ptr)); \
            return RPS_ERROR_OUT_OF_MEMORY;   \
        }                                     \
    } while (0)

namespace rps
{
    template <typename T>
    struct RpsHandleTrait
    {
    };

    template <typename T>
    constexpr typename RpsHandleTrait<T>::impl_type* FromHandle(T* pHandle)
    {
        return reinterpret_cast<typename RpsHandleTrait<T>::impl_type*>(pHandle);
    }

    template <typename T>
    constexpr const typename RpsHandleTrait<T>::impl_type* FromHandle(const T* pHandle)
    {
        return reinterpret_cast<const typename RpsHandleTrait<T>::impl_type*>(pHandle);
    }

    template <typename T>
    constexpr typename RpsHandleTrait<T>::impl_type** FromHandle(T** ppHandle)
    {
        return reinterpret_cast<typename RpsHandleTrait<T>::impl_type**>(ppHandle);
    }

    template <typename T>
    constexpr typename RpsHandleTrait<T>::handle_type* ToHandle(T* pImpl)
    {
        return reinterpret_cast<typename RpsHandleTrait<T>::handle_type*>(pImpl);
    }

    template <typename T>
    constexpr const typename RpsHandleTrait<T>::handle_type* ToHandle(const T* pImpl)
    {
        return reinterpret_cast<const typename RpsHandleTrait<T>::handle_type*>(pImpl);
    }

    template <typename T, typename H>
    constexpr T* FromOpaqueHandle(H hdl)
    {
        return static_cast<T*>(hdl.ptr);
    }

}  // namespace rps

#define RPS_ASSOCIATE_HANDLE(Type)         \
    template <>                            \
    struct RpsHandleTrait<Rps##Type##_T>   \
    {                                      \
        typedef rps::Type impl_type;       \
    };                                     \
    template <>                            \
    struct RpsHandleTrait<rps::Type>       \
    {                                      \
        typedef Rps##Type##_T handle_type; \
    };

#endif  // #ifndef RPS_CORE_HPP
