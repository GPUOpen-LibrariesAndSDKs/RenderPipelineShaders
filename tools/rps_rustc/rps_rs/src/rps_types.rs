// Contains types normally used directly in Rust-RPSL (`render_graph!{ ... }` blocks).

use core::ffi::c_void;

use crate::rpsl_runtime::{RpsTypeInfoTrait, RpsBuiltInTypeIds, CRpsTypeInfo, CRpsResourceDesc};

#[non_exhaustive]
#[repr(u32)]
#[derive(Default, Clone, Copy, PartialEq, Eq)]
#[allow(non_camel_case_types)]
pub enum RpsFormat {
    #[default]
    UNKNOWN,
    R32G32B32A32_TYPELESS,
    R32G32B32A32_FLOAT,
    R32G32B32A32_UINT,
    R32G32B32A32_SINT,
    R32G32B32_TYPELESS,
    R32G32B32_FLOAT,
    R32G32B32_UINT,
    R32G32B32_SINT,
    R16G16B16A16_TYPELESS,
    R16G16B16A16_FLOAT,
    R16G16B16A16_UNORM,
    R16G16B16A16_UINT,
    R16G16B16A16_SNORM,
    R16G16B16A16_SINT,
    R32G32_TYPELESS,
    R32G32_FLOAT,
    R32G32_UINT,
    R32G32_SINT,
    R32G8X24_TYPELESS,
    D32_FLOAT_S8X24_UINT,
    R32_FLOAT_X8X24_TYPELESS,
    X32_TYPELESS_G8X24_UINT,
    R10G10B10A2_TYPELESS,
    R10G10B10A2_UNORM,
    R10G10B10A2_UINT,
    R11G11B10_FLOAT,
    R8G8B8A8_TYPELESS,
    R8G8B8A8_UNORM,
    R8G8B8A8_UNORM_SRGB,
    R8G8B8A8_UINT,
    R8G8B8A8_SNORM,
    R8G8B8A8_SINT,
    R16G16_TYPELESS,
    R16G16_FLOAT,
    R16G16_UNORM,
    R16G16_UINT,
    R16G16_SNORM,
    R16G16_SINT,
    R32_TYPELESS,
    D32_FLOAT,
    R32_FLOAT,
    R32_UINT,
    R32_SINT,
    R24G8_TYPELESS,
    D24_UNORM_S8_UINT,
    R24_UNORM_X8_TYPELESS,
    X24_TYPELESS_G8_UINT,
    R8G8_TYPELESS,
    R8G8_UNORM,
    R8G8_UINT,
    R8G8_SNORM,
    R8G8_SINT,
    R16_TYPELESS,
    R16_FLOAT,
    D16_UNORM,
    R16_UNORM,
    R16_UINT,
    R16_SNORM,
    R16_SINT,
    R8_TYPELESS,
    R8_UNORM,
    R8_UINT,
    R8_SNORM,
    R8_SINT,
    A8_UNORM,
    R1_UNORM,
    R9G9B9E5_SHAREDEXP,
    R8G8_B8G8_UNORM,
    G8R8_G8B8_UNORM,
    BC1_TYPELESS,
    BC1_UNORM,
    BC1_UNORM_SRGB,
    BC2_TYPELESS,
    BC2_UNORM,
    BC2_UNORM_SRGB,
    BC3_TYPELESS,
    BC3_UNORM,
    BC3_UNORM_SRGB,
    BC4_TYPELESS,
    BC4_UNORM,
    BC4_SNORM,
    BC5_TYPELESS,
    BC5_UNORM,
    BC5_SNORM,
    B5G6R5_UNORM,
    B5G5R5A1_UNORM,
    B8G8R8A8_UNORM,
    B8G8R8X8_UNORM,
    R10G10B10_XR_BIAS_A2_UNORM,
    B8G8R8A8_TYPELESS,
    B8G8R8A8_UNORM_SRGB,
    B8G8R8X8_TYPELESS,
    B8G8R8X8_UNORM_SRGB,
    BC6H_TYPELESS,
    BC6H_UF16,
    BC6H_SF16,
    BC7_TYPELESS,
    BC7_UNORM,
    BC7_UNORM_SRGB,
    AYUV,
    Y410,
    Y416,
    NV12,
    P010,
    P016,
    YUV_420_OPAQUE,
    YUY2,
    Y210,
    Y216,
    NV11,
    AI44,
    IA44,
    P8,
    A8P8,
    B4G4R4A4_UNORM,
}

