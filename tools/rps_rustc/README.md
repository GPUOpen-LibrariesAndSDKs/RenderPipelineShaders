# RPS-RustC Intro

RPS-RustC is a rust lib + a small compilation driver tool to allow using Rust as an RPS frontend language. It tries to mimic RPSL (the HLSL-based RPS DSL) in both syntax and functionality.

Comparing below RPSL code with the equivalent RPS-Rust code:

```js
// Triangle.rpsl:

// Node declaration:
graphics node triangle([readwrite(render_target)] texture t : SV_Target0, uint triangleId);

// Render graph entry:
export void main([readonly(present)] texture back_buffer)
{
    clear(back_buffer, float4(0.0, 0.2, 0.4, 1.0));
    triangle(back_buffer, 0);
}
```

```rust
use rps_rs::{Texture, clear_color, render_graph};

// Macro that defines a render graph module.
render_graph! {
// Explicit module name. Multiple modules can coexist in the same file.
pub mod triangle {
    // node declaration:
    [graphics] triangle([readwrite(render_target)] t: &Texture : SV_Target0, triangle_id: u32);

    // Render graph entry:
    export main([readonly(present)] back_buffer: &Texture)
    {
        clear_color(back_buffer, &[0.0, 0.2, 0.4, 1.0]);
        triangle(back_buffer, 0);
    }
}}
```

They are pretty similar apart from some formalities.

## How to use?

### High level workflow

One way is to embed `render_graph!{}` modules directly in Rust engine code after adding the rps_rs crate as a dependency. The RPS entry points defined in the module becomes a unmangled static variable `pub static rpsl_M_<$module_name>_E_<$entry_name> : rps_rs::CRpslEntryPtr;`, which can be referenced directly.

Another way is to write a standalone Rust file just like an RPSL file, which only contains `render_graph!{}` blocks. Build /tool/cargo_rpslc (or use the pre-built binaries), and run `cargo_rpslc` to compile the RPS-Rust file into a binary (Only cdynlib is supported currently. Also this is a standalone tool that invokes `cargo` rather than a real cargo subcommand). The binary can be loaded in the same way as an regular RPSL-DLL. `cargo_rpslc` tool takes care of creating a temporary rust project and referencing the rps_rs lib. It's assumed that the rps_rs source is in the directory `../../rps_rs/` relative to the directory containing cargo_rpslc executable.

Cargo_rpslc usage:
```
cargo_rpslc <source_file_name> [cargo_rpslc options...] [-- [cargo options...]]
cargo_rpslc options:
    -o: output file name
    -od: output directory
    -expand: expand macros (use cargo-expand subcommand)
```

### Porting RPSL code to RPS-Rust

The RPSL `texture` and `buffer` types correspond to `rps_rs::Texture` and `rps_rs::Buffer`. Method names are changed slightly due to not supporting overload. Both value and reference types are supported through the FFI (E.g. as node/entry point parameters).

Most primitive types can be passed through the FFI directly. Fixed array reference `&[T; N]` is also supported. To use custom struct / enum types across FFI, ensure they have C layout (`repr[C]`) and derive the `RpsFFIType` attribute. For example:

```Rust
#[repr(C)]
#[derive(RpsFFIType)]
pub struct MyStruct {
    a: i32,
    b: f32,
    ...
}

render_graph! {
    [graphics] node_1(t: &Texture, a: &[f32;4], b: MyStruct, c: u32);
    ...
}
```

## How does it work?

The RPSL compiler mainly does 3 jobs (in addition to the basic compilation function):
- Collecting node and entry point reflection info;
- Inserting code to help uniquely identifying resources and nodes created or called at a certain source location;
- Name resources upon resource creation based on the variable names of the closest assignment of the resource.

With Rust macros we can loosely achieve the same result. The RPSL-like syntax is parsed and translated by both declarative (`rps_rs::declare_nodes_impl!`, `rps_rs::define_exports_impl`, etc.) and procedural macros (`render_graph_node_fn`, `render_graph_entry`, etc.); Reflection info is generated as const data arrays (with the exception of a few properties that require one-time runtime initialization of static data). The `render_graph_node_fn` macro generates node call stubs that marshal arguments. The `render_graph_entry` macro walks the AST to count number of resource creations and node calls (This is the hacky part, see [Limitations](#Limitations)), then inserts markers, function calls and additional parameters as needed.

Because RPSL/HLSL always inline, the RPSL runtime has made assumptions which don't hold with true function calls when generating node and resource IDs. This has been modified to support global ID generation from function-scope local IDs, allowing calling functions defined in the same module or different modules.

## Limitations

Unlike rps-hlslc which manipulates LLVM IR directly, rps-rustc is based on Rust macros and works above the AST level. There are some obvious limitations:

- Cross-module node reuseing is limited. Macro expansion can't peek into another module, so referencing nodes declared in a different module requires marking them as "node calls" explicitly for now. Also, to ensure that nodes declared in different modules can be addressed consistently, all nodes are put in one global table within a binary, this may bloat the node table and requires user to guarantee unique node names even if they are defined in different modules.

- Built-in resource creation functions and built-in nodes are recognized by path matching. So they must be directly used and not wrapped in user defined macros.
