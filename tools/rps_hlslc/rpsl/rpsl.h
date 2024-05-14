// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#error "This header file is not for include. It is reproduced here for documentation purposes."

typedef struct keyword keyword;
typedef struct AccessAttribute AccessAttribute;
typedef struct AccessAttributeArgument AccessAttributeArgument;
typedef struct SubgraphAttribute SubgraphAttribute;

/// @defgroup RPSL RPSL Language Extension
/// @{

/// @defgroup node Node Declaration
/// @{

/// @brief A opaque handle type used to declare a node.
/// A function declaration return a node handle is a node declaration.
typedef struct node node;

/// A keyword which declares the minimum queue family required for a node
/// must support graphics operations.
/// By default a node is a graphics node.
///
/// Example usage: `graphics node foo();`
/// which is equivalent to: `node foo();`
keyword graphics;

/// A keyword which declares the minimum queue family required for a node
/// must support compute operations.
///
/// Example usage: `compute node foo();`
keyword compute;

/// A keyword which declares the minimum queue family required for a node
/// must support copy (transfer) operations.
///
/// Example usage: `copy node foo();`
keyword copy;

/// A keyword that can be used before a node call as a hint that this node
/// is preferred to execute asynchronously on a queue differ from the main
/// graphics queue.
/// 
/// The scheduler is free to ignore this hint if it sees fit.
/// 
/// Example usage: `async Foo();`
keyword async;

/// A keyword indicating a null view.
/// A null view can be used in place of a texture or a buffer object, or to initialize
/// a texture or a buffer variable. The fields are initialized based on below rules:
/// - The <i>Resource</i> field is initialized to RPS_RESOURCE_ID_INVALID (UINT32_MAX)
/// - All other fields are zeroed.
/// Example usage:
/// `texture t = null;`
/// `buffer b = null;`
/// `// node foo(srv optionalSrv, rtv requiredRtv : SV_Target0);`
/// `foo(null, myRT);`
keyword null;

/// @defgroup Access Access Attributes
/// @{

/// @brief Attribute indicating an output parameter.
///
/// When applied on RenderGraph entry point parameters, it indicates the argument
/// value will be output from the RPSL execution during RenderGraph update.
/// If the parameter is a resource (with type 'texture' or 'buffer'), the resource
/// handle can be queried after RenderGraph update and used externally.
///
/// Example usage: `export rpsl_main([readonly(ps)] texture input, out [readonly(cs)] texture output);`
AccessAttribute out;

/// A base access attribute used when declaring the signatures of nodes in RPSL.
///
/// The readonly access attribute can access a comma separated list of access
/// attributes. Specifying this list, allows the RPSL program to more tightly
/// specify the type of access that the node will perform on the resource view
/// being passed to the node.
///
/// readonly specifies that the node can read the resource but will never
/// write to it.
///
/// When applied to render graph entry point parameters (including "out" parameters), it refers to external accesses
/// outside of the execution of the current render graph. This means RPS expects the resource to be in this access state
/// when it enters the render graph and will transit it to this state before exiting the render graph.
///
/// Example usage: `[readonly(ps, cs)] texture myShaderResourceView`
AccessAttribute readonly(x);

/// A base access attribute used when declaring the signatures of nodes in RPSL.
///
/// The writeonly access attribute can access a comma separated list of access
/// attributes. Specifying this list, allows the RPSL program to more tightly
/// specify the type of access that the node will perform on the resource view
/// being passed to the node.
///
/// writeonly specifies that the node can write to the resource and will not read
/// from it. This implies the previous data can be discarded, and is equivalent to
/// `[readwrite(discard_before, ...)]`.
///
/// When applied to render graph entry point parameters (including "out" parameters), it refers to external accesses
/// outside of the execution of the current render graph. This means RPS expects the resource to be in this access state
/// when it enters the render graph and will transit it to this state before exiting the render graph.
///
/// Example usage: `[writeonly(rendertarget)] texture myDiscardRenderTargetView`
AccessAttribute writeonly(x);

/// A base access attribute used when declaring the signatures of nodes in RPSL.
///
/// The readwrite access attribute can access a comma separated list of access
/// attributes. Specifying this list, allows the RPSL program to more tightly
/// specify the type of access that the node will perform on the resource view
/// being passed to the node.
///
/// readwrite specifies that the node can both read and write the resource.
///
/// When applied to render graph entry point parameters (including "out" parameters), it refers to external accesses
/// outside of the execution of the current render graph. This means RPS expects the resource to be in this access state
/// when it enters the render graph and will transit it to this state before exiting the render graph.
///
/// Example usage: `[readwrite(rendertarget)] texture myRenderTargetView`
AccessAttribute readwrite(x);

/// An access attribute used when declaring the signatures of nodes in RPSL.
///
/// Indicates the node access to this resource can be reordered w.r.t. other nodes
/// writing to an overlapping resource view that also has the relaxed attribute.
///
/// Example usage: `[relaxed][readwrite(cs)] texture myUAV`
AccessAttribute relaxed;

/// Reserved attribute name.
AccessAttribute before;

/// Reserved attribute name.
AccessAttribute after;

/// An access attribute argument indicating the resource view can be 
/// accessed by the vertex shader stage of the graphics pipeline.
///
/// Usually when applied to the readonly attribute, it indicates a shader resource view.
/// When applied to the readwrite / writeonly attribute, it indicates an unordered access view.
AccessAttributeArgument vs;

/// An access attribute argument indicating the resource view can be 
/// accessed by the pixel shader stage of the graphics pipeline.
AccessAttributeArgument ps;

/// An access attribute argument indicating the resource view can be 
/// accessed by the compute shader stage of the graphics pipeline.
AccessAttributeArgument cs;

