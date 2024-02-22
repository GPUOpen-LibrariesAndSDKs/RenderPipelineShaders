rps_rs::render_graph! {
mod test_scheduler {

[graphics] node draw(id: u32, [readwrite(render_target)] rt : &Texture : SV_Target0);
[compute]  node comp_draw(id: u32, [readwrite(cs)] rt : &Texture);
[graphics] node blt(id: u32, [readwrite(render_target)] dst : &Texture : SV_Target0, [readonly(ps)] src: &Texture);
[graphics] node blt_all(id: u32, [readwrite(render_target)] dst : &Texture : SV_Target0, [readonly(ps)] srcs: &[Texture; 12]);

export fn program_order(output: &Texture)
{
    let desc = output.desc();

    let t0 = create_texture!(desc);
    let t1 = create_texture!(desc);
    let t2 = create_texture!(desc);
    let t3 = create_texture!(desc);
    let t4 = create_texture!(desc);
    let t5 = create_texture!(desc);

    draw(0, &t0);
    draw(1, &t1);
    draw(2, &t2);
    draw(3, &t3);
    draw(4, &t4);
    draw(5, &t5);
    blt(6, output, &t0);
    blt(7, output, &t1);
    blt(8, output, &t2);
    blt(9, output, &t3);
    blt(10, output, &t4);
    blt(11, output, &t5);

    draw(12, &t0);
    blt(13, output, &t0);
    draw(14, &t1);
    blt(15, output, &t1);
    draw(16, &t2);
    blt(17, output, &t2);
    draw(18, &t3);
    blt(19, output, &t3);
    draw(20, &t4);
    blt(21, output, &t4);
    draw(22, &t5);
    blt(23, output, &t5);
}

export fn memory_saving(output: &Texture)
{
    let desc = output.desc();

    let t0 = create_texture!(desc);
    let t1 = create_texture!(desc);
    let t2 = create_texture!(desc);
    let t3 = create_texture!(desc);
    let t4 = create_texture!(desc);
    let t5 = create_texture!(desc);

    draw(0, &t0);
    draw(1, &t1);
    draw(2, &t2);
    draw(3, &t3);
    draw(4, &t4);
    draw(5, &t5);
    blt(6, output, &t0);
    blt(7, output, &t1);
    blt(8, output, &t2);
    blt(9, output, &t3);
    blt(10, output, &t4);
    blt(11, output, &t5);
}

export fn random_order(output: &Texture)
{
    let desc = output.desc();

    let mut tmp: [Texture; 12] = [Texture::null(); 12];

    for i in 0..12
    {
        tmp[i] = create_texture!(desc);
        draw(i as u32, &tmp[i]);
    }

    blt_all(12, output, &tmp);
}

export fn dead_code_elimination(output: &Texture, b_blt_0: bool, b_blt_1: bool)
{
    let desc = output.desc();

    let t0 = create_texture!(desc);
    let t1 = create_texture!(desc);

    draw(0, &t0);

    draw(1, &t1);

    if (b_blt_0)
    {
        blt(2, output, &t0);
    }

    if (b_blt_1)
    {
        blt(3, output, &t1);
    }
}

export fn gfx_comp_batching(output: &Texture)
{
    let desc = output.desc();

    let t0 = create_texture!(desc);
    let t1 = create_texture!(desc);
    let t2 = create_texture!(desc);
    let t3 = create_texture!(desc);
    let t4 = create_texture!(desc);
    let t5 = create_texture!(desc);

    draw(0, &t0);
    comp_draw(1, &t1);
    comp_draw(2, &t2);
    draw(3, &t3);
    draw(4, &t4);
    comp_draw(5, &t5);

    blt(6, output, &t0);
    blt(7, output, &t1);
    blt(8, output, &t2);
    blt(9, output, &t3);
    blt(10, output, &t4);
    blt(11, output, &t5);
}

[graphics] node draw_mrt_wo_rt0(id: u32, [writeonly(render_target)] rt : &Texture : SV_Target0, [readwrite(render_target)] rt1 : &Texture : SV_Target1);
[graphics] node draw_with_ds_clear(id: u32, [readwrite(render_target)] rt : &Texture : SV_Target0, [readwrite(depth)][writeonly(stencil)] d : &Texture : SV_DepthStencil, clearDepth : f32 : SV_ClearDepth, clearStencil : u32 : SV_ClearStencil);
[graphics] node draw_with_ds_wo_stencil(id: u32, [readwrite(render_target)] rt : &Texture : SV_Target0, [readwrite(depth)][writeonly(stencil)] d : &Texture: SV_DepthStencil);
[graphics] node draw_with_ds_wo_depth(id: u32, [readwrite(render_target)] rt : &Texture : SV_Target0, [writeonly(depth)][readwrite(stencil)] d : &Texture: SV_DepthStencil);
[graphics] node draw_with_ds_wo_depthstencil(id: u32, [readwrite(render_target)] rt : &Texture : SV_Target0, [writeonly(depth, stencil)] d : &Texture: SV_DepthStencil);
[graphics] node draw_with_ds_ro_depthstencil(id: u32, [readwrite(render_target)] rt : &Texture : SV_Target0, [readonly(depth, stencil)] d : &Texture: SV_DepthStencil);
[graphics] node draw_with_depth_srv(id: u32, [readwrite(render_target)] rt : &Texture : SV_Target0, [readonly(ps)] d : &Texture);
[graphics] node draw_with_stencil_srv(id: u32, [readwrite(render_target)] rt : &Texture : SV_Target0, [readonly(ps)] s : &Texture);

[graphics] node draw_with_overlapped_views(id : u32,
                                      [readonly(cs)] t0 : &Texture,
                                      [writeonly(cs)] u0 : &Texture,
                                      [readonly(cs, discard_before)] t1 : &Texture,
                                      [readonly(cs)] t2 : &Texture,
                                      [readonly(cs)] t3 : &Texture,
                                      [readonly(cs, discard_after)] t4 : &Texture,
                                      [readonly(cs)] t5 : &Texture,
                                      [readonly(cs)] t6 : &Texture,
                                      [readonly(cs)] t7 : &Texture,
                                      [readonly(cs, discard_after)] t8 : &Texture);

[graphics] node draw_with_overlapped_views2(id : u32,
                                       [readonly(cs, discard_after)] t0 : &Texture,
                                       [readonly(cs, discard_before)] t1 : &Texture);

export fn subres_data_lifetime(output : &Texture)
{
    let desc = output.desc();

    let mip_levels  = 5u32;
    let array_slices = 10u32;

    let t0 = create_tex2d!(desc.format, desc.width as u32, desc.height, mip_levels: mip_levels, array_slices: array_slices, temporal_layers: 1);
    let t1 = create_tex2d!(desc.format, desc.width as u32, desc.height, mip_levels: 1, array_slices: 1, temporal_layers: 3);
    let t2 = create_tex2d!(desc.format, desc.width as u32, desc.height, mip_levels: 5, array_slices: 1, temporal_layers: 1);
    let d0 = create_tex2d!(RpsFormat::D32_FLOAT_S8X24_UINT, desc.width as u32, desc.height, mip_levels: 1, array_slices: 1, temporal_layers: 1);

    draw(0, &t0.array_slice(0).mip_level(0));
    // array[1].mip[0] to be discarded
    draw_mrt_wo_rt0(1, &t0.array_slice(1).mip_level(0), &t0.array_slice(0).mip_level(0));

    draw(2, &t0.array_slice(2).mip_level(0));
    // array[2].mip[0] to be discarded
    draw_mrt_wo_rt0(3, &t0.array_slice(2).mip_level(0), &t0.array_slice(3).mip_level(0));

    draw(4, &t0.array(0, 4).mip_level(0));
    // array[0, 1].mip[0] to be discarded
    draw_mrt_wo_rt0(5, &t0.array(0, 2).mip_level(0), &t0.array_slice(3).mip_level(0));

    draw(6, &t0.array(0, 3).mip_level(0));
    // discard range not fully overlapping
    draw_mrt_wo_rt0(7, &t0.array(1, 3).mip_level(0), &t0.array_slice(5).mip_level(0));

    draw(8, &t1);
    draw_with_ds_clear(9, &t1, &d0, 1.0, 0);
    draw_with_ds_wo_stencil(10, &t1, &d0);
    draw_with_ds_wo_depth(11, &t1, &d0);
    draw_with_depth_srv(12, &t1, &d0.format(RpsFormat::R32_FLOAT_X8X24_TYPELESS));
    draw_with_ds_wo_depthstencil(13, &t1, &d0);
    draw_with_stencil_srv(14, &t1, &d0.format(RpsFormat::X32_TYPELESS_G8X24_UINT));
    draw_with_ds_ro_depthstencil(15, &t1, &d0);

    blt(16, output, &t0);
    blt(17, output, &t1);

    draw(18, &t2.mip_level(0)); // discard_before
    draw(19, &t2.mip_level(1)); // discard_before
    draw(20, &t2.mip_level(2)); // discard_before | discard_after (patched)
    draw(21, &t2.mip_level(3)); // discard_before

    draw_with_overlapped_views(22,
        &t2.mip_level(0), // RO                 -> 0
        &t2.mip_level(2), // WO                 -> discard_before | discard_after (patched)
        &t2.mip_level(0), // RO(discard_before) -> discard_before
        &t2.mip_level(0), // RO                 -> 0
        &t2.mip_level(1), // RO                 -> discard_after (patched)
        &t2.mip_level(1), // RO(discard_after)  -> discard_after
        &t2.mip_level(1), // RO                 -> discard_after (patched)
        &t2.mip_level(3), // RO                 -> 0
        &t2.mip_level(3), // RO                 -> 0
        &t2.mip_level(4));// RO(discard_after)  -> discard_after | discard_before

    draw_with_overlapped_views(23,
        &t2.mip_level(0), // RO                 -> 0
        &t2.mip_level(2), // WO                 -> discard_before
        &t2.mip_level(1), // RO(discard_before) -> discard_before
        &t2.mip_level(3), // RO                 -> discard_after (patched)
        &t2.mip_level(3), // RO                 -> discard_after (patched)
        &t2.mip_level(4), // RO(discard_after)  -> discard_after | discard_before(patched)
        &t2.mip_level(3), // RO                 -> discard_after (patched)
        &t2.mip_level(3), // RO                 -> discard_after (patched)
        &t2.mip_level(3), // RO                 -> discard_after (patched)
        &t2.mip_level(4));// RO(discard_after)  -> discard_after | discard_before(patched)

    blt(24, output, &t2.mip_level(1)); // 0
    blt(25, output, &t2.mip_level(2)); // 0
    blt(26, output, &t2.mip_level(4)); // discard_before | discard_after (patched)

    draw_with_overlapped_views2(27, &t2.mips(0, 3), &t2.mips(2, 3)); // discard_after, discard_before.

    blt(28, output, &t2.mip_level(2)); // 0
    blt(29, output, &t2.mip_level(1)); // discard_before (patched)
    blt(30, output, &t2);         // discard_after
}

}
}
