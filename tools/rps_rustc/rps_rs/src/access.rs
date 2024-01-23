// Access attribute related types, mostly for node signature reflection and
// not commonly used directly in Rust-RPSL.

use crate::rpsl_runtime::{RpsTypeInfoTrait, RpsBuiltInTypeIds, CRpsTypeInfo};

#[repr(C)]
#[derive(Default)]
#[allow(non_camel_case_types)]
pub enum CRpsSemanticType
{
    #[default]
    UNSPECIFIED,

    // Shaders:
    VERTEX_SHADER,
    PIXEL_SHADER,
    GEOMETRY_SHADER,
    COMPUTE_SHADER,
    HULL_SHADER,
    DOMAIN_SHADER,
    RAYTRACING_PIPELINE,
    AMPLIFICATION_SHADER,
    MESH_SHADER,

    // States:
    VERTEX_LAYOUT,
    STREAM_OUT_LAYOUT,
    STREAM_OUT_DESC,
    BLEND_STATE,
    RENDER_TARGET_BLEND,
    DEPTH_STENCIL_STATE,
    RASTERIZER_STATE,
    // DYNAMIC_STATE_BEGIN,

    // Usage as a viewport. The data type must be <c><i>RpsViewport</i></c>.
    VIEWPORT,

    /// Usage as a scissor rectangle. The data type must be <c><i>RpsRect</i></c>.
    SCISSOR,

    /// Usage as primitive topology. The data must be one of the values specified by <c><i>RpsPrimitiveTopology</i></c>.
    PRIMITIVE_TOPOLOGY,

    /// Reserved for future use.
    PATCH_CONTROL_POINTS,

    /// Reserved for future use.
    PRIMITIVE_STRIP_CUT_INDEX,

    /// Reserved for future use.
    BLEND_FACTOR,

    /// Reserved for future use.
    STENCIL_REF,

    /// Reserved for future use.
    DEPTH_BOUNDS,

    /// Reserved for future use.
    SAMPLE_LOCATION,

    /// Reserved for future use.
    SHADING_RATE,

    /// Usage as a color clear value. The data type must be float[4].
    COLOR_CLEAR_VALUE,

    /// Usage as a depth clear value. The data type must be float.
    DEPTH_CLEAR_VALUE,

    /// Usage as a stencil clear value. The data type must be uint32_t, only the lower 8 bit will be used.
    STENCIL_CLEAR_VALUE,

    // Resource bindings:

    /// Start of the resource binding enumeration values.
    // RESOURCE_BINDING_BEGIN,

    /// Bound as a vertex buffer. The semantic index indicates the vertex buffer binding slot.
    VERTEX_BUFFER,

    /// Bound as an index buffer.
    INDEX_BUFFER,

    /// Bound as an indirect argument buffer.
    INDIRECT_ARGS,

    /// Bound as an indirect count buffer.
    STREAM_OUT_BUFFER,

    /// Bound for write as a stream out buffer. The semantic index indicates the stream out buffer binding slot.
    INDIRECT_COUNT,

    /// Bound as a render target view. The semantic index indicates the render target slot.
    RENDER_TARGET,

    /// Bound as a depth stencil view.
    DEPTH_STENCIL_TARGET,

    /// Bound as a shading rate image in a Variable Rate Shading (VRS) pass.
    SHADING_RATE_IMAGE,

    /// Bound as a resolve target. The semantic index indicates the render
    /// target slot of the resolve source.
    RESOLVE_TARGET,

    /// User defined resource view binding. This is intended for shader resource views and unordered access views where
    /// resources are bound to programmable shaders instead of fixed function binding points.
    USER_RESOURCE_BINDING,

    // !! KEEP USER_RESOURCE_BINDING THE LAST ELEMENT !!

    COUNT,
}