/// An access attribute argument indicating the resource view can be 
/// accessed by the geometry shader stage of the graphics pipeline.
AccessAttributeArgument gs;

/// An access attribute argument indicating the resource view can be 
/// accessed by the hull shader stage of the graphics pipeline.
AccessAttributeArgument hs;

/// An access attribute argument indicating the resource view can be 
/// accessed by the domain shader stage of the graphics pipeline.
AccessAttributeArgument ds;

/// An access attribute argument indicating the resource view can be 
/// accessed by the amplification (task) shader stage of the graphics pipeline.
AccessAttributeArgument ts;

/// An access attribute argument indicating the resource view can be 
/// accessed by the mesh shader stage of the graphics pipeline.
AccessAttributeArgument ms;

/// An access attribute argument indicating the resource view can be
/// accessed by the raytracing shader stage of the graphics pipeline.
AccessAttributeArgument raytracing;

/// An access attribute argument indicating the resource view will
/// be accessed as a render target.
/// 
/// This is only valid when used with <c>writeonly</c> or <c>readwrite</c>.
AccessAttributeArgument rendertarget;

/// An access attribute argument indicating the resource view will
/// be accessed as a depth buffer.
AccessAttributeArgument depth;

/// An access attribute argument indicating the resource view will
/// be accessed as a stencil buffer.
AccessAttributeArgument stencil;

/// An access attribute argument indicating the resource view will
/// be accessed as a copy source (when used with the <c>readonly</c> attribute)
/// or destination (when used with the <c>writeonly</c> or <c>readwrite</c> attributes).
AccessAttributeArgument copy;

/// An access attribute argument indicating the resource view will
/// be accessed as a resolve source (when used with the <c>readonly</c> attribute)
/// or destination (when used with the <c>writeonly</c> or <c>readwrite</c> attributes).
AccessAttributeArgument resolve;

/// An access attribute argument indicating the resource view will
/// be accessed as present source.
/// 
/// This is only valid when used with <c>readonly</c>.
AccessAttributeArgument present;

/// An access attribute argument indicating the resource view will
/// be accessed from the CPU.
AccessAttributeArgument cpu;

/// An access attribute argument indicating the resource view will
/// be accessed accessed from the GPU frontend as a set of indirect arguments.
/// 
/// This is only valid when used with <c>readonly</c>.
AccessAttributeArgument indirectargs;

/// An access attribute argument indicating the resource view can
/// be accessed as a vertex buffer.
AccessAttributeArgument vb;

/// An access attribute argument indicating the resource view can
/// be accessed as an index buffer.
AccessAttributeArgument ib;

/// An access attribute argument indicating the resource view can
/// be accessed as a constant buffer.
AccessAttributeArgument cb;

/// An access attribute argument indicating the resource view can
/// be accessed as a VRS shading rate image.
/// 
/// This is only valid when used with <c>readonly</c>.
AccessAttributeArgument shadingrate;

/// An access attribute argument indicating the resource view can
/// be accessed as a GPU predication buffer.
/// 
/// This is only valid when used with <c>readonly</c>.
AccessAttributeArgument predication;

/// An access attribute argument indicating the resource view can
/// be accessed as a stream out buffer.
/// 
/// This is only valid when used with <c>writeonly</c> or <c>readwrite</c>.
AccessAttributeArgument streamout;

/// An access attribute argument indicating the resource view can
/// be accessed as a raytracing acceleration structure. Only applies to buffers.
AccessAttributeArgument rtas;

/// An access attribute argument indicating the resource view can
/// be accessed as a cubemap view.
AccessAttributeArgument cubemap;

/// An access attribute argument indicating the resource view is cleared before
/// the current access. Usually used together with other access args.
/// Implied if the node has a parameter with a clear value semantic (e.g.
/// SV_ClearColor, SV_ClearDepth) which matches the resource binding slot specified
/// by the semantic of the current argument.
AccessAttributeArgument clear;

/// An access attribute argument indicating existing data can
/// be discarded before the current node.
///
/// If the same subresource is accessed by multiple params of the same node,
/// it can be discarded only if all of these params have the discard_before flag.
AccessAttributeArgument discard_before;

/// An access attribute argument indicating data can be discarded
/// after the current node.
///
/// Usually, the runtime automatically determines if the subresource data can be
/// discarded after a node by checking if it may be read later on. (For example,
/// if the current access is the last access of a transient resource in a frame,
/// or if the next access is a full subresource overwrite.). As a result, this
/// argument typically doesn't need to be specified explicitly. It is provided
/// to allow forcing this behavior for debugging and testing purposes.
///
/// If the same subresource is accessed by multiple params of the same node,
/// it can be discarded after only if all of these params have the discard_after flag.
AccessAttributeArgument discard_after;

/// A view of a render target texture.
///
/// This define is a shortcut for using the underlying access attributes
/// directly.
#define rtv             [readwrite(rendertarget)] texture

/// A view of a depth/stencil texture.
///
/// This define is a shortcut for using the underlying access attributes
/// directly.
#define dsv             [readwrite(depth, stencil)] texture

/// A view of a render target resource that can be discarded before writing.
///
/// This define is a shortcut for using the underlying access attributes
/// directly.
#define discard_rtv     [writeonly(rendertarget)] texture 

/// A texture shader resource view accessible from both pixel shaders and
/// non-pixel shaders.
/// 
/// This define is a shortcut for using the underlying access attributes
/// directly.
#define srv             [readonly(ps, cs)] texture

/// A buffer shader resource view accessible from both pixel shaders and 
/// non-pixel shaders.
/// 
/// This define is a shortcut for using the underlying access attributes
/// directly.
#define srv_buf         [readonly(ps, cs)] buffer

