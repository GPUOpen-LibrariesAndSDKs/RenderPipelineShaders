// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_RUNTIME_DEVICE_HPP
#define RPS_RUNTIME_DEVICE_HPP

#include "rps/core/rps_api.h"
#include "rps/runtime/common/rps_runtime.h"

#include "core/rps_device.hpp"
#include "runtime/common/rps_render_graph.hpp"

namespace rps
{
    struct BuiltInNodeInfo
    {
        StrRef         name;
        RpsCmdCallback callbackInfo;
    };

    struct AccessTransitionInfo
    {
        bool          bTransition;
        bool          bMergedAccessStates;
        bool          bKeepOrdering;
        RpsAccessAttr mergedAccess;
    };

    class RuntimeDevice
    {
    protected:
        RuntimeDevice(Device* pDevice, const RpsRuntimeDeviceCreateInfo* pRuntimeCreateInfo)
            : m_pDevice(pDevice)
            , m_createInfo{pRuntimeCreateInfo ? *pRuntimeCreateInfo : RpsRuntimeDeviceCreateInfo{}}
        {
        }

    public:
        virtual ~RuntimeDevice()
        {
            if (m_createInfo.callbacks.pfnDestroyRuntime)
            {
                m_createInfo.callbacks.pfnDestroyRuntime(m_createInfo.pUserContext);
            }
        }

        virtual RpsResult Init()
        {
            return RPS_OK;
        }

        virtual RpsResult BuildDefaultRenderGraphPhases(RenderGraph& renderGraph)               = 0;
        virtual RpsResult InitializeSubresourceInfos(ArrayRef<ResourceInstance> resInstances)   = 0;
        virtual RpsResult InitializeResourceAllocInfos(ArrayRef<ResourceInstance> resInstances) = 0;
        virtual RpsResult GetSubresourceRangeFromImageView(SubresourceRangePacked& outRange,
                                                           const ResourceInstance& resourceInfo,
                                                           const RpsAccessAttr&    accessAttr,
                                                           const RpsImageView&     imageView) = 0;

        virtual ConstArrayRef<RpsMemoryTypeInfo> GetMemoryTypeInfos() const
        {
            return {};
        }

        virtual RpsResult DescribeMemoryType(uint32_t memoryTypeIndex, PrinterRef printer) const
        {
            return RPS_OK;
        }

        virtual RpsResult UpdateHeaps(ArrayRef<HeapInfo> heaps) const
        {
            return RPS_OK;
        }

        virtual void DestroyHeaps(ArrayRef<HeapInfo> heaps) const
        {
        }

        virtual ConstArrayRef<BuiltInNodeInfo> GetBuiltInNodes() const
        {
            return {};
        }

        virtual bool CalculateAccessTransition(const RpsAccessAttr&  beforeAccess,
                                               const RpsAccessAttr&  afterAccess,
                                               AccessTransitionInfo& results) const
        {
            return false;
        }

        virtual RpsImageAspectUsageFlags GetImageAspectUsages(uint32_t aspectMask) const
        {
            return RPS_IMAGE_ASPECT_UNKNOWN;
        }

        virtual void PrepareRenderGraphCreation(RpsRenderGraphCreateInfo& renderGraphCreateInfo) const
        {
            //Memory aliasing always requires lifetime analysis
            if (!rpsAnyBitsSet(renderGraphCreateInfo.renderGraphFlags, RPS_RENDER_GRAPH_NO_GPU_MEMORY_ALIASING))
            {
                renderGraphCreateInfo.renderGraphFlags &= (~(RPS_RENDER_GRAPH_NO_LIFETIME_ANALYSIS));
            }
        }

    public:
        static RuntimeDevice* Get(const Device& device)
        {
            return static_cast<RuntimeDevice*>(device.GetPrivateData());
        }

        template<typename T, typename = typename std::enable_if<std::is_base_of<RuntimeDevice, T>::value>::type>
        static T* Get(const Device& device)
        {
            return static_cast<T*>(Get(device));
        }

