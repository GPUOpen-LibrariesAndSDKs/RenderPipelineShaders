// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_ACCESS_H
#define RPS_ACCESS_H

#ifndef RPSL_COMPILER_BUILD

#include "rps/core/rps_api.h"
#include "rps/runtime/common/rps_format.h"
#include "rps/runtime/common/rps_resource.h"

#endif  //RPSL_COMPILER_BUILD

/// @addtogroup RpsRenderGraphRuntime
/// @{

//---------------------------------------------------------------------------------------
// Resource Access
//---------------------------------------------------------------------------------------

/// @defgroup RpsAccessAttr RpsAccessAttr
/// @{

/// @brief Bitflags for resource (view) access attributes.
///
/// If specified for a node parameter, it indicates the required resource layout and synchronizations before entering
/// and after exiting the node.
typedef enum RpsAccessFlagBits
{
    // Basic access flags
    RPS_ACCESS_UNKNOWN              = 0,           ///< Unknown access.
    RPS_ACCESS_INDIRECT_ARGS_BIT    = 1 << 0,      ///< Accessible as an indirect argument buffer.
    RPS_ACCESS_INDEX_BUFFER_BIT     = 1 << 1,      ///< Accessible as an index buffer.
    RPS_ACCESS_VERTEX_BUFFER_BIT    = 1 << 2,      ///< Accessible as a vertex buffer.
    RPS_ACCESS_CONSTANT_BUFFER_BIT  = 1 << 3,      ///< Accessible as a constant buffer.
    RPS_ACCESS_SHADER_RESOURCE_BIT  = 1 << 4,      ///< Accessible as a shader resource (readonly) view.
    RPS_ACCESS_UNORDERED_ACCESS_BIT = 1 << 5,      ///< Accessible as a unordered access (shader readwrite) view.
    RPS_ACCESS_SHADING_RATE_BIT     = 1 << 6,      ///< Accessible as a shading rate image in a Variable Rate Shading
                                                   ///  (VRS) pass.
    RPS_ACCESS_RENDER_TARGET_BIT       = 1 << 7,   ///< Accessible as a render target view.
    RPS_ACCESS_DEPTH_READ_BIT          = 1 << 8,   ///< Accessible as a readonly depth view.
    RPS_ACCESS_DEPTH_WRITE_BIT         = 1 << 9,   ///< Accessible as a writable depth view.
    RPS_ACCESS_STENCIL_READ_BIT        = 1 << 10,  ///< Accessible as a readonly stencil view.
    RPS_ACCESS_STENCIL_WRITE_BIT       = 1 << 11,  ///< Accessible as a writable stencil view.
    RPS_ACCESS_STREAM_OUT_BIT          = 1 << 12,  ///< Accessible for write as a stream out buffer.
    RPS_ACCESS_COPY_SRC_BIT            = 1 << 13,  ///< Accessible as a copy source.
    RPS_ACCESS_COPY_DEST_BIT           = 1 << 14,  ///< Accessible as a copy target.
    RPS_ACCESS_RESOLVE_SRC_BIT         = 1 << 15,  ///< Accessible as a resolve source.
    RPS_ACCESS_RESOLVE_DEST_BIT        = 1 << 16,  ///< Accessible as a resolve target.
    RPS_ACCESS_RAYTRACING_AS_BUILD_BIT = 1 << 17,  ///< Accessible for write (build) as a raytracing acceleration
                                                   ///  structure.
    RPS_ACCESS_RAYTRACING_AS_READ_BIT = 1 << 18,   ///< Accessible for read as a raytracing acceleration structure.
    RPS_ACCESS_PRESENT_BIT            = 1 << 19,   ///< Accessible as a present source.
    RPS_ACCESS_CPU_READ_BIT           = 1 << 20,   ///< Accessible for reads by the CPU.
    RPS_ACCESS_CPU_WRITE_BIT          = 1 << 21,   ///< Accessible for writes by the CPU.

    // Additional decorator flags not for standalone use but instead for combination with basic access flags.

    /// Access does not read existing data so it can be discarded. Does not apply to the stencil aspect.
    RPS_ACCESS_DISCARD_DATA_BEFORE_BIT = 1 << 22,

    /// Data can be discarded after current access (node). Does not apply to the stencil aspect.
    /// This is typically not programmed directly, but added internally during subresource data lifetime analysis
    /// based on if the data will be accessed afterwards. However it can be used to force discarding the data.
    RPS_ACCESS_DISCARD_DATA_AFTER_BIT = 1 << 23,

    /// Stencil access does not read existing data so it can be discarded. Applies only to the stencil aspect.
    RPS_ACCESS_STENCIL_DISCARD_DATA_BEFORE_BIT = 1 << 24,

    /// Stencil data can be discarded after current access (node). Applies only to the stencil aspect.
    /// This is typically not programmed directly, but added internally during subresource data lifetime analysis
    /// based on if the data will be accessed afterwards. However it can be used to force discarding the data.
    RPS_ACCESS_STENCIL_DISCARD_DATA_AFTER_BIT = 1 << 25,

    /// Initial state when entering the node. This allows a view to have a different state at entering and exiting,
    /// in case the node implementation needs to perform a transition but does not want to transition it back to the
    /// original state. Not implemented yet.
    RPS_ACCESS_BEFORE_BIT = 1 << 26,

    /// Final state when exiting the node. This allows a view to have a different state at entering and exiting,
    /// in case the node implementation needs to perform a transition but does not want to transition it back to the
    /// original state. Not implemented yet.
    RPS_ACCESS_AFTER_BIT = 1 << 27,

    /// View is cleared before the current access. Usually used together with other basic access flags.
    RPS_ACCESS_CLEAR_BIT = 1 << 28,

    /// Access can be used by a render pass attachment (as render target or depth stencil). Used to distinguish clear-only
    /// accesses (which may use special clear commands) and render target / depth stencil view accesses.
    RPS_ACCESS_RENDER_PASS = 1 << 29,

    /// Access does not care about the ordering with regard to other accesses which also have the
    /// RPS_ACCESS_RELAXED_ORDER_BIT flag.
    RPS_ACCESS_RELAXED_ORDER_BIT = 1 << 30,

    /// Access does not need a resource view to be created, (e.g. via
    /// ID3D12GraphicsCommandList::CopyResource).
    RPS_ACCESS_NO_VIEW_BIT = 1 << 31,

    // Aliases

    /// Accessible as a predication buffer.
    RPS_ACCESS_PREDICATION_BIT = RPS_ACCESS_INDIRECT_ARGS_BIT,

    /// Depth read write access.
    RPS_ACCESS_DEPTH = RPS_ACCESS_DEPTH_READ_BIT | RPS_ACCESS_DEPTH_WRITE_BIT,

    /// Stencil read write access.
    RPS_ACCESS_STENCIL = RPS_ACCESS_STENCIL_READ_BIT | RPS_ACCESS_STENCIL_WRITE_BIT,

    /// Depth / Stencil read access.
    RPS_ACCESS_DEPTH_STENCIL_READ = RPS_ACCESS_DEPTH_READ_BIT | RPS_ACCESS_STENCIL_READ_BIT,

    /// Depth / Stencil write access.
    RPS_ACCESS_DEPTH_STENCIL_WRITE = RPS_ACCESS_DEPTH_WRITE_BIT | RPS_ACCESS_STENCIL_WRITE_BIT,

    /// Depth / Stencil read write access.
    RPS_ACCESS_DEPTH_STENCIL = RPS_ACCESS_DEPTH_STENCIL_READ | RPS_ACCESS_DEPTH_STENCIL_WRITE,

    /// Bitwise OR of all possible GPU writeable access flags.
    RPS_ACCESS_ALL_GPU_WRITE = RPS_ACCESS_RENDER_TARGET_BIT | RPS_ACCESS_UNORDERED_ACCESS_BIT |
                               RPS_ACCESS_DEPTH_WRITE_BIT | RPS_ACCESS_STENCIL_WRITE_BIT | RPS_ACCESS_STREAM_OUT_BIT |
                               RPS_ACCESS_COPY_DEST_BIT | RPS_ACCESS_RESOLVE_DEST_BIT |
                               RPS_ACCESS_RAYTRACING_AS_BUILD_BIT,

    /// Bitwise OR of all possible GPU readonly access flags.
    RPS_ACCESS_ALL_GPU_READONLY =
        RPS_ACCESS_INDIRECT_ARGS_BIT | RPS_ACCESS_INDEX_BUFFER_BIT | RPS_ACCESS_VERTEX_BUFFER_BIT |
        RPS_ACCESS_CONSTANT_BUFFER_BIT | RPS_ACCESS_SHADER_RESOURCE_BIT | RPS_ACCESS_SHADING_RATE_BIT |
        RPS_ACCESS_DEPTH_READ_BIT | RPS_ACCESS_STENCIL_READ_BIT | RPS_ACCESS_COPY_SRC_BIT | RPS_ACCESS_RESOLVE_SRC_BIT |
        RPS_ACCESS_RAYTRACING_AS_READ_BIT | RPS_ACCESS_PRESENT_BIT,

    /// Bitwise OR of all possible GPU access flags.
    RPS_ACCESS_ALL_GPU = RPS_ACCESS_ALL_GPU_WRITE | RPS_ACCESS_ALL_GPU_READONLY,

    /// Bitwise OR of all possible CPU access flags.
    RPS_ACCESS_ALL_CPU = RPS_ACCESS_CPU_READ_BIT | RPS_ACCESS_CPU_WRITE_BIT,

    /// Bitwise OR of all GPU / CPU access, excluding decorator flags such as RPS_ACCESS_RELAXED_ORDER_BIT and RPS_ACCESS_NO_VIEW_BIT.
    RPS_ACCESS_ALL_ACCESS_MASK = RPS_ACCESS_ALL_GPU | RPS_ACCESS_ALL_CPU,

} RpsAccessFlagBits;

