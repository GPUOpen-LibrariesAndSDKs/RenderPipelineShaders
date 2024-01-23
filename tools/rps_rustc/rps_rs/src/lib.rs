mod rps_types;
mod built_in_nodes;
pub mod access;
pub mod support;
pub mod rpsl_runtime;

use core::ffi::{c_void, c_char, c_uint};

use access::{*};

pub use rps_types::{*};
#[allow(unused_imports)]
pub use built_in_nodes::{*};

use rpsl_runtime::CRpsTypeInfo;

#[non_exhaustive]
#[repr(u32)]
pub enum Error {
    InvalidArgs,
    IndexOutOfRange,
}

pub struct Node {
    _id: u32,
}

#[repr(C)]
pub struct CRpsParameterDesc
{
    pub type_info: CRpsTypeInfo,
    pub array_size: u32,
    pub attr: *const CRpsParamAttr,
    pub name: *const c_char,
    pub flags: CRpsParameterFlags,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct CStrPtr(pub *const c_char);
unsafe impl Send for CStrPtr {}
unsafe impl Sync for CStrPtr {}

impl CStrPtr {
    pub fn into_cstr<'a>(&self) -> &'a core::ffi::CStr {
        unsafe { core::ffi::CStr::from_ptr(self.0) }
    }
}

#[repr(C)]
pub struct CRpsParameterDescConstPtr(pub *const CRpsParameterDesc);

unsafe impl Sync for CRpsParameterDescConstPtr{}
unsafe impl Send for CRpsParameterDescConstPtr{}

#[repr(C)]
pub struct RawConstPtr<T: Sized>(pub *const T);
unsafe impl<T> Sync for RawConstPtr<T>{}
unsafe impl<T> Send for RawConstPtr<T>{}

impl<T: Sized> Clone for RawConstPtr<T> {
    fn clone(&self) -> Self {
        Self(self.0.clone())
    }
}

impl<T: Sized> Copy for RawConstPtr<T> {}

#[repr(C)]
pub struct CRpsNodeDesc
{
    pub flags: CRpsNodeDeclFlags,
    pub num_params: u32,
    pub p_param_descs: CRpsParameterDescConstPtr,
    pub name: CStrPtr,
}

pub type CFnRpslEntry = extern "C" fn(
    num_args: u32,
    pp_args: *const *const c_void,
    flags: u32);

#[repr(C)]
#[derive(Copy, Clone)]
pub struct CRpslEntry
{
    pub name : CStrPtr,
    pub pfn_entry : CFnRpslEntry,
    pub p_param_descs : RawConstPtr<CRpsParameterDesc>,
    pub p_node_decls : RawConstPtr<CRpsNodeDesc>,
    pub num_params : u32,
    pub num_node_decls : u32,
}

#[repr(C)]
pub struct CRpslEntryPtr(pub *const CRpslEntry);
unsafe impl Send for CRpslEntryPtr {}
unsafe impl Sync for CRpslEntryPtr {}

pub struct NodeInfo {
    pub name: CStrPtr,
    pub id_setter: fn(u32),
}

#[linkme::distributed_slice]
pub static DYN_LIB_ENTRIES: [CStrPtr];

#[linkme::distributed_slice]
pub static GLOBAL_ENTRY_PTRS: [CRpslEntryPtr];

#[linkme::distributed_slice]
pub static GLOBAL_NODE_DECLS: [CRpsNodeDesc];

#[linkme::distributed_slice]
pub static GLOBAL_NODE_INFO_TABLE: [NodeInfo];

fn init_node_ids()
{
    let name_id_map : std::collections::HashMap<&core::ffi::CStr, u32> =
        crate::GLOBAL_NODE_DECLS.iter().enumerate().map(|(i, e)| {
            (e.name.into_cstr(), i as u32)
        }).collect();

    GLOBAL_NODE_INFO_TABLE.iter().for_each(|node_info| {
        (node_info.id_setter)(name_id_map[node_info.name.into_cstr()]);
    });
}

fn init_entry_node_descs() {
    GLOBAL_ENTRY_PTRS.iter().for_each(|p_entry| {
        unsafe {
            let p_entry = p_entry.0 as *mut CRpslEntry;
            (*p_entry).p_node_decls.0 = crate::GLOBAL_NODE_DECLS.as_ptr();
            (*p_entry).num_node_decls = crate::GLOBAL_NODE_DECLS.len() as u32;
        }
    })
}

#[no_mangle]
pub extern "C" fn ___rps_dyn_lib_entries(p_num_entries: *mut c_uint) -> *const CStrPtr {

    unsafe {
        if let Some(num_entries) = p_num_entries.as_mut() {
            *num_entries = DYN_LIB_ENTRIES.len() as u32;
        }
    }

    (&DYN_LIB_ENTRIES).as_ptr()
}

struct BlockMarker;
impl BlockMarker {
    pub const FUNCTION_INFO : u32 = 0;
    pub const LOOP_BEGIN : u32 = 1;
    pub const LOOP_ITERATION : u32 = 2;
    pub const LOOP_END : u32 = 3;
}

#[inline]
#[doc(hidden)]
pub fn call_node(dcl_id: u32, local_id: u32, arg_ptrs: &[*const c_void]) -> Node {
    Node {
        _id: unsafe { crate::rpsl_runtime::callbacks::rpsl_node_call(
            dcl_id,
            arg_ptrs.len() as u32,
            arg_ptrs.as_ptr(),
            0,
            local_id)
        }
    }
}

#[doc(hidden)]
pub fn loop_iterate(block_index: u32, _iter_index: u32) {
    unsafe {
        crate::rpsl_runtime::callbacks::rpsl_block_marker(
            BlockMarker::LOOP_ITERATION,
            block_index,
            0,
            0,
            0,
            0,
            u32::MAX);
    }
}

pub struct FunctionMarkerGuard;

impl FunctionMarkerGuard {
    pub fn new(resource_count: u32, node_count: u32, children_cound: u32) -> FunctionMarkerGuard {
        unsafe {
            crate::rpsl_runtime::callbacks::rpsl_block_marker(
                BlockMarker::FUNCTION_INFO,
                0, resource_count,
                node_count,
                0,
                children_cound,
                u32::MAX);
        }
        FunctionMarkerGuard{}
    }
}

impl Drop for FunctionMarkerGuard {
    fn drop(&mut self) {
        // Reserved for function_end marker
    }
}

pub struct LoopGuard{block_index: u32}

impl LoopGuard {
    pub fn new(
        block_index: u32,
        resource_count: u32,
        node_count: u32,
        local_loop_index: u32,
        num_children: u32,
        parent_id: u32) -> LoopGuard
    {
        unsafe {
            crate::rpsl_runtime::callbacks::rpsl_block_marker(
                BlockMarker::LOOP_BEGIN,
                block_index,
                resource_count,
                node_count,
                local_loop_index,
                num_children,
                parent_id);
        }

        LoopGuard { block_index: block_index }
    }
}

impl Drop for LoopGuard {
    fn drop(&mut self) {
        unsafe {
            crate::rpsl_runtime::callbacks::rpsl_block_marker(
                BlockMarker::LOOP_END,
                self.block_index,
                0,
                0,
                0,
                0,
                u32::MAX);
        }
    }
}

#[macro_export]
macro_rules! convert_node_type {
    (gfx) => { NodeType::Graphics };
    (graphics) => { NodeType::Graphics };
    (comp) => { NodeType::Compute };
    (compute) => { NodeType::Compute };
    (copy) => { NodeType::Copy };
    () => { NodeType::Default };
}

#[macro_export]
#[doc(hidden)]
#[allow(unused_macros)]
macro_rules! parse_node_decl_flag {
    (gfx) => { $crate::access::CRpsNodeDeclFlags::GRAPHICS_BIT };
    (graphics) => { $crate::access::CRpsNodeDeclFlags::GRAPHICS_BIT };
    (comp) => { $crate::access::CRpsNodeDeclFlags::COMPUTE_BIT };
    (compute) => { $crate::access::CRpsNodeDeclFlags::COMPUTE_BIT };
    (copy) => { $crate::access::CRpsNodeDeclFlags::COPY_BIT };
    (rp) => { $crate::access::CRpsNodeDeclFlags::PREFER_RENDER_PASS };
    (render_pass) => { $crate::access::CRpsNodeDeclFlags::PREFER_RENDER_PASS };
    (async) => { $crate::access::CRpsNodeDeclFlags::PREFER_ASYNC };
    () => { $crate::access::CRpsNodeDeclFlags::NONE };
}

#[macro_export]
#[doc(hidden)]
#[allow(unused_macros)]
macro_rules! parse_node_decl_flags {

    ($item:ident $($tail:ident)*) => {
        $crate::access::CRpsNodeDeclFlags::from_bits_truncate($crate::parse_node_decl_flag!($item).bits() | $crate::parse_node_decl_flags!($($tail)*).bits())
    };
    () => { $crate::access::CRpsNodeDeclFlags::NONE };
}

#[macro_export]
#[doc(hidden)]
#[allow(unused_macros)]
macro_rules! declare_nodes_impl {
    // Finalize node declarations:
    (@nodes [$rg_vis:vis $name:ident]
        ($(
            $node_vis:vis $([$node_type:ident])?
            $fn_name:ident (
                $( $([$access_attr:ident $(( $($access_flags:ident),* ))?])*
                $arg_name:ident : $arg_type:ty
                $(: $arg_semantic:ident)? ),* );
        )*)
        ()
    ) => {
        #[allow(unused, non_camel_case_types, non_snake_case)]
        $rg_vis struct $name {
        }
        impl $name {
            #[allow(unused)]
            pub const CNODE_DESCS: &'static [$crate::CRpsNodeDesc] = &[
                $({
                    const PARAM_DESCS: &'static [$crate::CRpsParameterDesc] = &[
                        $( $crate::CRpsParameterDesc {
                            type_info: $crate::access::get_rps_ffi_type_info::<$arg_type>(),
                            array_size: $crate::access::get_rps_ffi_array_len::<$arg_type>(),
                            attr: (&$crate::access::CRpsParamAttr::new(
                                $crate::convert_access_attr!(@access_attr_list $([$access_attr $(( $($access_flags),* ))?])*),
                                stringify!($($arg_semantic)?)
                            )) as *const $crate::access::CRpsParamAttr,
                            name: ($crate::static_cstr!(stringify!($arg_name))).as_ptr(),
                            flags: $crate::access::get_rps_ffi_param_flags::<$arg_type>(),
                        }),*
                    ];

                    $crate::CRpsNodeDesc{
                        flags: $crate::parse_node_decl_flags!($($node_type)?),
                        num_params: PARAM_DESCS.len() as u32,
                        p_param_descs: $crate::CRpsParameterDescConstPtr((&PARAM_DESCS).as_ptr()),
                        name: $crate::CStrPtr(($crate::static_cstr!(stringify!($fn_name))).as_ptr()),
                    }
                },)*
            ];
            $(
                paste::item! {
                    #[allow(unused, non_snake_case)]
                    #[macro_utils::render_graph_node_fn]
                    $node_vis fn $fn_name ($( $arg_name : $arg_type ),* ) -> $crate::Node {
                        use $crate::rpsl_runtime::RpsTypeInfoTrait;
                        let arg_ptrs = unsafe {
                            [$(($arg_name).to_c_ptr(),)*]
                        };
                        unsafe { $crate::call_node([<$name:upper _ $fn_name:upper _NODE_ID>], local_node_id, &arg_ptrs) }
                    }
                }
            )*
        }

