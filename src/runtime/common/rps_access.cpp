// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "rps/runtime/common/rps_access.h"
#include "core/rps_util.hpp"

namespace rps
{
    void AccessAttr::Print(const RpsPrinter& printer) const
    {
        PrinterRef print(printer);

        static const struct
        {
            RpsAccessFlagBits flags;
            StrRef            name;
        } c_accessNames[] = {
            {RPS_ACCESS_INDIRECT_ARGS_BIT, "indirect_arg"},
            {RPS_ACCESS_INDEX_BUFFER_BIT, "ib"},
            {RPS_ACCESS_VERTEX_BUFFER_BIT, "vb"},
            {RPS_ACCESS_CONSTANT_BUFFER_BIT, "cb"},
            // {RPS_ACCESS_SHADER_RESOURCE_BIT, ""},
            {RPS_ACCESS_SHADING_RATE_BIT, "shading_rate"},
            {RPS_ACCESS_RENDER_TARGET_BIT, "color"},
            {RPS_ACCESS_DEPTH_READ_BIT, "depth_read"},
            {RPS_ACCESS_DEPTH_WRITE_BIT, "depth_write"},
            {RPS_ACCESS_STENCIL_READ_BIT, "stencil_read"},
            {RPS_ACCESS_STENCIL_WRITE_BIT, "stencil_write"},
            // {RPS_ACCESS_UNORDERED_ACCESS_BIT, ""},
            {RPS_ACCESS_STREAM_OUT_BIT, "stream_out"},
            {RPS_ACCESS_COPY_SRC_BIT, "copy_src"},
            {RPS_ACCESS_COPY_DEST_BIT, "copy_dst"},
            {RPS_ACCESS_RESOLVE_SRC_BIT, "resolve_src"},
            {RPS_ACCESS_RESOLVE_DEST_BIT, "resolve_dst"},
            {RPS_ACCESS_RAYTRACING_AS_BUILD_BIT, "rtas_build"},
            {RPS_ACCESS_RAYTRACING_AS_READ_BIT, "rtas_read"},
            {RPS_ACCESS_PRESENT_BIT, "present"},
            {RPS_ACCESS_CPU_READ_BIT, "cpu_read"},
            {RPS_ACCESS_CPU_WRITE_BIT, "cpu_write"},

            // Special flags
            {RPS_ACCESS_DISCARD_DATA_BEFORE_BIT, "discard_before"},
            {RPS_ACCESS_DISCARD_DATA_AFTER_BIT, "discard_after"},
            {RPS_ACCESS_STENCIL_DISCARD_DATA_BEFORE_BIT, "stencil_discard_before"},
            {RPS_ACCESS_STENCIL_DISCARD_DATA_AFTER_BIT, "stencil_discard_after"},
            {RPS_ACCESS_BEFORE_BIT, "before"},
            {RPS_ACCESS_AFTER_BIT, "after"},
            {RPS_ACCESS_CLEAR_BIT, "clear"},
            {RPS_ACCESS_RENDER_PASS, "render_pass"},
            {RPS_ACCESS_RELAXED_ORDER_BIT, "relaxed"},
            {RPS_ACCESS_NO_VIEW_BIT, "no_view"},
        };

        static const struct
        {
            RpsShaderStageBits stage;
            const StrRef       name;
        } c_shaderStageNames[] = {
            {RPS_SHADER_STAGE_VS, "vs"},
            {RPS_SHADER_STAGE_PS, "ps"},
            {RPS_SHADER_STAGE_GS, "gs"},
            {RPS_SHADER_STAGE_CS, "cs"},
            {RPS_SHADER_STAGE_HS, "hs"},
            {RPS_SHADER_STAGE_DS, "ds"},
            {RPS_SHADER_STAGE_RAYTRACING, "rt"},
            {RPS_SHADER_STAGE_AS, "as"},
            {RPS_SHADER_STAGE_MS, "ms"},
        };

        auto printShaderStages = [&]() {
            uint32_t accessStagesMask = accessStages;
            for (auto i = std::begin(c_shaderStageNames), e = std::end(c_shaderStageNames); i != e; ++i)
            {
                if (i->stage & accessStagesMask)
                {
                    print("%s%.*s", (accessStagesMask == accessStages) ? "" : ", ", i->name.len, i->name.str);
                    accessStagesMask &= ~i->stage;
                }
            }

            if (accessStagesMask != 0)
            {
                print("%sRpsShaderStageBits(0x%x)", (accessStagesMask == accessFlags) ? "" : ", ", accessStagesMask);
            }
        };

        if (accessFlags != RPS_ACCESS_UNKNOWN)
        {
            uint32_t accessFlagsMask = accessFlags;

            for (auto i = std::begin(c_accessNames), e = std::end(c_accessNames); i != e; ++i)
            {
                if (i->flags & accessFlagsMask)
                {
                    print("%s%.*s", (accessFlagsMask == accessFlags) ? "" : ", ", i->name.len, i->name.str);
                    accessFlagsMask &= ~i->flags;
                }
            }

            if (rpsAnyBitsSet(accessFlags, RPS_ACCESS_UNORDERED_ACCESS_BIT))
            {
                print((accessFlagsMask == accessFlags) ? "" : ", ");
                accessFlagsMask &= ~RPS_ACCESS_UNORDERED_ACCESS_BIT;

                print("uav(");
                printShaderStages();
                print(")");
            }

            if (rpsAnyBitsSet(accessFlags, RPS_ACCESS_SHADER_RESOURCE_BIT))
            {
                print((accessFlagsMask == accessFlags) ? "" : ", ");
                accessFlagsMask &= ~RPS_ACCESS_SHADER_RESOURCE_BIT;

                print("srv(");
                printShaderStages();
                print(")");
            }

            if (accessFlagsMask != 0)
            {
                print("%sRpsAccessFlags(0x%x)", (accessFlagsMask == accessFlags) ? "" : ", ", accessFlagsMask);
            }
        }
        else
        {
            print("*");
        }
    }

