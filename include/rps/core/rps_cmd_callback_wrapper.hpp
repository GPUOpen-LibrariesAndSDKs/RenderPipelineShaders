// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_CMD_CALLBACK_WRAPPER_HPP
#define RPS_CMD_CALLBACK_WRAPPER_HPP

#include <functional>
#include <utility>

namespace rps
{
    /// @brief Place holder for an unused argument of a callback function.
    ///
    /// It can be used to skip parameter marshalling during command node callbacks,
    /// while keeping the parameter ordinals match between the callback functions and node declarations.
    /// For example for node declaration:
    /// ```
    /// node foo( rtv param0, srv param1 );
    /// ```
    /// If the callback function does not need to bind the render target param0 explicitly, it can be declared as:
    /// ```
    /// void FooCallback( const RpsCmdCallbackContext* pContext, rps::UnusedArg unusedParam0, D3D12_CPU_DESCRIPTOR_HANDLE usedParam1 );
    /// ```
    /// So that the runtime will ignore unusedParam0, while still pass usedParam1 to the callback.
    ///
    /// @ingroup RpsRenderGraphCommandRecording
    struct UnusedArg
    {
    };

    namespace details
    {
        template <int32_t Index, typename T, typename EnableT = void>
        struct CommandArgUnwrapper
        {
        };

        // Value types or const ref types
        template <int32_t Index, typename T>
        struct CommandArgUnwrapper<Index, T, typename std::enable_if<!std::is_pointer<T>::value>::type>
        {
            using ValueT = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

            const ValueT& operator()(const RpsCmdCallbackContext* pContext)
            {
                return *static_cast<const ValueT*>(pContext->ppArgs[Index]);
            }
        };

        // Const pointer types
        template <int32_t Index, typename T>
        struct CommandArgUnwrapper<
            Index,
            T,
            typename std::enable_if<std::is_pointer<T>::value &&
                                    std::is_const<typename std::remove_pointer<T>::type>::value>::type>
        {
            T operator()(const RpsCmdCallbackContext* pContext)
            {
                return static_cast<T>(pContext->ppArgs[Index]);
            }
        };

        // Skipping unused args
        template <int32_t Index>
        struct CommandArgUnwrapper<Index, rps::UnusedArg>
        {
            rps::UnusedArg operator()(const RpsCmdCallbackContext* pContext)
            {
                return {};
            }
        };

        // Converting RpsBool to bool
        template <int32_t Index>
        struct CommandArgUnwrapper<Index, bool>
        {
            bool operator()(const RpsCmdCallbackContext* pContext)
            {
                const RpsBool value = *static_cast<const RpsBool*>(pContext->ppArgs[Index]);
                return !!value;
            }
        };

#if __cplusplus >= 201402L

        // Non-recursive argument unwrapping with index_sequence
        template <typename TRet, typename... TArgs>
        struct FunctionWrapper
        {
            template <std::size_t... Index>
            static TRet Call(const RpsCmdCallbackContext* pContext,
                             TRet (*pFn)(const RpsCmdCallbackContext*, TArgs...),
                             std::index_sequence<Index...>)
            {
                return pFn(pContext, CommandArgUnwrapper<Index, TArgs>()(pContext)...);
            }

            template <typename TClass, std::size_t... Index>
            static TRet Call(const RpsCmdCallbackContext* pContext,
                             TClass*                      pThis,
                             TRet (TClass::*pFn)(const RpsCmdCallbackContext*, TArgs...),
                             std::index_sequence<Index...>)
            {
                return (pThis->*pFn)(pContext, CommandArgUnwrapper<Index, TArgs>()(pContext)...);
            }

            template <std::size_t... Index>
            static TRet Call(const RpsCmdCallbackContext*                                pContext,
                             std::function<TRet(const RpsCmdCallbackContext*, TArgs...)> fn,
                             std::index_sequence<Index...>)
            {
                return fn(pContext, CommandArgUnwrapper<Index, TArgs>()(pContext)...);
            }

            template <std::size_t... Index>
            static TRet Call(const RpsCmdCallbackContext* pContext,
                             TRet (*pFn)(const RpsCmdCallbackContext&, TArgs...),
                             std::index_sequence<Index...>)
            {
                return pFn(*pContext, CommandArgUnwrapper<Index, TArgs>()(pContext)...);
            }