/// @brief Bitmask type for <c><i>RpsAccessFlagBits</i></c>.
typedef RpsFlags32 RpsAccessFlags;

/// @brief Bitflags for shader stages.
typedef enum RpsShaderStageBits
{
    RPS_SHADER_STAGE_NONE       = 0,              ///< No shader stages.
    RPS_SHADER_STAGE_VS         = 1 << 0,         ///< Vertex shader stage.
    RPS_SHADER_STAGE_PS         = 1 << 1,         ///< Pixel shader stage.
    RPS_SHADER_STAGE_GS         = 1 << 2,         ///< Geometry shader stage.
    RPS_SHADER_STAGE_CS         = 1 << 3,         ///< Compute shader stage.
    RPS_SHADER_STAGE_HS         = 1 << 4,         ///< Hull shader stage.
    RPS_SHADER_STAGE_DS         = 1 << 5,         ///< Domain shader stage.
    RPS_SHADER_STAGE_RAYTRACING = 1 << 6,         ///< Raytracing shader stage.
    RPS_SHADER_STAGE_AS         = 1 << 7,         ///< Amplification shader stage.
    RPS_SHADER_STAGE_MS         = 1 << 8,         ///< Mesh shader stage.
    RPS_SHADER_STAGE_ALL        = (1u << 9) - 1,  ///< All shader stages.
} RpsShaderStageBits;