/// A texture shader resource view that can only be accessed by pixel shaders.
/// 
/// This define is a shortcut for using the underlying access attributes
/// directly.
#define ps_srv          [readonly(ps)] texture

/// A buffer shader resource view that can only be accessed by pixel shaders.
/// 
/// This define is a shortcut for using the underlying access attributes
/// directly.
#define ps_srv_buf      [readonly(ps)] buffer

/// An unordered access texture resource view.
///
/// This define is a shortcut for using the underlying access attributes
/// directly.
#define uav             [readwrite(ps, cs)] texture

/// An unordered access buffer resource view.
///
/// This define is a shortcut for using the underlying access attributes
/// directly.
#define uav_buf         [readwrite(ps, cs)] buffer

/// @} end defgroup access
/// @} end defgroup node

/// @defgroup Resource Resource Declarations
/// @{

/// An enumeration of available format values for resources and resource views.
/// see @ref RpsFormat.
typedef enum RPS_FORMAT
{
    RPS_FORMAT_UNKNOWN,
    RPS_FORMAT_R32G32B32A32_TYPELESS,
    RPS_FORMAT_R32G32B32A32_FLOAT,
    RPS_FORMAT_R32G32B32A32_UINT,
    RPS_FORMAT_R32G32B32A32_SINT,
    RPS_FORMAT_R32G32B32_TYPELESS,
    RPS_FORMAT_R32G32B32_FLOAT,
    RPS_FORMAT_R32G32B32_UINT,
    RPS_FORMAT_R32G32B32_SINT,
    RPS_FORMAT_R16G16B16A16_TYPELESS,
    RPS_FORMAT_R16G16B16A16_FLOAT,
    RPS_FORMAT_R16G16B16A16_UNORM,
    RPS_FORMAT_R16G16B16A16_UINT,
    RPS_FORMAT_R16G16B16A16_SNORM,
    RPS_FORMAT_R16G16B16A16_SINT,
    RPS_FORMAT_R32G32_TYPELESS,
    RPS_FORMAT_R32G32_FLOAT,
    RPS_FORMAT_R32G32_UINT,
    RPS_FORMAT_R32G32_SINT,
    RPS_FORMAT_R32G8X24_TYPELESS,
    RPS_FORMAT_D32_FLOAT_S8X24_UINT,
    RPS_FORMAT_R32_FLOAT_X8X24_TYPELESS,
    RPS_FORMAT_X32_TYPELESS_G8X24_UINT,
    RPS_FORMAT_R10G10B10A2_TYPELESS,
    RPS_FORMAT_R10G10B10A2_UNORM,
    RPS_FORMAT_R10G10B10A2_UINT,
    RPS_FORMAT_R11G11B10_FLOAT,
    RPS_FORMAT_R8G8B8A8_TYPELESS,
    RPS_FORMAT_R8G8B8A8_UNORM,
    RPS_FORMAT_R8G8B8A8_UNORM_SRGB,
    RPS_FORMAT_R8G8B8A8_UINT,
    RPS_FORMAT_R8G8B8A8_SNORM,
    RPS_FORMAT_R8G8B8A8_SINT,
    RPS_FORMAT_R16G16_TYPELESS,
    RPS_FORMAT_R16G16_FLOAT,
    RPS_FORMAT_R16G16_UNORM,
    RPS_FORMAT_R16G16_UINT,
    RPS_FORMAT_R16G16_SNORM,
    RPS_FORMAT_R16G16_SINT,
    RPS_FORMAT_R32_TYPELESS,
    RPS_FORMAT_D32_FLOAT,
    RPS_FORMAT_R32_FLOAT,
    RPS_FORMAT_R32_UINT,
    RPS_FORMAT_R32_SINT,
    RPS_FORMAT_R24G8_TYPELESS,
    RPS_FORMAT_D24_UNORM_S8_UINT,
    RPS_FORMAT_R24_UNORM_X8_TYPELESS,
    RPS_FORMAT_X24_TYPELESS_G8_UINT,
    RPS_FORMAT_R8G8_TYPELESS,
    RPS_FORMAT_R8G8_UNORM,
    RPS_FORMAT_R8G8_UINT,
    RPS_FORMAT_R8G8_SNORM,
    RPS_FORMAT_R8G8_SINT,
    RPS_FORMAT_R16_TYPELESS,
    RPS_FORMAT_R16_FLOAT,
    RPS_FORMAT_D16_UNORM,
    RPS_FORMAT_R16_UNORM,
    RPS_FORMAT_R16_UINT,
    RPS_FORMAT_R16_SNORM,
    RPS_FORMAT_R16_SINT,
    RPS_FORMAT_R8_TYPELESS,
    RPS_FORMAT_R8_UNORM,
    RPS_FORMAT_R8_UINT,
    RPS_FORMAT_R8_SNORM,
    RPS_FORMAT_R8_SINT,
    RPS_FORMAT_A8_UNORM,
    RPS_FORMAT_R1_UNORM,
    RPS_FORMAT_R9G9B9E5_SHAREDEXP,
    RPS_FORMAT_R8G8_B8G8_UNORM,
    RPS_FORMAT_G8R8_G8B8_UNORM,
    RPS_FORMAT_BC1_TYPELESS,
    RPS_FORMAT_BC1_UNORM,
    RPS_FORMAT_BC1_UNORM_SRGB,
    RPS_FORMAT_BC2_TYPELESS,
    RPS_FORMAT_BC2_UNORM,
    RPS_FORMAT_BC2_UNORM_SRGB,
    RPS_FORMAT_BC3_TYPELESS,
    RPS_FORMAT_BC3_UNORM,
    RPS_FORMAT_BC3_UNORM_SRGB,
    RPS_FORMAT_BC4_TYPELESS,
    RPS_FORMAT_BC4_UNORM,
    RPS_FORMAT_BC4_SNORM,
    RPS_FORMAT_BC5_TYPELESS,
    RPS_FORMAT_BC5_UNORM,
    RPS_FORMAT_BC5_SNORM,
    RPS_FORMAT_B5G6R5_UNORM,
    RPS_FORMAT_B5G5R5A1_UNORM,
    RPS_FORMAT_B8G8R8A8_UNORM,
    RPS_FORMAT_B8G8R8X8_UNORM,
    RPS_FORMAT_B8G8R8A8_TYPELESS,
    RPS_FORMAT_B8G8R8A8_UNORM_SRGB,
    RPS_FORMAT_B8G8R8X8_TYPELESS,
    RPS_FORMAT_B8G8R8X8_UNORM_SRGB
    RPS_FORMAT_BC6H_TYPELESS,
    RPS_FORMAT_BC6H_UF16,
    RPS_FORMAT_BC6H_SF16,
    RPS_FORMAT_BC7_TYPELESS,
    RPS_FORMAT_BC7_UNORM,
    RPS_FORMAT_BC7_UNORM_SRGB,
    RPS_FORMAT_AYUV,
    RPS_FORMAT_Y410,
    RPS_FORMAT_Y416,
    RPS_FORMAT_NV12,
    RPS_FORMAT_P010,
    RPS_FORMAT_P016,
    RPS_FORMAT_420_OPAQUE,
    RPS_FORMAT_YUY2,
    RPS_FORMAT_Y210,
    RPS_FORMAT_Y216,
    RPS_FORMAT_NV11,
    RPS_FORMAT_AI44,
    RPS_FORMAT_IA44,
    RPS_FORMAT_P8,
    RPS_FORMAT_A8P8,
    RPS_FORMAT_B4G4R4A4_UNORM,
} RPS_FORMAT;

