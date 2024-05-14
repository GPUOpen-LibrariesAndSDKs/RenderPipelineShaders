// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#define CATCH_CONFIG_MAIN

#include "rps/rps.h"

#include "utils/rps_test_common.h"

extern "C" {

typedef struct PrivateUpdateInfo
{
    uint32_t width;
    uint32_t height;
    RpsBool  bUseOffscreenRT;
    RpsBool  bUseMSAA;
} PrivateUpdateInfo;

RpsResult buildRenderToTexture(RpsRenderGraphBuilder hBuilder, const RpsConstant* ppArgs, uint32_t numArgs);

}  // extern "C"

RpsResult buildRenderToTextureCpp(RpsRenderGraphBuilder hBuilder, const RpsConstant* ppArgs, uint32_t numArgs)
{
    using namespace rps;

    REQUIRE(numArgs == 2);

    const RpsResourceDesc*   pBackBufferDesc    = static_cast<const RpsResourceDesc*>(ppArgs[0]);
    const PrivateUpdateInfo* pPrivateUpdateInfo = static_cast<const PrivateUpdateInfo*>(ppArgs[1]);

    RpsNodeDeclId drawTriangle = rpsRenderGraphDeclareDynamicNode(
        hBuilder,
        "Triangle",
        RPS_NODE_DECL_FLAG_NONE,
        {ParameterDesc::Make<ImageView>(AccessAttr(RPS_ACCESS_RENDER_TARGET_BIT), "renderTarget"),
         ParameterDesc::Make<RpsClearValue>(SemanticAttr(RPS_SEMANTIC_COLOR_CLEAR_VALUE), "clearValue"),
         ParameterDesc::Make<bool>("bMSAA"),
         ParameterDesc::Make<ImageView>(
             AccessAttr(RPS_ACCESS_RESOLVE_DEST_BIT), "resolveTarget", RPS_PARAMETER_FLAG_OPTIONAL_BIT)});

    RpsNodeDeclId bltQuad = rpsRenderGraphDeclareDynamicNode(
        hBuilder,
        "Quad",
        RPS_NODE_DECL_FLAG_NONE,
        {ParameterDesc::Make<ImageView>(SemanticAttr(RPS_SEMANTIC_RENDER_TARGET), "backBuffer"),
         ParameterDesc::Make<ImageView>(AccessAttr(RPS_ACCESS_SHADER_RESOURCE_BIT, RPS_SHADER_STAGE_PS), "offScreen")});

    struct RTTVariables
    {
        ResourceDesc  offscreenRTDesc;
        ResourceDesc  msaaRTDesc;
        ImageView     backBufferView;
        ImageView     offscreenRTView;
        ImageView     msaaRTView;
        RpsClearValue clearValue;
        bool          bUseMSAA;
    };

    enum NodeIdentifiers
    {
        NODE_ID_TRIANGLE,
        NODE_ID_MSAA_TRIANGLE,
        NODE_BLT,
    };

    enum ResourceIdentifiers
    {
        RESOURCE_ID_OFFSCREEN_RT,
        RESOURCE_ID_OFFSCREEN_MSAA_RT,
    };

    RTTVariables* rttVars = rpsRenderGraphAllocateData<RTTVariables>(hBuilder);
    REQUIRE(rttVars);

    const auto backBufferResId = rpsRenderGraphGetParamResourceId(hBuilder, 0);
    REQUIRE(backBufferResId != RPS_RESOURCE_ID_INVALID);

    rttVars->offscreenRTDesc = ResourceDesc(
        RPS_RESOURCE_TYPE_IMAGE_2D, RPS_FORMAT_R8G8B8A8_UNORM, pPrivateUpdateInfo->width, pPrivateUpdateInfo->height);
    rttVars->backBufferView = ImageView{backBufferResId};
    rttVars->clearValue     = RpsClearValue{{{0.2f, 0.2f, 0.8f, 1.0f}}};
    rttVars->bUseMSAA       = pPrivateUpdateInfo->bUseMSAA;

    if (pPrivateUpdateInfo->bUseOffscreenRT)
    {
        RpsResourceId offscreenRTResId =
            rpsRenderGraphDeclareResource(hBuilder, "OffscreenRT", RESOURCE_ID_OFFSCREEN_RT, &rttVars->offscreenRTDesc);
        rttVars->offscreenRTView = ImageView{offscreenRTResId};

        if (pPrivateUpdateInfo->bUseMSAA)
        {
            rttVars->msaaRTDesc                   = rttVars->offscreenRTDesc;
            rttVars->msaaRTDesc.image.sampleCount = 4;

            RpsResourceId offscreenRTMsaaResId = rpsRenderGraphDeclareResource(
                hBuilder, "OffscreenRTMsaa", RESOURCE_ID_OFFSCREEN_MSAA_RT, &rttVars->msaaRTDesc);

            rttVars->msaaRTView = ImageView{offscreenRTMsaaResId};

            rpsRenderGraphAddNode(
                hBuilder,
                drawTriangle,
                NODE_ID_MSAA_TRIANGLE,
                nullptr,
                nullptr,
                RPS_CMD_CALLBACK_FLAG_NONE,
                {&rttVars->msaaRTView, &rttVars->clearValue, &rttVars->bUseMSAA, &rttVars->offscreenRTView});
        }
        else
        {
            rpsRenderGraphAddNode(hBuilder,
                                  drawTriangle,
                                  NODE_ID_TRIANGLE,
                                  nullptr,
                                  nullptr,
                                  RPS_CMD_CALLBACK_FLAG_NONE,
                                  {&rttVars->offscreenRTView, &rttVars->clearValue, &rttVars->bUseMSAA, nullptr});
        }

        rpsRenderGraphAddNode(hBuilder,
                              bltQuad,
                              NODE_BLT,
                              nullptr,
                              nullptr,
                              RPS_CMD_CALLBACK_FLAG_NONE,
                              {&rttVars->offscreenRTView, &rttVars->backBufferView});
    }
    else
    {
        rpsRenderGraphAddNode(hBuilder,
                              drawTriangle,
                              pPrivateUpdateInfo->bUseMSAA ? NODE_ID_MSAA_TRIANGLE : NODE_ID_TRIANGLE,
                              nullptr,
                              nullptr,
                              RPS_CMD_CALLBACK_FLAG_NONE,
                              {&rttVars->backBufferView, &rttVars->clearValue, &rttVars->bUseMSAA, nullptr});
    }

    return RPS_OK;
}

