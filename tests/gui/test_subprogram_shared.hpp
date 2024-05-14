// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#include "utils/rps_test_host.hpp"

RPS_DECLARE_RPSL_ENTRY(test_subprogram, Main);
RPS_DECLARE_RPSL_ENTRY(test_subprogram, Scene);
RPS_DECLARE_RPSL_ENTRY(test_subprogram, GUI);

static const char c_Shader[] = R"(

struct V2P
{
    float4 Pos : SV_Position;
};

struct CBData
{
    float4 color;
};

#ifndef VULKAN
ConstantBuffer<CBData> cb : register(b0);
#else
[[vk::push_constant]] CBData cb;
#endif

V2P VS(uint vertexId : SV_VertexID)
{
    V2P result;
    result.Pos = float4(
        (vertexId & 1) * 4.0f - 1.0f,
        (vertexId & 2) * -2.0f + 1.0f,
        0, 1);
    return result;
}

float4 PSGUI(V2P psIn) : SV_Target0
{
    return cb.color;
}

float4 PSScene(V2P psIn) : SV_Target0
{
    int2 tile = int2(psIn.Pos.xy) >> 5;
    return (0.2f + 0.6f * float((tile.x + tile.y) & 1)) * cb.color;
}
)";

#define TEST_APP_NAME_RAW "TestSubprogram"

using namespace DirectX;

class TestRpsSubprogram : public RpsTestHost
{
protected:
    void Init(RpsDevice hRpsDevice)
    {
        RpsTestHost::OnInit(hRpsDevice, rpsTestLoadRpslEntry(test_subprogram, Main));

        m_mainRpslProgram = rpsRenderGraphGetMainEntry(GetRpsRenderGraph());

        RpsProgramCreateInfo programCreateInfo = {};
        programCreateInfo.hRpslEntryPoint      = rpsTestLoadRpslEntry(test_subprogram, Scene);

        RpsResult result = rpsProgramCreate(hRpsDevice, &programCreateInfo, &m_drawSceneSubprogram);

        REQUIRE(result == RPS_OK);
        REQUIRE(m_drawSceneSubprogram != RPS_NULL_HANDLE);

        programCreateInfo.hRpslEntryPoint = rpsTestLoadRpslEntry(test_subprogram, GUI);

        result = rpsProgramCreate(hRpsDevice, &programCreateInfo, &m_drawGUISubprogram);
        REQUIRE(result == RPS_OK);
        REQUIRE(m_drawGUISubprogram != RPS_NULL_HANDLE);

        BindNodes(m_drawSceneSubprogram);
        BindNodes(m_drawGUISubprogram);
    }

    virtual void OnDestroy() override
    {
        rpsProgramDestroy(m_drawSceneSubprogram);
        rpsProgramDestroy(m_drawGUISubprogram);

        RpsTestHost::OnDestroy();
    }

protected:
    RpsSubprogram m_mainRpslProgram     = RPS_NULL_HANDLE;
    RpsSubprogram m_drawSceneSubprogram = RPS_NULL_HANDLE;
    RpsSubprogram m_drawGUISubprogram   = RPS_NULL_HANDLE;
};