/// The type of resource.
typedef enum RPS_RESOURCE_TYPE
{
    /// The resource is a buffer.
    RPS_RESOURCE_BUFFER = 0,

    /// The resource is a 1D texture.
    RPS_RESOURCE_TEX1D = 1,

    /// The resource is a 2D texture.
    RPS_RESOURCE_TEX2D = 2,

    /// The resource is a 3D texture.
    RPS_RESOURCE_TEX3D = 3
} RPS_RESOURCE_TYPE;

/// An enumeration of additional resource flags.
typedef enum RPS_RESOURCE_FLAGS
{
    /// There are no flags specified on the resource.
    RPS_RESOURCE_FLAG_NONE = 0,

    /// The resource supports cubemap views.
    RPS_RESOURCE_FLAG_CUBEMAP_COMPATIBLE = 1 << 1,

    /// Force rowmajor image layout.
    RPS_RESOURCE_FLAG_ROWMAJOR_IMAGE = 1 << 2,

    /// The resource is preferred to be in GPU-local CPU-visible heap if available.
    RPS_RESOURCE_FLAG_PREFER_GPU_LOCAL_CPU_VISIBLE = 1 << 3,

    /// The resource is preferred to be in dedicated allocation or as committed resource.
    RPS_RESOURCE_FLAG_PREFER_DEDICATED_ALLOCATION = 1 << 4,

    /// The resource data is persistent from frame to frame. (i.e.: It should not be destroyed or aliased).
    RPS_RESOURCE_FLAG_PERSISTENT = 1 << 15,

} RPS_RESOURCE_FLAGS;

/// A structure encapsulating a description of a resource.
typedef struct ResourceDesc
{
    RPS_RESOURCE_TYPE   Type;                   ///< The type of resource.
    uint                TemporalLayers;         ///< The number of temporal layers in the resource.
    RPS_RESOURCE_FLAGS  Flags;                  ///< The flags of the resource.
    uint                Width;                  ///< The width if the resource is a texture, or the lower 32 bit of the byte size if the resource is a buffer.
    uint                Height;                 ///< The height if the resource is a texture, or the higher 32 bit of the byte size if the resource is a buffer.
    uint                DepthOrArraySize;       ///< The depth if the resource is a 3D texture, or the array size if the resource is a 1D/2D texture.
    uint                MipLevels;              ///< The number of mipmap levels in the resource.
    RPS_FORMAT          Format;                 ///< The format of the resource.
    uint                SampleCount;            ///< The sample count of the resource.
} ResourceDesc;

/// A structure encapsulating a description of a texture sub resource range.
typedef struct SubResourceRange
{
    uint                BaseMipLevel;           ///< The base mip level of the view.
    uint                MipLevelCount;          ///< The mipmap level count of the view.
    uint                BaseArrayLayer;         ///< The first array layer of the view.
    uint                ArrayLayerCount;        ///< The array layer count of the view.
} SubResourceRange;

/// An enumeration of resource view flags.
typedef enum RESOURCE_VIEW_FLAGS
{
    RPS_RESOURCE_VIEW_FLAG_NONE        = 0,         ///< No special resource view flags.
    RPS_RESOURCE_VIEW_FLAG_CUBEMAP_BIT = 1 << 0,    ///< The resource view is used as cubemaps.
} RESOURCE_VIEW_FLAGS;

