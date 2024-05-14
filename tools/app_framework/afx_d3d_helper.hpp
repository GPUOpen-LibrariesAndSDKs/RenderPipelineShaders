// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#include <stdexcept>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>

#include <stdlib.h>
#include <wchar.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
using Microsoft::WRL::ComPtr;

inline std::string HrToString(HRESULT hr)
{
    char s_str[64] = {};
    sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
    return std::string(s_str);
}

class HrException : public std::runtime_error
{
public:
    HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
    HRESULT Error() const { return m_hr; }
private:
    const HRESULT m_hr;
};

#define SAFE_RELEASE(p) if (p) (p)->Release()

inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw HrException(hr);
    }
}

template<typename TBlob>
inline void ThrowIfFailedEx(HRESULT hr, TBlob& errorBlob)
{
    if (errorBlob)
    {
        ::OutputDebugStringA((const char*)errorBlob->GetBufferPointer());
    }

    if (FAILED(hr))
    {
        throw HrException(hr);
    }
}

template <UINT pathSize>
inline void GetExeDirPath(WCHAR (&path)[pathSize])
{
    if (path == nullptr)
    {
        throw std::exception();
    }

    DWORD size = GetModuleFileNameW(nullptr, path, pathSize);
    if ((size == 0) || (size == pathSize))
    {
        // Method failed or path was truncated.
        throw std::exception();
    }

    WCHAR* lastSlash = wcsrchr(path, L'\\');
    if (lastSlash)
    {
        *(lastSlash + 1) = L'\0';
    }
}

// Assign a name to the object to aid with debugging.
#if defined(_DEBUG) || defined(DBG)
template<typename TObject>
inline void SetName(TObject* pObject, LPCWSTR name)
{
    pObject->SetName(name);
}
template <typename TObject>
inline void SetNameIndexed(TObject* pObject, LPCWSTR name, UINT index)
{
    WCHAR fullName[50];
    if (swprintf_s(fullName, L"%s[%u]", name, index) > 0)
    {
        pObject->SetName(fullName);
    }
}
#else
template <typename TObject>
inline void SetName(TObject*, LPCWSTR)
{
}
template <typename TObject>
inline void SetNameIndexed(TObject*, LPCWSTR, UINT)
{
}
#endif

// Naming helper for ComPtr<T>.
// Assigns the name of the variable as the name of the object.
// The indexed variant will include the index in the name of the object.
#define NAME_D3D12_OBJECT(x) SetName((x).Get(), L#x)
#define NAME_D3D12_OBJECT_INDEXED(x, n) SetNameIndexed((x)[n].Get(), L#x, n)

#ifdef D3D_COMPILE_STANDARD_FILE_INCLUDE
inline ComPtr<ID3DBlob> CompileShader(
    const std::wstring& filename,
    const D3D_SHADER_MACRO* defines,
    const std::string& entrypoint,
    const std::string& target)
{
    UINT compileFlags = 0;
#if defined(_DEBUG) || defined(DBG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr;

    ComPtr<ID3DBlob> byteCode = nullptr;
    ComPtr<ID3DBlob> errors;
    hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

    if (errors != nullptr)
    {
        OutputDebugStringA((char*)errors->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    return byteCode;
}
#endif

// Resets all elements in a ComPtr array.
template<class T>
void ResetComPtrArray(T* comPtrArray)
{
    for (auto &i : *comPtrArray)
    {
        i.Reset();
    }
}


// Resets all elements in a unique_ptr array.
template<class T>
void ResetUniquePtrArray(T* uniquePtrArray)
{
    for (auto &i : *uniquePtrArray)
    {
        i.reset();
    }
}

template <typename TFunc>
void FindAdapter(IDXGIFactory4* pFactory, TFunc testDeviceCapability, IDXGIAdapter1** ppAdapter, bool& useWarpDevice)
{
    ComPtr<IDXGIAdapter1> selectedAdapter = nullptr;

    if (!useWarpDevice)
    {
        for (UINT adapterIndex = 0; S_OK == pFactory->EnumAdapters1(adapterIndex, &selectedAdapter); ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            ThrowIfFailed(selectedAdapter->GetDesc1(&desc));

            static const wchar_t* const warpAdapterNames[] = {L"Microsoft Basic Render Driver",
                                                              L"Microsoft Basic Display Adapter"};

            if (std::cend(warpAdapterNames) !=
                std::find_if(
                    std::cbegin(warpAdapterNames), std::cend(warpAdapterNames), [&](const wchar_t* warpAdapterName) {
                        return wcscmp(desc.Description, warpAdapterName) == 0;
                    }))
            {
                // Skip warp
                continue;
            }

            // Check to see if the adapter supports desired feature level, but don't create
            // the actual device yet.
            if (testDeviceCapability(selectedAdapter.Get()))
            {
                break;
            }
        }
    }

    if ((selectedAdapter == nullptr) || useWarpDevice)
    {
        ThrowIfFailed(pFactory->EnumWarpAdapter(IID_PPV_ARGS(ppAdapter)));
        useWarpDevice = true;
    }
    else
    {
        *ppAdapter = selectedAdapter.Detach();
    }
}
