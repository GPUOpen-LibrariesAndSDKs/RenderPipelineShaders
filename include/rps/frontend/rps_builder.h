// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_BUILDER_H
#define RPS_BUILDER_H

#include "rps/runtime/common/rps_runtime.h"



#ifdef __cplusplus

namespace rps
{
    namespace details
    {
        template <typename T>
        struct TypeIdGetter
        {
            static constexpr RpsTypeId value = 0;
        };

        // clang-format off
#define RPS_DEFINE_BUILT_IN_TYPE_ID(TypeName, Value)     \
        template <>                                      \
        struct TypeIdGetter<TypeName>                    \
        {                                                \
            static constexpr RpsTypeId value = Value;    \
        };

        RPS_DEFINE_BUILT_IN_TYPE_ID(RpsImageView,    RPS_TYPE_IMAGE_VIEW);
        RPS_DEFINE_BUILT_IN_TYPE_ID(rps::ImageView,  RPS_TYPE_IMAGE_VIEW);
        RPS_DEFINE_BUILT_IN_TYPE_ID(RpsBufferView,   RPS_TYPE_BUFFER_VIEW);
        RPS_DEFINE_BUILT_IN_TYPE_ID(rps::BufferView, RPS_TYPE_BUFFER_VIEW);

#undef RPS_DEFINE_BUILT_IN_TYPE_ID
        // clang-format on

    }  // namespace details

    struct ParameterDesc : public RpsParameterDesc
    {
    public:
        ParameterDesc(RpsTypeInfo          typeInfo,
                      const ParamAttrList* inAttrs     = nullptr,
                      const char*          inName      = nullptr,
                      RpsParameterFlags    inFlags     = RPS_PARAMETER_FLAG_NONE,
                      uint32_t             inArraySize = 0)
            : RpsParameterDesc{typeInfo, inArraySize, inAttrs, inName, inFlags}
        {
        }

        ParameterDesc(size_t               elementSize,
                      const ParamAttrList* inAttrs     = nullptr,
                      const char*          inName      = nullptr,
                      RpsParameterFlags    inFlags     = RPS_PARAMETER_FLAG_NONE,
                      uint32_t             inArraySize = 0)
            : RpsParameterDesc{{static_cast<uint16_t>(elementSize), 0}, inArraySize, inAttrs, inName, inFlags}
        {
        }

        template <typename T>
        static ParameterDesc Make(const ParamAttrList* inAttrs,
                                  const char*          inName      = nullptr,
                                  RpsParameterFlags    inFlags     = RPS_PARAMETER_FLAG_NONE,
                                  uint32_t             inArraySize = 0)
        {
            return ParameterDesc(GetTypeInfo<T>(), inAttrs, inName, inFlags, inArraySize);
        }

        template <typename T>
        static ParameterDesc Make(const ParamAttrList& inAttrs,
                                  const char*          inName      = nullptr,
                                  RpsParameterFlags    inFlags     = RPS_PARAMETER_FLAG_NONE,
                                  uint32_t             inArraySize = 0)
        {
            return ParameterDesc(GetTypeInfo<T>(), &inAttrs, inName, inFlags, inArraySize);
        }

        template <typename T>
        static ParameterDesc Make(const char*       inName      = nullptr,
                                  RpsParameterFlags inFlags     = RPS_PARAMETER_FLAG_NONE,
                                  uint32_t          inArraySize = 0)
        {
            return ParameterDesc(GetTypeInfo<T>(), nullptr, inName, inFlags, inArraySize);
        }

    private:
        template<typename T>
        static constexpr RpsTypeInfo GetTypeInfo()
        {
            return RpsTypeInfo{sizeof(T), rps::details::TypeIdGetter<T>::value};
        }
    };

    class RenderGraphBuilder;

    class RenderGraphBuilderRef
    {
    public:
        RenderGraphBuilderRef(RpsRenderGraphBuilder builder);

        ~RenderGraphBuilderRef()
        {
        }

    public:
        template <typename T>
        struct TNodeArg
        {
            using ValueType = T;