/// A texture resource view.
typedef struct texture
{
    uint                Resource;                   ///< The resource Id of the texture view.
    RPS_FORMAT          ViewFormat;                 ///< The format of the texture view. RPS_FORMAT_UNKNOWN indicates the format is inherited from the resource.
    uint                TemporalLayer;              ///< The temporal layer of the texture view.
    RESOURCE_VIEW_FLAGS Flags;                      ///< The flags of the texture view.
    SubResourceRange    SubresourceRange;           ///< The subresource range of the texture view.
    float               MinLodClamp;                ///< The min LOD clamp of the texture view.
    uint                ComponentMapping;           ///< A 32 bit value specifying the color component (RGBA channel) mapping of the view.

    /// Get the resource description.
    ///
    ResourceDesc desc() const;
    /// Get the parent texture view (a full, default view for the resource)
    /// which the current view is derived from. Or the view itself if it is the full default view.
    ///
    texture base() const;
    /// Create a derived texture view with the given array slice index range.
    ///
    /// @param baseArrayLayer       The first array layer to view.
    /// @param arrayLayerCount      The array layer count to view. -1 means all remaining array layers starting from <c>baseArrayLayer</c>.
    /// 
    /// @returns
    /// The derived texture view.
    texture array(in uint baseArrayLayer, in int arrayLayerCount);
    /// Create a derived texture view with one single array layer at the given array layer index.
    ///
    /// @param singleLayer          The array layer to view.
    /// 
    /// @returns                    The derived texture view.
    texture array(in uint singleLayer);
    /// Create a derived texture view with the given mip level range.
    ///
    /// @param baseMipLevel         The first mip level to view.
    /// @param mipLevelCount        The number of mips in the view. -1 means all remaining mips starting from <c>baseMipLevel</c>.
    /// 
    /// @returns                    The derived texture view.
    texture mips(in uint baseMipLevel, in int mipLevelCount);
    /// Create a derived texture view with one single mip level.
    ///
    /// @param singleMipLevel       The mip level to view.
    /// 
    /// @returns                    The derived texture view.
    texture mips(in uint singleMipLevel);
    /// Create a derived texture view with the given format override.
    /// Refer to API-specific restrictions of what formats can be used to create view.
    ///
    /// @param viewFormat           The view format.
    /// 
    /// @returns                    The derived texture view.
    texture format(in uint viewFormat);
    /// Create a derived texture view with the given temporal layer for temporal textures.
    /// 
    /// 0 means the current frame (default), 1 means the previous frame, 2 means two frames before, etc. If
    /// temporalLayer is larger than the amount of previous frames, temporalLayer = RpsRenderGraphUpdateInfo::frameIndex
    /// will be used instead, resolving to the very first slice. Accesses with temporalLayer >= ResourceDesc::TemporalLayers
    /// use temporalLayer % ResourceDesc::TemporalLayers instead. For any access with temporalLayer = n you may need to
    /// make sure that n frames ago or earlier all attributes of the access already have at least been used once for an
    /// access to the resource. Note: This requirement is a temporary solution and will change in the future.
    ///
    /// @param temporalLayer        The temporal layer.
    /// 
    /// @returns                    The derived texture view.
    texture temporal(in uint temporalLayer);
    /// Create a cubemap view.
    /// The texture must be a texture array with at least 6 array layers.
    ///
    /// @returns                    The derived texture view.
    texture cubemap();
} texture;

/// A buffer resource or buffer resource view.
typedef struct buffer {
    uint                 Resource;          ///< The resource Id of the buffer view.
    RpsFormat            ViewFormat;        ///< The format of the buffer view. RPS_FORMAT_UNKNOWN indicates the view is unformatted (e.g. a structured buffer, raw buffer, or not accessed via a shader).
    uint32_t             TemporalLayer;     ///< The temporal layer of the buffer view.
    RESOURCE_VIEW_FLAGS  Flags;             ///< The flags of the texture view.
    uint64_t             Offset;            ///< The offset in bytes from the start of the buffer resource.
    uint64_t             SizeInBytes;       ///< The size in bytes of the buffer view.
    uint32_t             Stride;            ///< The stride in bytes of the buffer view. Non-0 value indicates a structured buffer.

    /// Get the resource description.
    ///
    ResourceDesc desc();
    /// Get the parent view (a full, default view for the resource)
    /// which the current view is derived from. Or the view itself if it is the full default view.
    ///
    buffer base();
    /// Create a derived buffer view with the given format override.
    /// Usually used to bind as a formatted buffer in the shader.
    /// Default value is RPS_FORMAT_UNKNOWN.
    ///
    /// @param viewFormat           The view format.
    /// 
    /// @returns
    /// The derived buffer view.
    buffer format(in uint viewFormat);
    /// Create a derived buffer view with the given structure stride.
    /// Usually used to bind as a structured buffer in the shader.
    /// Default value is 0.
    ///
    /// @param structByteStride     The struct stride in bytes.
    /// 
    /// @returns
    /// The derived buffer view.
    buffer stride(in uint structByteStride);
    /// Create a derived buffer view with the given byte range.
    ///
    /// @param offset               The byte offset of the view.
    /// @param size                 The size of the view in bytes.
    /// 
    /// @returns
    /// The derived buffer view.
    buffer bytes(in uint64_t offset, in uint64_t size);
    /// Create a derived buffer view with the given element range.
    /// If the view's Format is RPS_FORMAT_UNKNOWN and StructureByteStride is 0,
    /// element size is treated as 1 byte.
    ///
    /// @param firstElement         The offset of the view in number of elements.
    /// @param elementCount         The size of the view in number of elements.
    /// 
    /// @returns
    /// The derived buffer view.
    buffer elements(in uint64_t firstElement, in uint64_t elementCount);
    /// Create a derived buffer view with the given temporal layer for temporal buffers.
    /// 0 means the current frame (default), 1 means the previous frame, 2 means two frames before, etc.
    ///
    /// @param temporalLayer        The temporal layer.
    /// 
    /// @returns
    /// The derived texture view.
    texture temporal(in uint temporalLayer);
} buffer;