impl CRpsSemanticType {
    const fn from_bytes(s: &[u8]) -> Option<Self> {
        match s {
            b"SV_Viewport" =>               Some(CRpsSemanticType::VIEWPORT),
            b"SV_ScissorRect" =>            Some(CRpsSemanticType::SCISSOR),
            b"SV_PrimitiveTopology" =>      Some(CRpsSemanticType::PRIMITIVE_TOPOLOGY),
            b"SV_PatchControlPoints" =>     Some(CRpsSemanticType::PATCH_CONTROL_POINTS),
            b"SV_PrimitiveStripCutIndex" => Some(CRpsSemanticType::PRIMITIVE_STRIP_CUT_INDEX),
            b"SV_BlendFactor" =>            Some(CRpsSemanticType::BLEND_FACTOR),
            b"SV_StencilRef" =>             Some(CRpsSemanticType::STENCIL_REF),
            b"SV_DepthBounds" =>            Some(CRpsSemanticType::DEPTH_BOUNDS),
            b"SV_SampleLocation" =>         Some(CRpsSemanticType::SAMPLE_LOCATION),
            b"SV_ShadingRate" =>            Some(CRpsSemanticType::SHADING_RATE),
            b"SV_ClearColor" =>             Some(CRpsSemanticType::COLOR_CLEAR_VALUE),
            b"SV_ClearDepth" =>             Some(CRpsSemanticType::DEPTH_CLEAR_VALUE),
            b"SV_ClearStencil" =>           Some(CRpsSemanticType::STENCIL_CLEAR_VALUE),

            b"SV_VertexBuffer" =>           Some(CRpsSemanticType::VERTEX_BUFFER),
            b"SV_IndexBuffer" =>            Some(CRpsSemanticType::INDEX_BUFFER),
            b"SV_IndirectArgs" =>           Some(CRpsSemanticType::INDIRECT_ARGS),
            b"SV_IndirectCount" =>          Some(CRpsSemanticType::INDIRECT_COUNT),
            b"SV_StreamOutBuffer" =>        Some(CRpsSemanticType::STREAM_OUT_BUFFER),
            b"SV_Target" =>                 Some(CRpsSemanticType::RENDER_TARGET),
            b"SV_DepthStencil" =>           Some(CRpsSemanticType::DEPTH_STENCIL_TARGET),
            b"SV_ShadingRateImage" =>       Some(CRpsSemanticType::SHADING_RATE_IMAGE),
            b"SV_ResolveTarget" =>          Some(CRpsSemanticType::RESOLVE_TARGET),

            b"" => Some(CRpsSemanticType::UNSPECIFIED),
            _ => None,
        }
    }
}

#[derive(Default)]
pub struct CRpsSemanticAttr
{
    pub semantic: CRpsSemanticType,
    pub index: u32,
}

bitflags::bitflags! {
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    pub struct CRpsParameterFlags: u32
    {
        const NONE         = 0;
        const OUT_BIT      = 1 << 0;
        const OPTIONAL_BIT = 1 << 1;
        const RESOURCE_BIT = 1 << 2;
    }

    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    pub struct CRpsNodeDeclFlags: u32
    {
        const NONE               = 0;
        const GRAPHICS_BIT       = 1 << 0;
        const COMPUTE_BIT        = 1 << 1;
        const COPY_BIT           = 1 << 2;
        const PREFER_RENDER_PASS = 1 << 3;
        const PREFER_ASYNC       = 1 << 4;
    }
}


bitflags::bitflags! {
    pub struct AccessFlags : u64 {
        const UNKNOWN                         = 0;
        const INDIRECT_ARGS_BIT               = 1u64 << 0;
        const INDEX_BUFFER_BIT                = 1u64 << 1;
        const VERTEX_BUFFER_BIT               = 1u64 << 2;
        const CONSTANT_BUFFER_BIT             = 1u64 << 3;
        const SHADER_RESOURCE_BIT             = 1u64 << 4;
        const UNORDERED_ACCESS_BIT            = 1u64 << 5;
        const SHADING_RATE_BIT                = 1u64 << 6;
        const RENDER_TARGET_BIT               = 1u64 << 7;
        const DEPTH_READ_BIT                  = 1u64 << 8;
        const DEPTH_WRITE_BIT                 = 1u64 << 9;
        const STENCIL_READ_BIT                = 1u64 << 10;
        const STENCIL_WRITE_BIT               = 1u64 << 11;
        const STREAM_OUT_BIT                  = 1u64 << 12;
        const COPY_SRC_BIT                    = 1u64 << 13;
        const COPY_DEST_BIT                   = 1u64 << 14;
        const RESOLVE_SRC_BIT                 = 1u64 << 15;
        const RESOLVE_DEST_BIT                = 1u64 << 16;
        const RAYTRACING_AS_BUILD_BIT         = 1u64 << 17;
        const RAYTRACING_AS_READ_BIT          = 1u64 << 18;
        const PRESENT_BIT                     = 1u64 << 19;
        const CPU_READ_BIT                    = 1u64 << 20;
        const CPU_WRITE_BIT                   = 1u64 << 21;
        const DISCARD_DATA_BEFORE_BIT         = 1u64 << 22;
        const DISCARD_DATA_AFTER_BIT          = 1u64 << 23;
        const STENCIL_DISCARD_DATA_BEFORE_BIT = 1u64 << 24;
        const STENCIL_DISCARD_DATA_AFTER_BIT  = 1u64 << 25;
        const BEFORE_BIT                      = 1u64 << 26;
        const AFTER_BIT                       = 1u64 << 27;
        const CLEAR_BIT                       = 1u64 << 28;
        const RENDER_PASS                     = 1u64 << 29;
        const RELAXED_ORDER_BIT               = 1u64 << 30;
        const NO_VIEW_BIT                     = 1u64 << 31;

        const SHADER_STAGE_NONE               = 0;
        const SHADER_STAGE_VS                 = 1u64 << 32;
        const SHADER_STAGE_PS                 = 1u64 << 33;
        const SHADER_STAGE_GS                 = 1u64 << 34;
        const SHADER_STAGE_CS                 = 1u64 << 35;
        const SHADER_STAGE_HS                 = 1u64 << 36;
        const SHADER_STAGE_DS                 = 1u64 << 37;
        const SHADER_STAGE_RAYTRACING         = 1u64 << 38;
        const SHADER_STAGE_AS                 = 1u64 << 39;
        const SHADER_STAGE_MS                 = 1u64 << 40;
        const SHADER_STAGE_ALL                = ((1u64 << 9) - 1) << 32;

        const CUBEMAP_BIT                     = 1u64 << 63;

        const TMP_READ_BIT                    = 1u64 << 61;
        const TMP_WRITE_BIT                   = 1u64 << 62;
    }
}

