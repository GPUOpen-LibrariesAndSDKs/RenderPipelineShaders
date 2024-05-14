// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#include <chrono>
#include <functional>
#include <inttypes.h>
#include <string>

#include "rps/core/rps_api.h"

static constexpr uint32_t DivRoundUp(uint32_t dividend, uint32_t divisor)
{
    return (dividend + divisor - 1) / divisor;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
static constexpr T AlignUp(T offset, T alignment)
{
    return alignment ? (offset + (alignment - T(1))) & ~(alignment - T(1)) : offset;
}

static inline uint32_t AsUInt(float f)
{
    uint32_t result;
    static_assert(sizeof(float) == sizeof(uint32_t), "Unsupported float size");
    memcpy(&result, &f, sizeof(float));
    return result;
}

struct RpsAfxCpuTimer
{
    using time_point = std::chrono::time_point<std::chrono::steady_clock>;

    static_assert(std::chrono::steady_clock::period::num * std::micro::den <=
                      std::micro::num * std::chrono::steady_clock::period::den,
                  "Timer resolution not enough.");

    static time_point Now()
    {
        return std::chrono::steady_clock::now();
    }
    static std::chrono::duration<double> SecondsSinceEpoch()
    {
        return Now().time_since_epoch();
    }
};

struct RpsAfxScopedCpuTimer
{
    std::chrono::time_point<std::chrono::steady_clock> m_StartTime;

    const char* m_Id        = nullptr;
    int64_t*    m_pDuration = nullptr;

    RpsAfxScopedCpuTimer(const char* id)
        : RpsAfxScopedCpuTimer(id, nullptr)
    {
    }

    RpsAfxScopedCpuTimer(int64_t* pDuration)
        : RpsAfxScopedCpuTimer(nullptr, pDuration)
    {
    }

    RpsAfxScopedCpuTimer(const char* id, int64_t* pDuration)
        : m_Id(id)
        , m_pDuration(pDuration)
    {
        m_StartTime = RpsAfxCpuTimer::Now();
    }

    ~RpsAfxScopedCpuTimer()
    {
        std::chrono::duration<double, std::micro> duration = RpsAfxCpuTimer::Now() - m_StartTime;

        const int64_t microSecs = int64_t(duration.count());

        if (m_Id)
        {
#if _MSC_VER
            fprintf_s
#else
            fprintf
#endif
                (stderr, "\nTimer '%s' : %" PRId64 " us\n", m_Id, microSecs);
        }

        if (m_pDuration)
        {
            *m_pDuration = microSecs;
        }
    }
};

template<typename TValue = int64_t>
struct RpsAfxAveragedSampler
{
    std::chrono::milliseconds  m_resetInterval = std::chrono::milliseconds(1000);
    RpsAfxCpuTimer::time_point m_lastResetTime = RpsAfxCpuTimer::Now();
    TValue                     m_lastSample    = 0;
    TValue                     m_sum           = 0;
    TValue                     m_avg           = 0;
    uint32_t                   m_count         = 0;

    void Update(TValue sample)
    {
        auto nowTime = RpsAfxCpuTimer::Now();
        if ((m_count > 0) && ((nowTime - m_lastResetTime) > m_resetInterval))
        {
            m_avg = m_sum / m_count;

            m_lastResetTime = nowTime;
            m_count         = 0;
            m_sum           = 0;
        }

        m_lastSample = sample;
        m_sum        = m_sum + sample;
        m_count++;
    }
};

struct RpsAfxJITHelper
{
    RpsAfxJITHelper(int argc = 0, const char** argv = nullptr)
    {
#if USE_RPSL_JIT
        hRpsJITDll = ::LoadLibraryA("rps-jit.dll");

        if (hRpsJITDll)
        {
            pfnRpsJITStartup = PFN_RpsJITStartup(GetProcAddress(hRpsJITDll, RPS_JIT_PROC_NAME_STARTUP));

            pfnRpsJITShutdown = PFN_RpsJITShutdown(GetProcAddress(hRpsJITDll, RPS_JIT_PROC_NAME_SHUTDOWN));

            pfnRpsJITLoad = PFN_RpsJITLoad(GetProcAddress(hRpsJITDll, RPS_JIT_PROC_NAME_LOAD));

            pfnRpsJITUnload = PFN_RpsJITUnload(GetProcAddress(hRpsJITDll, RPS_JIT_PROC_NAME_UNLOAD));

            pfnRpsJITGetEntryPoint =
                PFN_RpsJITGetEntryPoint(GetProcAddress(hRpsJITDll, RPS_JIT_PROC_NAME_GETENTRYPOINT));

            if (pfnRpsJITStartup)
            {
                const char* emptyArgs[]   = {""};
                int32_t     startupResult = pfnRpsJITStartup(argc ? argc : 1, argv ? argv : emptyArgs);

                if (startupResult < 0)
                {
                    pfnRpsJITStartup = nullptr;
                }
            }
        }
#endif  //USE_RPSL_JIT
    }

    ~RpsAfxJITHelper()
    {
#if USE_RPSL_JIT
        if (pfnRpsJITShutdown)
        {
            pfnRpsJITShutdown();
        }

        if (hRpsJITDll)
        {
            ::FreeLibrary(hRpsJITDll);
        }
#endif  //USE_RPSL_JIT
    }

    bool IsValid() const
    {
#if USE_RPSL_JIT
        return hRpsJITDll && pfnRpsJITStartup && pfnRpsJITShutdown && pfnRpsJITLoad && pfnRpsJITUnload &&
               pfnRpsJITGetEntryPoint;
#else
        return false;
#endif  //USE_RPSL_JIT
    }

    RpsJITModule LoadBitcode(const char* bitcodeFile, int64_t* pJitTiming = nullptr)
    {
        RpsJITModule hJITModule = RPS_NULL_HANDLE;
        RpsResult    result     = RpsResult(pfnRpsJITLoad(bitcodeFile, &hJITModule));

        if (RPS_FAILED(result))
            return nullptr;

        uint64_t dynLibInitFnAddr = 0;

        do
        {
            RpsAfxScopedCpuTimer timer(pJitTiming);
            result = RpsResult(pfnRpsJITGetEntryPoint(hJITModule, "___rps_dyn_lib_init", &dynLibInitFnAddr));
        } while (false);

        if (dynLibInitFnAddr != 0)
        {
            auto pfnDynLibInit = PFN_rpslDynLibInit(uintptr_t(dynLibInitFnAddr));
            result             = rpsRpslDynamicLibraryInit(pfnDynLibInit);

            if (RPS_SUCCEEDED(result))
            {
                return hJITModule;
            }
        }

        pfnRpsJITUnload(hJITModule);

        return nullptr;
    }

    const char* GetModuleName(RpsJITModule hJIT)
    {
        const char* const* ppName = GetEntryPoint<const char*>(hJIT, RPS_MODULE_ID_NAME);
        return ppName ? *ppName : nullptr;
    }

    const char* const* GetEntryNameTable(RpsJITModule hJITModule)
    {
        const char* const* ppNameTable = GetEntryPoint<const char*>(hJITModule, RPS_ENTRY_TABLE_NAME);
        return ppNameTable;
    }

    RpsRpslEntry GetEntryPoint(RpsJITModule hJITModule, const char* symbolName) const
    {
        const RpsRpslEntry* pEntry = GetEntryPoint<RpsRpslEntry>(hJITModule, symbolName);
        return (pEntry != 0) ? *pEntry : nullptr;
    }

    template <typename T>
    const T* GetEntryPoint(RpsJITModule hJITModule, const char* symbolName) const
    {
        uint64_t rpslEntryAddr = 0;
        pfnRpsJITGetEntryPoint(hJITModule, symbolName, &rpslEntryAddr);
        return reinterpret_cast<const T*>(static_cast<uintptr_t>(rpslEntryAddr));
    }

#if USE_RPSL_JIT
    HMODULE hRpsJITDll = {};
#endif  //USE_RPSL_JIT
    PFN_RpsJITStartup       pfnRpsJITStartup       = {};
    PFN_RpsJITShutdown      pfnRpsJITShutdown      = {};
    PFN_RpsJITLoad          pfnRpsJITLoad          = {};
    PFN_RpsJITUnload        pfnRpsJITUnload        = {};
    PFN_RpsJITGetEntryPoint pfnRpsJITGetEntryPoint = {};

private:
    RPS_CLASS_NO_COPY_MOVE(RpsAfxJITHelper);
};

bool WriteToFile(const std::string& fileName, const void* pBuf, size_t bufSize)
{
    FILE* fp = {};
#ifdef _MSC_VER
    fopen_s(&fp, fileName.c_str(), "wb");
#else
    fp = fopen(fileName.c_str(), "wb");
#endif

    if (fp)
    {
        size_t written = fwrite(pBuf, 1, bufSize, fp);

        fclose(fp);

        return written == bufSize;
    }

    return false;
}