        $(
            paste::item! {
                #[linkme::distributed_slice($crate::GLOBAL_NODE_DECLS)]
                static [<$name:upper _ $fn_name:upper>]: $crate::CRpsNodeDesc = {
                    const PARAM_DESCS: &'static [$crate::CRpsParameterDesc] = &[
                        $( $crate::CRpsParameterDesc {
                            type_info: $crate::access::get_rps_ffi_type_info::<$arg_type>(),
                            array_size: $crate::access::get_rps_ffi_array_len::<$arg_type>(),
                            attr: (&$crate::access::CRpsParamAttr::new(
                                $crate::convert_access_attr!(@access_attr_list $([$access_attr $(( $($access_flags),* ))?])*),
                                stringify!($($arg_semantic)?)
                            )) as *const $crate::access::CRpsParamAttr,
                            name: ($crate::static_cstr!(stringify!($arg_name))).as_ptr(),
                            flags: $crate::access::get_rps_ffi_param_flags::<$arg_type>(),
                        }),*
                    ];

                    $crate::CRpsNodeDesc{
                        flags: $crate::parse_node_decl_flags!($($node_type)?),
                        num_params: PARAM_DESCS.len() as u32,
                        p_param_descs: $crate::CRpsParameterDescConstPtr((&PARAM_DESCS).as_ptr()),
                        name: $crate::CStrPtr(($crate::static_cstr!(stringify!($fn_name))).as_ptr()),
                    }
                };

                static mut [<$name:upper _ $fn_name:upper _NODE_ID>]: u32 = u32::MAX;

                #[linkme::distributed_slice($crate::GLOBAL_NODE_INFO_TABLE)]
                static [<$name:upper _ $fn_name:upper _NODE_INFO>]: $crate::NodeInfo = $crate::NodeInfo {
                    name: $crate::CStrPtr(($crate::static_cstr!(stringify!($fn_name))).as_ptr()),
                    id_setter: |id| {
                        unsafe {
                            if ([<$name:upper _ $fn_name:upper _NODE_ID>] != u32::MAX) &&
                               ([<$name:upper _ $fn_name:upper _NODE_ID>] != id) {
                                panic!("Already set node id for '{}'",
                                    std::concat!(module_path!(), "::", stringify!($name), "::", stringify!($fn_name)))
                            }
                            [<$name:upper _ $fn_name:upper _NODE_ID>] = id;
                        }
                    },
                };
            }
        )*
    };

    // Exclude export entries from node decls:
    (@nodes [$rg_vis:vis $name:ident]
        ($(
            $node_vis:vis $([$node_type:ident])?
            $fn_name:ident (
                $( $([$access_attr:ident $(( $($access_flags:ident),* ))?])*
                $arg_name:ident : $arg_type:ty
                $(: $arg_semantic:ident)? ),* ); )*)
        (
            export $curr_fn_name:ident (
                $( $([$curr_access_attr:ident $(( $($curr_access_flags:ident),* ))?])*
                $curr_arg_name:ident : $curr_arg_type:ty
                $(: $curr_arg_semantic:ident)? ),*
            ) $body:block

            $($t:tt)*
        )
    ) => {
        $crate::declare_nodes_impl!{@nodes
            [$rg_vis $name] ($(
            $node_vis $([$node_type])? $fn_name(
                $( $([$access_attr $(( $($access_flags),* ))?])*
                $arg_name : $arg_type $(: $arg_semantic)? ),*
        ); )*)

        ($($t)*)}
    };

    // Append node decl:
    (@nodes [$rg_vis:vis $name:ident]
        ($(
            $node_vis:vis $([$node_type:ident])? $fn_name:ident(
                $( $([$access_attr:ident $(( $($access_flags:ident),* ))?])*
                $arg_name:ident : $arg_type:ty $(: $arg_semantic:ident)? ),*
            ); )*)
        (
            $curr_node_vis:vis $([$curr_node_type:ident])? $curr_fn_name:ident(
                $( $([$curr_access_attr:ident $(( $($curr_access_flags:ident),* ))?])*
                $curr_arg_name:ident : $curr_arg_type:ty $(: $curr_arg_semantic:ident)? ),*
            );

            $($t:tt)*
        )
    ) => {
        $crate::declare_nodes_impl!{@nodes [$rg_vis $name]
        (
            $($node_vis $([$node_type])? $fn_name(
                $( $([$access_attr $(( $($access_flags),* ))?])*
                $arg_name : $arg_type $(: $arg_semantic)? ),* );
            )*
            $curr_node_vis $([$curr_node_type])? $curr_fn_name(
                $( $([$curr_access_attr $(( $($curr_access_flags),* ))?])*
                $curr_arg_name : $curr_arg_type $(: $curr_arg_semantic)? ),*
            );
        ) ($($t)*) }
    };
}