/// @brief Bitmask type for <c><i>RpsShaderStageBits</i></c>.
typedef RpsFlags32 RpsShaderStageFlags;

/// @brief Resource access attribute.
typedef struct RpsAccessAttr
{
    RpsAccessFlags      accessFlags;   ///< Access flags.
    RpsShaderStageFlags accessStages;  ///< Shader stages allowed for access if applicable.
} RpsAccessAttr;

/// @} end defgroup RpsAccessAttr

/// @defgroup RpsSemanticAttr RpsSemanticAttr
/// @{

/// @brief Graphics resource and argument data usage semantics.
typedef enum RpsSemantic
{
    RPS_SEMANTIC_UNSPECIFIED = 0,  ///< No semantics.

    // Shaders:
    RPS_SEMANTIC_VERTEX_SHADER,         ///< Reserved for future use.
    RPS_SEMANTIC_PIXEL_SHADER,          ///< Reserved for future use.
    RPS_SEMANTIC_GEOMETRY_SHADER,       ///< Reserved for future use.
    RPS_SEMANTIC_COMPUTE_SHADER,        ///< Reserved for future use.
    RPS_SEMANTIC_HULL_SHADER,           ///< Reserved for future use.
    RPS_SEMANTIC_DOMAIN_SHADER,         ///< Reserved for future use.
    RPS_SEMANTIC_RAYTRACING_PIPELINE,   ///< Reserved for future use.
    RPS_SEMANTIC_AMPLIFICATION_SHADER,  ///< Reserved for future use.
    RPS_SEMANTIC_MESH_SHADER,           ///< Reserved for future use.

    // States:
    RPS_SEMANTIC_VERTEX_LAYOUT,        ///< Reserved for future use.
    RPS_SEMANTIC_STREAM_OUT_LAYOUT,    ///< Reserved for future use.
    RPS_SEMANTIC_STREAM_OUT_DESC,      ///< Reserved for future use.
    RPS_SEMANTIC_BLEND_STATE,          ///< Reserved for future use.
    RPS_SEMANTIC_RENDER_TARGET_BLEND,  ///< Reserved for future use.
    RPS_SEMANTIC_DEPTH_STENCIL_STATE,  ///< Reserved for future use.
    RPS_SEMANTIC_RASTERIZER_STATE,     ///< Reserved for future use.
    RPS_SEMANTIC_DYNAMIC_STATE_BEGIN,  ///< Start of the dynamic state semantic enumeration values.

    /// Usage as a viewport. The data type must be <c><i>RpsViewport</i></c>.
    RPS_SEMANTIC_VIEWPORT = RPS_SEMANTIC_DYNAMIC_STATE_BEGIN,

    /// Usage as a scissor rectangle. The data type must be <c><i>RpsRect</i></c>.
    RPS_SEMANTIC_SCISSOR,

    /// Usage as primitive topology. The data must be one of the values specified by <c><i>RpsPrimitiveTopology</i></c>.
    RPS_SEMANTIC_PRIMITIVE_TOPOLOGY,

    /// Reserved for future use.
    RPS_SEMANTIC_PATCH_CONTROL_POINTS,

    /// Reserved for future use.
    RPS_SEMANTIC_PRIMITIVE_STRIP_CUT_INDEX,

    /// Reserved for future use.
    RPS_SEMANTIC_BLEND_FACTOR,

    /// Reserved for future use.
    RPS_SEMANTIC_STENCIL_REF,

    /// Reserved for future use.
    RPS_SEMANTIC_DEPTH_BOUNDS,

    /// Reserved for future use.
    RPS_SEMANTIC_SAMPLE_LOCATION,

    /// Reserved for future use.
    RPS_SEMANTIC_SHADING_RATE,

    /// Usage as a color clear value. The data type must be float[4].
    RPS_SEMANTIC_COLOR_CLEAR_VALUE,

    /// Usage as a depth clear value. The data type must be float.
    RPS_SEMANTIC_DEPTH_CLEAR_VALUE,

    /// Usage as a stencil clear value. The data type must be uint32_t, only the lower 8 bit will be used.
    RPS_SEMANTIC_STENCIL_CLEAR_VALUE,

    // Resource bindings:

    /// Start of the resource binding enumeration values.
    RPS_SEMANTIC_RESOURCE_BINDING_BEGIN,

    /// Bound as a vertex buffer. The semantic index indicates the vertex buffer binding slot.
    RPS_SEMANTIC_VERTEX_BUFFER = RPS_SEMANTIC_RESOURCE_BINDING_BEGIN,

    /// Bound as an index buffer.
    RPS_SEMANTIC_INDEX_BUFFER,

    /// Bound as an indirect argument buffer.
    RPS_SEMANTIC_INDIRECT_ARGS,

    /// Bound as an indirect count buffer.
    RPS_SEMANTIC_STREAM_OUT_BUFFER,

    /// Bound for write as a stream out buffer. The semantic index indicates the stream out buffer binding slot.
    RPS_SEMANTIC_INDIRECT_COUNT,

    /// Bound as a render target view. The semantic index indicates the render target slot.
    RPS_SEMANTIC_RENDER_TARGET,

    /// Bound as a depth stencil view.
    RPS_SEMANTIC_DEPTH_STENCIL_TARGET,

    /// Bound as a shading rate image in a Variable Rate Shading (VRS) pass.
    RPS_SEMANTIC_SHADING_RATE_IMAGE,

    /// Bound as a resolve target. The semantic index indicates the render
    /// target slot of the resolve source.
    RPS_SEMANTIC_RESOLVE_TARGET,

    /// User defined resource view binding. This is intended for shader resource views and unordered access views where
    /// resources are bound to programmable shaders instead of fixed function binding points.
    RPS_SEMANTIC_USER_RESOURCE_BINDING,

    // !! KEEP RPS_SEMANTIC_USER_RESOURCE_BINDING THE LAST ELEMENT !!

    RPS_SEMANTIC_COUNT,  ///< Number of defined semantics.
} RpsSemantic;

