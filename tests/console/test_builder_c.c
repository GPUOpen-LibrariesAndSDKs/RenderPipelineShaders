// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "utils/rps_test_common.h"

RpsResult buildRenderToTexture(RpsRenderGraphBuilder cmdBuf, const RpsConstant* ppArgs, uint32_t numArgs)
{
    typedef struct PrivateUpdateInfo
    {
        uint32_t width;
        uint32_t height;
        RpsBool  bUseOffscreenRT;
        RpsBool  bUseMSAA;
    } PrivateUpdateInfo;

    REQUIRE(numArgs == 2);

    const PrivateUpdateInfo* pUpdateInfo = (const PrivateUpdateInfo*)ppArgs[1];

    RpsParameterDesc pTriangleNodeParamDescs[4];
    memset(&pTriangleNodeParamDescs, 0, sizeof(pTriangleNodeParamDescs));

    RpsParamAttr rtvAccessAttr, clearColorAttr, resolveDstAttr, psSrvAttr;
    rpsInitParamAttrAccess(&rtvAccessAttr, RPS_ACCESS_RENDER_TARGET_BIT, RPS_SHADER_STAGE_NONE);

    pTriangleNodeParamDescs[0].flags    = RPS_PARAMETER_FLAG_RESOURCE_BIT;
    pTriangleNodeParamDescs[0].name     = "renderTarget";
    pTriangleNodeParamDescs[0].attr     = &rtvAccessAttr;
    pTriangleNodeParamDescs[0].typeInfo = rpsTypeInfoInitFromTypeAndID(RpsImageView, RPS_TYPE_IMAGE_VIEW);

    pTriangleNodeParamDescs[1].name     = "clearValue";
    pTriangleNodeParamDescs[1].attr     = rpsInitParamAttrSemantic(&clearColorAttr, RPS_SEMANTIC_COLOR_CLEAR_VALUE, 0);
    pTriangleNodeParamDescs[1].typeInfo = rpsTypeInfoInitFromType(RpsClearValue);

    pTriangleNodeParamDescs[2].name     = "bEnableMSAA";
    pTriangleNodeParamDescs[2].typeInfo = rpsTypeInfoInitFromType(RpsBool);

    if (pUpdateInfo->bUseMSAA)
    {
        pTriangleNodeParamDescs[3].flags = RPS_PARAMETER_FLAG_RESOURCE_BIT;
        pTriangleNodeParamDescs[3].name  = "resolveTarget";
        pTriangleNodeParamDescs[3].attr =
            rpsInitParamAttrAccess(&resolveDstAttr, RPS_ACCESS_RESOLVE_DEST_BIT, RPS_SHADER_STAGE_NONE);
        pTriangleNodeParamDescs[3].typeInfo = rpsTypeInfoInitFromTypeAndID(RpsImageView, RPS_TYPE_IMAGE_VIEW);
    }

    RpsParameterDesc pQuadNodeParamDescs[2];
    memset(&pQuadNodeParamDescs, 0, sizeof(pQuadNodeParamDescs));

    pQuadNodeParamDescs[0].flags = RPS_PARAMETER_FLAG_RESOURCE_BIT;
    pQuadNodeParamDescs[0].name  = "target";
    pQuadNodeParamDescs[0].attr  = &rtvAccessAttr;
    pQuadNodeParamDescs[1].flags = RPS_PARAMETER_FLAG_RESOURCE_BIT;
    pQuadNodeParamDescs[1].name  = "source";
    pQuadNodeParamDescs[1].attr =
        rpsInitParamAttrAccess(&psSrvAttr, RPS_ACCESS_SHADER_RESOURCE_BIT, RPS_SHADER_STAGE_PS);

    RpsNodeDesc* pTriangleNodeDesc = rpsRenderGraphAllocateDataOfTypeZeroed(cmdBuf, RpsNodeDesc);
    pTriangleNodeDesc->name        = "Triangle";
    pTriangleNodeDesc->numParams   = pUpdateInfo->bUseMSAA ? 4 : 3;
    pTriangleNodeDesc->pParamDescs = pTriangleNodeParamDescs;

    const RpsNodeDeclId triangleNodeId = rpsRenderGraphDeclareDynamicNode(cmdBuf, pTriangleNodeDesc);
    REQUIRE(RPS_NODEDECL_ID_INVALID != triangleNodeId);

    RpsNodeDesc* pQuadNodeDesc = rpsRenderGraphAllocateDataOfTypeZeroed(cmdBuf, RpsNodeDesc);
    pQuadNodeDesc->name        = "Quad";
    pQuadNodeDesc->numParams   = 2;
    pQuadNodeDesc->pParamDescs = pQuadNodeParamDescs;

    const RpsNodeDeclId quadNodeId = rpsRenderGraphDeclareDynamicNode(cmdBuf, pQuadNodeDesc);
    REQUIRE(RPS_NODEDECL_ID_INVALID != quadNodeId);

    RpsImageView* pBackBufferView    = rpsRenderGraphAllocateDataOfTypeZeroed(cmdBuf, RpsImageView);
    pBackBufferView->base.resourceId = rpsRenderGraphGetParamResourceId(cmdBuf, 0);
    REQUIRE(pBackBufferView->base.resourceId != RPS_RESOURCE_ID_INVALID);
    pBackBufferView->subresourceRange.arrayLayers = 1;
    pBackBufferView->subresourceRange.mipLevels   = 1;

    RpsClearValue* pClearValue = rpsRenderGraphAllocateDataOfType(cmdBuf, RpsClearValue);
    *pClearValue               = (RpsClearValue){{{0.2f, 0.2f, 0.8f, 1.0f}}};

    enum
    {
        NODE_ID_TRIANGLE,
        NODE_ID_MSAA_TRIANGLE,
        NODE_BLT,
    };

    enum
    {
        RESOURCE_ID_OFFSCREEN_RT,
        RESOURCE_ID_OFFSCREEN_MSAA_RT,
    };

    if (pUpdateInfo->bUseOffscreenRT)
    {
        RpsResourceDesc* pTriangleRTDesc   = rpsRenderGraphAllocateDataOfTypeZeroed(cmdBuf, RpsResourceDesc);
        pTriangleRTDesc->type              = RPS_RESOURCE_TYPE_IMAGE_2D;
        pTriangleRTDesc->image.width       = pUpdateInfo->width;
        pTriangleRTDesc->image.height      = pUpdateInfo->height;
        pTriangleRTDesc->image.arrayLayers = 1;
        pTriangleRTDesc->image.mipLevels   = 1;
        pTriangleRTDesc->image.format      = RPS_FORMAT_R8G8B8A8_UNORM;
        pTriangleRTDesc->image.sampleCount = 1;

        RpsResourceId offscreenRTResId =
            rpsRenderGraphDeclareResource(cmdBuf, "OffscreenRT", RESOURCE_ID_OFFSCREEN_RT, pTriangleRTDesc);
        RpsImageView* pTriangleRTView =
            rpsRenderGraphAllocateDataOfTypeAndCopyFrom(cmdBuf, RpsImageView, pBackBufferView);
        pTriangleRTView->base.resourceId = offscreenRTResId;

        RpsBool* pUseMsaa = rpsRenderGraphAllocateDataOfTypeAndCopyFrom(cmdBuf, RpsBool, &pUpdateInfo->bUseMSAA);

        if (pUpdateInfo->bUseMSAA)
        {
            RpsResourceDesc* pTriangleRTMsaaDesc =
                rpsRenderGraphAllocateDataOfTypeAndCopyFrom(cmdBuf, RpsResourceDesc, pTriangleRTDesc);
            pTriangleRTMsaaDesc->image.sampleCount = 4;

            RpsResourceId offscreenRTMsaaResId = rpsRenderGraphDeclareResource(
                cmdBuf, "OffscreenRTMsaa", RESOURCE_ID_OFFSCREEN_MSAA_RT, pTriangleRTMsaaDesc);

            RpsImageView* pTriangleMsaaRTView =
                rpsRenderGraphAllocateDataOfTypeAndCopyFrom(cmdBuf, RpsImageView, pTriangleRTView);
            pTriangleMsaaRTView->base.resourceId = offscreenRTMsaaResId;

            RpsVariable ppTriArgs[] = {pTriangleMsaaRTView, pClearValue, pUseMsaa, pTriangleRTView};
            rpsRenderGraphAddNode(
                cmdBuf, triangleNodeId, NODE_ID_MSAA_TRIANGLE, NULL, NULL, RPS_CMD_CALLBACK_FLAG_NONE, ppTriArgs, RPS_TEST_COUNTOF(ppTriArgs));
        }
        else
        {
            RpsVariable ppTriArgs[] = {pTriangleRTView, pClearValue, pUseMsaa};
            rpsRenderGraphAddNode(cmdBuf,
                                  triangleNodeId,
                                  NODE_ID_TRIANGLE,
                                  NULL,
                                  NULL,
                                  RPS_CMD_CALLBACK_FLAG_NONE,
                                  ppTriArgs,
                                  RPS_TEST_COUNTOF(ppTriArgs));
        }

        RpsVariable ppQuadArgs[] = {pTriangleRTView, pBackBufferView};
        rpsRenderGraphAddNode(cmdBuf,
                              quadNodeId,
                              NODE_BLT,
                              NULL,
                              NULL,
                              RPS_CMD_CALLBACK_FLAG_NONE,
                              ppQuadArgs,
                              RPS_TEST_COUNTOF(ppQuadArgs));
    }
    else
    {
        RpsBool  bUseMSAA = RPS_FALSE;
        RpsBool* pUseMsaa = rpsRenderGraphAllocateDataOfTypeAndCopyFrom(cmdBuf, RpsBool, &bUseMSAA);

        RpsVariable ppTriArgs[] = {pBackBufferView, pClearValue, pUseMsaa, NULL};
        rpsRenderGraphAddNode(cmdBuf,
                              triangleNodeId,
                              NODE_ID_TRIANGLE,
                              NULL,
                              NULL,
                              RPS_CMD_CALLBACK_FLAG_NONE,
                              ppTriArgs,
                              pTriangleNodeDesc->numParams);
    }

    return RPS_OK;
}