#[repr(C)]
#[derive(Default)]
pub struct CRpsAccessAttr
{
    pub access_flags: u32,
    pub access_stages: u32,
}

#[repr(C)]
#[derive(Default)]
pub struct CRpsParamAttr
{
    pub access: CRpsAccessAttr,
    pub semantic: CRpsSemanticAttr,
}

impl CRpsParamAttr {
    pub const fn const_default() -> CRpsParamAttr {
        CRpsParamAttr {
            access: CRpsAccessAttr { access_flags: 0, access_stages: 0 },
            semantic: CRpsSemanticAttr { semantic: CRpsSemanticType::UNSPECIFIED, index: 0 },
        }
    }

    pub const fn new(flags: AccessFlags, sema_str: &'static str) -> CRpsParamAttr {
        let mut access_flags = flags.bits() as u32;
        let stage = ((flags.bits() & AccessFlags::SHADER_STAGE_ALL.bits()) >> 32) as u32;

        let sema_str_bytes = sema_str.as_bytes();
        let sema_str_len = sema_str_bytes.len();

        let mut sema_index = 0;
        let mut trailing_digits = 0;

        while trailing_digits < sema_str_len {
            let c = sema_str_bytes[sema_str_len - 1 - trailing_digits];
            if c.is_ascii_digit() {
                sema_index += (10 * trailing_digits) as usize + (c - b'0') as usize;
                trailing_digits += 1
            } else {
                break
            }
        }

        let sema_str_bytes = sema_str_bytes.split_at(sema_str_len - trailing_digits).0;

        let sema = match CRpsSemanticType::from_bytes(sema_str_bytes) {
            Some(sema) => sema,
            _ => panic!(""),
        };

        let (is_write, is_read) = (
            flags.contains(AccessFlags::TMP_WRITE_BIT),
            flags.contains(AccessFlags::TMP_READ_BIT)
        );

        // Apply implicit access from rw attrs:
        if stage != 0 {
            if is_write {
                access_flags |= AccessFlags::UNORDERED_ACCESS_BIT.bits() as u32;
            } else if is_read {
                access_flags |= AccessFlags::SHADER_RESOURCE_BIT.bits() as u32;
            } 
        }

        // Apply implicit access from semantics:
        access_flags |= match sema {
            CRpsSemanticType::VERTEX_BUFFER => AccessFlags::VERTEX_BUFFER_BIT,
            CRpsSemanticType::INDEX_BUFFER => AccessFlags::INDEX_BUFFER_BIT,
            CRpsSemanticType::INDIRECT_ARGS => AccessFlags::INDIRECT_ARGS_BIT,
            CRpsSemanticType::STREAM_OUT_BUFFER => AccessFlags::STREAM_OUT_BIT,
            CRpsSemanticType::INDIRECT_COUNT => AccessFlags::INDIRECT_ARGS_BIT,
            CRpsSemanticType::RENDER_TARGET => AccessFlags::RENDER_TARGET_BIT,
            CRpsSemanticType::DEPTH_STENCIL_TARGET =>
                if is_write { AccessFlags::DEPTH_WRITE_BIT }
                else { AccessFlags::DEPTH_READ_BIT },
            CRpsSemanticType::SHADING_RATE_IMAGE => AccessFlags::SHADING_RATE_BIT,
            CRpsSemanticType::RESOLVE_TARGET => AccessFlags::RESOLVE_DEST_BIT,
            _ => AccessFlags::UNKNOWN,
        }.bits() as u32;

        CRpsParamAttr {
            access: CRpsAccessAttr {
                access_flags: access_flags,
                access_stages: stage,
            },
            semantic: CRpsSemanticAttr {
                semantic: sema,
                index: sema_index as u32,
            }
        }
    }
}