/// Get a description of a texture resource.
///
/// @param resource                     The resource to obtain a description of.
/// 
/// @returns                            A description of a resource.
ResourceDesc describe_resource(texture resource);

/// Get a description of a buffer resource.
///
/// @param resource                     The resource to obtain a description of.
/// 
/// @returns                            A description of a resource.
ResourceDesc describe_resource(buffer resource);

/// Get a description of a texture resource.
///
/// @param resource                     The resource to obtain a description of.
/// 
/// @returns                            A description of a resource.
ResourceDesc describe_texture(texture resource);

/// Get a description of a buffer resource.
///
/// @param resource                     The resource to obtain a description of.
/// 
/// @returns                            A description of a resource.
ResourceDesc describe_buffer(buffer resource);

/// Create a texture resource.
///
/// @param description                  The description of the resource to create
/// 
/// @returns
/// A new resource matching the description.
texture create_texture(ResourceDesc description);

/// Create a buffer resource.
///
/// @param description                  The description of the resource to create
/// 
/// @returns                            A new resource matching the description.
texture create_buffer(ResourceDesc description);

/// Create a texture 1D resource.
///
/// @param format                       The format of the texture.
/// @param width                        The width of the texture.
/// @param numMips                      The number of mipmap levels. Default value is 1.
/// @param arraySlices                  The number of array slices. Default value is 1.
/// @param numTemporalLayers            The number of temporal layers. Default value is 1.
/// @param flags                        Any additional resource flags. Default value is <c>rps::resource_flags::none</c>.
/// 
/// @returns                            A default view of the new texture 1D resource matching the description.
texture create_tex1d(RPS_FORMAT format, uint width, uint numMips = 1, uint arraySlices = 1, uint numTemporalLayers = 1, RPS_RESOURCE_FLAGS flags = RPS_RESOURCE_FLAG_NONE);

/// Create a texture 2D resource.
///
/// @param format                       The format of the texture.
/// @param width                        The width of the texture.
/// @param height                       The height of the texture.
/// @param numMips                      The number of mipmap levels. Default value is 1.
/// @param arraySlices                  The number of array slices. Default value is 1.
/// @param numTemporalLayers            The number of temporal layers. Default value is 1.
/// @param sampleCount                  The number of samples. Default value is 1.
/// @param sampleQuality                The level of sample quality. Default value is 0.
/// @param flags                        Any additional resource flags. Default value is <c>rps::resource_flags::none</c>.
/// 
/// @returns                            A default view of the new texture 2D resource matching the description.
texture create_tex2d( RPS_FORMAT format, uint width, uint height, uint numMips = 1, uint arraySlices = 1, uint numTemporalLayers = 1, uint sampleCount = 1, uint sampleQuality = 0, RPS_RESOURCE_FLAGS flags = RPS_RESOURCE_FLAG_NONE);

/// Create a texture 3D resource.
///
/// @param format                       The format of the texture.
/// @param width                        The width of the texture.
/// @param height                       The height of the texture.
/// @param depth                        The depth of the texture.
/// @param numMips                      The number of mipmap levels. Default value is 1.
/// @param numTemporalLayers            The number of temporal layers. Default value is 1.
/// @param flags                        Any additional resource flags. Default value is <c>rps::resource_flags::none</c>.
/// 
/// @returns                            A default view of the new texture 3D resource matching the description.
texture create_tex3d(RPS_FORMAT format, uint width, uint height, uint depth, uint numMips = 1, uint numTemporalLayers = 1, RPS_RESOURCE_FLAGS flags = RPS_RESOURCE_FLAG_NONE);

/// Create a buffer resource.
///
/// @param width                        The width of the buffer.
/// @param numTemporalLayers            The number of temporal layers. Default value is 1.
/// @param flags                        Any additional resource flags. Default value is <c>rps::resource_flags::none</c>.
/// 
/// @returns                            A default view of the new buffer resource matching the description.
buffer create_buffer(uint64_t width, uint numTemporalLayers = 1, RPS_RESOURCE_FLAGS flags = RPS_RESOURCE_FLAG_NONE);

/// Create a texture resource view.
/// 
/// @param resource                     The texture view to derive from.
/// @param baseMipLevel                 The base mip level of the derived view.
/// @param mipLevelCount                The mipmap level count of the derived view.
/// @param baseArrayLayer               The first array layer of the derived view.
/// @param arrayLayerCount              The array layer count of the derived view.
/// @param temporalLayer                The temporal layer of the derived view.
/// @param format                       The override format of the derived view.
/// 
/// @returns                            The derived texture view.
texture create_texture_view(texture resource, uint baseMipLevel = 0, uint mipLevelCount = 1, uint baseArrayLayer = 0, uint arrayLayerCount = 1, uint temporalLayer = 0, RPS_FORMAT format = RPS_FORMAT_UNKNOWN);

/// @brief Create a derived buffer resource view.
/// @param resource                     The buffer view to derive from.
/// @param offset                       The offset in bytes of the derived buffer.
/// @param sizeInBytes                  The size in bytes of the derived buffer.
/// @param temporalLayer                The temporal layer of the derived buffer.
/// @param format                       The format of the derived buffer.
/// 
/// @return                             The derived buffer view.
buffer create_buffer_view(buffer resource, uint64_t offset = 0, uint64_t sizeInBytes = 0, uint temporalLayer = 0, RPS_FORMAT format = RPS_FORMAT_UNKNOWN);

/// @} end defgroup Resource

/// @defgroup BuiltInNodes Built-in Nodes
/// @{

