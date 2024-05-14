// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_D3D_COMMON_UTIL_H
#define RPS_D3D_COMMON_UTIL_H

#include <d3dcommon.h>
#include "rps/core/rps_result.h"
#include "rps/runtime/d3d_common/rps_d3d_common.h"

#include "core/rps_util.hpp"

namespace rps
{
    static inline RpsResult HRESULTToRps(HRESULT hr)
    {
        switch (hr)
        {
        case S_OK:
        case S_FALSE:
            return RPS_OK;
        case E_INVALIDARG:
            return RPS_ERROR_INVALID_ARGUMENTS;
        case E_OUTOFMEMORY:
            return RPS_ERROR_OUT_OF_MEMORY;
        case E_NOTIMPL:
            return RPS_ERROR_NOT_IMPLEMENTED;
        }
        return RPS_ERROR_UNSPECIFIED;
    }

    template <typename T>
    inline void SafeRelease(T*& ptr)
    {
        if (ptr)
        {
            ptr->Release();
            ptr = nullptr;
        }
    }

    template <typename T>
    struct ScopedComPtr
    {
        T* Ptr;

        ScopedComPtr(T* p = nullptr)
            : Ptr(p)
        {
        }

        ~ScopedComPtr()
        {
            if (Ptr)
            {
                Ptr->Release();
            }
        }

        T* Get() const
        {
            return Ptr;
        }

        T** ReleaseAndGetAddressOf()
        {
            SafeRelease(Ptr);
            return &Ptr;
        }

        T* operator->()
        {
            return Get();
        }

        T* operator=(T* p)
        {
            SafeRelease(Ptr);
            Ptr = p;
        }

    private:
        RPS_CLASS_NO_COPY_MOVE(ScopedComPtr);
    };
}  // namespace rps

#endif  //RPS_D3D_COMMON_UTIL_H