#[doc(hidden)]
pub const fn get_rps_ffi_type_info<T: RpsTypeInfoTrait>() -> CRpsTypeInfo {
    return T::RPS_TYPE_INFO;
}

#[doc(hidden)]
pub const fn get_rps_ffi_array_len<T: RpsTypeInfoTrait>() -> u32 {
    return T::RPS_ARRAY_LEN;
}

#[doc(hidden)]
pub const fn get_rps_ffi_param_flags<T: RpsTypeInfoTrait>() -> CRpsParameterFlags {
    let mut flags = CRpsParameterFlags::NONE;

    if (T::RPS_TYPE_INFO.id == RpsBuiltInTypeIds::RPS_TYPE_IMAGE_VIEW as u16) ||
        (T::RPS_TYPE_INFO.id == RpsBuiltInTypeIds::RPS_TYPE_BUFFER_VIEW as u16)
    {
        flags = flags.union(CRpsParameterFlags::RESOURCE_BIT);
    }

    flags
}

#[macro_export]
#[doc(hidden)]
macro_rules! convert_access_flag {
    (readonly, $flag:ident) => {$crate::convert_access_flag!(@r, $flag).bits() | $crate::access::AccessFlags::TMP_READ_BIT.bits() };
    (readwrite, $flag:ident) => {$crate::convert_access_flag!(@w, $flag).bits() | $crate::access::AccessFlags::TMP_WRITE_BIT.bits() };
    (writeonly, $flag:ident) => {$crate::convert_access_flag!(@w, $flag).bits() | $crate::access::AccessFlags::TMP_WRITE_BIT.bits() | $crate::access::AccessFlags::DISCARD_DATA_BEFORE_BIT.bits()};

    // Shader / pipeline stages:
    (@$x:ident, vs) => {$crate::access::AccessFlags::SHADER_STAGE_VS};
    (@$x:ident, ps) => {$crate::access::AccessFlags::SHADER_STAGE_PS};
    (@$x:ident, cs) => {$crate::access::AccessFlags::SHADER_STAGE_CS};
    (@$x:ident, gs) => {$crate::access::AccessFlags::SHADER_STAGE_GS};
    (@$x:ident, hs) => {$crate::access::AccessFlags::SHADER_STAGE_HS};
    (@$x:ident, ds) => {$crate::access::AccessFlags::SHADER_STAGE_DS};
    (@$x:ident, ts) => {$crate::access::AccessFlags::SHADER_STAGE_TS};
    (@$x:ident, ms) => {$crate::access::AccessFlags::SHADER_STAGE_MS};
    (@$x:ident, ray_tracing) => {AccessFlags::SHADER_STAGE_RAYTRACING};
    (@$x:ident, discard_before) => {$crate::access::AccessFlags::DISCARD_DATA_BEFORE_BIT};
    (@$x:ident, discard_after) => {$crate::access::AccessFlags::DISCARD_DATA_AFTER_BIT};

    // Read access:
    (@r, depth) => { $crate::access::AccessFlags::DEPTH_READ_BIT };
    (@r, stencil) => { $crate::access::AccessFlags::STENCIL_READ_BIT };
    (@r, copy) => { $crate::access::AccessFlags::COPY_SRC_BIT };
    (@r, resolve) => { $crate::access::AccessFlags::RESOLVE_SRC_BIT };
    (@r, present) => { $crate::access::AccessFlags::PRESENT_BIT };
    (@r, cpu) => { $crate::access::AccessFlags::CPU_READ_BIT };
    (@r, indirectargs) => { $crate::access::AccessFlags::INDIRECT_ARGS_BIT };
    (@r, vb) => { $crate::access::AccessFlags::VERTEX_BUFFER_BIT };
    (@r, ib) => { $crate::access::AccessFlags::INDEX_BUFFER_BIT };
    (@r, cb) => { $crate::access::AccessFlags::CONSTANT_BUFFER_BIT };
    (@r, shadingrate) => { $crate::access::AccessFlags::SHADING_RATE_BIT };
    (@r, predication) => { $crate::access::AccessFlags::INDIRECT_ARGS_BIT };
    (@r, rtas) => { $crate::access::AccessFlags::RAYTRACING_AS_READ_BIT };
    (@r, cubemap) => { $crate::access::AccessFlags::CUBEMAP_BIT };
    (@r, discard_after) => { $crate::access::AccessFlags::DISCARD_DATA_AFTER_BIT };

    // Write access:
    (@w, render_target) => { $crate::access::AccessFlags::RENDER_TARGET_BIT };
    // Aliasing render_target for rpsl-compatibility:
    (@w, rendertarget) => { $crate::access::AccessFlags::RENDER_TARGET_BIT };
    (@w, depth) => { $crate::access::AccessFlags::DEPTH_WRITE_BIT };
    (@w, stencil) => { $crate::access::AccessFlags::STENCIL_WRITE_BIT };
    (@w, copy) => { $crate::access::AccessFlags::COPY_DEST_BIT };
    (@w, resolve) => { $crate::access::AccessFlags::RESOLVE_DEST_BIT };
    (@w, cpu) => { $crate::access::AccessFlags::CPU_WRITE_BIT };
    (@w, streamout) => { $crate::access::AccessFlags::STREAM_OUT_BIT };
    (@w, rtas) => { $crate::access::AccessFlags::RAYTRACING_AS_BUILD_BIT };
    (@w, clear) => { $crate::access::AccessFlags::CLEAR_BIT };
    (@w, discard_before) => { $crate::access::AccessFlags::DISCARD_DATA_BEFORE_BIT };
    (@w, discard_after) => { $crate::access::AccessFlags::DISCARD_DATA_AFTER_BIT };
}

