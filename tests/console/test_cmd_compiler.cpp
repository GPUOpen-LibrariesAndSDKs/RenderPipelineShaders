// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#define CATCH_CONFIG_MAIN

#include "rps/rps.h"

#include "utils/rps_test_common.h"

using namespace rps;

class MiniRenderer
{
public:
    struct RenderOptions
    {
        bool      EnableShadowMap;
        bool      ReverseZ;
        bool      EnableZPrePass;
        bool      EnableDeferred;
        bool      EnableTransparency;
        bool      EnablePostProcess;
        RpsFormat DepthFormat;
        RpsFormat ShadowMapFormat;
        uint32_t  ShadowMapSize;
    };

    MiniRenderer()
    {
        std::fill(std::begin(m_resourceIds), std::end(m_resourceIds), RPS_RESOURCE_ID_INVALID);
    }

    static RpsResult BuildRenderGraphCb(RpsRenderGraphBuilder cmdBuf, const RpsConstant* ppArgs, uint32_t numArgs)
    {
        MiniRenderer*        pThis            = *static_cast<MiniRenderer* const*>(ppArgs[0]);
        const ResourceDesc&  outputBufferDesc = *static_cast<const ResourceDesc*>(ppArgs[1]);
        const RenderOptions& options          = *static_cast<const RenderOptions*>(ppArgs[2]);

        pThis->Render(cmdBuf, outputBufferDesc, options);

        return RPS_OK;
    }