TEST_CASE("BuildGraphUsingCApiCommon")
{
    RpsDevice device = rpsTestUtilCreateDevice([](auto pCreateInfo, auto phDevice) {
        RpsNullRuntimeDeviceCreateInfo nullDeviceCreateInfo = {};
        nullDeviceCreateInfo.pDeviceCreateInfo              = pCreateInfo;
        return rpsNullRuntimeDeviceCreate(&nullDeviceCreateInfo, phDevice);
    });

    // Graph params:
    RpsParameterDesc graphParams[2] = {};
    graphParams[0].typeInfo         = rpsTypeInfoInitFromType(RpsResourceDesc);
    graphParams[0].flags            = RPS_PARAMETER_FLAG_RESOURCE_BIT;
    graphParams[0].name             = "backBuffer";
    graphParams[1].typeInfo         = rpsTypeInfoInitFromType(PrivateUpdateInfo);
    graphParams[1].name             = "userContext";

    RpsRenderGraphSignatureDesc entryInfo = {0};
    entryInfo.name                        = "RenderToTexture_C";
    entryInfo.numParams                   = RPS_TEST_COUNTOF(graphParams);
    entryInfo.pParamDescs                 = graphParams;

    RpsRenderGraphCreateInfo renderGraphCreateInfo           = {};
    renderGraphCreateInfo.mainEntryCreateInfo.pSignatureDesc = &entryInfo;

    RpsRenderGraph hRenderGraph = RPS_NULL_HANDLE;
    REQUIRE_RPS_OK(rpsRenderGraphCreate(device, &renderGraphCreateInfo, &hRenderGraph));

    struct Resolution
    {
        uint32_t w;
        uint32_t h;
    };

    Resolution resolutions[] = {{1280, 720}, {3840, 2160}};

    RpsRenderGraphUpdateInfo renderGraphUpdateInfo = {};
    renderGraphUpdateInfo.gpuCompletedFrameIndex   = RPS_GPU_COMPLETED_FRAME_INDEX_NONE;
    renderGraphUpdateInfo.diagnosticFlags |= RPS_DIAGNOSTIC_ENABLE_ALL;

    RpsResourceDesc backBufferResDesc   = {};
    backBufferResDesc.type              = RPS_RESOURCE_TYPE_IMAGE_2D;
    backBufferResDesc.temporalLayers    = 1;
    backBufferResDesc.image.arrayLayers = 1;
    backBufferResDesc.image.format      = RPS_FORMAT_R8G8B8A8_UNORM;
    backBufferResDesc.image.mipLevels   = 1;
    backBufferResDesc.image.sampleCount = 1;

    for (auto res = std::cbegin(resolutions); res != std::cend(resolutions); ++res)
    {
        backBufferResDesc.image.width       = res->w;
        backBufferResDesc.image.height      = res->h;

        for (int32_t iUseOffscreenRT = 0; iUseOffscreenRT < 2; iUseOffscreenRT++)
        {
            for (int32_t iUseMSAA = 0; iUseMSAA < 2; iUseMSAA++)
            {
                PrivateUpdateInfo privateUpdateInfo = {res->w, res->h, iUseOffscreenRT, iUseMSAA};
                RpsConstant       args[]            = {&backBufferResDesc, &privateUpdateInfo};
                renderGraphUpdateInfo.numArgs       = RPS_TEST_COUNTOF(args);
                renderGraphUpdateInfo.ppArgs        = args;

                renderGraphUpdateInfo.pfnBuildCallback = &buildRenderToTexture;
                REQUIRE_RPS_OK(rpsRenderGraphUpdate(hRenderGraph, &renderGraphUpdateInfo));

                renderGraphUpdateInfo.pfnBuildCallback = &buildRenderToTextureCpp;
                REQUIRE_RPS_OK(rpsRenderGraphUpdate(hRenderGraph, &renderGraphUpdateInfo));
            }
        }
    }

    rpsRenderGraphDestroy(hRenderGraph);

    rpsTestUtilDestroyDevice(device);
}