            template <typename TClass, std::size_t... Index>
            static TRet Call(const RpsCmdCallbackContext* pContext,
                             TClass*                      pThis,
                             TRet (TClass::*pFn)(const RpsCmdCallbackContext&, TArgs...),
                             std::index_sequence<Index...>)
            {
                return (pThis->*pFn)(*pContext, CommandArgUnwrapper<Index, TArgs>()(pContext)...);
            }

            template <std::size_t... Index>
            static TRet Call(const RpsCmdCallbackContext*                                pContext,
                             std::function<TRet(const RpsCmdCallbackContext&, TArgs...)> fn,
                             std::index_sequence<Index...>)
            {
                return fn(*pContext, CommandArgUnwrapper<Index, TArgs>()(pContext)...);
            }
        };

        template <typename TRet, typename... TArgs>
        TRet WrappedFunction(const RpsCmdCallbackContext* pContext, TRet (*pFn)(const RpsCmdCallbackContext*, TArgs...))
        {
            return FunctionWrapper<TRet, TArgs...>::template Call<>(pContext, pFn, std::index_sequence_for<TArgs...>{});
        }

        template <typename TClass, typename TRet, typename... TArgs>
        TRet WrappedFunction(const RpsCmdCallbackContext* pContext,
                             TClass*                      pThis,
                             TRet (TClass::*pFn)(const RpsCmdCallbackContext*, TArgs...))
        {
            return FunctionWrapper<TRet, TArgs...>::template Call<TClass>(
                pContext, pThis, pFn, std::index_sequence_for<TArgs...>{});
        }

        template <typename TRet, typename... TArgs>
        TRet WrappedFunction(const RpsCmdCallbackContext*                                pContext,
                             std::function<TRet(const RpsCmdCallbackContext*, TArgs...)> fn)
        {
            return FunctionWrapper<TRet, TArgs...>::template Call<>(pContext, fn, std::index_sequence_for<TArgs...>{});
        }

        template <typename TRet, typename... TArgs>
        TRet WrappedFunction(const RpsCmdCallbackContext* pContext, TRet (*pFn)(const RpsCmdCallbackContext&, TArgs...))
        {
            return FunctionWrapper<TRet, TArgs...>::template Call<>(pContext, pFn, std::index_sequence_for<TArgs...>{});
        }

        template <typename TClass, typename TRet, typename... TArgs>
        TRet WrappedFunction(const RpsCmdCallbackContext* pContext,
                             TClass*                      pThis,
                             TRet (TClass::*pFn)(const RpsCmdCallbackContext&, TArgs...))
        {
            return FunctionWrapper<TRet, TArgs...>::template Call<TClass>(
                pContext, pThis, pFn, std::index_sequence_for<TArgs...>{});
        }

        template <typename TRet, typename... TArgs>
        TRet WrappedFunction(const RpsCmdCallbackContext*                                pContext,
                             std::function<TRet(const RpsCmdCallbackContext&, TArgs...)> fn)
        {
            return FunctionWrapper<TRet, TArgs...>::template Call<>(pContext, fn, std::index_sequence_for<TArgs...>{});
        }

#else   //#if __cplusplus >= 201402L

        // Recursive argument unwrapping
        template <int32_t N>
        struct FunctionWrapperRecursive
        {
            template <typename TRet, typename... TArgs, typename... TUnwrappedArgs>
            static TRet Wrapped(const RpsCmdCallbackContext* pContext,
                                TRet (*pFn)(const RpsCmdCallbackContext*, TArgs...),
                                TUnwrappedArgs&&... unwrappedArgs)
            {
                using TupleType = std::tuple<TArgs...>;
                return FunctionWrapperRecursive<N - 1>::Wrapped(
                    pContext,
                    pFn,
                    CommandArgUnwrapper<N - 1, typename std::tuple_element<N - 1, TupleType>::type>()(pContext),
                    unwrappedArgs...);
            }

            template <typename TClass, typename TRet, typename... TArgs, typename... TUnwrappedArgs>
            static TRet Wrapped(const RpsCmdCallbackContext* pContext,
                                TClass*                      pThis,
                                TRet (TClass::*pFn)(const RpsCmdCallbackContext*, TArgs...),
                                TUnwrappedArgs&&... unwrappedArgs)
            {
                using TupleType = std::tuple<TArgs...>;
                return FunctionWrapperRecursive<N - 1>::Wrapped(
                    pContext,
                    pThis,
                    pFn,
                    CommandArgUnwrapper<N - 1, typename std::tuple_element<N - 1, TupleType>::type>()(pContext),
                    unwrappedArgs...);
            }