/// @brief Constant for a semantic index value indicating the actual semantic index should occur immediately following
/// the previously defined semantic of the same type.
#define RPS_SEMANTIC_INDEX_APPEND (UINT32_MAX)

/// @brief Graph entry and node parameter semantic attribute.
typedef struct RpsSemanticAttr
{
    RpsSemantic semantic;    //< Semantic type.
    uint32_t semanticIndex;  //< Index of the semantic if required. The meaning of the semantic index differs depending
                             //  on the semantic. See <c><i>RpsSemantic</i></c> for more info.
} RpsSemanticAttr;

/// @} end defgroup RpsSemanticAttr

/// @defgroup RpsResourceView RpsResourceView
/// @{

/// @brief Bitflags for resource view properties.
typedef enum RpsResourceViewFlagBits
{
    RPS_RESOURCE_VIEW_FLAG_NONE        = 0,       ///< No special resource view flags.
    RPS_RESOURCE_VIEW_FLAG_CUBEMAP_BIT = 1 << 0,  ///< Resource view is used as a cubemap.
} RpsResourceViewFlagBits;

/// @brief Bitmask for <c><i>RpsResourceViewFlagBits</i></c>.
typedef RpsFlags32 RpsResourceViewFlags;

/// @brief General resource view.
///
/// All shared parameters between buffer and image views.
typedef struct RpsResourceView
{
    RpsResourceId resourceId;  ///< Resource ID of the viewed resource. RPS_RESOURCE_ID_INVALID indicates a null
                               ///  resource view.
    RpsFormat viewFormat;      ///< Format of the view. RPS_FORMAT_UNKNOWN indicates the format should be inherited from
                               ///  the resource or the view does not require a format (e.g. structured buffers).
    uint32_t             temporalLayer;  ///< Temporal layer of the view. 0 means the current frame,
                                         ///  1 means the previous frame, 2 means two frames before, etc.
    RpsResourceViewFlags flags;          ///< Flags for additional view properties.
} RpsResourceView;

