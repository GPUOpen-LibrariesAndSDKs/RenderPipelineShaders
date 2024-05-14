// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "runtime/d3d12/rps_d3d12_runtime_backend.hpp"

namespace rps
{

    void SetObjectDebugName(ID3D12Object* pObject, const char *str, uint32_t globalIndex)
    {
        if ((pObject == nullptr) || (str == nullptr))
            return;

        WCHAR buf[RPS_NAME_MAX_LEN];

        if (globalIndex != RPS_INDEX_NONE_U32)
        {
            swprintf(buf, RPS_NAME_MAX_LEN, L"%S_%u", str, globalIndex);
        }
        else
        {
            swprintf(buf, RPS_NAME_MAX_LEN, L"%S", str);
        }

        pObject->SetName(buf);
    }

    void D3D12RuntimeBackend::SetHeapDebugName(
        ID3D12Heap* pHeap, const D3D12_HEAP_DESC &heapDesc, uint32_t index)
    {
        if (!pHeap) return;

        auto buf = StrBuilder<>("rps_heap_");

        switch(heapDesc.Properties.Type) {
        case D3D12_HEAP_TYPE_CUSTOM:
            switch(heapDesc.Properties.MemoryPoolPreference)
            {
            case D3D12_MEMORY_POOL_L0:
                buf.Append("custom_L0");
                break;
            case D3D12_MEMORY_POOL_L1:
                buf.Append("custom_L1");
                break;
            default:
                // bad config. cannot have custom heap and unknown mem pool.
                RPS_ASSERT(false && "Invalid MemoryPoolPreference value for custom heap type");
                buf.Append("custom_unknown");
                break;
            }
            switch(heapDesc.Properties.CPUPageProperty)
            {
            case D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE:
                buf.Append("_na");
                break;
            case D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE:
                buf.Append("_wc");
                break;
            case D3D12_CPU_PAGE_PROPERTY_WRITE_BACK:
                buf.Append("_wb");
                break;
            default:
                // bad config. cannot have custom heap and unknown page prop.
                RPS_ASSERT(false && "Invalid CPUPageProperty value for custom heap type");
                buf.Append("_unknown");
                break;
            }
            break;
        case D3D12_HEAP_TYPE_READBACK:
            buf.Append("readback");
            break;
        case D3D12_HEAP_TYPE_UPLOAD:
            buf.Append("upload");
            break;
        case D3D12_HEAP_TYPE_DEFAULT:
            buf.Append("default");
            break;
        default:
            RPS_ASSERT(false && "Invalid heap type");
            buf.Append("unknown");
            break;
        }

        SetObjectDebugName(pHeap, buf.c_str(), index);
    }

    void D3D12RuntimeBackend::SetDescriptorHeapDebugName(
        ID3D12DescriptorHeap* pHeap, const D3D12_DESCRIPTOR_HEAP_DESC &heapDesc, uint32_t index)
    {
        if (!pHeap) return;

        const char *descHeapNames[] =
        {
            "rps_descriptor_heap_cbv_srv_uav",
            "rps_descriptor_heap_sampler",
            "rps_descriptor_heap_rtv",
            "rps_descriptor_heap_dsv",
            "rps_descriptor_heap_unknown"
        };

        static_assert(D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES == RPS_COUNTOF(descHeapNames) - 1,
            "SetDescriptorHeapDebugName needs update.");

        size_t idx = rpsMin((size_t)heapDesc.Type, RPS_COUNTOF(descHeapNames) - 1);
        RPS_ASSERT(idx != (RPS_COUNTOF(descHeapNames) - 1) && "Invalid descriptor heap type");

        SetObjectDebugName(pHeap, descHeapNames[idx], index);
    }

    void D3D12RuntimeBackend::SetResourceDebugName(ID3D12Object* pObject, StrRef name, uint32_t index)
    {
        if (!pObject || name.empty())
        {
            return;
        }

        char buf[RPS_NAME_MAX_LEN];

        if (index != RPS_INDEX_NONE_U32)
        {
           snprintf(buf, RPS_NAME_MAX_LEN, "%s[%u]", name.str, index);
        }
        else
        {
           snprintf(buf, RPS_NAME_MAX_LEN, "%s", name.str);
        }

        SetObjectDebugName(pObject, buf, RPS_INDEX_NONE_U32);
    }

}