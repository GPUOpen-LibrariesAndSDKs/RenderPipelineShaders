rps_rs::render_graph!{
mod test_triangle {

[graphics] node Triangle([readwrite(render_target)] render_target : &Texture : SV_Target0, triangle_id: u32 );

export fn main([readonly(present)] back_buffer: &Texture)
{
    // clear and then render geometry to back buffer
    clear_color(back_buffer, float4(0.0, 0.2, 0.4, 1.0));
    Triangle(back_buffer, 0);
}

}}