            template <typename TRet, typename... TArgs, typename... TUnwrappedArgs>
            static TRet Wrapped(const RpsCmdCallbackContext*                                pContext,
                                std::function<TRet(const RpsCmdCallbackContext*, TArgs...)> func,
                                TUnwrappedArgs&&... unwrappedArgs)
            {
                using TupleType = std::tuple<TArgs...>;
                return FunctionWrapperRecursive<N - 1>::Wrapped(
                    pContext,
                    func,
                    CommandArgUnwrapper<N - 1, typename std::tuple_element<N - 1, TupleType>::type>()(pContext),
                    unwrappedArgs...);
            }

            template <typename TRet, typename... TArgs, typename... TUnwrappedArgs>
            static TRet Wrapped(const RpsCmdCallbackContext* pContext,
                                TRet (*pFn)(const RpsCmdCallbackContext&, TArgs...),
                                TUnwrappedArgs&&... unwrappedArgs)
            {
                using TupleType = std::tuple<TArgs...>;
                return FunctionWrapperRecursive<N - 1>::Wrapped(
                    pContext,
                    pFn,
                    CommandArgUnwrapper<N - 1, typename std::tuple_element<N - 1, TupleType>::type>()(pContext),
                    unwrappedArgs...);
            }

            template <typename TClass, typename TRet, typename... TArgs, typename... TUnwrappedArgs>
            static TRet Wrapped(const RpsCmdCallbackContext* pContext,
                                TClass*                      pThis,
                                TRet (TClass::*pFn)(const RpsCmdCallbackContext&, TArgs...),
                                TUnwrappedArgs&&... unwrappedArgs)
            {
                using TupleType = std::tuple<TArgs...>;
                return FunctionWrapperRecursive<N - 1>::Wrapped(
                    pContext,
                    pThis,
                    pFn,
                    CommandArgUnwrapper<N - 1, typename std::tuple_element<N - 1, TupleType>::type>()(pContext),
                    unwrappedArgs...);
            }

            template <typename TRet, typename... TArgs, typename... TUnwrappedArgs>
            static TRet Wrapped(const RpsCmdCallbackContext*                                pContext,
                                std::function<TRet(const RpsCmdCallbackContext&, TArgs...)> func,
                                TUnwrappedArgs&&... unwrappedArgs)
            {
                using TupleType = std::tuple<TArgs...>;
                return FunctionWrapperRecursive<N - 1>::Wrapped(
                    pContext,
                    func,
                    CommandArgUnwrapper<N - 1, typename std::tuple_element<N - 1, TupleType>::type>()(pContext),
                    unwrappedArgs...);
            }
        };

        template <>
        struct FunctionWrapperRecursive<0>
        {
            template <typename TRet, typename... TArgs, typename... TUnwrappedArgs>
            static TRet Wrapped(const RpsCmdCallbackContext* pContext,
                                TRet (*pFn)(const RpsCmdCallbackContext*, TArgs...),
                                TUnwrappedArgs&&... unwrappedArgs)
            {
                return pFn(pContext, unwrappedArgs...);
            }

            template <typename TClass, typename TRet, typename... TArgs, typename... TUnwrappedArgs>
            static TRet Wrapped(const RpsCmdCallbackContext* pContext,
                                TClass*                      pThis,
                                TRet (TClass::*pFn)(const RpsCmdCallbackContext*, TArgs...),
                                TUnwrappedArgs&&... unwrappedArgs)
            {
                return (pThis->*pFn)(pContext, unwrappedArgs...);
            }

            template <typename TRet, typename... TArgs, typename... TUnwrappedArgs>
            static TRet Wrapped(const RpsCmdCallbackContext*                                pContext,
                                std::function<TRet(const RpsCmdCallbackContext*, TArgs...)> func,
                                TUnwrappedArgs&&... unwrappedArgs)
            {
                return func(pContext, unwrappedArgs...);
            }

            template <typename TRet, typename... TArgs, typename... TUnwrappedArgs>
            static TRet Wrapped(const RpsCmdCallbackContext* pContext,
                                TRet (*pFn)(const RpsCmdCallbackContext&, TArgs...),
                                TUnwrappedArgs&&... unwrappedArgs)
            {
                return pFn(*pContext, *unwrappedArgs...);
            }