            TNodeArg(T&                   inValue,
                     const ParamAttrList* inAttrs = nullptr,
                     RpsParameterFlags    inFlags = RPS_PARAMETER_FLAG_NONE)
                : value(inValue)
                , attrs(inAttrs)
                , flags(inFlags)
            {
            }

            T* operator&() const
            {
                return &value;
            }

            T&                   value;
            const ParamAttrList* attrs;
            RpsParameterFlags    flags;
        };

    private:
        struct NodeArgHelper
        {
            template <typename T>
            static const ParamAttrList* GetAttrList(const TNodeArg<T>& arg)
            {
                return arg.attrs;
            }

            template <typename T>
            static const ParamAttrList* GetAttrList(const T& arg)
            {
                return nullptr;
            }

            template <typename T>
            static RpsParameterFlags GetFlag(const TNodeArg<T>& arg)
            {
                return arg.flags;
            }

            template <typename T>
            static RpsParameterFlags GetFlag(const T& arg)
            {
                return RPS_PARAMETER_FLAG_NONE;
            }
        };

        template <typename T>
        struct NodeArgTypeHelper
        {
            using Type = T;
        };

        template <typename T>
        struct NodeArgTypeHelper<TNodeArg<T>>
        {
            using Type = T;
        };

    public:
        void* AllocateData(size_t size, size_t alignment) const;

        template <typename T, typename... TArgs>
        T* New(TArgs&&... args)
        {
            static_assert(std::is_trivially_destructible<T>::value, "Type must be trivially destructible.");
            void* pMemory = AllocateData(sizeof(T), alignof(T));
            return pMemory ? (new (pMemory) T(args...)) : nullptr;
        }

        RpsNodeDeclId DeclNode(const RpsNodeDesc& nodeDesc) const;

        template <typename... TParams>
        RpsNodeDeclId DeclNode(const char* name, RpsNodeDeclFlags flags, TParams... params) const
        {
            RpsParameterDesc paramDescs[] = {
                params...,
            };

            RpsNodeDesc nodeDesc = {};

            nodeDesc.numParams   = sizeof...(params);
            nodeDesc.pParamDescs = paramDescs;
            nodeDesc.flags       = flags;
            nodeDesc.name        = name;

            return DeclNode(nodeDesc);
        }

        RpsNodeId AddNode(RpsNodeDeclId                      nodeDeclId,
                          uint32_t                           tag,
                          PFN_rpsCmdCallback                 callback,
                          void*                              callbackUserContext,
                          std::initializer_list<RpsVariable> args);

        template <typename TArg, typename... TAttrs>
        TNodeArg<TArg> MakeNodeArg(TArg& value, TAttrs... attrs)
        {
            auto* pAttrList = New<rps::ParamAttrList>(attrs...);
            return TNodeArg<TArg>(value, pAttrList, RPS_PARAMETER_FLAG_NONE);
        }

        template <typename TNodeFunc, typename... TArgs>
        RpsNodeId AddNode(TNodeFunc nodeFunc, uint32_t tag, const char* name, TArgs&&... args)
        {
            RpsParameterDesc paramDescs[] = {
                ParameterDesc::Make<typename NodeArgTypeHelper<TArgs>::Type>(
                    NodeArgHelper::GetAttrList(args), nullptr, NodeArgHelper::GetFlag(args))...,
            };

            RpsNodeDesc nodeDesc = {};
            nodeDesc.numParams   = uint32_t(sizeof...(TArgs));
            nodeDesc.pParamDescs = paramDescs;
            nodeDesc.name        = name;

            const RpsNodeDeclId nodeDeclId = DeclNode(nodeDesc);

            using ContextType = rps::details::NonMemberNodeCallbackContext<TNodeFunc>;
            static_assert(std::is_trivially_destructible<ContextType>::value, "");

            ContextType* callbackUserContext = New<ContextType>(nodeFunc);

            return AddNode(nodeDeclId, tag, &ContextType::Callback, callbackUserContext, {&args...});
        }