/// @brief Image resource view.
typedef struct RpsImageView
{
    RpsResourceView     base;              ///< Base resource view properties.
    RpsSubresourceRange subresourceRange;  ///< Subresource range of the view.
    float               minLodClamp;       ///< Min LOD clamp value of the texture view.
    uint32_t            componentMapping;  ///< 32-bit value for the color component (RGBA channel) mapping of the view.
} RpsImageView;

/// @defgroup RpsResourceViewComponentMapping RpsResourceViewComponentMapping
/// @{

/// @brief Resource components or value to map to for component mapping.
typedef enum RpsResourceViewComponentMapping
{
    RPS_RESOURCE_VIEW_COMPONENT_MAPPING_R    = 0,  ///< Red component.
    RPS_RESOURCE_VIEW_COMPONENT_MAPPING_G    = 1,  ///< Green component.
    RPS_RESOURCE_VIEW_COMPONENT_MAPPING_B    = 2,  ///< Blue component.
    RPS_RESOURCE_VIEW_COMPONENT_MAPPING_A    = 3,  ///< Alpha component
    RPS_RESOURCE_VIEW_COMPONENT_MAPPING_ZERO = 4,  ///< Constant value of 0.
    RPS_RESOURCE_VIEW_COMPONENT_MAPPING_ONE  = 5,  ///< Constant value of 1.

    ///  Specifies the default component mapping (where R, G, B, A components map to R, G, B, A without swizzling).
    ///  Note this is using 1 byte each channel, different from DX12 default.
    RPS_RESOURCE_VIEW_COMPONENT_MAPPING_DEFAULT =
        RPS_RESOURCE_VIEW_COMPONENT_MAPPING_R | (RPS_RESOURCE_VIEW_COMPONENT_MAPPING_G << 8) |
        (RPS_RESOURCE_VIEW_COMPONENT_MAPPING_B << 16) | (RPS_RESOURCE_VIEW_COMPONENT_MAPPING_A << 24),
} RpsResourceViewComponentMapping;

/// @brief Macro for encoding a set of component mappings as a 32-bit color value.
///
/// @param R                            Red channel value.
/// @param G                            Green channel value.
/// @param B                            Blue channel value.
/// @param A                            Alpha channel value.
///
/// @returns                            Encoded 32-bit value.
#define RPS_IMAGE_VIEW_MAKE_COMPONENT_MAPPING(R, G, B, A) \
    (((R)&0xFF) | (((G)&0xFF) << 8) | (((B)&0xFF) << 16) | (((A)&0xFF) << 24))

