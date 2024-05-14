// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_D3D11_RUNTIME_BACKEND_HPP
#define RPS_D3D11_RUNTIME_BACKEND_HPP

#include "runtime/common/rps_render_graph.hpp"
#include "runtime/d3d_common/rps_d3d_common_util.hpp"
#include "runtime/d3d11/rps_d3d11_runtime_device.hpp"

namespace rps
{
    class D3D11RuntimeBackend : public RuntimeBackend
    {
    private:
        struct D3D11RuntimeCmd : public RuntimeCmd
        {
            uint32_t resourceBindingInfo;

            D3D11RuntimeCmd() = default;

            D3D11RuntimeCmd(uint32_t inCmdId, uint32_t inResourceBindingInfo)
                : RuntimeCmd(inCmdId)
                , resourceBindingInfo(inResourceBindingInfo)
            {
            }
        };

        enum class ViewType
        {
            RTV,
            DSV,
            SRV,
            UAV,
        };

    public:
        D3D11RuntimeBackend(D3D11RuntimeDevice& device, RenderGraph& renderGraph)
            : RuntimeBackend(renderGraph)
            , m_device(device)
            , m_persistentPool(device.GetDevice().Allocator())
            , m_views(&m_persistentPool)
            , m_pendingReleaseResources(&m_persistentPool)
            , m_frameResources(&m_persistentPool)
        {
        }

        virtual RpsResult RecordCommands(const RenderGraph&                     renderGraph,
                                         const RpsRenderGraphRecordCommandInfo& recordInfo) const override final;

        virtual RpsResult RecordCmdRenderPassBegin(const RuntimeCmdCallbackContext& context) const override final;

        virtual RpsResult RecordCmdRenderPassEnd(const RuntimeCmdCallbackContext& context) const override final;

        virtual RpsResult RecordCmdFixedFunctionBindingsAndDynamicStates(
            const RuntimeCmdCallbackContext& context) const override final;

        virtual void DestroyRuntimeResourceDeferred(ResourceInstance& resource) override;

        RpsResult GetCmdArgResources(const RuntimeCmdCallbackContext& context,
                                     uint32_t                         argIndex,
                                     uint32_t                         srcArrayIndex,
                                     ID3D11Resource**                 ppResources,
                                     uint32_t                         count) const;

        RpsResult GetCmdArgViews(const RuntimeCmdCallbackContext& context,
                                 uint32_t                         argIndex,
                                 uint32_t                         srcArrayIndex,
                                 ID3D11View**                     ppViews,
                                 uint32_t                         count) const;

        static RpsResult GetCmdArgResources(const RpsCmdCallbackContext* pContext,
                                            uint32_t                     argIndex,
                                            uint32_t                     srcArrayIndex,
                                            ID3D11Resource**             ppResources,
                                            uint32_t                     count);

        static RpsResult GetCmdArgViews(const RpsCmdCallbackContext* pContext,
                                        uint32_t                     argIndex,
                                        uint32_t                     srcArrayIndex,
                                        ID3D11View**                 ppViews,
                                        uint32_t                     count);

        template<typename TView>
        static RpsResult GetCmdArgViews(const RpsCmdCallbackContext* pContext,
                                        uint32_t                     argIndex,
                                        uint32_t                     srcArrayIndex,
                                        TView**                      ppViews,
                                        uint32_t                     count)
        {
            RPS_V_RETURN(
                GetCmdArgViews(pContext, argIndex, srcArrayIndex, reinterpret_cast<ID3D11View**>(ppViews), count));

            for (uint32_t i = 0; i < count; i++)
            {
                ppViews[i] = static_cast<TView*>(reinterpret_cast<ID3D11View**>(ppViews)[i]);
            }

            return RPS_OK;
        }

        static const D3D11RuntimeBackend* Get(const RpsCmdCallbackContext* pContext);

        static ID3D11DeviceContext* GetD3DDeviceContext(const RuntimeCmdCallbackContext& context)
        {
            return rpsD3D11DeviceContextFromHandle(context.hCommandBuffer);
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

    private:

        void SetResourceDebugName(ID3D11DeviceChild* pObject, StrRef name, uint32_t index);

        RPS_NO_DISCARD
        RpsResult CreateResourceViews(const RenderGraphUpdateContext& context,
                                      ViewType                        viewType,
                                      ConstArrayRef<uint32_t>         accessIndices);

    private:
        D3D11RuntimeDevice&                 m_device;
        Arena                               m_persistentPool;

        ArenaVector<D3D11RuntimeCmd> m_runtimeCmds;
        ArenaVector<ID3D11View*>     m_views;

        struct FrameResources
        {
            ArenaVector<ID3D11DeviceChild*> pendingResources;

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

        ArenaVector<ID3D11DeviceChild*> m_pendingReleaseResources;
        ArenaVector<FrameResources>     m_frameResources;
        uint32_t                        m_currentResourceFrame = 0;
    };
}  // namespace rps

#endif  //RPS_D3D11_RUNTIME_BACKEND_HPP
