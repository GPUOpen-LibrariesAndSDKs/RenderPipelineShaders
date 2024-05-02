use crate::rps_types::{*};

#[allow(unused)]
#[no_mangle]
#[cfg(not(feature = "rpsl_dylib"))]
pub(crate) mod callbacks {
use core::ffi::{c_void, c_char};
extern "C" {
pub fn rpsl_abort(result: u32);

pub fn rpsl_node_call(
    node_decl_id: u32,
    num_args: u32,
    pp_args: *const *const c_void,
    node_call_flags: u32,
    node_local_id: u32) -> u32;

pub fn rpsl_node_dependencies(
    num_deps: u32,
    p_deps: *const u32,
    dst_node_id: u32);

pub fn rpsl_block_marker(
    marker_type: u32,
    block_index: u32,
    resource_count: u32,
    node_count: u32,
    local_loop_index: u32,
    num_children: u32,
    parent_id: u32);

pub fn rpsl_scheduler_marker(
    op_code: u32,
    flags: u32,
    name: *const c_char,
    name_length: u32);

pub fn rpsl_describe_handle(
    p_out_data: *mut c_void,
    data_size: u32,
    in_handle: *const u32,
    describe_op: u32);

pub fn rpsl_create_resource(
    res_type: u32,
    flags: u32,
    res_format: u32,
    width: u32,
    height: u32,
    depth_or_array_size: u32,
    mip_levels: u32,
    sample_count: u32,
    sample_quality: u32,
    temporal_layers: u32,
    id: u32) -> u32;

pub fn rpsl_name_resource(
    resource_hdl: u32,
    name: *const c_char,
    name_length: u32);

pub fn rpsl_notify_out_param_resources(
    param_id: u32,
    p_views: *const c_void);
}
}