#[repr(u32)]
#[derive(Default, Clone, Copy, PartialEq, Eq)]
#[allow(non_camel_case_types)]
pub enum ResourceType
{
    /// Resource type is unknown / invalid.
    #[default]
    UNKNOWN = 0,
    /// A buffer resource type.
    BUFFER,
    /// A 1D image resource type.
    IMAGE_1D,
    /// A 2D image resource type.
    IMAGE_2D,
    /// A 3D image resource type.
    IMAGE_3D,
    /// Count of defined resource type values.
    COUNT,
}

bitflags::bitflags! {
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    pub struct ResourceFlags : u32
    {
        /// No special properties.
        const NONE = 0;
        /// Supports cubemap views.
        const CUBEMAP_COMPATIBLE_BIT = (1 << 1);
        /// Uses rowmajor image layout.
        const ROWMAJOR_IMAGE_BIT = (1 << 2);
        /// Preferred to be in GPU-local CPU-visible heap if available
        const PREFER_GPU_LOCAL_CPU_VISIBLE_BIT = (1 << 3);
        /// Preferred to be in dedicated allocation or as committed resource.
        const PREFER_DEDICATED_ALLOCATION_BIT = (1 << 4);
        /// Resource data is persistent from frame to frame.
        const PERSISTENT_BIT = (1 << 15);
    }

    /// An enumeration of resource view flags.
    #[derive(Default, Clone, Copy, PartialEq, Eq)]
    pub struct ResourceViewFlags : u32
    {
        const NONE        = 0;         // No special resource view flags.
        const CUBEMAP_BIT = 1 << 0;    // The resource view is used as cubemaps.
    }

    pub struct ComponentMapping : u32 {
        const R = 0;
        const G = 1;
        const B = 2;
        const A = 3;
        const ZERO = 4;
        const ONE = 5;
        const DEFAULT = 0x03020100;
    }
}

/// A structure encapsulating a description of a sample.
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct SampleDesc
{
    /// The number of samples.
    pub count : u32,
    /// The quality of the samples.
    pub quality : u32,
}

impl Default for SampleDesc {
    fn default() -> Self {
        Self { count: 1u32, quality: 0u32 }
    }
}

#[repr(C)]
#[derive(Default, Clone, Copy, PartialEq, Eq)]
pub struct ResourceDesc
{
    /// The type of resource.
    pub dimension : ResourceType,
    /// The format of the resource.
    pub format : RpsFormat,
    /// The width of the resource.
    pub width : u64,
    /// The height of the resource.
    pub height : u32,
    /// The depth (or array size) of the resource.
    pub depth_or_array_size : u32,
    /// The number of mipmap levels in the resource.
    pub mip_levels : u32,
    /// A description of the sample configuration in the resource.
    pub sample_desc : SampleDesc,
    /// The number of temporal layers in the resource.
    pub temporal_layers : u32,
}

/// A structure encapsulating a description of a texture sub resource range.
#[repr(C)]
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct SubResourceRange
{
    /// The base mipmapping level for the resource access.
    pub base_mip : u16,
    /// The number of mipmap levels accessible to the view.
    pub num_mips : u16,
    /// The first layer accessible to the view.
    pub base_array_slice : u32,
    /// The number of layers.
    pub num_array_layers : u32,
}

impl Default for SubResourceRange {
    fn default() -> Self {
        Self {
            base_mip: 0,
            num_mips: 1,
            base_array_slice: 0,
            num_array_layers: 1,
        }
    }
}

impl SubResourceRange {
    pub fn new() -> SubResourceRange {
        SubResourceRange{
            ..Default::default()
        }
    }
}