    void Render(RpsRenderGraphBuilder cmdBuf, const ResourceDesc& outputBufferDesc, const RenderOptions& options)
    {
        RenderGraphBuilderRef builder(cmdBuf);

        const RpsResourceId outputResId = builder.GetParamResourceId(1);
        REQUIRE(outputResId != RPS_RESOURCE_ID_INVALID);

        m_resourceDescs[RES_ID_OUTPUT_BUFFER] = outputBufferDesc;
        m_resourceViews[RES_ID_OUTPUT_BUFFER] = ImageView{outputResId};

        builder.SetParamVariable(1, m_resourceDescs[RES_ID_OUTPUT_BUFFER]);

        RpsFormat resourceFormats[] = {
            RPS_FORMAT_UNKNOWN,
            options.ShadowMapFormat,
            options.DepthFormat,
            RPS_FORMAT_R8G8B8A8_UNORM,
            RPS_FORMAT_R11G11B10_FLOAT,
            RPS_FORMAT_R16G16B16A16_FLOAT,
        };

        m_resourceDescs[RES_ID_SHADOW_MAP] = ResourceDesc{RPS_RESOURCE_TYPE_IMAGE_2D,
                                                          resourceFormats[RES_ID_SHADOW_MAP],
                                                          options.ShadowMapSize,
                                                          options.ShadowMapSize};

        for (uint32_t i = RES_ID_DEPTH_BUFFER; i < NUM_RESOURCES; i++)
        {
            m_resourceDescs[i] = ResourceDesc{RPS_RESOURCE_TYPE_IMAGE_2D,
                                              resourceFormats[i],
                                              outputBufferDesc.image.width,
                                              outputBufferDesc.image.height};
        }

        REQUIRE_RPS_OK(builder.DeclareResource(RES_ID_DEPTH_BUFFER,
                                                &m_resourceDescs[RES_ID_DEPTH_BUFFER],
                                                "DepthBuffer",
                                                &m_resourceIds[RES_ID_DEPTH_BUFFER]));

        m_resourceViews[RES_ID_DEPTH_BUFFER] = ImageView{m_resourceIds[RES_ID_DEPTH_BUFFER]};

        if (options.EnableZPrePass)
        {
            builder.AddNode(
                this,
                &MiniRenderer::RenderZPrePass,
                NODE_ID_ZPREPASS,
                "ZPrePass",
                builder.MakeNodeArg(m_resourceViews[RES_ID_DEPTH_BUFFER], AccessAttr(RPS_ACCESS_DEPTH_STENCIL_WRITE)));
        }

        if (options.EnableShadowMap)
        {
            REQUIRE_RPS_OK(builder.DeclareResource(RES_ID_SHADOW_MAP,
                                                   &m_resourceDescs[RES_ID_SHADOW_MAP],
                                                   "ShadowMap",
                                                   &m_resourceIds[RES_ID_SHADOW_MAP]));

            m_resourceViews[RES_ID_SHADOW_MAP] = ImageView(m_resourceIds[RES_ID_SHADOW_MAP]);

            builder.AddNode(
                this,
                &MiniRenderer::RenderShadowMap,
                NODE_ID_SHADOW_MAP,
                "ShadowMap",
                builder.MakeNodeArg(m_resourceViews[RES_ID_SHADOW_MAP], AccessAttr(RPS_ACCESS_DEPTH_STENCIL_WRITE)));
        }
        else
        {
            m_resourceViews[RES_ID_SHADOW_MAP] = ImageView(RPS_INDEX_NONE_U32);
        }

        m_resourceDescs[RES_ID_LIGHT_BUFFER].image.format =
            options.EnablePostProcess
                ? (options.EnableTransparency ? RPS_FORMAT_R16G16B16A16_FLOAT : RPS_FORMAT_R11G11B10_FLOAT)
                : outputBufferDesc.image.format;

        REQUIRE_RPS_OK(builder.DeclareResource(RES_ID_LIGHT_BUFFER,
                                                &m_resourceDescs[RES_ID_LIGHT_BUFFER],
                                                "LightBuffer",
                                                &m_resourceIds[RES_ID_LIGHT_BUFFER]));
        m_resourceViews[RES_ID_LIGHT_BUFFER] = ImageView{m_resourceIds[RES_ID_LIGHT_BUFFER]};

        if (options.EnableDeferred)
        {
            REQUIRE_RPS_OK(builder.DeclareResource(RES_ID_GBUFFER_MATERIAL,
                                                    &m_resourceDescs[RES_ID_GBUFFER_MATERIAL],
                                                    "MaterialBuffer",
                                                    &m_resourceIds[RES_ID_GBUFFER_MATERIAL]));

            REQUIRE_RPS_OK(builder.DeclareResource(RES_ID_GBUFFER_NORMAL,
                                                    &m_resourceDescs[RES_ID_GBUFFER_NORMAL],
                                                    "NormalBuffer",
                                                    &m_resourceIds[RES_ID_GBUFFER_NORMAL]));

            m_resourceViews[RES_ID_GBUFFER_MATERIAL] = ImageView{m_resourceIds[RES_ID_GBUFFER_MATERIAL]};
            m_resourceViews[RES_ID_GBUFFER_NORMAL]   = ImageView{m_resourceIds[RES_ID_GBUFFER_NORMAL]};

            builder.AddNode(this,
                            &MiniRenderer::RenderGBuffer,
                            NODE_ID_GBUFFER,
                            "GBuffer",
                            builder.MakeNodeArg(m_resourceViews[RES_ID_GBUFFER_MATERIAL],
                                                SemanticAttr(RPS_SEMANTIC_RENDER_TARGET, 0)),
                            builder.MakeNodeArg(m_resourceViews[RES_ID_GBUFFER_NORMAL],
                                                SemanticAttr(RPS_SEMANTIC_RENDER_TARGET, 1)),
                            builder.MakeNodeArg(m_resourceViews[RES_ID_DEPTH_BUFFER],
                                                AccessAttr(options.EnableZPrePass ? RPS_ACCESS_DEPTH_STENCIL_READ
                                                                                  : RPS_ACCESS_DEPTH_STENCIL_WRITE)));

            constexpr AccessAttr CS_SRV_ATTR = AccessAttr(RPS_ACCESS_SHADER_RESOURCE_BIT, RPS_SHADER_STAGE_CS);

            builder.AddNode(this,
                            &MiniRenderer::DeferredLighting,
                            NODE_ID_LIGHTING,
                            "DeferredLighting",
                            builder.MakeNodeArg(m_resourceViews[RES_ID_LIGHT_BUFFER],
                                                AccessAttr(RPS_ACCESS_UNORDERED_ACCESS_BIT, RPS_SHADER_STAGE_CS)),
                            builder.MakeNodeArg(m_resourceViews[RES_ID_GBUFFER_MATERIAL], CS_SRV_ATTR),
                            builder.MakeNodeArg(m_resourceViews[RES_ID_GBUFFER_NORMAL], CS_SRV_ATTR),
                            builder.MakeNodeArg(m_resourceViews[RES_ID_DEPTH_BUFFER], CS_SRV_ATTR),
                            builder.MakeNodeArg(m_resourceViews[RES_ID_SHADOW_MAP], CS_SRV_ATTR));
        }
        else
        {
            builder.AddNode(
                this,
                &MiniRenderer::RenderForward,
                NODE_ID_FORWARD,
                "Forward",
                builder.MakeNodeArg(m_resourceViews[RES_ID_LIGHT_BUFFER], SemanticAttr(RPS_SEMANTIC_RENDER_TARGET, 0)),
                builder.MakeNodeArg(m_resourceViews[RES_ID_DEPTH_BUFFER],
                                    AccessAttr(options.EnableZPrePass ? RPS_ACCESS_DEPTH_STENCIL_READ
                                                                      : RPS_ACCESS_DEPTH_STENCIL_WRITE)),
                builder.MakeNodeArg(m_resourceViews[RES_ID_SHADOW_MAP],
                                    AccessAttr(RPS_ACCESS_SHADER_RESOURCE_BIT, RPS_SHADER_STAGE_PS)));
        }

        if (options.EnableTransparency)
        {
            builder.AddNode(
                this,
                &MiniRenderer::RenderTransparency,
                NODE_ID_TRANSPARENCY,
                "Transparency",
                builder.MakeNodeArg(m_resourceViews[RES_ID_LIGHT_BUFFER], SemanticAttr(RPS_SEMANTIC_RENDER_TARGET, 0)),
                builder.MakeNodeArg(m_resourceViews[RES_ID_DEPTH_BUFFER],
                                    AccessAttr(RPS_ACCESS_SHADER_RESOURCE_BIT, RPS_SHADER_STAGE_PS)),
                builder.MakeNodeArg(m_resourceViews[RES_ID_DEPTH_BUFFER], AccessAttr(RPS_ACCESS_DEPTH_STENCIL_READ)),
                builder.MakeNodeArg(m_resourceViews[RES_ID_SHADOW_MAP],
                                    AccessAttr(RPS_ACCESS_SHADER_RESOURCE_BIT, RPS_SHADER_STAGE_PS)));
        }

        if (options.EnablePostProcess)
        {
            builder.AddNode(
                this,
                &MiniRenderer::PostProcess,
                NODE_ID_POST_PROCESS,
                "PostProcess",
                builder.MakeNodeArg(m_resourceViews[RES_ID_OUTPUT_BUFFER], SemanticAttr(RPS_SEMANTIC_RENDER_TARGET, 0)),
                builder.MakeNodeArg(m_resourceViews[RES_ID_LIGHT_BUFFER],
                                    AccessAttr(RPS_ACCESS_SHADER_RESOURCE_BIT, RPS_SHADER_STAGE_PS)),
                builder.MakeNodeArg(m_resourceViews[RES_ID_DEPTH_BUFFER], AccessAttr(RPS_ACCESS_DEPTH_STENCIL_READ)));
        }
        else
        {
            builder.AddNode(
                this,
                &MiniRenderer::Copy,
                NODE_ID_COPY_TO_OUTPUT,
                "CopyToOutput",
                builder.MakeNodeArg(m_resourceViews[RES_ID_OUTPUT_BUFFER], AccessAttr(RPS_ACCESS_COPY_DEST_BIT)),
                builder.MakeNodeArg(m_resourceViews[RES_ID_LIGHT_BUFFER], AccessAttr(RPS_ACCESS_COPY_SRC_BIT)));
        }
    }

private:
    void RenderShadowMap(const RpsCmdCallbackContext& context)
    //(const RpsImageView& shadowMap)
    {
    }