#[doc(hidden)]
pub const fn post_process_access_attr(flags: u64) -> u64 {
    let mut flags = flags;

    let has_depth = (flags & (AccessFlags::DEPTH_READ_BIT.bits() | AccessFlags::DEPTH_WRITE_BIT.bits())) != 0;
    let has_stencil = (flags & (AccessFlags::STENCIL_READ_BIT.bits() | AccessFlags::STENCIL_WRITE_BIT.bits())) != 0;

    let has_discard_before = (flags & AccessFlags::DISCARD_DATA_BEFORE_BIT.bits()) != 0;
    let has_discard_after = (flags & AccessFlags::DISCARD_DATA_AFTER_BIT.bits()) != 0;

    if has_stencil {
        if has_discard_before {
            flags |= AccessFlags::STENCIL_DISCARD_DATA_BEFORE_BIT.bits();
            if !has_depth {
                flags &= !AccessFlags::DISCARD_DATA_BEFORE_BIT.bits();
            }
        }
        if has_discard_after {
            flags |= AccessFlags::STENCIL_DISCARD_DATA_AFTER_BIT.bits();
            if !has_depth {
                flags &= !AccessFlags::DISCARD_DATA_AFTER_BIT.bits();
            }
        }
    }

    flags
}

#[macro_export]
#[doc(hidden)]
#[allow(unused_macros)]
macro_rules! convert_access_attr {
    // Process attribute list
    (@access_attr_list) => {
        $crate::access::AccessFlags::UNKNOWN
    };
    (@access_attr_list $([$access_attr:ident $(( $($access_flags:ident),+ ))?])+) => {
        $crate::convert_access_attr!(@access_attr_list @INNER (0u64), $([$access_attr $(( $($access_flags),+ ))?])+)
    };
    (@access_attr_list @INNER ($output:expr),
        [$access_attr0:ident $(( $($access_flags0:ident),+ ))?]
        $([$access_attr:ident $(( $($access_flags:ident),+ ))?])*) =>
    {
        $crate::convert_access_attr!(@access_attr_list @INNER
            ($crate::convert_access_attr!(@access_attr ($output), [$access_attr0], $($($access_flags0),*)?)),
            $([$access_attr $(( $($access_flags),+ ))?])*)
    };
    (@access_attr_list @INNER ($output:expr), ) => { $crate::access::AccessFlags::from_bits_truncate($output) };

    // Process one attribute
    (@access_attr ($output:expr), [$attr:ident], $($flags:ident),*) => {
        ($output | $crate::access::post_process_access_attr(0u64 $(| $crate::convert_access_flag!($attr, $flags))*))
    };
}