        template <typename TTarget, typename TNodeFunc, typename... TArgs>
        RpsNodeId AddNode(TTarget* pTarget, TNodeFunc nodeFunc, uint32_t tag, const char* name, TArgs&&... args)
        {
            RpsParameterDesc paramDescs[] = {
                ParameterDesc::Make<typename NodeArgTypeHelper<TArgs>::Type>(
                    NodeArgHelper::GetAttrList(args), nullptr, NodeArgHelper::GetFlag(args))...,
            };

            RpsNodeDesc nodeDesc = {};
            nodeDesc.numParams   = uint32_t(sizeof...(TArgs));
            nodeDesc.pParamDescs = paramDescs;
            nodeDesc.name        = name;

            const RpsNodeDeclId nodeDeclId = DeclNode(nodeDesc);

            using ContextType = rps::details::MemberNodeCallbackContext<TTarget, TNodeFunc>;
            static_assert(std::is_trivially_destructible<ContextType>::value, "");

            ContextType* callbackUserContext = New<ContextType>(pTarget, nodeFunc);

            return AddNode(nodeDeclId, tag, &ContextType::Callback, callbackUserContext, {&args...});
        }

        RpsResourceId GetParamResourceId(RpsParamId paramId, uint32_t arrayIndex = 0) const;
        RpsResult     DeclareResource(uint32_t       localResourceId,
                                      RpsVariable    hDescVar,
                                      const char*    name,
                                      RpsResourceId* pOutResId);

        RpsVariable GetParamVariable(RpsParamId paramId, size_t* pSize = nullptr) const;

        template <typename T>
        T* GetParamVariable(RpsParamId paramId) const
        {
            size_t varSize = 0;
            auto   result  = GetParamVariable(paramId, &varSize);

            return (varSize == sizeof(T)) ? static_cast<T*>(result) : nullptr;
        }

        template <typename T>
        RpsResult SetParamVariable(RpsParamId paramId, const T& value)
        {
            auto pData = GetParamVariable<T>(paramId);
            if (pData)
            {
                *pData = value;
                return RPS_OK;
            }
            return RPS_ERROR_INDEX_OUT_OF_BOUNDS;
        }

    private:
        RenderGraphBuilder& m_builder;
        RpsResult           m_result = RPS_OK;
    };

}  // namespace rps

#endif // __cplusplus

/// @defgroup Frontend Frontend
/// @{

/// @defgroup RpsRenderGraphHelpers RpsRenderGraph Helpers
/// @{

// Render Graph Helpers

/// @brief Allocates memory from a render graph builder and initializes it by copying data from an existing buffer.
///
/// @param hRenderGraphBuilder              Handle to the render graph builder. Must not be RPS_NULL_HANDLE.
/// @param size                             Size of the data in bytes.
/// @param pCopyFrom                        Pointer to the data. Must not be NULL for size != 0.
///
/// @returns                                Pointer to the allocated memory where the data was copied to if the
///                                         allocation was successful, NULL otherwise. Only valid until the next
///                                         render graph update.
static inline void* rpsRenderGraphAllocAndCopyFrom(RpsRenderGraphBuilder hRenderGraphBuilder,
                                                   size_t                size,
                                                   const void*           pCopyFrom)

{
    void* pResult = rpsRenderGraphAllocateData(hRenderGraphBuilder, size);
    if (pResult)
    {
        memcpy(pResult, pCopyFrom, size);
    }
    return pResult;
}

/// @brief Allocates memory from a render graph builder and zeroes it.
///
/// @param hRenderGraphBuilder              Handle to the render graph builder. Must not be RPS_NULL_HANDLE.
/// @param size                             Size of the data in bytes.
///
/// @returns                                Pointer to the allocated memory if the allocation was successful,
///                                         NULL otherwise. Only valid until the next render graph update.
static inline void* rpsRenderGraphAllocAndZero(RpsRenderGraphBuilder hRenderGraphBuilder, size_t size)
{
    void* pResult = rpsRenderGraphAllocateData(hRenderGraphBuilder, size);
    if (pResult)
    {
        memset(pResult, 0, size);
    }
    return pResult;
}

