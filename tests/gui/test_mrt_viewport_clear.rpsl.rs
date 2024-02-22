use rps_rs::{*};

render_graph! {
pub mod test_mrt_viewport_clear {

[graphics] node test_unordered_5_mrt_no_ds(
    [readwrite(render_target)] rt1 : Texture : SV_Target1,
    vp : RpsViewport : SV_Viewport,
    clear_color1 : Vec4 : SV_ClearColor1,
    [readwrite(render_target)] rt3 : Texture : SV_Target3,
    [readwrite(render_target)] rt2 : Texture : SV_Target2,
    clear_color0 : Vec4 : SV_ClearColor0,
    clear_color3 : Vec4 : SV_ClearColor3,
    [readwrite(render_target)] rt4 : Texture : SV_Target4,
    [readwrite(render_target)] rt0 : Texture : SV_Target0,
    scissor : UVec4 : SV_ScissorRect);

[graphics] node test_unordered_3_mrt_ds(
    [readwrite(render_target)] rt2 : Texture : SV_Target2,
    [readwrite(depth, stencil)] ds : Texture : SV_DepthStencil,
    clear_depth : f32 : SV_ClearDepth,
    clear_stencil : u32 : SV_ClearStencil,
    [readwrite(render_target)] rt0 : Texture : SV_Target0,
    [readwrite(render_target)] rt1 : Texture : SV_Target1);

[graphics] node test_bind_dsv_write_depth_stencil(
    [readwrite(render_target)] rt : Texture : SV_Target0,
    [readwrite(depth, stencil)] ds : Texture : SV_DepthStencil);
[graphics] node test_bind_dsv_read_depth_write_stencil(
    [readonly(ps)] depth_srv: Texture,
    [readwrite(render_target)] rt : Texture : SV_Target0,
    [readonly(depth)][readwrite(stencil)] ds : Texture : SV_DepthStencil);
[graphics] node test_bind_dsv_read_depth_stencil(
    [readonly(ps)] depth_srv : Texture,
    [readonly(ps)] stencil_srv : Texture,
    [readwrite(render_target)] rt : Texture : SV_Target0,
    [readonly(depth, stencil)] ds : Texture : SV_DepthStencil);

[graphics] node test_mrt_with_array(
    [readwrite(render_target)] rt_arr0 : &[Texture;3] : SV_Target0,
    [readwrite(render_target)] rts1 : Texture : SV_Target5,
    [readonly(ps, cs)] src : &[Texture;12],
    [readwrite(render_target)] rt_arr1 : &[Texture;2] : SV_Target3);

[graphics] node test_large_array(
    [readwrite(render_target)] rt_arr : &[Texture;22],
    [readonly(ps, cs)] src : &[Texture;48]);

[graphics] node test_rt_array([readwrite(render_target)] rt0 : Texture : SV_Target0, clear_col : Vec4 : SV_ClearColor0);

[graphics] node blt_to_swapchain([readwrite(render_target)] dst : &Texture : SV_Target0, [readonly(ps, cs)] src : Texture, dst_viewport : RpsViewport : SV_Viewport);
[graphics] node draw_cube_to_swapchain([readwrite(render_target)] dst : &Texture : SV_Target, [readonly(ps, cubemap)] src : Texture, dst_viewport : RpsViewport : SV_Viewport);

fn test_unordered_mrt_and_clear(backbuffer: &Texture, in_viewport: &UVec4)
{
    let w = in_viewport[2];
    let h = in_viewport[3];

    let wf = w as f32;
    let hf = h as f32;

    let rt0 = create_tex2d!(RpsFormat::R8G8B8A8_UNORM, w, h);
    let rt1 = create_tex2d!(RpsFormat::R16G16B16A16_FLOAT, w, h);
    let rt23 = create_tex2d!(RpsFormat::B8G8R8A8_UNORM, w, h, mip_levels: 1, array_slices: 2);
    let rt4 = create_tex2d!(RpsFormat::R10G10B10A2_UNORM, w, h);
    let ds = create_tex2d!(RpsFormat::R32G8X24_TYPELESS, w, h);

    let clear0 = vec4(1.0, 0.0, 0.0, 1.0);
    let clear1 = vec4(0.0, 1.0, 0.0, 1.0);
    let clear3 = vec4(0.0, 0.0, 1.0, 1.0);

    let sub_viewport = RpsViewport::new_rect(
        in_viewport[0] as f32 + wf * 0.1,
        in_viewport[1] as f32 + hf * 0.2,
        wf * 0.7,
        hf * 0.5);

    test_unordered_5_mrt_no_ds(
        rt1, sub_viewport, clear1, rt23.array_slice(1), rt23.array_slice(0), clear0, clear3, rt4, rt0, uint4(0, 0, w, h));

    blt_to_swapchain(backbuffer, rt0, RpsViewport::new_rect_u(0, 0, w, h));
    blt_to_swapchain(backbuffer, rt1, RpsViewport::new_rect_u(w, 0, w, h));
    blt_to_swapchain(backbuffer, rt23.array_slice(0), RpsViewport::new_rect_u(w * 2, 0, w, h));
    blt_to_swapchain(backbuffer, rt23.array_slice(1), RpsViewport::new_rect_u(w * 3, 0, w, h));

    blt_to_swapchain(backbuffer, rt4, RpsViewport::new_rect(0.0, hf, wf, hf));

    test_unordered_3_mrt_ds(rt23.array_slice(0), ds.format(RpsFormat::D32_FLOAT_S8X24_UINT), 0.5, 0x7f, rt0, rt1);

    blt_to_swapchain(backbuffer, rt0, RpsViewport::new_rect_u(w, h, w, h));
    blt_to_swapchain(backbuffer, rt1, RpsViewport::new_rect_u(w * 2, h, w, h));
    blt_to_swapchain(backbuffer, rt23.array_slice(0), RpsViewport::new_rect_u(w * 3, h, w, h));

    test_rt_array(rt23, float4(0.0, 1.0, 1.0, 1.0));

    blt_to_swapchain(backbuffer, rt23.array_slice(0), RpsViewport::new_rect_u(0, h * 2, w, h));
    blt_to_swapchain(backbuffer, rt23.array_slice(1), RpsViewport::new_rect_u(w, h * 2, w, h));

    test_unordered_5_mrt_no_ds(
        rt1, sub_viewport, clear1, rt23.array_slice(1), rt23.array_slice(0), clear0, clear3, rt4, rt0,
        uint4(w / 3, h / 3, 2 * w / 3, 2 * h / 3));

    blt_to_swapchain(backbuffer, rt0, RpsViewport::new_rect_u(w * 2, h * 2, w, h));
    blt_to_swapchain(backbuffer, rt1, RpsViewport::new_rect_u(w * 3, h * 2, w, h));
}

fn test_array_node_params(backbuffer: &Texture, in_viewport: &UVec4) -> Texture
{
    let w = in_viewport[2];
    let h = in_viewport[3];

    let cubemaps = create_tex2d!(RpsFormat::R8G8B8A8_UNORM, 64, 64, mip_levels: 1, array_slices: 128);

    // TODO: Add a helper to convert subresource range to array of views and vice versa.
    let rt_arr_012 : [Texture;3] = [ cubemaps.array(6, 6), cubemaps.array(2 * 6, 6), cubemaps.array(3 * 6, 6) ];
    let rt3 = cubemaps.array(4 * 6, 6);
    let rt_arr_45 : [Texture;2] = [ cubemaps.array(5 * 6, 6), cubemaps.array(6 * 6, 6) ];

    let mut srvs : [Texture;12] = [Texture::null(); 12];

    for i in 0 .. 12
    {
        let clear_slice : u32 = (if (i < 6) {0} else {7}) * 6 + (i % 6);
        clear_color( &cubemaps.array_slice(clear_slice), float4((i & 1) as f32, ((i & 2) >> 1) as f32, ((i & 4) >> 2) as f32, 1.0) );

        srvs[i as usize] = cubemaps.array_slice(clear_slice);
    }

    test_mrt_with_array( &rt_arr_012, rt3, &srvs, &rt_arr_45 );

    for i in 0 .. 8
    {
        draw_cube_to_swapchain(backbuffer, cubemaps.array(6 * i, 6).cubemap(), RpsViewport::new_rect_u(w * (i % 4), h * (3 + i / 4), w, h));
    }

    let mut large_rtv_array : [Texture; 22] = [Texture::null(); 22];
    let mut large_srv_array : [Texture; 48] = [Texture::null(); 48];

    for (i, rtv) in large_rtv_array.iter_mut().enumerate()
    {
        *rtv = cubemaps.array_slice(48 + i as u32);
    }

    for (i, srv) in large_srv_array.iter_mut().enumerate()
    {
        *srv = cubemaps.array_slice(i as u32);
    }

    test_large_array(&large_rtv_array, &large_srv_array);

    return cubemaps;
}

fn test_depth_stencil_rw(backbuffer: &Texture, in_viewport: &UVec4)
{
    let w = in_viewport[2];
    let h = in_viewport[3];

    let off_screen_img = create_tex2d!(RpsFormat::R8G8B8A8_UNORM, w, h);
    let depth_stencil = create_tex2d!(RpsFormat::D32_FLOAT_S8X24_UINT, w, h);

    clear_color(&off_screen_img, float4(1.0, 0.0, 0.0, 1.0));
    clear_depth_stencil(&depth_stencil, RpsClearFlags::DEPTHSTENCIL, 1.0, 0);

    test_bind_dsv_write_depth_stencil(off_screen_img, depth_stencil);

    blt_to_swapchain(backbuffer, off_screen_img, RpsViewport::new_rect_u(0, h * 5, w, h));
    blt_to_swapchain(
        backbuffer, depth_stencil.format(RpsFormat::R32_FLOAT_X8X24_TYPELESS), RpsViewport::new_rect_u(w, h * 5, w, h));

    test_bind_dsv_read_depth_write_stencil(depth_stencil.format(RpsFormat::R32_FLOAT_X8X24_TYPELESS),
                                           off_screen_img,
                                           depth_stencil);

    test_bind_dsv_read_depth_stencil(depth_stencil.format(RpsFormat::R32_FLOAT_X8X24_TYPELESS),
                                     depth_stencil.format(RpsFormat::X32_TYPELESS_G8X24_UINT),
                                     off_screen_img,
                                     depth_stencil);

    blt_to_swapchain(backbuffer, off_screen_img, RpsViewport::new_rect_u(w * 2, h * 5, w, h));
    blt_to_swapchain(
        backbuffer, depth_stencil.format(RpsFormat::R32_FLOAT_X8X24_TYPELESS), RpsViewport::new_rect_u(w * 3, h * 5, w, h));
}

[graphics] node test_buffer_rtv_clear(
    [readwrite(rendertarget)] rt0 : Buffer : SV_Target0,
    clear_color_1 : Vec4 : SV_ClearColor0);

[graphics] node test_buffer_rtv(
    [readwrite(rendertarget)] rt0 : Buffer : SV_Target0,
    clear_color_1 : Vec4 : SV_ClearColor0);

fn test_buffer_rtv_and_clear(backbuffer: &Texture, in_viewport: UVec4)
{
    // TODO: We want to add rtv_buf as well as RPS_BUFFER_WHOLE_SIZE defs in RPSL.
    let w = in_viewport[2];
    let h = in_viewport[3];

    let row_pitch = ((w * 4) + 255) & !255u32;
    let w_aligned = row_pitch / 4;

    let buf_size   = w_aligned * h * 4;
    let buf_offset = buf_size / 2;
    let clear0    = float4(1.0, 0.0, 0.0, 1.0);

    let buf           = create_buffer!(buf_size.into(), flags: ResourceFlags::ROWMAJOR_IMAGE_BIT);
    let buf_view_first  = buf.range(0, buf_offset.into()).unwrap().format(RpsFormat::B8G8R8A8_UNORM);
    let buf_view_second = buf.range(buf_offset.into(), (buf_size - buf_offset).into()).unwrap().format(RpsFormat::B8G8R8A8_UNORM);

    test_buffer_rtv_clear(buf_view_first, clear0);
    test_buffer_rtv(buf_view_second, clear0);

    let rt0 = create_tex2d!(RpsFormat::R8G8B8A8_UNORM, w_aligned, h);
    copy_buffer_to_texture(
        &rt0, uint3(0, 0, 0), &buf, 0, row_pitch, uint3(w_aligned, h, 1), uint3(0, 0, 0), uint3(w_aligned, h, 1));

    blt_to_swapchain(backbuffer, rt0, RpsViewport::new_rect_u(in_viewport[0], in_viewport[1], w, h));
}

export fn rps_main([readonly(present)] backbuffer: &Texture, b_buffer_rtv_supported: bool)
{
    let backbuffer_desc = backbuffer.desc();

    let dst_viewport = uint4(0, 0, backbuffer_desc.width as u32 / 4, backbuffer_desc.height / 6);

    test_unordered_mrt_and_clear(backbuffer, &dst_viewport);

    if (!b_buffer_rtv_supported)
    {
        test_buffer_rtv_and_clear(backbuffer, uint4(0, 0, 256, 120));
    }

    test_array_node_params(backbuffer, &dst_viewport);

    test_depth_stencil_rw(backbuffer, &dst_viewport);
}

}
}