        template <typename T, typename... TRuntimeCreateArgs>
        static RpsResult Create(RpsDevice*                 phDevice,
                                const RpsDeviceCreateInfo* pDeviceCreateInfo,
                                TRuntimeCreateArgs... runtimeCreateArgs)
        {
            RPS_CHECK_ARGS(phDevice);

            RpsDeviceCreateInfo deviceCreateInfo = pDeviceCreateInfo ? *pDeviceCreateInfo : RpsDeviceCreateInfo{};

            AllocInfo privateDataAllocInfo = rps::AllocInfo::FromType<T>();

            deviceCreateInfo.pfnDeviceOnDestroy   = &OnDestroy;
            deviceCreateInfo.privateDataAllocInfo = privateDataAllocInfo;

            RPS_V_RETURN(rpsDeviceCreate(&deviceCreateInfo, phDevice));

            void* pPrivateData = rpsDeviceGetPrivateData(*phDevice);

            T* pRuntimeDevice = new (pPrivateData) T(FromHandle(*phDevice), runtimeCreateArgs...);

            RpsResult result = pRuntimeDevice->Init();
            if (RPS_FAILED(result))
            {
                rpsDeviceDestroy(*phDevice);
                *phDevice = RPS_NULL_HANDLE;
            }

            return RPS_OK;
        }

        Device& GetDevice() const
        {
            return *m_pDevice;
        }

        const RpsRuntimeDeviceCreateInfo& GetCreateInfo() const
        {
            return m_createInfo;
        }

    private:

        static void OnDestroy(RpsDevice device)
        {
            Get(*FromHandle(device))->~RuntimeDevice();
        }

        RpsResult BuildUserDefinedRenderGraphPhases(RenderGraph& renderGraph)
        {
            if (!m_createInfo.callbacks.pfnBuildRenderGraphPhases)
            {
                return RPS_OK;
            }

            const RpsRenderGraphPhaseInfo* pPhases   = {};
            uint32_t                       numPhases = 0;

            RPS_V_RETURN(m_createInfo.callbacks.pfnBuildRenderGraphPhases(
                m_createInfo.pUserContext, ToHandle(&renderGraph), &pPhases, &numPhases));

            RpsResult result     = RPS_OK;
            uint32_t  phaseIndex = 0;

            result = renderGraph.ReservePhases(numPhases);

            if (RPS_SUCCEEDED(result))
            {
                for (; phaseIndex < numPhases; phaseIndex++)
                {
                    result = renderGraph.AddPhase<RenderGraphPhaseWrapper>(pPhases[phaseIndex]);

                    if (RPS_FAILED(result))
                        break;
                }
            }

            RPS_ASSERT(RPS_SUCCEEDED(result) || (phaseIndex != numPhases));

            for (; phaseIndex < numPhases; phaseIndex++)
            {
                pPhases[phaseIndex].pfnDestroy(pPhases[phaseIndex].hPhase);
            }

            return result;
        }

    private:
        Device* const m_pDevice = nullptr;
        const RpsRuntimeDeviceCreateInfo m_createInfo = {};
    };

    class NullRuntimeDevice final : public RuntimeDevice
    {
    public:
        NullRuntimeDevice(Device* pDevice)
            : RuntimeDevice(pDevice, nullptr)
        {
        }

        virtual RpsResult BuildDefaultRenderGraphPhases(RenderGraph& renderGraph) override final;
        virtual RpsResult InitializeSubresourceInfos(ArrayRef<ResourceInstance> resInstances) override final;
        virtual RpsResult InitializeResourceAllocInfos(ArrayRef<ResourceInstance> resInstances) override final;
        virtual RpsResult GetSubresourceRangeFromImageView(SubresourceRangePacked& outRange,
                                                           const ResourceInstance& resourceInfo,
                                                           const RpsAccessAttr&    accessAttr,
                                                           const RpsImageView&     imageView) override final;
        virtual RpsImageAspectUsageFlags GetImageAspectUsages(uint32_t aspectMask) const override final;
        virtual ConstArrayRef<RpsMemoryTypeInfo> GetMemoryTypeInfos() const override final;

    };

}  // namespace rps

#endif  //RPS_RUNTIME_DEVICE_HPP
