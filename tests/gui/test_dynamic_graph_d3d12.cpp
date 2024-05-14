// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#define RPS_D3D12_RUNTIME 1

#include "test_dynamic_graph_shared.hpp"

#include "utils/rps_test_win32.hpp"
#include "utils/rps_test_d3d12_renderer.hpp"

using namespace DirectX;

class TestD3D12RpsDynamicGraph : public RpsTestD3D12Renderer, public TestRpsDynamicGraph
{
protected:
    virtual void OnInit(ID3D12GraphicsCommandList*         pInitCmdList,
                        std::vector<ComPtr<ID3D12Object>>& tempResources) override
    {
        TestRpsDynamicGraph::OnInit();
    }

    virtual void OnPostResize() override
    {
    }

    virtual void OnCleanUp() override
    {
        TestRpsDynamicGraph::OnCleanUp();
    }

    virtual void OnUpdate(uint32_t frameIndex) override
    {
        TestRpsDynamicGraph::OnUpdate(frameIndex, m_width, m_height);
        UpdatePipeline(frameIndex, CalcGuaranteedCompletedFrameIndexForRps());
    }

    virtual void OnRender(uint32_t frameIndex) override
    {
        REQUIRE(RPS_SUCCEEDED(ExecuteRenderGraph(frameIndex, GetRpsRenderGraph())));
    }

protected:
    virtual void CreateRpsDevice(RpsDevice& rpsDeviceOut) override final
    {
        rpsDeviceOut = rpsTestUtilCreateDevice(
            [this](auto pCreateInfo, auto phDevice) { return CreateRpsRuntimeDevice(*pCreateInfo, *phDevice); });
    }

    virtual void BindNodes(RpsSubprogram hProgram) override final
    {
        REQUIRE_RPS_OK(rpsProgramBindNode(hProgram, "GfxDraw", &TestD3D12RpsDynamicGraph::GfxDraw, this));
        REQUIRE_RPS_OK(rpsProgramBindNode(hProgram, "ComputeDraw", &TestD3D12RpsDynamicGraph::ComputeDraw, this));
        REQUIRE_RPS_OK(rpsProgramBindNode(hProgram, "Blt", &TestD3D12RpsDynamicGraph::Blt, this));
    }

private:
    virtual void GfxDraw(const RpsCmdCallbackContext* pContext)
    {
    }
    virtual void ComputeDraw(const RpsCmdCallbackContext* pContext)
    {
    }
    virtual void Blt(const RpsCmdCallbackContext* pContext)
    {
    }

private:

    void UpdatePipeline(uint64_t frameIndex, uint64_t completedFrameIndex)
    {
        RpsRuntimeResource backBuffers[DXGI_MAX_SWAP_CHAIN_BUFFERS];
        for (uint32_t i = 0; i < m_backBufferCount; i++)
        {
            backBuffers[i] = rpsD3D12ResourceToHandle(m_backBuffers[i].Get());
        }

        RpsResourceDesc backBufferDesc   = {};
        backBufferDesc.type              = RPS_RESOURCE_TYPE_IMAGE_2D;
        backBufferDesc.temporalLayers    = uint32_t(m_backBuffers.size());
        backBufferDesc.image.arrayLayers = 1;
        backBufferDesc.image.mipLevels   = 1;
        backBufferDesc.image.format      = rpsFormatFromDXGI(m_swapChain.GetFormat());
        backBufferDesc.image.width       = m_width;
        backBufferDesc.image.height      = m_height;
        backBufferDesc.image.sampleCount = 1;

        TestRpsDynamicGraph::UpdateRpsPipeline(frameIndex, completedFrameIndex, backBufferDesc, backBuffers);
    }
};

TEST_CASE(TEST_APP_NAME)
{
#if _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    TestD3D12RpsDynamicGraph renderer;

    RpsTestRunWindowInfo runInfo = {};
    runInfo.title                = TEXT(TEST_APP_NAME);
    runInfo.numFramesToRender    = g_exitAfterFrame;
    runInfo.width                = 1280;
    runInfo.height               = 720;
    runInfo.pRenderer            = &renderer;
    RpsTestRunWindowApp(&runInfo);
}
