// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_DEVICE_HPP
#define RPS_DEVICE_HPP

#include "core/rps_util.hpp"

namespace rps
{
    class Device
    {
        RPS_CLASS_NO_COPY_MOVE(Device);

    private:
        Device(const RpsDeviceCreateInfo& createInfo);
        ~Device();

    public:
        static RpsResult Create(const RpsDeviceCreateInfo* pCreateInfo, Device** ppDevice);
        void             Destroy();

        void* GetPrivateData() const
        {
            return m_pPrivateData;
        }

        void Print(const char* format, va_list vl) const
        {
            m_printer.pfnVPrintf(m_printer.pContext, format, vl);
        }

        template <typename... TArgs>
        void Print(const char* format, TArgs... args) const
        {
            m_printer.pfnPrintf(m_printer.pContext, format, args...);
        }

        void* Allocate(const AllocInfo& allocInfo) const
        {
            return Allocate(allocInfo.size, allocInfo.alignment);
        }

        void* Allocate(size_t sizeInBytes, size_t alignment) const
        {
            return m_allocator.pfnAlloc(m_allocator.pContext, sizeInBytes, alignment);
        }

        void* AllocateZeroed(size_t sizeInBytes, size_t alignment) const
        {
            void* pMemory = Allocate(sizeInBytes, alignment);
            if (pMemory)
            {
                memset(pMemory, 0, sizeInBytes);
            }
            return pMemory;
        }

        void* Reallocate(void* originalBuffer, size_t originalSize, size_t newSize, size_t alignment) const
        {
            return m_allocator.pfnRealloc(m_allocator.pContext, originalBuffer, originalSize, newSize, alignment);
        }

        void Free(void* buffer) const
        {
            m_allocator.pfnFree(m_allocator.pContext, buffer);
        }

        // TODO:
        const RpsAllocator& Allocator() const
        {
            return m_allocator;
        }

        const RpsPrinter& Printer() const
        {
            return m_printer;
        }

    private:
        const RpsAllocator     m_allocator;
        const RpsPrinter       m_printer;
        PFN_rpsDeviceOnDestroy m_pfnOnDestory;
        void*                  m_pPrivateData;
    };

    RPS_ASSOCIATE_HANDLE(Device);

}  // namespace rps

#endif  //RPS_DEVICE_HPP