#[macro_export]
#[doc(hidden)]
#[allow(unused_macros)]
macro_rules! expand_entry_arg_ptrs {
    ($pp_args:ident, ($($output:stmt)*), $index:expr, ) => {
        $($output)*
    };

    ($pp_args:ident, ($($output:stmt)*), $index:expr, ($arg_name:ident : $arg_type:tt), $($tail:tt)* ) => {
        expand_entry_arg_ptrs!{$pp_args, (
            $($output)*
            let $arg_name = unsafe { &*( $pp_args[$index] as *const $arg_type ) };
        ), $index + 1, $($tail)*}
    };
}

#[macro_export]
#[doc(hidden)]
#[allow(unused_macros)]
macro_rules! define_exports_impl {
    // Finalize exports:
    (@exports [$name:ident] ($num_entries:expr) ($($node_names:ident,)*) ($($export_decls:tt)*) ($($export_fns:item)*) ($($export_c_entries:item)*)) => {
        impl $name {
            $($export_fns)*
        }

        $($export_decls)*

        $($export_c_entries)*
    };

    (@exports [$name:ident] ($num_entries:expr) ($($node_names:ident,)*) ($($export_decls:tt)*) ($($export_fns:item)*) ($($export_c_entries:item)*)
        export $fn_name:ident ( $( $([$access_attr:ident $(( $($access_flags:ident),* ))?])* $arg_name:ident : $arg_type:ty ),* ) $body:block
        $($t:tt)*
    ) => {
        $crate::define_exports_impl!{@exports [$name] ($num_entries + 1) ($($node_names,)*) (
            $($export_decls)*
            
            paste::item! {
                #[allow(unused)]
                static mut [<$name:upper _ $fn_name:upper _ENTRY>]: $crate::CRpslEntry = 
                {
                    const PARAM_DESCS: &'static [$crate::CRpsParameterDesc] = &[ $( $crate::CRpsParameterDesc {
                        type_info: $crate::access::get_rps_ffi_type_info::<$arg_type>(),
                        array_size: $crate::access::get_rps_ffi_array_len::<$arg_type>(),
                        attr: (&$crate::access::CRpsParamAttr::new(
                            $crate::convert_access_attr!(@access_attr_list $([$access_attr $(( $($access_flags),* ))?])*),
                            ""
                        )) as *const $crate::access::CRpsParamAttr,
                        name: ($crate::static_cstr!(stringify!($arg_name))).as_ptr(),
                        flags: $crate::access::get_rps_ffi_param_flags::<$arg_type>(),
                    }),* ];
    
                    $crate::CRpslEntry
                    {
                        name: $crate::CStrPtr(($crate::static_cstr!(stringify!($fn_name))).as_ptr()),
                        pfn_entry: paste::item!{ $name :: [<entry_wrapper_ $fn_name>] },
                        p_param_descs: $crate::RawConstPtr::<$crate::CRpsParameterDesc>(PARAM_DESCS.as_ptr()),
                        p_node_decls: $crate::RawConstPtr::<$crate::CRpsNodeDesc>(std::ptr::null()),
                        num_params: PARAM_DESCS.len() as u32,
                        num_node_decls: 0,
                    }
                };
            }
        )(
            $($export_fns)*

            // Original export entry with argument attributes removed:
            #[allow(unused)]
            #[macro_utils::render_graph_entry($($node_names,)*)]
            fn $fn_name ( $($arg_name : $arg_type),* ) $body
        )(
            $($export_c_entries)*

            paste::item! {
                // RPS naming convention: rpsl_M_##ModuleName##_E_##EntryName
                #[no_mangle]
                #[linkme::distributed_slice($crate::GLOBAL_ENTRY_PTRS)]
                pub static [<rpsl_M_ $name _E_ $fn_name>] : $crate::CRpslEntryPtr =
                    unsafe { $crate::CRpslEntryPtr(&[<$name:upper _ $fn_name:upper _ENTRY>]) };

                #[allow(non_upper_case_globals)]
                #[linkme::distributed_slice($crate::DYN_LIB_ENTRIES)]
                static [<rpsl_M_ $name _E_ $fn_name _NAME_STR>]: $crate::CStrPtr =
                    $crate::CStrPtr(($crate::static_cstr!(concat!("rpsl_M_", stringify!($name), "_E_", stringify!($fn_name)))).as_ptr());
            }
        ) $($t)*}
    };

    // Exclude node decls from export entries
    (@exports [$name:ident] ($num_entries:expr) ($($node_names:ident,)*) ($($export_decls:tt)*) ($($export_fns:item)*) ($($export_c_entries:item)*)
        // Ignore pattern:
        $curr_node_vis:vis $([$curr_node_type:ident])? $curr_fn_name:ident(
            $( $([$curr_access_attr:ident $(( $($curr_access_flags:ident),* ))?])*
            $curr_arg_name:ident : $curr_arg_type:ty $(: $curr_arg_semantic:ident)? ),*
        );
        $($t:tt)*
    ) => {
        $crate::define_exports_impl!{@exports [$name] ($num_entries) ($($node_names,)* $curr_fn_name,) ($($export_decls)*) ($($export_fns)*) ($($export_c_entries)*) $($t)* }
    };
}