/// Flags used by a built-in clear node. Defines the target view access and clear data format.
typedef enum RPS_CLEAR_FLAGS
{
    RPS_CLEAR_COLOR = 1 << 0,                                       ///< Clear the target as render target view.
    RPS_CLEAR_DEPTH = 1 << 1,                                       ///< Clear the depth plane.
    RPS_CLEAR_STENCIL = 1 << 2,                                     ///< Clear the stencil plane.
    RPS_CLEAR_DEPTHSTENCIL = RPS_CLEAR_DEPTH | RPS_CLEAR_STENCIL,   ///< Clear both depth and stencil plane.
    RPS_CLEAR_UAV_FLOAT = 1 << 3,                                   ///< Clear the target as float UAV.
    RPS_CLEAR_UAV_UINT = 1 << 4,                                    ///< Clear the target as uint UAV.
} RPS_CLEAR_FLAGS;

/// Built-in node for clearing a color texture by regions.
///
/// @param t                            View to the target texture to clear.
/// @param data                         The clear color value.
/// @param numRects                     The number of subregion rectangles.
/// @param rects                        An array of subregion rectangles.
graphics node clear_color_regions( [readwrite(rendertarget, clear)] texture t, float4 data : SV_ClearColor, uint numRects, int4 rects[] );

/// Built-in node for clearing a depth-stencil texture by regions.
///
/// @param t                            View to the target texture to clear.
/// @param option                       Flags indicating which aspects to clear. Must be a bit wise combination of RPS_CLEAR_DEPTH and RPS_CLEAR_STENCIL.
/// @param d                            The depth value to clear to. Ignored if option flags doesn't contain RPS_CLEAR_DEPTH.
/// @param s                            The stencil value to clear to. Ignored if option flags doesn't contain RPS_CLEAR_STENCIL.
/// @param numRects                     The number of subregion rectangles.
/// @param rects                        An array of subregion rectangles.
graphics node clear_depth_stencil_regions( [readwrite(depth, stencil, clear)] texture t, RPS_CLEAR_FLAGS option, float d : SV_ClearDepth, uint s : SV_ClearStencil, uint numRects, int4 rects[] );

/// Built-in node for clearing a texture by regions.
///
/// @param t                            View to the target texture to clear.
/// @param data                         The clear value. If the texture view has a floating point format, this is the bit representation of the floating point clear values.
/// @param numRects                     The number of subregion rectangles.
/// @param rects                        An array of subregion rectangles.
compute node clear_texture_regions( [readwrite(clear)] texture t, uint4 data : SV_ClearColor, uint numRects, int4 rects[] );

/// Built-in node for clearing a texture.
///
/// @param t                            View to the target texture to clear.
/// @param option                       Flags specifying the clear behavior.
/// @param data                         The clear value. If the texture view has a floating point format, this is the bit representation of the floating point clear values.
compute node clear_texture( [writeonly(clear)] texture t, RPS_CLEAR_FLAGS option, uint4 data );

/// Built-in node for clearing a buffer view.
///
/// @param b                            View to the target buffer to clear.
/// @param option                       Flags specifying the clear behavior.
/// @param data                         The clear value. If the buffer view has a floating point format, this is the bit representation of the floating point clear values.
compute node clear_buffer( [writeonly(clear)] buffer b, RPS_CLEAR_FLAGS option, uint4 data );

/// Built-in node for copying between two texture views.
///
/// @param dst                              ///< The destination texture view.
/// @param dstOffset                        ///< The upper left front corner of the destination region to copy to.
/// @param src                              ///< The source texture view.
/// @param srcOffset                        ///< The upper left front corner of the source region to copy from.
/// @param extent                           ///< The extent of the copy region.
copy node copy_texture( [readwrite(copy)] texture dst, uint3 dstOffset, [readonly(copy)] texture src, uint3 srcOffset, uint3 extent );

/// Built-in node for copying between two buffer views.
///
/// @param dst                              ///< The destination buffer view.
/// @param dstOffset                        ///< The byte offset of the destination.
/// @param src                              ///< The source buffer view.
/// @param srcOffset                        ///< The byte offset of the source.
/// @param size                             ///< The size in bytes of the copy region.
copy node copy_buffer( [readwrite(copy)] buffer dst, uint64_t dstOffset, [readonly(copy)] buffer src, uint64_t srcOffset, uint64_t size );

/// Built-in node for copying data from a texture view to a buffer view.
///
/// @param dst                              ///< The destination buffer view.
/// @param dstByteOffset                    ///< The byte offset of the destination buffer image.
/// @param rowPitch                         ///< The row pitch in bytes of the destination buffer image.
/// @param bufferImageSize                  ///< The dimension of the destination buffer image in texels.
/// @param dstOffset                        ///< The texel offset of the destination region.
/// @param src                              ///< The source texture view.
/// @param srcOffset                        ///< The texel offset of the source.
/// @param extent                           ///< The extent in texels of the copy region.
copy node copy_texture_to_buffer( [readwrite(copy)] buffer dst, uint64_t dstByteOffset, uint rowPitch, uint3 bufferImageSize, uint3 dstOffset, [readonly(copy)] texture src, uint3 srcOffset, uint3 extent );

/// Built-in node for copying data from a buffer view to a texture view.
///
/// @param dst                              ///< The destination texture view.
/// @param dstOffset                        ///< The texel offset of the destination region.
/// @param src                              ///< The source buffer view.
/// @param srcByteOffset                    ///< The byte offset of the source.
/// @param rowPitch                         ///< The row pitch in bytes of the source buffer image.
/// @param bufferImageSize                  ///< The dimension of the source buffer image in texels.
/// @param srcOffset                        ///< The texel offset of the source region.
/// @param extent                           ///< The extent in texels of the copy region.
copy node copy_buffer_to_texture( [readwrite(copy)] texture dst, uint3 dstOffset, [readonly(copy)] buffer src, uint64_t srcByteOffset, uint rowPitch, uint3 bufferImageSize, uint3 srcOffset, uint3 extent );