    void SemanticAttr::Print(const RpsPrinter& printer) const
    {
        // clang-format off
        static const StrRef c_semanticNames[] = {
            "",                         //RPS_SEMANTIC_UNSPECIFIED = 0,

            "VertexShader",             //RPS_SEMANTIC_VERTEX_SHADER,
            "PixelShader",              //RPS_SEMANTIC_PIXEL_SHADER,
            "GeometryShader",           //RPS_SEMANTIC_GEOMETRY_SHADER,
            "ComputeShader",            //RPS_SEMANTIC_COMPUTE_SHADER,
            "HullShader",               //RPS_SEMANTIC_HULL_SHADER,
            "DomainShader",             //RPS_SEMANTIC_DOMAIN_SHADER,
            "RaytracingPipeline",       //RPS_SEMANTIC_RAYTRACING_PIPELINE,
            "AmplificationShader",      //RPS_SEMANTIC_AMPLIFICATION_SHADER,
            "MeshShader",               //RPS_SEMANTIC_MESH_SHADER,

            "VertexLayout",             //RPS_SEMANTIC_VERTEX_LAYOUT,
            "StreamOutLayout",          //RPS_SEMANTIC_STREAM_OUT_LAYOUT,
            "StreamOutDesc",            //RPS_SEMANTIC_STREAM_OUT_DESC,
            "BlendState",               //RPS_SEMANTIC_BLEND_STATE,
            "RenderTargetBlend",        //RPS_SEMANTIC_RENDER_TARGET_BLEND,
            "DepthStencil",             //RPS_SEMANTIC_DEPTH_STENCIL_STATE,
            "RasterizerState",          //RPS_SEMANTIC_RASTERIZER_STATE,

            "SV_Viewport",              //RPS_SEMANTIC_VIEWPORT = RPS_SEMANTIC_DYNAMIC_STATE_BEGIN,
            "SV_ScissorRect",           //RPS_SEMANTIC_SCISSOR,
            "SV_PrimitiveTopology",     //RPS_SEMANTIC_PRIMITIVE_TOPOLOGY,
            "SV_PatchControlPoints",    //RPS_SEMANTIC_PATCH_CONTROL_POINTS,
            "SV_PrimitiveStripCutIndex",//RPS_SEMANTIC_PRIMITIVE_STRIP_CUT_INDEX,
            "SV_BlendFactor",           //RPS_SEMANTIC_BLEND_FACTOR,
            "SV_StencilRef",            //RPS_SEMANTIC_STENCIL_REF,
            "SV_DepthBounds",           //RPS_SEMANTIC_DEPTH_BOUNDS,
            "SV_SampleLocation",        //RPS_SEMANTIC_SAMPLE_LOCATION,
            "SV_ShadingRate",           //RPS_SEMANTIC_SHADING_RATE,
            "SV_ClearColor",            //RPS_SEMANTIC_COLOR_CLEAR_VALUE,
            "SV_ClearDepth",            //RPS_SEMANTIC_DEPTH_CLEAR_VALUE,
            "SV_ClearStencil",          //RPS_SEMANTIC_STENCIL_CLEAR_VALUE,

            "SV_VertexBuffer",          //RPS_SEMANTIC_VERTEX_BUFFER = RPS_SEMANTIC_RESOURCE_BINDING_BEGIN,
            "SV_IndexBuffer",           //RPS_SEMANTIC_INDEX_BUFFER,
            "SV_IndirectArgs",          //RPS_SEMANTIC_INDIRECT_ARGS,
            "SV_IndirectCount",         //RPS_SEMANTIC_INDIRECT_COUNT,
            "SV_StreamOutBuffer",       //RPS_SEMANTIC_STREAM_OUT_BUFFER,
            "SV_Target",                //RPS_SEMANTIC_RENDER_TARGET,
            "SV_DepthStencil",          //RPS_SEMANTIC_DEPTH_STENCIL_TARGET,
            "SV_ShadingRateImage",      //RPS_SEMANTIC_SHADING_RATE_IMAGE,
            "SV_ResolveTarget",         //RPS_SEMANTIC_RESOLVE_TARGET,
        };
        // clang-format on

        static_assert(RPS_COUNTOF(c_semanticNames) == RPS_SEMANTIC_USER_RESOURCE_BINDING, "SemanticName needs update");

        auto print = PrinterRef(printer);

        if (semantic != RPS_SEMANTIC_UNSPECIFIED)
        {
            if (size_t(semantic) < RPS_COUNTOF(c_semanticNames))
                print(" : %.*s[%u]", c_semanticNames[semantic].len, c_semanticNames[semantic].str, semanticIndex);
            else
                print(" : <Unknown Semantic %d>[%u]", semantic, semanticIndex);
        }
    }
}  // namespace rps