#[macro_export]
#[allow(unused_macros)]
macro_rules! render_graph {
    ([[$rg_vis:vis $rg_name:ident]] $($t:tt)*) => {
        $crate::declare_nodes_impl!{@nodes [$rg_vis $rg_name] () (
            // TODO: Stick built-in nodes here until we support importing nodes from a different module.
            [graphics] clear_color( [readwrite(render_target, clear)] t: &$crate::Texture, data : &[f32;4] : SV_ClearColor );
            [graphics] clear_color_regions( [readwrite(render_target, clear)] t: &$crate::Texture, data : &[f32;4] : SV_ClearColor, num_rects: u32, rects: &[i32;4] );
            [graphics] clear_depth_stencil( [readwrite(depth, stencil, clear)] t: &$crate::Texture, option: $crate::RpsClearFlags, d : f32 : SV_ClearDepth, s : u32 : SV_ClearStencil );
            [graphics] clear_depth_stencil_regions( [readwrite(depth, stencil, clear)] t: &$crate::Texture, option: $crate::RpsClearFlags, d : f32 : SV_ClearDepth, s : u32 : SV_ClearStencil, num_rects: u32, rects: &[i32;4] );
            [compute] clear_texture( [writeonly(clear)] t: &$crate::Texture, option: $crate::RpsClearFlags, data: &[u32;4] );
            [compute] clear_texture_regions( [readwrite(clear)] t: &$crate::Texture, data : &[u32;4] : SV_ClearColor, num_rects: u32, rects: &[i32;4] );
            [compute] clear_buffer( [writeonly(clear)] b: &$crate::Buffer, option: $crate::RpsClearFlags, data: &[u32;4] );
            [copy] copy_texture( [readwrite(copy)] dst: &$crate::Texture, dst_offset: &[u32;3], [readonly(copy)] src: &$crate::Texture, src_offset: &[u32;3], extent: &[u32;3] );
            [copy] copy_buffer( [readwrite(copy)] dst: &$crate::Buffer, dst_offset: u64, [readonly(copy)] src: &$crate::Buffer, src_offset: u64, size: u64 );
            [copy] copy_texture_to_buffer( [readwrite(copy)] dst: &$crate::Buffer, dst_byte_offset: u64, row_pitch: u32, buffer_image_size: &[u32;3], dst_offset: &[u32;3], [readonly(copy)] src: &$crate::Texture, src_offset: &[u32;3], extent: &[u32;3] );
            [copy] copy_buffer_to_texture( [readwrite(copy)] dst: &$crate::Texture, dst_offset: &[u32;3], [readonly(copy)] src: &$crate::Buffer, src_byte_offset: u64, row_pitch: u32, buffer_image_size: &[u32;3], src_offset: &[u32;3], extent: &[u32;3] );
            [graphics] resolve( [readwrite(resolve)] dst: &$crate::Texture, dst_offset: &[u32;2], [readonly(resolve)] src: &$crate::Texture, src_offset: &[u32;2], extent: &[u32;2], resolve_mode: $crate::RpsResolveMode );
            $($t)*
        )}
        $crate::define_exports_impl!{@exports [$rg_name] (0)
        (
            clear_color,
            clear_color_regions,
            clear_depth_stencil,
            clear_depth_stencil_regions,
            clear_texture,
            clear_texture_regions,
            clear_buffer,
            copy_texture,
            copy_buffer,
            copy_texture_to_buffer,
            copy_buffer_to_texture,
            resolve,
        ) () () () $($t)*}
    };
    () => {}
}
