use proc_macro::TokenStream;
use quote::{quote, ToTokens};
use syn::fold::{self, Fold};
use syn::parse::{Parse, ParseStream, Result};
use syn::punctuated::Punctuated;
use syn::spanned::Spanned;
use syn::{parse_macro_input, parse_quote, Type, Expr, Ident, ItemFn, Local, Stmt, Token, PathSegment, Block, FnArg, DeriveInput};

#[derive(Default)]
struct BlockInfo {
    pub id : u32,
    pub resource_count : u32,
    pub node_count : u32,
    pub child_block_count : u32,
    pub local_loop_index : u32,
}

#[derive(Default)]
struct RenderGraphEntryProcessor {
    nodes: std::collections::HashMap<Ident, u32>,
    assign_stack: std::vec::Vec<u32>,
    block_stack: std::vec::Vec<BlockInfo>,
    num_blocks: u32,
}

impl RenderGraphEntryProcessor {
    fn try_name_resource_on_assign(&self, e: syn::ExprAssign) -> syn::ExprAssign {

        let mut e = e;

        if let Expr::Path(ref e_path) = *e.left {
            let res_name_lit = e_path.path.to_token_stream().to_string();
            let e_right = *e.right;
            *e.right = parse_quote! {
                rps_rs::set_name_if_resource!(#e_right, (#res_name_lit).as_bytes())
            };
        }

        e
    }

    fn try_name_resource_on_init(&self, local: Local) -> Local {

        let mut local = local;
        let pat = local.pat;

        if let Some(mut init) = local.init {
            let init_expr = *init.expr;
            let modified_expr = parse_quote!{
                rps_rs::set_name_if_resource!(#init_expr, (stringify!(#pat)).as_bytes())
            };
            *init.expr = modified_expr;
            local.init = Some(init);
        }

        local.pat = pat;
        local
    }

    fn get_new_block_id(&mut self) -> u32 {
        self.num_blocks = self.num_blocks + 1;
        self.num_blocks
    }

    fn loop_begin(&mut self) {
        let new_block_id = self.get_new_block_id();
        let mut local_id = 0;
        if let Some(parent_block) = self.block_stack.last_mut() {
            local_id = parent_block.child_block_count;
            parent_block.child_block_count += 1;
        }
        self.block_stack.push(BlockInfo{
            id: new_block_id,
            local_loop_index: local_id,
            ..Default::default() });
    }

    fn loop_iterate(&self, body: &mut Block) {
        let BlockInfo { id, .. } = self.block_stack.last().unwrap();

        let call_loop_iterate = parse_quote!{
            rps_rs::loop_iterate(#id, u32::MAX);
        };
        body.stmts.insert(0, call_loop_iterate);
    }

    fn loop_end(&mut self, e: Expr) -> Expr {
        let BlockInfo {
            id,
            resource_count,
            node_count,
            child_block_count,
            local_loop_index,
        } = self.block_stack.pop().unwrap();

        let parent_id = self.block_stack.last().unwrap().id;

        let guard_ident = Ident::new(
            &format!("_loop_guard_{}", id),
            e.span());

        let new_block = parse_quote!{
            {
                let #guard_ident = rps_rs::LoopGuard::new(#id, #resource_count, #node_count, #local_loop_index, #child_block_count, #parent_id);
                #e
            }
        };

        Expr::Block(new_block)
    }
}

impl Parse for RenderGraphEntryProcessor {
    fn parse(input: ParseStream) -> Result<Self> {
        let node_names = Punctuated::<Ident, Token![,]>::parse_terminated(input)?;
        Ok(RenderGraphEntryProcessor {
            nodes: node_names.into_iter().enumerate().map(|(i, n)| (n, i as u32)).collect(),
            ..Default::default()
        })
    }
}

impl Fold for RenderGraphEntryProcessor {
    fn fold_expr(&mut self, e: Expr) -> Expr {
        match e {
            Expr::Call(mut e) => {
                let span = e.span();
                if let Expr::Path(func_path) = e.func.as_mut() {

                    let is_ext_node_call = match func_path.path.segments.first() {
                        Some(first_path_seg) => first_path_seg.ident == "ext",
                        _ => false,
                    };
                    let is_node_call = is_ext_node_call || match func_path.path.segments.last() {
                        Some(last_path_seg) => self.nodes.contains_key(&last_path_seg.ident),
                        _ => false,
                    };

                    if is_node_call {
                        if is_ext_node_call {
                            let segments : Vec<PathSegment> = func_path.path.segments.iter().map(|s| s.clone()).collect();
                            func_path.path.segments.clear();

                            segments.into_iter().skip(1).for_each(|s|
                                func_path.path.segments.push(s)
                            );
                        }
                        else if func_path.path.segments.len() == 1 {
                            // Add 'Self::' to self node calls
                            let self_ident = Ident::new("Self", span);
                            func_path.path.segments.insert(0, PathSegment::from(self_ident));
                        }

                        let node_count = self.block_stack.last().unwrap().node_count;
                        let node_count_lit_expr = parse_quote!(#node_count);
                        e.args.insert(0, Expr::Lit(node_count_lit_expr));

                        self.block_stack.last_mut().unwrap().node_count += 1;
                    }

                    // Check resource creation calls
                    let path_str = func_path.to_token_stream().to_string();

                    match path_str.as_str() {
                        "Texture :: new" |
                        "Buffer :: new" |
                        "rps_rs :: Texture :: new" |
                        "rps_rs :: Buffer :: new" => {
                            if let Some(last_assign) = self.assign_stack.last_mut()
                            {
                                *last_assign += 1u32;
                            }
                            self.block_stack.last_mut().unwrap().resource_count += 1;
                        },
                        _ => {}
                    };
                }
                fold::fold_expr(self, Expr::Call(e))
            },

            Expr::Macro(mut e) => {
                let path_str = e.mac.path.to_token_stream().to_string();

                match path_str.as_str() {
                    "create_tex1d" |
                    "create_tex2d" |
                    "create_tex3d" |
                    "create_texture" |
                    "create_buffer" |
                    "rps_rs :: create_tex1d" |
                    "rps_rs :: create_tex2d" |
                    "rps_rs :: create_tex3d" |
                    "rps_rs :: create_texture" |
                    "rps_rs :: create_buffer" => {
                        let last_block = self.block_stack.last_mut().unwrap();
                        let resource_id = last_block.resource_count;
                        let mac_args = e.mac.tokens;
                        e.mac.tokens = quote!(#mac_args, ___id: #resource_id );
                        last_block.resource_count += 1;

                        if let Some(last_assign) = self.assign_stack.last_mut()
                        {
                            *last_assign += 1u32;
                        }
                    },
                    _ => {}
                }

                let e = fold::fold_expr_macro(self, e);
                Expr::Macro(e)
            },

            // Resource naming on assign:
            Expr::Assign(mut e) => {
                self.assign_stack.push(0);
                e = fold::fold_expr_assign(self, e);
                let num_res_creations = self.assign_stack.pop().unwrap();
                if num_res_creations > 0 {
                    e = self.try_name_resource_on_assign(e);
                }
                Expr::Assign(e)
            },

            Expr::ForLoop(e) => {
                self.loop_begin();
                let mut e = fold::fold_expr_for_loop(self, e);
                self.loop_iterate(&mut e.body);
                self.loop_end(Expr::ForLoop(e))
            },

            Expr::While(e) => {
                self.loop_begin();
                let mut e = fold::fold_expr_while(self, e);
                self.loop_iterate(&mut e.body);
                self.loop_end(Expr::While(e))
            },

            Expr::Loop(e) => {
                self.loop_begin();
                let mut e = fold::fold_expr_loop(self, e);
                self.loop_iterate(&mut e.body);
                self.loop_end(Expr::Loop(e))
            },

            _ => fold::fold_expr(self, e),
        }
    }

    fn fold_stmt(&mut self, s: Stmt) -> Stmt {
        match s {
            // Resource naming on init:
            Stmt::Local(s) => {
                self.assign_stack.push(0);
                let mut s = fold::fold_local(self, s);
                if self.assign_stack.pop().unwrap() > 0 {
                    s = self.try_name_resource_on_init(s);
                }
                Stmt::Local(s)
            }
            _ => fold::fold_stmt(self, s),
        }
    }
}

fn emit_wrapper_arg_forwarding_stmts(
    out_stmts: &mut Vec<Stmt>,
    arg_name: &Ident,
    ty: &Type,
    array_len: &Option<Expr>,
    arg_index: usize)
{
    let mut emit_res_conv = |res_type: &Type|
    {
        match array_len {
            Some(array_len) => {
                out_stmts.push(parse_quote!{
                    let #arg_name = unsafe { #res_type ::from_c_desc_array::<#array_len>(
                        &*( pp_args[#arg_index] as *const [rps_rs::rpsl_runtime::CRpsResourceDesc; #array_len]), res_offset) };
                });
                out_stmts.push(parse_quote!( res_offset += #array_len; ));
            },
            None => {
                out_stmts.push(parse_quote!{
                    let #arg_name = unsafe { #res_type ::from_c_desc(
                        &*( pp_args[#arg_index] as *const rps_rs::rpsl_runtime::CRpsResourceDesc ), res_offset) };
                });
                out_stmts.push(parse_quote!( res_offset += 1; ));
            }
        }
    };

    match ty {
        Type::Array(ty_array) => {
            if array_len.is_some() {
                panic!("Multidimensional array not supported!");
            }
            emit_wrapper_arg_forwarding_stmts(
                out_stmts, arg_name, ty_array.elem.as_ref(), &Some(ty_array.len.clone()), arg_index)
        },
        Type::Group(ty_group) => {
            emit_wrapper_arg_forwarding_stmts(
                out_stmts, arg_name, ty_group.elem.as_ref(), array_len, arg_index)
        },
        Type::Path(ty_path) => {
            let path_str = ty_path.to_token_stream().to_string();

            match path_str.as_str() {
                "Texture" |
                "rps_rs :: Texture" => {
                    emit_res_conv(&parse_quote!(Texture));
                },
                "Buffer" |
                "rps_rs :: Buffer" => {
                    emit_res_conv(&parse_quote!(Buffer));
                },
                _ => {
                    out_stmts.push(parse_quote!{
                        let #arg_name = unsafe { *( pp_args[#arg_index] as *const #ty ) };
                    });
                }
            };
        },
        Type::Ptr(ty_ptr) => {
            out_stmts.push(parse_quote!{
                let #arg_name = unsafe { pp_args[#arg_index] as #ty_ptr };
            });
        },
        Type::Slice(_ty_slice) => {
            todo!("Dynamic length not supported!");
        },
        Type::Reference(ty_ref) => {
            let ty_deref = ty_ref.elem.as_ref();
            emit_wrapper_arg_forwarding_stmts(out_stmts, arg_name, ty_deref, array_len, arg_index);
            out_stmts.push(parse_quote!{
                let #arg_name = &#arg_name;
            });
        },
        _ => {
            out_stmts.push(parse_quote!{
                let #arg_name = unsafe { &*( pp_args[#arg_index] as *const #ty ) };
            });
        }
    }
}

#[proc_macro_attribute]
pub fn render_graph_entry(args: TokenStream, input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as ItemFn);

    let sig_span = input.sig.span();

    let mut rg_processor = parse_macro_input!(args as RenderGraphEntryProcessor);
    rg_processor.block_stack.push(BlockInfo::default());

    let mut output = rg_processor.fold_item_fn(input);

    // Insert function marker guard
    let BlockInfo {
        resource_count,
        node_count,
        child_block_count,
        ..
    } = rg_processor.block_stack[0];

    output.block.as_mut().stmts.insert(0, parse_quote!{
        let _fn_marker_guard = rps_rs::FunctionMarkerGuard::new(
            #resource_count, #node_count, #child_block_count);
    });

    let wrapper_fn_name = syn::Ident::new(
        &format!("entry_wrapper_{}", output.sig.ident),
        sig_span);

    let mut wrapper_output: ItemFn = parse_quote!(
        pub extern "C" fn #wrapper_fn_name ( num_args: u32, pp_args: *const *const core::ffi::c_void, flags: u32 ) {
            let mut res_offset = 0u32;
            let pp_args = unsafe {
                core::slice::from_raw_parts(pp_args, num_args as usize)
            };
        }
    );

    let mut arg_list = Punctuated::<Ident, Token![,]>::new();
    let mut arg_index = 0;

    for arg in &output.sig.inputs {

        let mut converted = false;

        if let FnArg::Typed(arg) = arg {
            if let syn::Pat::Ident(arg_name) = arg.pat.as_ref() {
                emit_wrapper_arg_forwarding_stmts(
                    &mut wrapper_output.block.stmts,
                    &arg_name.ident,
                    &arg.ty,
                    &None,
                    arg_index);

                arg_list.push(arg_name.ident.clone());

                converted = true;
            }
        }

        if !converted {
            wrapper_output.block.stmts.push(parse_quote!( panic!("Failed to convert arg {}", stringify!(#arg)) ));
        }

        arg_index += 1;
    }

    let entry_fn_name = output.sig.ident.clone();
    wrapper_output.block.stmts.push(parse_quote!( Self:: #entry_fn_name ( #arg_list ); ));

    TokenStream::from(quote!{
        #output
        #wrapper_output
    })
}

struct RenderGraphNodeFnProcessor
{
    pub is_first_signature: bool,
}

impl Fold for RenderGraphNodeFnProcessor {
    fn fold_signature(&mut self, i: syn::Signature) -> syn::Signature {

        let mut i = i;

        if self.is_first_signature
        {
            self.is_first_signature = false;

            let arg_template: ItemFn = syn::parse_str("fn f(local_node_id: u32) {}").unwrap();
            let node_id_arg = arg_template.sig.inputs.first().unwrap();
            i.inputs.insert(0, node_id_arg.clone());
        }

        fold::fold_signature(self, i)
    }
}

#[proc_macro_attribute]
pub fn render_graph_node_fn(_args: TokenStream, input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as ItemFn);

    let mut node_processor = RenderGraphNodeFnProcessor { is_first_signature: true };

    let output = node_processor.fold_item_fn(input);

    TokenStream::from(quote!(#output))
}

/// Derive this attribute on custom types used as node parameters.
#[proc_macro_derive(RpsFFIType)]
pub fn derive_rps_ffi_type(item: TokenStream) -> TokenStream {
    let DeriveInput{ ident, .. } = parse_macro_input!(item);

    let output = quote! {

        impl RpsTypeInfoTrait for & #ident {
            const RPS_TYPE_INFO : CRpsTypeInfo = CRpsTypeInfo {
                size: std::mem::size_of::<#ident>() as u16,
                id: RpsBuiltInTypeIds::RPS_TYPE_OPAQUE as u16,
            };

            fn to_c_ptr(&self) -> *const c_void {
                (*self) as *const _ as *const c_void
            }
        }

        impl RpsTypeInfoTrait for #ident {
            const RPS_TYPE_INFO : CRpsTypeInfo = CRpsTypeInfo {
                size: std::mem::size_of::<#ident>() as u16,
                id: RpsBuiltInTypeIds::RPS_TYPE_OPAQUE as u16,
            };
        }
    };

    output.into()
}