pub trait Resource {
    fn desc(&self) -> ResourceDesc {
        let mut desc = CRpsResourceDesc::default();
        let res_id = self.id();
        unsafe {
            crate::rpsl_runtime::callbacks::rpsl_describe_handle(
                &mut desc as *mut CRpsResourceDesc as *mut c_void,
                std::mem::size_of::<CRpsResourceDesc>() as u32,
                &res_id as *const u32,
                0);
        }
        desc.into()
    }

    fn id(&self) -> u32;
    fn set_name(&self, name: &'static [i8]);
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Buffer {
    resource : u32,                 // The resource Id of the buffer view.
    pub view_format : RpsFormat,    // The format of the buffer view. RPS_FORMAT_UNKNOWN indicates the view is unformatted (e.g. a structured buffer, raw buffer, or not accessed via a shader).
    pub temporal_layer : u32,       // The temporal layer of the buffer view.
    pub flags : ResourceViewFlags,  // The flags of the texture view.
    pub offset : u64,               // The offset in bytes from the start of the buffer resource.
    pub size_in_bytes : u64,        // The size in bytes of the buffer view.
    pub stride : u32,               // The stride in bytes of the buffer view. Non-0 value indicates a structured buffer.
}

impl Default for Buffer {
    fn default() -> Self {
        Buffer {
            resource: u32::MAX,
            view_format : RpsFormat::UNKNOWN,
            temporal_layer : 1,
            flags : ResourceViewFlags::NONE,
            offset : 0,
            size_in_bytes : 0,
            stride : 0,
        }
    }
}

impl Buffer {
    pub fn null() -> Self {
        Self {
            resource: u32::MAX,
            view_format : RpsFormat::UNKNOWN,
            temporal_layer : 0,
            flags : ResourceViewFlags::NONE,
            offset : 0,
            size_in_bytes : u64::MAX,
            stride : 0,
        }
    }

    pub fn new(width: u64, temporal_layers: u32, flags: ResourceFlags, id: u32) -> Buffer {
        let h_res = unsafe { crate::rpsl_runtime::callbacks::rpsl_create_resource(
            ResourceType::BUFFER as u32,
            flags.bits(),
            RpsFormat::UNKNOWN as u32,
            (width & (u32::MAX as u64)) as u32,
            (width >> 32) as u32,
            1,
            1,
            1,
            0,
            temporal_layers,
            id)
        };

        Buffer {
            resource : h_res,
            view_format : RpsFormat::UNKNOWN,
            temporal_layer : 0,
            flags : ResourceViewFlags::default(),
            offset : 0,
            size_in_bytes : width,
            stride : 0,
        }
    }

    pub(crate) fn from_c_desc(res_desc: &CRpsResourceDesc, res_id: u32) -> Buffer
    {
        Buffer {
            resource : res_id,
            view_format : RpsFormat::UNKNOWN,
            temporal_layer : 0,
            flags : ResourceViewFlags::default(),
            offset : 0,
            size_in_bytes :
                ((res_desc.image_height_or_buffer_size_hi as u64) << 32) |
                (res_desc.image_width_or_buffer_size_lo as u64),
            stride : 0,
        }
    }

    pub fn from_c_desc_array<const N: usize>(res_desc: &[CRpsResourceDesc; N], res_id: u32) -> [Buffer; N]
    {
        std::array::from_fn(|i| Buffer::from_c_desc(&res_desc[i], res_id + (i as u32)))
    }

    pub fn range(&self, offset: u64, size_in_bytes: u64) -> Result<Buffer, crate::Error> {
        if offset + size_in_bytes <= self.size_in_bytes {
            let mut new_buf = *self;
            new_buf.offset += offset;
            new_buf.size_in_bytes = size_in_bytes;
            Ok(new_buf)
        } else {
            Err(crate::Error::IndexOutOfRange)
        }
    }

    pub fn format(&self, fmt: RpsFormat) -> Buffer {
        Buffer {
            view_format: fmt,
            stride: 0,
            ..*self
        }
    }

    pub fn stride(&self, stride: u32) -> Buffer {
        Buffer {
            view_format: RpsFormat::UNKNOWN,
            stride: stride,
            ..*self
        }
    }
}

impl Resource for Buffer {
    fn id(&self) -> u32 { self.resource }

    fn set_name(&self, name: &'static [i8]) {
        unsafe {
            crate::rpsl_runtime::callbacks::rpsl_name_resource(
                self.resource,
                name.as_ptr(),
                name.len() as u32)
        }
    }
}

/// Create a buffer resource.
///
/// [`create_buffer`]: crate::rps_rs::create_buffer
///
/// ```
/// use rps_rs::create_buffer;
///
/// fn main() {
///     let buf1 = create_buffer!(1024);
/// }
/// ```
#[macro_export]
macro_rules! create_buffer {
    ($w:expr $(,$($tail:tt)*)?) => {
        {
            let mut temporal_layers = 1u32;
            let mut flags = $crate::ResourceFlags::NONE;
            let mut id = u32::MAX;
            $crate::create_buffer!(@ARGS (id, flags, desc) $(,$($tail)*)?);
            $crate::Buffer::new($w, temporal_layers, flags, id)
        }
    };
    (@ARGS ($id:ident, $flags:ident, $temporal_layers:ident), flags: $value:expr $(,$($tail:tt)*)?) => {
        $flags = $value;
        $crate::create_buffer!(@ARGS ($id, $flags, $desc) $(,$($tail)*)?);
    };
    (@ARGS ($id:ident, $flags:ident, $temporal_layers:ident), temporal_layers:$tmp:expr $(,$($tail:tt)*)?) => {
        $temporal_layers = $tmp;
        $crate::create_buffer!(@ARGS ($id, $flags, $desc) $(,$($tail)*)?);
    };
    (@ARGS ($id:ident, $flags:ident, $desc:ident), ___id: $value:literal) => {
        $id = $value;
    }
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct Texture {
    resource : u32,                             // The resource Id of the texture view.
    pub view_format : RpsFormat,                // The format of the texture view. RPS_FORMAT_UNKNOWN indicates the format is inherited from the resource.
    pub temporal_layer : u32,                   // The temporal layer of the texture view.
    pub flags : ResourceViewFlags,              // The flags of the texture view.
    pub subresource_range : SubResourceRange,   // The subresource range of the texture view.
    pub min_lod_clamp : f32,                    // The min LOD clamp of the texture view.
    pub component_mapping : u32,                // A 32 bit value specifying the color component (RGBA channel) mapping of the view.
}

#[macro_export]
macro_rules! create_tex1d {
    ($fmt:expr, $w:expr $(,$($tail:tt)*)?) => {
        {
            let mut desc = $crate::ResourceDesc {
                dimension : $crate::ResourceType::IMAGE_1D,
                format : $fmt,
                width : $w as u64,
                height : 1u32,
                depth_or_array_size : 1u32,
                mip_levels : 1u32,
                sample_desc : $crate::SampleDesc::default(),
                temporal_layers : 1u32,
            };
            let mut flags = $crate::ResourceFlags::NONE;
            let mut id = u32::MAX;
            $crate::create_tex1d!(@ARGS (id, flags, desc) $(,$($tail)*)?);
            $crate::Texture::new(desc, flags, id)
        }
    };
    (@ARGS ($id:ident, $flags:ident, $desc:ident), flags: $value:expr $(,$($tail:tt)*)?) => {
        $flags = $value;
        $crate::create_tex1d!(@ARGS ($id, $flags, $desc) $(,$($tail)*)?);
    };
    (@ARGS ($id:ident, $flags:ident, $desc:ident), mip_levels: $value:expr $(,$($tail:tt)*)?) => {
        $desc.mip_levels = $value;
        $crate::create_tex1d!(@ARGS ($id, $flags, $desc) $(,$($tail)*)?);
    };
    (@ARGS ($id:ident, $flags:ident, $desc:ident), array_slices:$arr:expr $(,$($tail:tt)*)?) => {
        desc.depth_or_array_size = $arr;
        $crate::create_tex1d!(@ARGS ($id, $flags, $desc) $(,$($tail)*)?);
    };
    (@ARGS ($id:ident, $flags:ident, $desc:ident), temporal_layers:$tmp:expr $(,$($tail:tt)*)?) => {
        desc.temporal_layers = $tmp;
        $crate::create_tex1d!(@ARGS ($id, $flags, $desc) $(,$($tail)*)?);
    };
    (@ARGS ($id:ident, $flags:ident, $desc:ident), ___id: $value:literal) => {
        $id = $value;
    }
}

#[macro_export]
macro_rules! create_tex2d {
    ($fmt:expr, $w:expr, $h:expr $(,$($tail:tt)*)?) => {
        {
            let mut desc = $crate::ResourceDesc {
                dimension : $crate::ResourceType::IMAGE_2D,
                format : $fmt,
                width : $w as u64,
                height : $h,
                depth_or_array_size : 1u32,
                mip_levels : 1u32,
                sample_desc : $crate::SampleDesc::default(),
                temporal_layers : 1u32,
            };
            let mut flags = $crate::ResourceFlags::NONE;
            let mut id = u32::MAX;
            $crate::create_tex2d!(@ARGS (id, flags, desc) $(,$($tail)*)?);
            $crate::Texture::new(desc, flags, id)
        }
    };
    (@ARGS ($id:ident, $flags:ident, $desc:ident), flags: $value:expr $(,$($tail:tt)*)?) => {
        $flags = $value;
        $crate::create_tex2d!(@ARGS ($id, $flags, $desc) $(,$($tail)*)?);
    };
    (@ARGS ($id:ident, $flags:ident, $desc:ident), mip_levels: $value:expr $(,$($tail:tt)*)?) => {
        $desc.mip_levels = $value;
        $crate::create_tex2d!(@ARGS ($id, $flags, $desc) $(,$($tail)*)?);
    };
    (@ARGS ($id:ident, $flags:ident, $desc:ident), array_slices:$arr:expr $(,$($tail:tt)*)?) => {
        $desc.depth_or_array_size = $arr;
        $crate::create_tex2d!(@ARGS ($id, $flags, $desc) $(,$($tail)*)?);
    };
    (@ARGS ($id:ident, $flags:ident, $desc:ident), temporal_layers:$tmp:expr $(,$($tail:tt)*)?) => {
        $desc.temporal_layers = $tmp;
        $crate::create_tex2d!(@ARGS ($id, $flags, $desc) $(,$($tail)*)?);
    };
    (@ARGS ($id:ident, $flags:ident, $desc:ident), sample_count:$smpc:expr $(,$($tail:tt)*)?) => {
        $desc.sample_desc.count = $smpc;
        $crate::create_tex2d!(@ARGS ($id, $flags, $desc) $(,$($tail)*)?);
    };
    (@ARGS ($id:ident, $flags:ident, $desc:ident), sample_quality:$smpq:expr $(,$($tail:tt)*)?) => {
        $desc.sample_desc.quality = $smpq;
        $crate::create_tex2d!(@ARGS ($id, $flags, $desc) $(,$($tail)*)?);
    };
    (@ARGS ($id:ident, $flags:ident, $desc:ident), ___id: $value:literal) => {
        $id = $value;
    }
}

//texture create_tex3d(RPS_FORMAT format, uint width, uint height, uint depth, uint numMips = 1, uint numTemporalLayers = 1, RPS_RESOURCE_FLAGS flags = RPS_RESOURCE_FLAG_NONE);
#[macro_export]
macro_rules! create_tex3d {
    ($fmt:expr, $w:expr, $h:expr, $d:expr $(,$($tail:tt)*)?) => {
        {
            let mut desc = $crate::ResourceDesc {
                dimension : $crate::ResourceType::IMAGE_3D,
                format : $fmt,
                width : $w as u64,
                height : $h,
                depth_or_array_size : $d,
                mip_levels : 1u32,
                sample_desc : $crate::SampleDesc::default(),
                temporal_layers : 1u32,
            };
            let mut flags = $crate::ResourceFlags::NONE;
            let mut id = u32::MAX;
            $crate::create_tex3d!(@ARGS (id, flags, desc) $(,$($tail)*)?);
            $crate::Texture::new(desc, flags, id)
        }
    };
    (@ARGS ($id:ident, $flags:ident, $desc:ident), flags: $value:expr $(,$($tail:tt)*)?) => {
        $flags = $value;
        $crate::create_tex3d!(@ARGS ($id, $flags, $desc) $(,$($tail)*)?);
    };
    (@ARGS ($id:ident, $flags:ident, $desc:ident), mip_levels: $value:expr $(,$($tail:tt)*)?) => {
        $desc.mip_levels = $value;
        $crate::create_tex3d!(@ARGS ($id, $flags, $desc) $(,$($tail)*)?);
    };
    (@ARGS ($id:ident, $flags:ident, $desc:ident), temporal_layers:$tmp:expr $(,$($tail:tt)*)?) => {
        $desc.temporal_layers = $tmp;
        $crate::create_tex3d!(@ARGS ($id, $flags, $desc) $(,$($tail)*)?);
    };
    (@ARGS ($id:ident, $flags:ident, $desc:ident), ___id: $value:literal) => {
        $id = $value;
    }
}

#[macro_export]
macro_rules! create_texture {
    ($desc:expr $(,$($tail:tt)*)?) => {
        {
            let mut flags = $crate::ResourceFlags::NONE;
            let mut id = u32::MAX;
            $crate::create_texture!(@ARGS (id, flags) $(,$($tail)*)?);
            $crate::Texture::new($desc, flags, id)
        }
    };
    (@ARGS ($id:ident, $flags:ident), flags: $value:expr $(,$($tail:tt)*)?) => {
        $flags = $value;
        $crate::create_texture!(@ARGS ($flags) $(,$($tail)*)?);
    };
    (@ARGS ($id:ident, $flags:ident), ___id: $value:literal) => {
        $id = $value;
    }
}

impl Default for Texture {
    fn default() -> Self {
        Self {
            resource : u32::MAX,
            view_format : RpsFormat::UNKNOWN,
            temporal_layer : 1,
            flags : ResourceViewFlags::NONE,
            subresource_range : SubResourceRange::default(),
            min_lod_clamp : 0.0,
            component_mapping : ComponentMapping::DEFAULT.bits(),
        }
    }
}

impl Texture {
    pub fn null() -> Self {
        Self {
            resource : u32::MAX,
            view_format : RpsFormat::UNKNOWN,
            temporal_layer : 0,
            flags : ResourceViewFlags::NONE,
            subresource_range : SubResourceRange {
                base_mip : 0,
                num_mips : 0,
                base_array_slice : 0,
                num_array_layers : 0,
            },
            min_lod_clamp : 0.0,
            component_mapping : 0,
        }
    }

    pub fn new(desc: ResourceDesc, flags: ResourceFlags, id: u32) -> Texture {
        let h_res = unsafe { crate::rpsl_runtime::callbacks::rpsl_create_resource(
            desc.dimension as u32,
            flags.bits(),
            desc.format as u32,
            desc.width as u32,
            desc.height,
            desc.depth_or_array_size,
            desc.mip_levels,
            desc.sample_desc.count,
            desc.sample_desc.quality,
            desc.temporal_layers,
            id)
        };

        Texture {
            resource : h_res,
            view_format : RpsFormat::UNKNOWN,
            temporal_layer : 0,
            flags : ResourceViewFlags::default(),
            subresource_range : SubResourceRange {
                base_mip : 0,
                num_mips : desc.mip_levels as u16,
                base_array_slice : 0,
                num_array_layers : match desc.dimension { 
                    ResourceType::IMAGE_1D | ResourceType::IMAGE_2D => desc.depth_or_array_size,
                    _ => 1,
                },
            },
            min_lod_clamp : 0.0,
            component_mapping : ComponentMapping::DEFAULT.bits(),
            ..Default::default()
        }
    }

    pub fn from_c_desc(res_desc: &CRpsResourceDesc, res_id: u32) -> Texture
    {
        Texture {
            resource : res_id,
            view_format : res_desc.image_format,
            temporal_layer : 0,
            flags : ResourceViewFlags::default(),
            subresource_range : SubResourceRange {
                base_mip : 0,
                num_mips : res_desc.image_mip_levels as u16,
                base_array_slice : 0,
                num_array_layers : match res_desc.resource_type { 
                    ResourceType::IMAGE_1D | ResourceType::IMAGE_2D => res_desc.image_depth_or_array_layers,
                    _ => 1,
                },
            },
            min_lod_clamp : 0.0,
            component_mapping : ComponentMapping::DEFAULT.bits(),
        }
    }

    pub fn from_c_desc_array<const N: usize>(res_desc: &[CRpsResourceDesc; N], res_id: u32) -> [Texture; N]
    {
        std::array::from_fn(|i| Texture::from_c_desc(&res_desc[i], res_id + (i as u32)))
    }

    // Texture view functions:

    pub fn array_slice(&self, base_slice: u32) -> Self {
        Self {
            subresource_range : SubResourceRange {
                base_array_slice : base_slice,
                num_array_layers : 1,
                ..self.subresource_range
            },
            ..*self
        }
    }

    pub fn array(&self, base_slice: u32, num_slices: u32) -> Self {
        Self {
            subresource_range : SubResourceRange {
                base_array_slice : base_slice,
                num_array_layers : num_slices,
                ..self.subresource_range
            },
            ..*self
        }
    }

    pub fn mip_level(&self, base_mip: u32) -> Self {
        Self {
            subresource_range : SubResourceRange {
                base_mip : base_mip as u16,
                num_mips : 1,
                ..self.subresource_range
            },
            ..*self
        }
    }

    pub fn mips(&self, base_mip: u32, num_mips: u32) -> Self {
        Self {
            subresource_range : SubResourceRange {
                base_mip : base_mip as u16,
                num_mips : num_mips as u16,
                ..self.subresource_range
            },
            ..*self
        }
    }

    pub fn format(&self, fmt: RpsFormat) -> Self {
        Self {
            view_format: fmt,
            ..*self
        }
    }

    pub fn temporal(&self, layer: u32) -> Self {
        Self {
            temporal_layer: layer,
            ..*self
        }
    }
}

impl Resource for Texture {
    fn id(&self) -> u32 { self.resource }

    fn set_name(&self, name: &'static [i8]) {
        unsafe {
            crate::rpsl_runtime::callbacks::rpsl_name_resource(
                self.resource,
                name.as_ptr(),
                name.len() as u32)
        }
    }
}

#[repr(C)]
#[derive(Default, Clone, Copy, Debug)]
pub struct RpsViewport {
    pub x: f32,
    pub y: f32,
    pub width: f32,
    pub height: f32,
    pub min_z: f32,
    pub max_z: f32,
}

impl RpsViewport {
    pub fn new(x: f32, y: f32, w: f32, h: f32, min_z: f32, max_z: f32) -> Self {
        Self {
            x: x, y: y, width: w, height: h, min_z: min_z, max_z: max_z
        }
    }

    pub fn new_size(w: f32, h: f32) -> Self {
        Self {
            x: 0.0, y: 0.0, width: w, height: h, min_z: 0.0, max_z: 1.0
        }
    }
}

bitflags::bitflags! {
    #[derive(macro_utils::RpsFFIType)]
    pub struct RpsClearFlags : u32
    {
        const COLOR = 1 << 0;
        const DEPTH = 1 << 1;
        const STENCIL = 1 << 2;
        const DEPTHSTENCIL = RpsClearFlags::DEPTH.bits() | RpsClearFlags::STENCIL.bits();
        const UAV_FLOAT = 1 << 3;
        const UAV_UINT = 1 << 4;
    }
}

#[repr(C)]
#[allow(non_camel_case_types)]
#[derive(macro_utils::RpsFFIType)]
pub enum RpsResolveMode
{
    AVERAGE = 0,
    MIN,
    MAX,
    ENCODE_SAMPLER_FEEDBACK,
    DECODE_SAMPLER_FEEDBACK,
}


macro_rules! impl_rps_built_in_type_info {
    (&$type_name:ty, $sz:expr, $id:ident) => {
        impl RpsTypeInfoTrait for &$type_name {
            const RPS_TYPE_INFO : CRpsTypeInfo = CRpsTypeInfo {
                size: std::mem::size_of::<$type_name>() as u16,
                id: RpsBuiltInTypeIds:: $id as u16,
            };

            fn to_c_ptr(&self) -> *const c_void {
                (*self) as *const _ as *const c_void
            }
        }
    };
    ($type_name:ty, $sz:expr, $id:ident) => {
        impl RpsTypeInfoTrait for $type_name {
            const RPS_TYPE_INFO : CRpsTypeInfo = CRpsTypeInfo {
                size: std::mem::size_of::<$type_name>() as u16,
                id: RpsBuiltInTypeIds:: $id as u16,
            };
        }
    };
}

impl<U: RpsTypeInfoTrait, const N: usize> RpsTypeInfoTrait for &[U; N] {
    // Array size is stored separately in CRpsParameterDesc. Ignore here.
    const RPS_TYPE_INFO : CRpsTypeInfo = U::RPS_TYPE_INFO;
    const RPS_ARRAY_LEN : u32 = N as u32;

    fn to_c_ptr(&self) -> *const c_void {
        (*self) as *const _ as *const c_void
    }
}

impl<const N: usize> RpsTypeInfoTrait for &[Texture; N] {
    // Array size is stored separately in CRpsParameterDesc. Ignore here.
    const RPS_TYPE_INFO : CRpsTypeInfo = CRpsTypeInfo {
        size: std::mem::size_of::<Texture>() as u16,
        id: RpsBuiltInTypeIds::RPS_TYPE_IMAGE_VIEW as u16,
    };
    const RPS_ARRAY_LEN : u32 = N as u32;

    fn to_c_ptr(&self) -> *const c_void {
        (*self) as *const _ as *const c_void
    }
}

impl<const N: usize> RpsTypeInfoTrait for &[Buffer; N] {
    // Array size is stored separately in CRpsParameterDesc. Ignore here.
    const RPS_TYPE_INFO : CRpsTypeInfo = CRpsTypeInfo {
        size: std::mem::size_of::<Buffer>() as u16,
        id: RpsBuiltInTypeIds::RPS_TYPE_BUFFER_VIEW as u16,
    };
    const RPS_ARRAY_LEN : u32 = N as u32;

    fn to_c_ptr(&self) -> *const c_void {
        (*self) as *const _ as *const c_void
    }
}

impl_rps_built_in_type_info!(bool, 1, RPS_TYPE_BUILT_IN_BOOL);
impl_rps_built_in_type_info!(i8, 1, RPS_TYPE_BUILT_IN_INT8);
impl_rps_built_in_type_info!(u8, 1, RPS_TYPE_BUILT_IN_UINT8);
impl_rps_built_in_type_info!(i16, 2, RPS_TYPE_BUILT_IN_INT16);
impl_rps_built_in_type_info!(u16, 2, RPS_TYPE_BUILT_IN_UINT16);
impl_rps_built_in_type_info!(i32, 4, RPS_TYPE_BUILT_IN_INT32);
impl_rps_built_in_type_info!(u32, 4, RPS_TYPE_BUILT_IN_UINT32);
impl_rps_built_in_type_info!(i64, 8, RPS_TYPE_BUILT_IN_INT64);
impl_rps_built_in_type_info!(u64, 8, RPS_TYPE_BUILT_IN_UINT64);
impl_rps_built_in_type_info!(f32, 4, RPS_TYPE_BUILT_IN_FLOAT32);
impl_rps_built_in_type_info!(f64, 8, RPS_TYPE_BUILT_IN_FLOAT64);
impl_rps_built_in_type_info!(&Texture, 8, RPS_TYPE_IMAGE_VIEW);
impl_rps_built_in_type_info!(&Buffer, 8, RPS_TYPE_BUFFER_VIEW);