/// @brief Macro for decoding the red channel of a 32-bit component mapping encoded in the layout defined by
/// RPS_IMAGE_VIEW_MAKE_COMPONENT_MAPPING.
///
/// @param Value                        Encoded 32-bit value.
///
/// returns                             Decoded red channel.
#define RPS_IMAGE_VIEW_GET_COMPONENT_MAPPING_CHANNEL_R(Value) ((RpsResourceViewComponentMapping)((Value)&0xFF))
/// @brief Macro for decoding the green channel of a 32-bit component mapping encoded in the layout defined by
/// RPS_IMAGE_VIEW_MAKE_COMPONENT_MAPPING..
///
/// @param Value                        Encoded 32-bit value.
///
/// returns                             Decoded green channel.
#define RPS_IMAGE_VIEW_GET_COMPONENT_MAPPING_CHANNEL_G(Value) ((RpsResourceViewComponentMapping)(((Value) >> 8) & 0xFF))
/// @brief Macro for decoding the blue channel of a 32-bit component mapping encoded in the layout defined by
/// RPS_IMAGE_VIEW_MAKE_COMPONENT_MAPPING..
///
/// @param Value                        Encoded 32-bit value.
///
/// returns                             Decoded blue channel.
#define RPS_IMAGE_VIEW_GET_COMPONENT_MAPPING_CHANNEL_B(Value) \
    ((RpsResourceViewComponentMapping)(((Value) >> 16) & 0xFF))
/// @brief Macro for decoding the alpha channel of a 32-bit component mapping encoded in the layout defined by
/// RPS_IMAGE_VIEW_MAKE_COMPONENT_MAPPING.
///
/// @param Value                        Encoded 32-bit value.
///
/// returns                             Decoded alpha channel.
#define RPS_IMAGE_VIEW_GET_COMPONENT_MAPPING_CHANNEL_A(Value) \
    ((RpsResourceViewComponentMapping)(((Value) >> 24) & 0xFF))
/// @brief @brief Macro for decoding a channel of a 32-bit component mapping encoded in the layout defined by
/// RPS_IMAGE_VIEW_MAKE_COMPONENT_MAPPING.
///
/// @param Value                        Encoded 32-bit value.
/// @param Channel                      Channel to decode. Has to be either R, G B or A.
///
/// returns                             Decoded red channel.
#define RPS_IMAGE_VIEW_GET_COMPONENT_MAPPING_CHANNEL(Value, Channel) \
    RPS_CONCATENATE_DIRECT(RPS_IMAGE_VIEW_GET_COMPONENT_MAPPING_CHANNEL_, Channel)(Value)

/// @} end defgrouop RpsResourceViewComponentMapping

/// @brief Constant for a buffer range value indicating the entire remaining size of the buffer.
#define RPS_BUFFER_WHOLE_SIZE UINT64_MAX

/// @brief Buffer resource view.
typedef struct RpsBufferView
{
    RpsResourceView base;         ///< Base view properties.
    uint64_t        offset;       ///< Offset of the buffer range in bytes.
    uint64_t        sizeInBytes;  ///< Size of the buffer range in bytes.
    uint32_t        stride;       ///< Stride of a structured buffer view. If the API does not support altering per-view
                                  ///  buffer stride ( e.g. DX11), the stride applies to the whole buffer resource.
} RpsBufferView;

/// @brief Enumeration of runtime defined built-in type IDs.
typedef enum RpsRuntimeBuiltInTypeIds
{
    RPS_TYPE_IMAGE_VIEW = RPS_TYPE_RUNTIME_DEFINED_BEGIN,   ///< Type ID of RpsImageView.
    RPS_TYPE_BUFFER_VIEW,                                   ///< Type ID of RpsBufferView.
} RpsRuntimeBuiltInTypeIds;

/// @} end defgroup RpsResourceView4758

#ifndef RPSL_COMPILER_BUILD

#ifdef __cplusplus

namespace rps
{
    /// @brief C++ helper type for RpsAccessAttr.
    ///
    /// @ingroup RpsAccessAttr
    struct AccessAttr : public RpsAccessAttr
    {
        constexpr AccessAttr(RpsAccessFlags      accessFlags  = RPS_ACCESS_UNKNOWN,
                             RpsShaderStageFlags shaderStages = RPS_SHADER_STAGE_NONE)
            : RpsAccessAttr{accessFlags, shaderStages}
        {
        }
        constexpr AccessAttr(const RpsAccessAttr& attr)
            : RpsAccessAttr{attr}
        {
        }

