// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_D3D12_RUNTIME_BACKEND_HPP
#define RPS_D3D12_RUNTIME_BACKEND_HPP

#include "rps/runtime/d3d_common/rps_d3d_common.h"

#include "runtime/common/rps_render_graph.hpp"
#include "runtime/d3d12/rps_d3d12_runtime_device.hpp"
#include "runtime/d3d12/rps_d3d12_barrier.hpp"
#include "runtime/d3d12/rps_d3d12_enhanced_barrier.hpp"

namespace rps
{
    class D3D12RuntimeBackend : public RuntimeBackend
    {
    private:
        struct D3D12RuntimeCmd : public RuntimeCmd
        {
            uint32_t barrierBatchId;
            uint32_t resourceBindingInfo;

            D3D12RuntimeCmd() = default;

            D3D12RuntimeCmd(uint32_t inCmdId, uint32_t inBarrierBatchId, uint32_t inResourceBindingInfo)
                : RuntimeCmd(inCmdId)
                , barrierBatchId(inBarrierBatchId)
                , resourceBindingInfo(inResourceBindingInfo)
            {
            }
        };

    public:
        D3D12RuntimeBackend(D3D12RuntimeDevice& device, RenderGraph& renderGraph)
            : RuntimeBackend(renderGraph)
            , m_device(device)
            , m_persistentPool(device.GetDevice().Allocator())
            , m_pendingReleaseResources(&m_persistentPool)
            , m_frameResources(&m_persistentPool)
        {
            for (uint32_t iType = 0; iType < RPS_COUNTOF(m_cpuDescriptorHeaps); iType++)
            {
                m_cpuDescriptorHeaps[iType].descriptorIncSize =
                    m_device.GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE(iType));
            }

#if RPS_D3D12_ENHANCED_BARRIER_SUPPORT
            if (device.GetEnhancedBarrierEnabled())
            {
                m_pBarriers = m_persistentPool.New<D3D12EnhancedBarrierBuilder>(device);
            }
            else
#endif  //RPS_D3D12_ENHANCED_BARRIER_SUPPORT
            {
                m_pBarriers = m_persistentPool.New<D3D12ConventionalBarrierBuilder>(device);
            }
        }

        virtual RpsResult RecordCommands(const RenderGraph&                     renderGraph,
                                         const RpsRenderGraphRecordCommandInfo& recordInfo) const override final;

        virtual RpsResult RecordCmdRenderPassBegin(const RuntimeCmdCallbackContext& context) const override final;

        virtual RpsResult RecordCmdRenderPassEnd(const RuntimeCmdCallbackContext& context) const override final;

        virtual RpsResult RecordCmdFixedFunctionBindingsAndDynamicStates(
            const RuntimeCmdCallbackContext& context) const override final;

        virtual void DestroyRuntimeResourceDeferred(ResourceInstance& resource) override final;

        RpsResult GetCmdArgResources(const RuntimeCmdCallbackContext& context,
                                     uint32_t                         argIndex,
                                     uint32_t                         srcArrayIndex,
                                     ID3D12Resource**                 ppResources,
                                     uint32_t                         count) const;

        RpsResult GetCmdArgDescriptors(const RuntimeCmdCallbackContext& context,
                                       uint32_t                         argIndex,
                                       uint32_t                         srcArrayIndex,
                                       D3D12_CPU_DESCRIPTOR_HANDLE*     pDescriptors,
                                       uint32_t                         count) const;

        static RpsResult GetCmdArgResources(const RpsCmdCallbackContext* pContext,
                                            uint32_t                     argIndex,
                                            uint32_t                     srcArrayIndex,
                                            ID3D12Resource**             ppResources,
                                            uint32_t                     count);

        static RpsResult GetCmdArgDescriptors(const RpsCmdCallbackContext* pContext,
                                              uint32_t                     argIndex,
                                              uint32_t                     srcArrayIndex,
                                              D3D12_CPU_DESCRIPTOR_HANDLE* pDescriptors,
                                              uint32_t                     count);

        static RpsResult CopyCmdArgDescriptors(const RpsCmdCallbackContext* pContext,
                                               uint32_t                     argIndex,
                                               uint32_t                     srcArrayOffset,
                                               uint32_t                     count,
                                               RpsBool                      singleHandleToArray,
                                               D3D12_CPU_DESCRIPTOR_HANDLE* pDstHandles);

        static const D3D12RuntimeBackend* Get(const RpsCmdCallbackContext* pContext);

        static ID3D12GraphicsCommandList* GetContextD3DCmdList(const RuntimeCmdCallbackContext& context)
        {
            return rpsD3D12CommandListFromHandle(context.hCommandBuffer);
        }

    protected:
        virtual RpsResult UpdateFrame(const RenderGraphUpdateContext& context) override final;
        virtual RpsResult CreateHeaps(const RenderGraphUpdateContext& context, ArrayRef<HeapInfo> heaps) override final;
        virtual void      DestroyHeaps(ArrayRef<HeapInfo> heaps) override final;
        virtual RpsResult CreateResources(const RenderGraphUpdateContext& context,
                                          ArrayRef<ResourceInstance>      resources) override final;
        virtual void      DestroyResources(ArrayRef<ResourceInstance> resources) override final;
        virtual RpsResult CreateCommandResources(const RenderGraphUpdateContext& context) override final;
        virtual void      OnDestroy() override final;

