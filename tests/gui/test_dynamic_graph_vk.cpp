// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#else
#error "TODO"
#endif

#define RPS_VK_RUNTIME 1

#include "test_dynamic_graph_shared.hpp"

#include "utils/rps_test_common.h"
#include "utils/rps_test_win32.hpp"
#include "utils/rps_test_vk_renderer.hpp"

#include <DirectXMath.h>

class TestVkRpsDynamicGraph : public RpsTestVulkanRenderer, public TestRpsDynamicGraph
{
protected:
    virtual void OnInit(VkCommandBuffer initCmdBuf, InitTempResources& tempResources) override
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
        REQUIRE_RPS_OK(rpsProgramBindNode(hProgram, "GfxDraw", &TestVkRpsDynamicGraph::GfxDraw, this));
        REQUIRE_RPS_OK(rpsProgramBindNode(hProgram, "ComputeDraw", &TestVkRpsDynamicGraph::ComputeDraw, this));
        REQUIRE_RPS_OK(rpsProgramBindNode(hProgram, "Blt", &TestVkRpsDynamicGraph::Blt, this));
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
        RpsRuntimeResource backBuffers[16];

        if (m_swapChainImages.size() > RPS_TEST_COUNTOF(backBuffers))
            throw;

        for (uint32_t i = 0; i < m_swapChainImages.size(); i++)
        {
            backBuffers[i] = rpsVKImageToHandle(m_swapChainImages[i].image);
        }

        RpsResourceDesc backBufferDesc   = {};
        backBufferDesc.type              = RPS_RESOURCE_TYPE_IMAGE_2D;
        backBufferDesc.temporalLayers    = uint32_t(m_swapChainImages.size());
        backBufferDesc.image.arrayLayers = 1;
        backBufferDesc.image.mipLevels   = 1;
        backBufferDesc.image.format      = rpsFormatFromVK(m_swapChainFormat.format);
        backBufferDesc.image.width       = m_width;
        backBufferDesc.image.height      = m_height;
        backBufferDesc.image.sampleCount = 1;

        TestRpsDynamicGraph::UpdateRpsPipeline(frameIndex, completedFrameIndex, backBufferDesc, backBuffers);
    }
};

TEST_CASE(TEST_APP_NAME)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#if defined(BREAK_AT_ALLOC_ID)
    _CrtSetBreakAlloc(BREAK_AT_ALLOC_ID);
#endif

    TestVkRpsDynamicGraph renderer;

    RpsTestRunWindowInfo runInfo = {};
    runInfo.title                = TEXT(TEST_APP_NAME);
    runInfo.numFramesToRender    = g_exitAfterFrame;
    runInfo.width                = 1280;
    runInfo.height               = 720;
    runInfo.pRenderer            = &renderer;
    RpsTestRunWindowApp(&runInfo);
}