        AccessAttr& operator|=(const AccessAttr& rhs);
        AccessAttr& operator&=(const AccessAttr& rhs);

        void Print(const RpsPrinter& printer) const;
    };

    /// @brief C++ helper type for RpsSemanticAttr.
    struct SemanticAttr : public RpsSemanticAttr
    {
        constexpr SemanticAttr(RpsSemantic semantic, uint32_t semanticIndex = 0)
            : RpsSemanticAttr{semantic, semanticIndex}
        {
        }
        constexpr SemanticAttr(const RpsSemanticAttr& attr)
            : RpsSemanticAttr{attr}
        {
        }

        void Print(const RpsPrinter& printer) const;
    };

    /// @addtogroup RpsAccessAttr
    /// @{

    /// @brief Per field bitwise-OR operator for RpsAccessAttr.
    inline RpsAccessAttr operator|(const RpsAccessAttr& lhs, const RpsAccessAttr& rhs)
    {
        return RpsAccessAttr{lhs.accessFlags | rhs.accessFlags, lhs.accessStages | rhs.accessStages};
    }

    /// @brief Per field bitwise-AND operator for RpsAccessAttr.
    inline RpsAccessAttr operator&(const RpsAccessAttr& lhs, const RpsAccessAttr& rhs)
    {
        return RpsAccessAttr{lhs.accessFlags & rhs.accessFlags, lhs.accessStages & rhs.accessStages};
    }

    /// @brief Returns if two RpsAccessAttr structures are equal.
    inline bool operator==(const RpsAccessAttr& lhs, const RpsAccessAttr& rhs)
    {
        return (lhs.accessFlags == rhs.accessFlags) && (lhs.accessStages == rhs.accessStages);
    }

    /// @brief Returns if two RpsAccessAttr structures are not equal.
    inline bool operator!=(const RpsAccessAttr& lhs, const RpsAccessAttr& rhs)
    {
        return !(lhs == rhs);
    }

    inline AccessAttr& AccessAttr::operator|=(const AccessAttr& rhs)
    {
        *this = *this | rhs;
        return *this;
    }

    inline AccessAttr& AccessAttr::operator&=(const AccessAttr& rhs)
    {
        *this = *this & rhs;
        return *this;
    }

    /// @} end addtogroup RpsAccessAttr

    /// @addtogroup RpsResourceView
    /// @{

    /// @brief C++ helper type for RpsImageView.
    struct ImageView : public RpsImageView
    {
        ImageView()
            : ImageView(RPS_RESOURCE_ID_INVALID)
        {
        }

        ImageView(RpsResourceId        inResId,
                  RpsFormat            inFormat        = RPS_FORMAT_UNKNOWN,
                  uint32_t             inTemporalLayer = 0,
                  RpsResourceViewFlags inFlags         = RPS_RESOURCE_VIEW_FLAG_NONE,
                  SubresourceRange     inSubResRange   = {})
        {
            base.resourceId    = inResId;
            base.viewFormat    = inFormat;
            base.temporalLayer = inTemporalLayer;
            base.flags         = inFlags;
            subresourceRange   = inSubResRange;
            minLodClamp        = 0.0f;
            componentMapping   = RPS_RESOURCE_VIEW_COMPONENT_MAPPING_DEFAULT;
        }
    };

    /// @brief C++ helper type for RpsBufferView.
    struct BufferView : public RpsBufferView
    {
        BufferView()
            : BufferView(RPS_RESOURCE_ID_INVALID)
        {
        }

        BufferView(RpsResourceId inResId,
                   RpsFormat     inFormat        = RPS_FORMAT_UNKNOWN,
                   uint64_t      inOffset        = 0,
                   uint64_t      inSizeInBytes   = UINT64_MAX,
                   uint16_t      inStride        = 0,
                   uint32_t      inTemporalLayer = 0)
        {
            base.resourceId    = inResId;
            base.viewFormat    = inFormat;
            base.temporalLayer = inTemporalLayer;
            base.flags         = RPS_RESOURCE_VIEW_FLAG_NONE;
            offset             = inOffset;
            sizeInBytes        = inSizeInBytes;
            stride             = inStride;
        }
    };

    /// @} end addtogroup RpsResourceView

}  // namespace rps

#endif  //__cplusplus

#endif  //RPSL_COMPILER_BUILD

/// @} end addtogroup RpsRenderGraphRuntime

#endif  //RPS_ACCESS_H