/// @brief Allocates an object from a render graph builder.
///
/// @param hRenderGraphBuilder              Handle to the render graph builder. Must not be RPS_NULL_HANDLE.
/// @param TypeName                         Type name of the object.
///
/// @returns                                Pointer to the allocated object if the allocation was successful, NULL
///                                         otherwise.
#define rpsRenderGraphAllocateDataOfType(hRenderGraphBuilder, TypeName) \
    ((TypeName*)rpsRenderGraphAllocateData(hRenderGraphBuilder, sizeof(TypeName)))

/// @brief Allocates an array of objects in a render graph.
///
/// @param hRenderGraphBuilder              Handle to the render graph builder. Must not be RPS_NULL_HANDLE.
/// @param TypeName                         Type name of the objects.
/// @param NumElements                      Number of elements in the array.
///
/// @returns                                Pointer to the allocated array of <c><i>TypeName</i></c> if the allocation was
///                                         successful, NULL otherwise.
#define rpsRenderGraphAllocateArrayOfType(hRenderGraphBuilder, TypeName, NumElements) \
    ((TypeName*)rpsRenderGraphAllocateData(hRenderGraphBuilder, sizeof(TypeName) * NumElements))

/// @brief Allocates an object with zeroed memory in a render graph builder.
///
/// @param hRenderGraphBuilder              Handle to the render graph builder. Must not be RPS_NULL_HANDLE.
/// @param TypeName                         Type name of the object.
///
/// @returns                                Pointer to the allocated object if the allocation was successful, NULL
///                                         otherwise.
#define rpsRenderGraphAllocateDataOfTypeZeroed(hRenderGraphBuilder, TypeName) \
    ((TypeName*)rpsRenderGraphAllocAndZero(hRenderGraphBuilder, sizeof(TypeName)))

/// @brief Allocates an array of objects with zeroed memory in a render graph builder.
///
/// @param hRenderGraphBuilder              Handle to the render graph builder. Must not be RPS_NULL_HANDLE.
/// @param TypeName                         Type name of the objects.
/// @param NumElements                      Number of elements in the array.
///
/// @returns                                Pointer to the allocated array of <c><i>TypeName</i></c> if the allocation was successful,
///                                         NULL otherwise.
#define rpsRenderGraphAllocateArrayOfTypeZeroed(hRenderGraphBuilder, TypeName, NumElements) \
    ((TypeName*)rpsRenderGraphAllocAndZero(hRenderGraphBuilder, sizeof(TypeName) * NumElements))

/// @brief Allocates memory in a render graph and copies an object into it.
///
/// @param hRenderGraphBuilder              Handle to the render graph builder. Must not be RPS_NULL_HANDLE.
/// @param TypeName                         Type name of the object.
/// @param CopyFrom                         Pointer to the object to copy. Must not be NULL.
///
/// @returns                                Pointer to the allocated object if the allocation was successful, NULL
///                                         otherwise.
#define rpsRenderGraphAllocateDataOfTypeAndCopyFrom(hRenderGraphBuilder, TypeName, CopyFrom) \
    ((TypeName*)rpsRenderGraphAllocAndCopyFrom(hRenderGraphBuilder, sizeof(TypeName), CopyFrom))

/// @brief Allocates memory in a render graph and copies an array of objects into it.
///
///
/// @param hRenderGraphBuilder              Handle to the render graph builder. Must not be RPS_NULL_HANDLE.
/// @param TypeName                         Type name of the objects.
/// @param NumElements                      Number of elements in the array.
/// @param CopyFrom                         Pointer to the array of <c><i>TypeName</i></c> to copy. Must not be NULL if NumElements != 0.
///
/// @returns                                Pointer to the allocated array of <c><i>TypeName</i></c> if the allocation was
///                                         successful, NULL otherwise.
#define rpsRenderGraphAllocateArrayOfTypeAndCopyFrom(hRenderGraphBuilder, TypeName, NumElements, CopyFrom) \
    ((TypeName*)rpsRenderGraphAllocAndCopyFrom(hRenderGraphBuilder, sizeof(TypeName) * (Count), CopyFrom))

/// @} end addtogroup RpsRenderGraphHelpers

/// @} end addtogroup Frontend

#endif  //RPS_BUILDER_H