#[allow(non_camel_case_types)]
#[allow(unused)]
#[cfg(feature = "rpsl_dylib")]
pub(crate) mod callbacks{
use core::ffi::{c_void, c_char};

type PFN_rpsl_abort = extern "C" fn(result: u32);

type PFN_rpsl_node_call = extern "C" fn (
    node_decl_id: u32,
    num_args: u32,
    pp_args: *const *const c_void,
    node_call_flags: u32,
    node_local_id: u32) -> u32;

type PFN_rpsl_node_dependencies = extern "C" fn(
    num_deps: u32,
    p_deps: *const u32,
    dst_node_id: u32);

type PFN_rpsl_block_marker = extern "C" fn(
    marker_type: u32,
    block_index: u32,
    resource_count: u32,
    node_count: u32,
    local_loop_index: u32,
    num_children: u32,
    parent_id: u32);

type PFN_rpsl_scheduler_marker = extern "C" fn(
    op_code: u32,
    flags: u32,
    name: *const c_char,
    name_length: u32);

type PFN_rpsl_describe_handle = extern "C" fn(
    p_out_data: *mut c_void,
    data_size: u32,
    in_handle: *const u32,
    describe_op: u32);

type PFN_rpsl_create_resource = extern "C" fn(
    res_type: u32,
    flags: u32,
    res_format: u32,
    width: u32,
    height: u32,
    depth_or_array_size: u32,
    mip_levels: u32,
    sample_count: u32,
    sample_quality: u32,
    temporal_layers: u32,
    id: u32) -> u32;

type PFN_rpsl_name_resource = extern "C" fn(
    resource_hdl: u32,
    name: *const c_char,
    name_length: u32);

type PFN_rpsl_notify_out_param_resources = extern "C" fn(
    param_id: u32,
    p_views: *const c_void);

#[repr(C)]
#[allow(non_snake_case)]
#[derive(Copy, Clone)]
pub struct RpslRuntimeProcs {
    pub pfn_rpsl_abort : PFN_rpsl_abort,
    pub pfn_rpsl_node_call : PFN_rpsl_node_call,
    pub pfn_rpsl_node_dependencies : PFN_rpsl_node_dependencies,
    pub pfn_rpsl_block_marker : PFN_rpsl_block_marker,
    pub pfn_rpsl_scheduler_marker : PFN_rpsl_scheduler_marker,
    pub pfn_rpsl_describe_handle : PFN_rpsl_describe_handle,
    pub pfn_rpsl_create_resource : PFN_rpsl_create_resource,
    pub pfn_rpsl_name_resource : PFN_rpsl_name_resource,
    pub pfn_rpsl_notify_out_param_resources : PFN_rpsl_notify_out_param_resources,
    pub pfn_rpsl_dxop_unary_i32 : PFN_rpsl_abort,
    pub pfn_rpsl_dxop_binary_i32 : PFN_rpsl_abort,
    pub pfn_rpsl_dxop_tertiary_i32 : PFN_rpsl_abort,
    pub pfn_rpsl_dxop_unary_f32 : PFN_rpsl_abort,
    pub pfn_rpsl_dxop_binary_f32 : PFN_rpsl_abort,
    pub pfn_rpsl_dxop_tertiary_f32 : PFN_rpsl_abort,
    pub pfn_rpsl_dxop_isSpecialFloat_f32 : PFN_rpsl_abort,
}

static mut RPSL_RUNTIME_PROCS: Option<RpslRuntimeProcs> = None;

#[no_mangle]
pub extern "C" fn ___rps_dyn_lib_init(p_procs: *const RpslRuntimeProcs, size_of_procs: u32) -> i32 {
    if std::mem::size_of::<RpslRuntimeProcs>() != size_of_procs as usize {
        -1
    } else {
        super::___rps_runtime_init();
        unsafe { RPSL_RUNTIME_PROCS = Some(*p_procs); }
        0
    }
}

pub unsafe fn rpsl_abort(result: u32)
{
    (RPSL_RUNTIME_PROCS.as_ref().unwrap().pfn_rpsl_abort)(result)
}

pub unsafe fn rpsl_node_call(
    node_decl_id: u32,
    num_args: u32,
    pp_args: *const *const c_void,
    node_call_flags: u32,
    node_local_id: u32) -> u32
{
    (RPSL_RUNTIME_PROCS.as_ref().unwrap().pfn_rpsl_node_call)(
        node_decl_id,
        num_args,
        pp_args,
        node_call_flags,
        node_local_id)
}

pub unsafe fn rpsl_node_dependencies(
    num_deps: u32,
    p_deps: *const u32,
    dst_node_id: u32)
{
    (RPSL_RUNTIME_PROCS.as_ref().unwrap().pfn_rpsl_node_dependencies)(
        num_deps, p_deps, dst_node_id)
}

pub unsafe fn rpsl_block_marker(
    marker_type: u32,
    block_index: u32,
    resource_count: u32,
    node_count: u32,
    local_loop_index: u32,
    num_children: u32,
    parent_id: u32)
{
    (RPSL_RUNTIME_PROCS.as_ref().unwrap().pfn_rpsl_block_marker)(
        marker_type,
        block_index,
        resource_count,
        node_count,
        local_loop_index,
        num_children,
        parent_id)
}

pub unsafe fn rpsl_scheduler_marker(
    op_code: u32,
    flags: u32,
    name: *const c_char,
    name_length: u32)
{
    (RPSL_RUNTIME_PROCS.as_ref().unwrap().pfn_rpsl_scheduler_marker)(
        op_code,
        flags,
        name,
        name_length)
}

pub unsafe fn rpsl_describe_handle(
    p_out_data: *mut c_void,
    data_size: u32,
    in_handle: *const u32,
    describe_op: u32)
{
    (RPSL_RUNTIME_PROCS.as_ref().unwrap().pfn_rpsl_describe_handle)(
        p_out_data,
        data_size,
        in_handle,
        describe_op)
}

pub unsafe fn rpsl_create_resource(
    res_type: u32,
    flags: u32,
    res_format: u32,
    width: u32,
    height: u32,
    depth_or_array_size: u32,
    mip_levels: u32,
    sample_count: u32,
    sample_quality: u32,
    temporal_layers: u32,
    id: u32) -> u32
{
    (RPSL_RUNTIME_PROCS.as_ref().unwrap().pfn_rpsl_create_resource)(
        res_type,
        flags,
        res_format,
        width,
        height,
        depth_or_array_size,
        mip_levels,
        sample_count,
        sample_quality,
        temporal_layers,
        id)
}

pub unsafe fn rpsl_name_resource(
    resource_hdl: u32,
    name: *const c_char,
    name_length: u32)
{
    (RPSL_RUNTIME_PROCS.as_ref().unwrap().pfn_rpsl_name_resource)(
        resource_hdl,
        name,
        name_length)
}

pub unsafe fn rpsl_notify_out_param_resources(
    param_id: u32,
    p_views: *const c_void)
{
    (RPSL_RUNTIME_PROCS.as_ref().unwrap().pfn_rpsl_notify_out_param_resources)(
        param_id, p_views)
}

}