    void RenderZPrePass(const RpsCmdCallbackContext& context)
    //(const RpsImageView& depthBuffer, float depthClear)
    {
    }

    void RenderGBuffer(const RpsCmdCallbackContext& context)
    //(const RpsImageView& materialBuffer, const RpsImageView& normalBuffer, const RpsImageView& depthBuffer)
    {
    }

    void RenderForward(const RpsCmdCallbackContext& context)
    //(const RpsImageView& lightBuffer, const RpsImageView& depthBuffer, const RpsImageView& shadowMap)
    {
    }

    void DeferredLighting(const RpsCmdCallbackContext& context)
    //(const RpsImageView& materialBuffer, const RpsImageView& normalBuffer, const RpsImageView& depthBuffer, const RpsImageView& shadowMap)
    {
    }

    void RenderTransparency(const RpsCmdCallbackContext& context)
    //(const RpsImageView& lightBuffer, const RpsImageView& depthBuffer, const RpsImageView& shadowMap)
    {
    }

    void PostProcess(const RpsCmdCallbackContext& context)
    //(const RpsImageView& outputBuffer, const RpsImageView& lightBuffer, const RpsImageView& depthBuffer)
    {
    }

    void Copy(const RpsCmdCallbackContext& context)
    //(const RpsImageView& dstBuffer, const RpsImageView& srcBuffer)
    {
    }

private:
    enum ResourceIds
    {
        RES_ID_OUTPUT_BUFFER,
        RES_ID_SHADOW_MAP,
        RES_ID_DEPTH_BUFFER,
        RES_ID_GBUFFER_MATERIAL,
        RES_ID_GBUFFER_NORMAL,
        RES_ID_LIGHT_BUFFER,
        NUM_RESOURCES,
    };

