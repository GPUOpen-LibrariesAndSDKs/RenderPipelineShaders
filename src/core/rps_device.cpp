// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "core/rps_device.hpp"
#include "core/rps_util.hpp"

#ifndef RPS_ENABLE_DEFAULT_DEVICE_IMPL
#define RPS_ENABLE_DEFAULT_DEVICE_IMPL 1
#endif  //RPS_ENABLE_DEFAULT_DEVICE_IMPL

namespace rps
{
#if RPS_ENABLE_DEFAULT_DEVICE_IMPL
    void* rpsDefaultMalloc(void* pContext, size_t size, size_t alignment)
    {
#if defined(_MSC_VER) || defined(__MINGW32__)
        return _aligned_malloc(size, alignment);
#else
        return aligned_alloc(alignment, rpsAlignUp(size, alignment));
#endif
    }

    void rpsDefaultFree(void* pContext, void* ptr)
    {
#if defined(_MSC_VER) || defined(__MINGW32__)
        return _aligned_free(ptr);
#else
        return free(ptr);
#endif
    }

    void* rpsDefaultRealloc(void* pContext, void* pOldBuffer, size_t oldSize, size_t newSize, size_t alignment);

    // Used when user Allocator provides Alloc/Free but not Realloc
    void* rpsFallbackRealloc(
        const RpsAllocator* pAllocator, void* pOldBuffer, size_t oldSize, size_t newSize, size_t alignment)
    {
        if (newSize <= oldSize)
        {
            return pOldBuffer;
        }
        else if (alignment <= alignof(std::max_align_t))
        {
            return realloc(pOldBuffer, newSize);
        }

        void* pNewBuffer = pAllocator->pfnAlloc(pAllocator->pContext, newSize, alignment);

        if (pOldBuffer)
        {
            if (pNewBuffer)
            {
                memcpy(pNewBuffer, pOldBuffer, rpsMin(oldSize, newSize));
            }
            pAllocator->pfnFree(pAllocator->pContext, pOldBuffer);
        }
        return pNewBuffer;
    }

    static const RpsAllocator s_DefaultAllocator = {
        &rpsDefaultMalloc,   // pfnAlloc;
        &rpsDefaultFree,     // pfnFree;
        &rpsDefaultRealloc,  // pfnRealloc;
        nullptr,             // pContext;
    };

    void* rpsDefaultRealloc(void* pContext, void* pOldBuffer, size_t oldSize, size_t newSize, size_t alignment)
    {
#if defined(_MSC_VER) || defined(__MINGW32__)
        return _aligned_realloc(pOldBuffer, newSize, alignment);
#else
        return rpsFallbackRealloc(&s_DefaultAllocator, pOldBuffer, oldSize, newSize, alignment);
#endif
    }

    void rpsDefaultPrint(void* pUserContext, const char* format, ...)
    {
        va_list vl;
        va_start(vl, format);
        vprintf(format, vl);
        va_end(vl);
    }

    void rpsDefaultVPrint(void* pUserContext, const char* format, va_list vl)
    {
        vprintf(format, vl);
    }

    static const RpsPrinter s_DefaultPrinter = {
        &rpsDefaultPrint,   // pfnPrintf;
        &rpsDefaultVPrint,  // pfnVPrinf;
        nullptr,            // pContext;
    };
#endif //RPS_ENABLE_DEFAULT_DEVICE_IMPL

    RpsResult Device::Create(const RpsDeviceCreateInfo* pCreateInfo, Device** ppDevice)
    {
        RPS_CHECK_ARGS(ppDevice);

        RpsDeviceCreateInfo createInfo = pCreateInfo ? *pCreateInfo : RpsDeviceCreateInfo{};

#if RPS_ENABLE_DEFAULT_DEVICE_IMPL

        if (!createInfo.allocator.pfnAlloc)
        {
            createInfo.allocator = s_DefaultAllocator;
        }

        if (!createInfo.printer.pfnPrintf)
        {
            createInfo.printer = s_DefaultPrinter;
        }

#else //RPS_ENABLE_DEFAULT_DEVICE_IMPL

        RPS_CHECK_ARGS(createInfo.allocator.pfnAlloc && createInfo.allocator.pfnFree &&
                       createInfo.allocator.pfnRealloc);
        RPS_CHECK_ARGS(createInfo.printer.pfnPrintf && createInfo.printer.pfnVPrintf);

#endif //RPS_ENABLE_DEFAULT_DEVICE_IMPL

        Device* pDevice;
        void*   pPrivateData;

        void* pMemory = AllocateCompound(
            createInfo.allocator, &pDevice, CompoundEntry(&pPrivateData, createInfo.privateDataAllocInfo));

        RPS_CHECK_ALLOC(pMemory);

        RPS_ASSERT(pMemory == pDevice);

        new (pMemory) Device(createInfo);

        pDevice->m_pPrivateData = (createInfo.privateDataAllocInfo.size > 0) ? pPrivateData : nullptr;

        *ppDevice = pDevice;

        return RPS_OK;
    }

    void Device::Destroy()
    {
        RpsAllocator allocator = Allocator();

        this->~Device();

        allocator.pfnFree(allocator.pContext, this);
    }

    Device::Device(const RpsDeviceCreateInfo& createInfo)
        : m_allocator(createInfo.allocator)
        , m_printer(createInfo.printer)
        , m_pfnOnDestory(createInfo.pfnDeviceOnDestroy)
        , m_pPrivateData(nullptr)
    {
    }

    Device::~Device()
    {
        if (m_pfnOnDestory)
        {
            m_pfnOnDestory(ToHandle(this));
        }
    }

}  // namespace rps

extern "C" {

RpsResult rpsDeviceCreate(const RpsDeviceCreateInfo* pCreateInfo, RpsDevice* pDevice)
{
    return rps::Device::Create(pCreateInfo, rps::FromHandle(pDevice));
}

void rpsDeviceDestroy(RpsDevice device)
{
    if (device != RPS_NULL_HANDLE)
    {
        rps::FromHandle(device)->Destroy();
    }
}

void* rpsDeviceGetPrivateData(RpsDevice hDevice)
{
    return hDevice ? rps::FromHandle(hDevice)->GetPrivateData() : nullptr;
}

} // extern "C"