use std::sync::Once;
static RPSL_RS_RUNTIME_INIT: Once = Once::new();

#[no_mangle]
pub extern "C" fn ___rps_runtime_init()
{
    RPSL_RS_RUNTIME_INIT.call_once(||{
        crate::init_node_ids();
        crate::init_entry_node_descs();
    });
}

#[cfg(feature = "rpsl_dylib")]
pub use callbacks::___rps_dyn_lib_init;

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct CRpsResourceDesc
{
    pub(crate) resource_type: ResourceType,
    pub(crate) temporal_layers: u32,
    pub(crate) flags: ResourceFlags,
    pub(crate) image_width_or_buffer_size_lo: u32,
    pub(crate) image_height_or_buffer_size_hi: u32,
    pub(crate) image_depth_or_array_layers: u32,
    pub(crate) image_mip_levels: u32,
    pub(crate) image_format: RpsFormat,
    pub(crate) image_sample_count: u32,
}

impl Into<ResourceDesc> for CRpsResourceDesc {
    fn into(self) -> ResourceDesc {
        ResourceDesc {
            dimension: self.resource_type,
            format: self.image_format,
            width: match self.resource_type {
                ResourceType::BUFFER => {
                    (self.image_width_or_buffer_size_lo as u64) | ((self.image_height_or_buffer_size_hi as u64) << 32)
                },
                _ => self.image_width_or_buffer_size_lo as u64,
            },
            height: match self.resource_type {
                ResourceType::BUFFER | ResourceType::IMAGE_1D => 1,
                _ => self.image_height_or_buffer_size_hi,
            },
            depth_or_array_size: self.image_depth_or_array_layers,
            mip_levels: self.image_mip_levels,
            sample_desc: SampleDesc {
                count: self.image_sample_count,
                quality: 0
            },
            temporal_layers: self.temporal_layers,
        }
    }
}


#[repr(C)]
#[derive(PartialEq)]
pub struct CRpsTypeInfo
{
    pub size: u16,
    pub id: u16,
}

#[allow(non_camel_case_types)]
pub enum RpsBuiltInTypeIds
{
    RPS_TYPE_OPAQUE = 0,
    RPS_TYPE_BUILT_IN_BOOL,
    RPS_TYPE_BUILT_IN_INT8,
    RPS_TYPE_BUILT_IN_UINT8,
    RPS_TYPE_BUILT_IN_INT16,
    RPS_TYPE_BUILT_IN_UINT16,
    RPS_TYPE_BUILT_IN_INT32,
    RPS_TYPE_BUILT_IN_UINT32,
    RPS_TYPE_BUILT_IN_INT64,
    RPS_TYPE_BUILT_IN_UINT64,
    RPS_TYPE_BUILT_IN_FLOAT32,
    RPS_TYPE_BUILT_IN_FLOAT64,
    _RPS_TYPE_BUILT_IN_MAX_VALUE,
    RPS_TYPE_IMAGE_VIEW = 64,
    RPS_TYPE_BUFFER_VIEW,
}

pub trait RpsTypeInfoTrait : Sized {
    const RPS_TYPE_INFO : CRpsTypeInfo;
    const RPS_ARRAY_LEN : u32 = 0;

    fn to_c_ptr(&self) -> *const core::ffi::c_void {
        self as *const _ as *const core::ffi::c_void
    }
}