            template <typename TClass, typename TRet, typename... TArgs, typename... TUnwrappedArgs>
            static TRet Wrapped(const RpsCmdCallbackContext* pContext,
                                TClass*                      pThis,
                                TRet (TClass::*pFn)(const RpsCmdCallbackContext&, TArgs...),
                                TUnwrappedArgs&&... unwrappedArgs)
            {
                return (pThis->*pFn)(*pContext, unwrappedArgs...);
            }

            template <typename TRet, typename... TArgs, typename... TUnwrappedArgs>
            static TRet Wrapped(const RpsCmdCallbackContext*                                pContext,
                                std::function<TRet(const RpsCmdCallbackContext&, TArgs...)> func,
                                TUnwrappedArgs&&... unwrappedArgs)
            {
                return func(*pContext, unwrappedArgs...);
            }
        };

        template <typename TRet, typename... TArgs>
        TRet WrappedFunction(const RpsCmdCallbackContext* pContext, TRet (*pFn)(const RpsCmdCallbackContext*, TArgs...))
        {
            return FunctionWrapperRecursive<sizeof...(TArgs)>::Wrapped(pContext, pFn);
        }

        template <typename TClass, typename TRet, typename... TArgs>
        TRet WrappedFunction(const RpsCmdCallbackContext* pContext,
                             TClass*                      pThis,
                             TRet (TClass::*pFn)(const RpsCmdCallbackContext*, TArgs...))
        {
            return FunctionWrapperRecursive<sizeof...(TArgs)>::Wrapped(pContext, pThis, pFn);
        }

        template <typename TRet, typename... TArgs>
        TRet WrappedFunction(const RpsCmdCallbackContext*                                pContext,
                             std::function<TRet(const RpsCmdCallbackContext*, TArgs...)> fn)
        {
            return FunctionWrapperRecursive<sizeof...(TArgs)>::Wrapped(pContext, fn);
        }

        template <typename TRet, typename... TArgs>
        TRet WrappedFunction(const RpsCmdCallbackContext* pContext, TRet (*pFn)(const RpsCmdCallbackContext&, TArgs...))
        {
            return FunctionWrapperRecursive<sizeof...(TArgs)>::Wrapped(pContext, pFn);
        }

        template <typename TClass, typename TRet, typename... TArgs>
        TRet WrappedFunction(const RpsCmdCallbackContext* pContext,
                             TClass*                      pThis,
                             TRet (TClass::*pFn)(const RpsCmdCallbackContext&, TArgs...))
        {
            return FunctionWrapperRecursive<sizeof...(TArgs)>::Wrapped(pContext, pThis, pFn);
        }

        template <typename TRet, typename... TArgs>
        TRet WrappedFunction(const RpsCmdCallbackContext*                                pContext,
                             std::function<TRet(const RpsCmdCallbackContext&, TArgs...)> fn)
        {
            return FunctionWrapperRecursive<sizeof...(TArgs)>::Wrapped(pContext, fn);
        }
#endif  //#if __cplusplus >= 201402L

        template <typename TClass>
        void WrappedFunction(const RpsCmdCallbackContext* pContext, TClass* pThis, std::nullptr_t n)
        {
        }

        template <typename TObject, typename TFunc>
        struct MemberNodeCallbackContext
        {
            TObject* target;
            TFunc    method;

            MemberNodeCallbackContext(TObject* inTarget, TFunc inFunc)
                : target(inTarget)
                , method(inFunc)
            {
            }

            static void Callback(const RpsCmdCallbackContext* pContext)
            {
                auto pThis = static_cast<MemberNodeCallbackContext<TObject, TFunc>*>(pContext->pCmdCallbackContext);

                details::WrappedFunction<TObject>(pContext, pThis->target, pThis->method);
            }
        };

        template <typename TFunc>
        struct NonMemberNodeCallbackContext
        {
            TFunc func;

            NonMemberNodeCallbackContext(TFunc inFunc)
                : func(inFunc)
            {
            }

            static void Callback(const RpsCmdCallbackContext* pContext)
            {
                auto pThis = static_cast<NonMemberNodeCallbackContext<TFunc>*>(pContext->pCmdCallbackContext);

                details::WrappedFunction(pContext, pThis->func);
            }
        };

    }  // namespace details

}  // namespace rps

#endif  //RPS_CMD_CALLBACK_WRAPPER_HPP