        virtual bool ShouldResetAliasedResourcesPrevFinalAccess() const override final
        {
            return m_device.GetEnhancedBarrierEnabled();
        }

    private:
        RPS_NO_DISCARD
        RpsResult CreateResourceViews(const RenderGraphUpdateContext& context,
                                      D3D12_DESCRIPTOR_HEAP_TYPE      type,
                                      ConstArrayRef<uint32_t>         accessIndices);

        static void SetResourceDebugName(ID3D12Object* pObject, StrRef name, uint32_t index);
        static void SetHeapDebugName(ID3D12Heap* pHeap, const D3D12_HEAP_DESC &heapDesc, uint32_t index);
        static void SetDescriptorHeapDebugName(ID3D12DescriptorHeap* pHeap, const D3D12_DESCRIPTOR_HEAP_DESC &heapDesc, uint32_t index);

    private:
        D3D12RuntimeDevice& m_device;
        Arena               m_persistentPool;

        ArenaVector<D3D12RuntimeCmd> m_runtimeCmds;
        ArenaVector<uint32_t>        m_accessToDescriptorMap;

        D3D12BarrierBuilder* m_pBarriers = {};

        struct DescriptorHeap
        {
            static constexpr uint32_t DescriptorHeapAllocGranularity = 64;

            ID3D12DescriptorHeap* pHeap             = nullptr;
            uint32_t              capacity          = 0;
            uint32_t              descriptorIncSize = 0;

            ~DescriptorHeap()
            {
                SafeRelease(pHeap);
            }

            RpsResult Reserve(const RenderGraphUpdateContext& context,
                ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t count)
            {
                // Reserve 1 element for default NULL descriptor.
                const uint32_t allocCount =
                    rpsDivRoundUp(count + 1, DescriptorHeapAllocGranularity) * DescriptorHeapAllocGranularity;

                if (capacity < allocCount)
                {
                    SafeRelease(pHeap);

                    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
                    desc.NodeMask                   = 1;
                    desc.NumDescriptors             = allocCount;
                    desc.Type                       = type;

                    HRESULT hr = pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pHeap));
                    if (FAILED(hr))
                    {
                        capacity = 0;
                        return HRESULTToRps(hr);
                    }
                    capacity = allocCount;
                    // generate debug names
                    const bool bEnableDebugNames =
                        !!(context.pUpdateInfo->diagnosticFlags & RPS_DIAGNOSTIC_ENABLE_RUNTIME_DEBUG_NAMES);
                    if (bEnableDebugNames)
                    {
                        SetDescriptorHeapDebugName(pHeap, desc, RPS_INDEX_NONE_U32);
                    }

                    const uint32_t descriptorSize    = pDevice->GetDescriptorHandleIncrementSize(type);
                    const auto     defaultNullHandle = D3D12_CPU_DESCRIPTOR_HANDLE{
                        pHeap->GetCPUDescriptorHandleForHeapStart().ptr + descriptorSize * (capacity - 1)};

                    switch (type)
                    {
                    case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:
                    {
                        D3D12_RENDER_TARGET_VIEW_DESC nullRtvDesc = {};
                        // As of AgilitySDK 706, the SDK layer doesn't seem to care about
                        // the format/dimension of the null RTV.
                        nullRtvDesc.Format        = DXGI_FORMAT_R8G8B8A8_UNORM;
                        nullRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                        pDevice->CreateRenderTargetView(nullptr, &nullRtvDesc, defaultNullHandle);
                    }
                    break;
                    case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:
                    {
                        D3D12_DEPTH_STENCIL_VIEW_DESC nullDsvDesc = {};
                        nullDsvDesc.Format                        = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
                        nullDsvDesc.ViewDimension                 = D3D12_DSV_DIMENSION_TEXTURE2D;
                        pDevice->CreateDepthStencilView(nullptr, &nullDsvDesc, defaultNullHandle);
                    }
                    break;
                    default:
                        break;
                    }
                }
                return RPS_OK;
            }

            D3D12_CPU_DESCRIPTOR_HANDLE Get(uint32_t offset) const
            {
                RPS_ASSERT(capacity > 0);

                D3D12_CPU_DESCRIPTOR_HANDLE result = pHeap->GetCPUDescriptorHandleForHeapStart();
                result.ptr += descriptorIncSize * rpsMin(offset, capacity - 1);
                return result;
            }
        };

        DescriptorHeap m_cpuDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

        struct FrameResources
        {
            ArenaVector<ID3D12Resource*> pendingResources;

            void Reset(Arena& arena)
            {
                pendingResources.reset(&arena);
            }

            void DestroyDeviceResources()
            {
                std::for_each(pendingResources.begin(), pendingResources.end(), [&](auto& i) { i->Release(); });
                pendingResources.clear();
            }
        };

        ArenaVector<ID3D12Resource*> m_pendingReleaseResources;
        ArenaVector<FrameResources>  m_frameResources;
        uint32_t                     m_currentResourceFrame = 0;

    };
}  // namespace rps

#endif  //RPS_D3D12_RUNTIME_BACKEND_HPP
