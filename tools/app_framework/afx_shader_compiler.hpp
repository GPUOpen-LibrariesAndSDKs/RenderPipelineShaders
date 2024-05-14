// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#include <unknwn.h>
#include <dxcapi.h>
#include <vector>
#include <algorithm>

#include "afx_d3d_helper.hpp"

static DxcCreateInstanceProc s_DxcCreateInstance = nullptr;

static void InitDXC(bool isSpirvTarget)
{
    if (s_DxcCreateInstance != nullptr)
    {
        return;
    }

    WCHAR exeDir[MAX_PATH];
    // call below should throw an exception on failure. 
    GetExeDirPath(exeDir);

    std::wstring dxcPath(exeDir);
    dxcPath += (isSpirvTarget) ? L"spirv_dxc\\dxcompiler.dll" : L"dxcompiler.dll";

    HMODULE hDxcDll = ::LoadLibraryW(dxcPath.c_str());

    if (!hDxcDll)
    {
        throw "Failed to load dxcompiler.dll";
    }

    s_DxcCreateInstance = (DxcCreateInstanceProc)::GetProcAddress(hDxcDll, "DxcCreateInstance");
}

interface IncluderDxc : public IDxcIncludeHandler
{
    IDxcLibrary* m_pLibrary;

public:
    IncluderDxc(IDxcLibrary * pLibrary)
        : m_pLibrary(pLibrary)
    {
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**) override
    {
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return 0;
    }
    ULONG STDMETHODCALLTYPE Release() override
    {
        return 0;
    }
    HRESULT STDMETHODCALLTYPE LoadSource(LPCWSTR pFilename, IDxcBlob** ppIncludeSource) override
    {
        IDxcBlobEncoding* pSource;
        ThrowIfFailed(m_pLibrary->CreateBlobFromFile(pFilename, NULL, &pSource));

        *ppIncludeSource = pSource;

        return S_OK;
    }
};

#if RPS_HAS_MAYBE_UNUSED
[[maybe_unused]]
#endif
static bool DxcCompile(const char*           pSrcCode,
                       const WCHAR*          pEntryPoint,
                       const WCHAR*          pProfile,
                       const WCHAR*          pParams,
                       const DxcDefine*      defines,
                       uint32_t              defineCount,
                       std::vector<uint8_t>& byteCode)
{
    bool isSpirvTarget = false;

    std::vector<std::wstring> args;
    // splits params string into an array of strings
    {
        WCHAR params[1024];
        wcscpy_s(params, pParams);

        WCHAR* next_token;
        WCHAR* token = wcstok_s(params, L" ", &next_token);
        while (token != NULL)
        {
            args.push_back(token);
            if (L"-spirv" == args.back())
            {
                isSpirvTarget = true;
            }
            token = wcstok_s(NULL, L" ", &next_token);
        }
    }

    InitDXC(isSpirvTarget);

    ComPtr<IDxcLibrary> pLibrary;
    ThrowIfFailed(s_DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&pLibrary)));

    ComPtr<IDxcBlobEncoding> pSource;
    ThrowIfFailed(pLibrary->CreateBlobWithEncodingFromPinned((LPBYTE)pSrcCode, (UINT32)strlen(pSrcCode), CP_UTF8, &pSource));

    ComPtr<IDxcCompiler2> pCompiler;
    ThrowIfFailed(s_DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler)));

    IncluderDxc Includer(pLibrary.Get());

    LPCWSTR pTargetProfile = L"";

    

    if (isSpirvTarget)
    {
        args.push_back(L"-T");
        args.push_back(pProfile);
    }
    else
    {
        pTargetProfile = pProfile;
    }

    std::vector<LPCWSTR> ppArgs(args.size());
    std::transform(args.begin(), args.end(), ppArgs.begin(), [](auto& s) { return s.c_str(); });

    ComPtr<IDxcOperationResult> pOpRes;
    HRESULT                     res;

    if (true)
    {
        ppArgs.push_back(L"-Zi");
        ppArgs.push_back(L"-Qembed_debug");

        ComPtr<IDxcBlob> pPDB;
        LPWSTR           pDebugBlobName[1024];

        res = pCompiler->CompileWithDebug(pSource.Get(),
                                          NULL,
                                          pEntryPoint,
                                          pTargetProfile,
                                          ppArgs.data(),
                                          (UINT32)ppArgs.size(),
                                          defines,
                                          defineCount,
                                          &Includer,
                                          &pOpRes,
                                          pDebugBlobName,
                                          pPDB.GetAddressOf());
    }
    else
    {
        res = pCompiler->Compile(
            pSource.Get(), NULL, pEntryPoint, pTargetProfile, ppArgs.data(), (UINT32)ppArgs.size(), defines, defineCount, &Includer, &pOpRes);
    }

    ComPtr<IDxcBlob>         pResult;
    ComPtr<IDxcBlobEncoding> pError;
    if (pOpRes != NULL)
    {
        pOpRes->GetResult(&pResult);
        pOpRes->GetErrorBuffer(&pError);
    }

    if (pError)
    {
        ComPtr<IDxcBlobEncoding> pErrorUtf8;
        pLibrary->GetBlobAsUtf8(pError.Get(), &pErrorUtf8);
        if (pErrorUtf8 && pErrorUtf8->GetBufferSize() > 0)
        {
            fprintf(stderr, "%s", (const char*)pErrorUtf8->GetBufferPointer());
        }
    }

    if (pResult != NULL && pResult->GetBufferSize() > 0)
    {
        byteCode.resize(pResult->GetBufferSize());
        memcpy(byteCode.data(), pResult->GetBufferPointer(), pResult->GetBufferSize());

        return true;
    }

    return false;
}
