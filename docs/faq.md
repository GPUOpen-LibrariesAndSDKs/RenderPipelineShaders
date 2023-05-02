# RPS FAQs

**Q: Should frameIndex in the render graph update info struct be a running index?**

A: Yes, it is expected to be incremental instead of say wrapping around for num_backbuffer frames. Additionally, it is
important to provide the correct value for `gpuCompletedFrameIndex`.

If you pass `RPS_GPU_COMPLETED_FRAME_INDEX_NONE` for enough frames, this will overflow the maximum amount of RPS
internal frame resources. RPS cannot release these until it knows that the frames have completed.

Hence, you must pass a value to `gpuCompletedFrameIndex` that is an index to a prior frame that is known to have
completed execution on the GPU.

**Q: Can I use RPS to create resources for use outside a render graph?**

A: Yes!

Consider the following snippet,

```rpsl
export void main([readonly(present)] texture backBuffer, [readonly(ps)] out texture offscreenRT)
{
    ResourceDesc backbufferDesc   = backBuffer.desc();
    uint32_t     width            = (uint32_t)backbufferDesc.Width / 2;
    uint32_t     height           = (uint32_t)backbufferDesc.Height / 2;
    RPS_FORMAT   backbufferFormat = backbufferDesc.Format;
    offscreenRT = create_tex2d(backbufferFormat, width, height);
    // do stuff to offscreenRT ...
}
```

By marking an entry parameter with the HLSL keyword `out`, we specify that this external resource is an output resource.
The keyword `out` can only be used with the entry node parameters. When invoking the render graph, it is not needed to
provide any output resources. Instead, after execution of the render graph, they are available for query. Additional
work could then be recorded to be done on these resources from outside the scope of render graph execution.

To query information about output parameters for a render graph, you may use
`rpsRenderGraphGetOutputParameterResourceInfos`. The information structures most importantly contain the `hResource`
field - the graphics API handle to the resource.

For more details, please see [test_output_resource_d3d12.cpp](/tests/gui/test_output_resource_d3d12.cpp).

**Q: Can I #include a .rpsl inside a .rpsl?**

A: Yes! RPSL is a derivative of HLSL. This is supported by the DirectX Shader Compiler (which the RPSL compiler is a
fork of).

You may further ask, since this RPSL module can be generated into a .c, is the #include retained in the .c or does it
get inlined as soon as possible and we therefore see the compiled form of the inline in the .c? The behavior of the RPSL
compiler is the latter.

**Q: Can I allow compute work done by multiple nodes to overlap on the GPU?**

A: In RPSL there is an access attribute of `[relaxed]`. When applied to a pair of nodes with at least one RW / WO node
in the pair, this permits the pair to be reordered in the linear command stream, supposing that they are not separated
with a transition node.

If the two nodes happen to access the resource as an unordered access view (`uav = [readwrite(ps, cs)] texture`), the
execution of the commands recorded in the nodes may overlap on the GPU.

**Q: Can I pass an array of resource handles into the entry?**

While it is _technically possible_ to pass arrays of structures into the RPSL program, the RPS runtime does not support
this. When a pointer element in `ppArgResources` actually represents an array, this is interpreted / handled only for
the case that the corresponding resource description in `ppArgs` describes a temporal resource.