    RpsResourceId m_resourceIds[NUM_RESOURCES];
    ResourceDesc  m_resourceDescs[NUM_RESOURCES];
    ImageView     m_resourceViews[NUM_RESOURCES];

    RpsClearValue clearValue = {};

    enum NodeIdentifiers
    {
        NODE_ID_ZPREPASS,
        NODE_ID_SHADOW_MAP,
        NODE_ID_FORWARD,
        NODE_ID_GBUFFER,
        NODE_ID_LIGHTING,
        NODE_ID_TRANSPARENCY,
        NODE_ID_POST_PROCESS,
        NODE_ID_COPY_TO_OUTPUT,
        NUM_NODE_IDS,
    };
};

TEST_CASE("BuildCmdBufAndRenderGraph")
{
    RpsDevice device = rpsTestUtilCreateDevice([](const RpsDeviceCreateInfo* pCreateInfo, RpsDevice* pDevice) {
        RpsNullRuntimeDeviceCreateInfo runtimeDeviceCreateInfo = {};
        runtimeDeviceCreateInfo.pDeviceCreateInfo              = pCreateInfo;
        return rpsNullRuntimeDeviceCreate(&runtimeDeviceCreateInfo, pDevice);
    });

    RpsRenderGraph renderGraph = RPS_NULL_HANDLE;

    {
        RpsParameterDesc paramDescs[] = {
            ParameterDesc::Make<MiniRenderer*>(),
            ParameterDesc::Make<RpsResourceDesc>("backBuffer", RPS_PARAMETER_FLAG_RESOURCE_BIT),
            ParameterDesc::Make<MiniRenderer::RenderOptions>(),
        };

        RpsRenderGraphSignatureDesc entryInfo = {};
        entryInfo.name                        = "BasicPipeline";
        entryInfo.numParams                   = RPS_TEST_COUNTOF(paramDescs);
        entryInfo.pParamDescs                 = paramDescs;

        RpsRenderGraphCreateInfo renderGraphCreateInfo           = {};
        renderGraphCreateInfo.mainEntryCreateInfo.pSignatureDesc = &entryInfo;

        REQUIRE_RPS_OK(rpsRenderGraphCreate(device, &renderGraphCreateInfo, &renderGraph));
    }

    using ResolutionType = std::pair<uint32_t, uint32_t>;

    ResourceDesc outputBufferDesc{RPS_RESOURCE_TYPE_IMAGE_2D, RPS_FORMAT_R8G8B8A8_UNORM, 1, 1};

    MiniRenderer renderer;

    MiniRenderer::RenderOptions options = {};
    options.DepthFormat                 = RPS_FORMAT_D32_FLOAT_S8X24_UINT;
    options.ShadowMapFormat             = RPS_FORMAT_D16_UNORM;
    options.ShadowMapSize               = 1024;

    struct
    {
        bool*       pEnable;
        const char* name;
    } optionArray[] = {
        {&options.EnableShadowMap, "ShadowMap"},
        {&options.ReverseZ, "ReverseZ"},
        {&options.EnableZPrePass, "ZPrePass"},
        {&options.EnableDeferred, "Deferred"},
        {&options.EnableTransparency, "Transparency"},
        {&options.EnablePostProcess, "PostProcess"},
    };

#define DEBUG_OPTION 1
#if DEBUG_OPTION
    options.EnableShadowMap    = true;
    options.ReverseZ           = false;
    options.EnableZPrePass     = true;
    options.EnableDeferred     = true;
    options.EnableTransparency = false;
    options.EnablePostProcess  = false;
#endif

    constexpr uint32_t NumPermutations = 1u << RPS_TEST_COUNTOF(optionArray);

    ResolutionType resolutions[] = {{1280, 720}, {3840, 2160}};

    RpsRenderGraphUpdateInfo updateInfo = {};
    updateInfo.gpuCompletedFrameIndex   = RPS_GPU_COMPLETED_FRAME_INDEX_NONE;
    updateInfo.diagnosticFlags |= RPS_DIAGNOSTIC_ENABLE_ALL;

    for (auto res = std::cbegin(resolutions); res != std::cend(resolutions); ++res)
    {
        outputBufferDesc.image.width  = res->first;
        outputBufferDesc.image.height = res->second;

#if DEBUG_OPTION
        MiniRenderer*     pRenderer = &renderer;
        const RpsConstant args[]    = {&pRenderer, &outputBufferDesc, &options};
        updateInfo.ppArgs           = args;
        updateInfo.numArgs          = RPS_TEST_COUNTOF(args);
        updateInfo.pfnBuildCallback = &MiniRenderer::BuildRenderGraphCb;

        REQUIRE_RPS_OK(rpsRenderGraphUpdate(renderGraph, &updateInfo));
#endif

        for (uint32_t iPerm = 0; iPerm < NumPermutations; iPerm++)
        {
            for (uint32_t iOpt = 0; iOpt < RPS_TEST_COUNTOF(optionArray); iOpt++)
            {
                *optionArray[iOpt].pEnable = !!((1u << iOpt) & iPerm);

                printf("%s%s : %s",
                       iOpt == 0 ? "\n" : ", ",
                       optionArray[iOpt].name,
                       *optionArray[iOpt].pEnable ? "1" : "0");
            }

            REQUIRE_RPS_OK(rpsRenderGraphUpdate(renderGraph, &updateInfo));
        }
    }

    rpsRenderGraphDestroy(renderGraph);

    rpsTestUtilDestroyDevice(device);
}