/// Flags used to define a built-in resolve node operation.
typedef enum RPS_RESOLVE_MODE
{
    RPS_RESOLVE_MODE_AVERAGE,                   ///< MSAA resolve outputs the average value of all samples.
    RPS_RESOLVE_MODE_MIN,                       ///< MSAA resolve outputs the min value of all samples.
    RPS_RESOLVE_MODE_MAX,                       ///< MSAA resolve outputs the max value of all samples.
    RPS_RESOLVE_MODE_ENCODE_SAMPLER_FEEDBACK,   ///< The resolve operation is for encoding sampler feedback map (DX12 only)
    RPS_RESOLVE_MODE_DECODE_SAMPLER_FEEDBACK,   ///< The resolve operation is for decoding sampler feedback map (DX12 only)
} RPS_RESOLVE_MODE;

/// @brief Built-in node for resolving the source texture to a destination. Usually used for MSAA resolve or sampler feedback transcoding.
/// 
/// @param dst                              ///< The destination texture view.
/// @param dstOffset                        ///< The texel offset of the destination region.
/// @param src                              ///< The source texture view.
/// @param srcOffset                        ///< The upper left front corner of the source region to copy from.
/// @param extent                           ///< The extent of the copy region.
/// @param resolveMode                      ///< The mode of the resolve.
graphics node resolve( [readwrite(resolve)] texture dst, uint2 dstOffset, [readonly(resolve)] texture src, uint2 srcOffset, uint2 extent, RPS_RESOLVE_MODE resolveMode );

/// Built-in node to clear a texture.
///
/// @param dst                          ///< The texture view to clear.
/// @param clearValue                   ///< A <i><c>float4</c></i> expressing the clear value.
node clear( [writeonly(rendertarget, clear)] texture dst, float4 clearValue );

/// Built-in node to clear a texture with uint values.
///
/// @param dst                          ///< The texture view to clear.
/// @param clearValue                   ///< A <i><c>uint4</c></i> expressing the clear value.
node clear( [writeonly(rendertarget, clear)] texture dst, uint4 clearValue );

/// Built-in node to clear a depth stencil view.
///
/// @param dst                          ///< The depth / stencil texture view to clear.
/// @param depth                        ///< The depth value to clear to.
/// @param stencil                      ///< The stencil value to clear to.
node clear( [writeonly(depth, stencil, clear)] texture dst, float depth, uint stencil );

/// Built-in node to clear a depth view.
///
/// @param dst                          ///< The depth texture view to clear.
/// @param depth                        ///< The depth value to clear to.
node clear_depth( [writeonly(depth, clear)] texture dst, float depth );

/// Built-in node to clear a stencil view.
///
/// @param dst                          ///< The stencil texture view to clear.
/// @param stencil                      ///< The stencil value to clear to.
node clear_stencil( [writeonly(stencil, clear)] texture dst, uint stencil );

/// Built-in node to clear a buffer UAV with float values.
///
/// @param dst                          ///< The texture view to clear.
/// @param val                          ///< The value to clear to.
node clear( [readwrite(clear)] buffer dst, float4 val );

/// Built-in node to clear a buffer UAV with uint values.
///
/// @param dst                          ///< The texture view to clear.
/// @param val                          ///< The value to clear to.
node clear( [readwrite(clear)] buffer dst, uint4 val );

/// Built-in node to copy between two texture views with identical properties.
///
/// @param dst                          ///< The destination texture view.
/// @param src                          ///< The source texture view.
node copy_texture( [writeonly(copy)] texture dst, [readonly(copy)] texture src );

/// Built-in node to copy between two buffer views with identical properties.
///
/// @param dst                          ///< The destination buffer view.
/// @param src                          ///< The source buffer view.
node copy_buffer( [readwrite(copy)] buffer dst, [readonly(copy)] buffer src );

/// A structure encapsulating 6 floats describing a viewport.
/// Explicitly specifies viewports in a graphics node.
///
/// Example usage: `graphics node Foo( rtv myRT, RpsViewport vp : SV_Viewport0 );`
typedef struct RpsViewport
{
    float x;                ///< The left position of the viewport.
    float y;                ///< The top position of the viewport.
    float width;            ///< The width of the viewport.
    float height;           ///< The height of the viewport.
    float minZ;             ///< The min Z value of the viewport.
    float maxZ;             ///< The max Z value of the viewport.
} RpsViewport;

/// A helper function to construct a RpsViewport.
RpsViewport viewport(float x, float y, float width, float height, float minZ = 0.0f, float maxZ = 1.0f);

/// A helper function to construct a RpsViewport with x = 0, y = 0, minZ = 0.0f, maxZ = 1.0f, and custom width and height.
RpsViewport viewport(float width, float height);

/// @} end defgroup BuiltInNodes

/// @defgroup scheduling Scheduling Intrinsics
/// @{

/// An intrinsic indicating a node scheduling barrier, instruct the RPS scheduler to not reschedule nodes across this point.
void sch_barrier();

/// A attribute that applies to a scope or a function definition, indicating the subject scope forms a subgraph
/// which various subgraph scheduling attribute arguments apply.
keyword subgraph;

/// A subgraph attribute argument indicating nodes outside the subgraph cannot be scheduled in between nodes within the subgraph.
SubgraphAttribute atomic;

/// A subgraph attribute argument indicating the subgraph nodes cannot be reordered with other nodes in the same subgraph.
SubgraphAttribute sequential;

/// @} end defgroup scheduling

/// @} end defgroup RPSL
