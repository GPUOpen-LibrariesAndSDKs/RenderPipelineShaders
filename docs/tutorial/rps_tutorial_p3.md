# RPS Tutorial Part 3 - Interoperation between RPS and the host

Copyright (c) 2024 Advanced Micro Devices, Inc.

The AMD Render Pipeline Shaders (RPS) SDK is released under the MIT LICENSE. Please see file
[LICENSE.txt](LICENSE.txt) for full license details.

## Introduction

This tutorial guides the reader through extended RPS API usage for data interoperation of the host app and the RPS
runtime. It makes modifications to the [hello_triangle.cpp](/examples/hello_triangle.cpp) example that was built in the
first tutorial part. To enable such modifications, set `c_bBreathing` to true. The modifications involve the creation of
a new, secondary render graph that renders a "breathing" triangle.

## Overview

- [Binding member callbacks to nodes](#binding-member-callbacks-to-nodes)
- [Retrieve general argument and node information in the callback](#retrieve-general-argument-and-node-information-in-the-callback)
- [Accessing graphics context bindings in the callback](#accessing-graphics-context-bindings-in-the-callback)
  - [Creating Vulkan PSOs upfront](#creating-vulkan-psos-upfront)
- [Auto unwrapping of params into the node callback](#auto-unwrapping-of-params-into-the-node-callback)
  - [Writing a custom unwrapper](#writing-a-custom-unwrapper)

## Binding member callbacks to nodes

We saw in the first tutorial part that if our node callback is a member function but we are binding a non-member one,
that there was a required dance of storing the `this` pointer in the user data of `RpsCmdCallbackContext` (i.e. the
`pCmdCallbackContext` field).

We can avoid such boilerplate by directly binding to a member callback. This is accomplished by switching to a different
overload of `rpsProgramBindNode`.

We'll use this method in binding the node that our new render graph calls,

```C++
class HelloTriangle : public RpsAfxD3D12Renderer
{
    virtual void OnInit(ID3D12GraphicsCommandList*         pInitCmdList,
                        std::vector<ComPtr<ID3D12Object>>& tempResources) override 
    {
        // ... 
        AssertIfRpsFailed(rpsProgramBindNode(rpsRenderGraphGetMainEntry(m_rpsRenderGraph),
                                             "TriangleBreathing",
                                             &HelloTriangle::DrawTriangleBreathingCb,
                                             this));
    }
    // ...
    void DrawTriangleBreathingCb(const RpsCmdCallbackContext* pContext)
    {
        // ...
    }
    // ...
};
```

For this overload, instead of passing some user context, we must pass the `this` pointer. This ensures that RPS may
access the member callback via our class instance through a pointer to member.

## Retrieve general argument and node information in the callback

In our extension to the first tutorial part, we'll render the triangle without distortion despite any resize of the
application window. We'll use RPS to calculate a corrective aspect ratio parameter and pass this to the host app for
providing to the shader.

Here's the .rpsl for our new render graph,

```rpsl
// ...

graphics node TriangleBreathing([readwrite(rendertarget)] texture renderTarget
                                : SV_Target0, float oneOverAspectRatio);

export void mainBreathing([readonly(present)] texture backbuffer)
{
    ResourceDesc backbufferDesc = backbuffer.desc();

    uint32_t width  = (uint32_t)backbufferDesc.Width;
    uint32_t height = backbufferDesc.Height;

    float oneOverAspectRatio = height / (float)width;

    // clear and then render geometry to backbuffer
    clear(backbuffer, float4(0.0, 0.2, 0.4, 1.0));
    TriangleBreathing(backbuffer, oneOverAspectRatio);
}
```

Next, we'll update the `TriangleBreathing` callback to retrieve the aspect ratio parameter,

```C++
void DrawTriangleBreathingCb(const RpsCmdCallbackContext* pContext)
{   // ...
    float oneOverAspectRatio = *rpsCmdGetArg<float, 1>(pContext);
    pCmdList->SetGraphicsRoot32BitConstant(0, *(const UINT*)&oneOverAspectRatio, 0);
    // ...
}
```

Here, we use one of the `rpsCmdGetArg` template overloads to retrieve the node parameter. The function doesn't check if
the type casting is valid. If needed, you may use `rpsCmdGetParamDesc` to query the type info and array size of the
parameter before calling this function.

And that's it!

There are a few other changes required on the DX12 side to get things working (e.g. modifying the HLSL shader), but
we'll omit those. For now, please enjoy the image below,

![the aspect correct triangle](/docs/assets/rps_tutorial_p3.PNG)

Now, passing arbitrary parameters from the node invocation into the callback is not the most typical case. The likely
and useful thing to do would be for example to retrieve a descriptor associated with some resource view.

How is this done?

On the DX12 side, this is accomplished via a call to `rpsD3D12GetCmdArgDescriptor`. If needed, the resource handle can
also be retrieved via `rpsD3D12GetCmdArgResource`. In general, querying any information about a node parameter or the
node itself can be done by calling a function within the `rpsCmdGet*` family of functions. For example,
`rpsCmdGetArgResourceDesc` can be used to get the description of the resource under the view. On the VK side, there are
similarly the `rpsVKGetCmdArg*` series of functions for querying image/buffer views, etc.

If the node declaration parameter is an array of resource views, the `rpsCmdGetArg*Array`, `rpsD3D12*Array`, and
`rpsVK*Array` family of functions can be used to retrieve information from the node argument.

Let's play with the idea of retrieving descriptors from node arguments via a hypothetical example in which a triangle is
rendered with a procedurally generated texture.

Consider the following node declaration,

```rpsl
compute node PerlinNoise([readwrite(cs)] texture t);
```

This declares a node that we intend to use for generating perlin noise via a compute shader. We would sample this
texture when rendering the triangle.

Here's the rest of the RPSL,

```rpsl
graphics node DrawTriangle(rtv renderTarget : SV_Target0, srv noise);

export void main([readonly(present)] texture backBuffer)
{
    clear(backBuffer, float4(0.0, 0.0, 0.0, 1.0));

    uint noiseDim = 512;
    texture noise = create_tex2d(RPS_FORMAT_R8G8B8A8_UNORM, noiseDim, noiseDim);

    PerlinNoise(noise);

    DrawTriangle(backBuffer, noise);
}
```

Finally, on the host app side, we would modify the callback to retrieve the SRV descriptor,

```C++
void DrawTriangle(const RpsCmdCallbackContext* pContext)
{
    D3D12_CPU_DESCRIPTOR_HANDLE srv;
    rpsD3D12GetCmdArgDescriptor(pContext, 1, &srv);
    // ...
}
```

One last thing to note, and something that you may have pieced together for yourself, is that such descriptors
associated with resource view node parameters are stored in graphics API heaps that RPS manages. On the DX12 side, these
are non-shader visible heaps.

## Accessing graphics context bindings in the callback

Firstly, what exactly are graphics context bindings?

This is a concept that was mentioned in the first and second tutorial parts. Here's a refresher:

```rpsl
graphics node DrawTriangle(rtv backbuffer : SV_Target0);
```

Such a node has the `backbuffer` texture view bound to the semantic `SV_Target0`. This means that before entering the
node callback, the default behavior of RPS is to insert commands into your command buffer for binding this render target
to the pipeline. For the remainder of the tutorial parts, we'll refer canonically to this behavior as the _node
preamble_. Any shutdown behavior, such as MSAA resolves (should you have decorated a node parameter with
`SV_ResolveTarget[n]`), occurs at the _node postamble_.

Returning to the original question, we can say that graphics context bindings are state setup by the node preamble and
torn down at the node postamble.

As mentioned in the previous section, the `rpsCmdGet*` family of functions can be used to query general information
about a node. Such information includes the state that was setup by the preamble.

For example, querying such info is useful on the VK side to setup PSOs. The host app can query the render pass that was
setup by the preamble to use for creating an appropriate PSO.

Consider the below snippets,

```C++
// DrawGeo is the function bound as the node callback.
void DrawGeo(const RpsCmdCallbackContext* pContext)
{
    CreateDefaultPso(pContext);
    // record commands ...
}

void CreateDefaultPso(const RpsCmdCallbackContext* pContext)
{
    if (!m_pso)
    {
        VkRenderPass rp;
        rpsVKGetCmdRenderPass(pContext, &rp);
        CreateGraphicsPipeline(rp, &m_pso);
    }
}

void CreateGraphicsPipeline(VkRenderPass renderPass, VkPipeline* pPso)
{
    // ...
    VkGraphicsPipelineCreateInfo psoCI = {};
    // ...
    psoCI.renderPass = renderPass;
    // ...
}
```

### Creating Vulkan PSOs upfront

It can be expensive to compile PSOs and therefore apps typically desire to create them upfront. If using a Vulkan
runtime backend, creating PSOs early, and not using custom render passes, the host app must use a compatible render pass
object to the one that is automatically setup by the node callback preamble.

There isn't a way to query the structure of the RPS-created RP; however, it is a simple one, and we will describe its
structure here:

- Defaults are otherwise used.
- It contains a single subpass.
- It has no dependencies.

When taking this approach, make sure to ensure the attachments and descriptions are setup properly according to the
access attributes and semantics of the node, as well as the resource views passed to the node.

As mentioned in the [second tutorial part](/docs/tutorial/rps_tutorial_p2.md#hlsl-semantics), the node semantics control
the attachments used with the RP created by RPS.

## Auto unwrapping of params into the node callback

If your host application is written in C++, you may leverage auto unwrapping of parameters in the node callback. This is
supported only for the template versions of `rpsProgramBindNode`.

Continuing with our modifications to [hello_triangle.cpp](/examples/hello_triangle.cpp), let's add a time parameter to
our `TriangleBreathing` callback.

Starting with the RPSL modifications,

```rpsl
graphics node TriangleBreathing([readwrite(rendertarget)] texture renderTarget
                                : SV_Target0, float oneOverAspectRatio, float timeInSeconds);

export void mainBreathing([readonly(present)] texture backbuffer, float timeInSeconds)
{
    // ...

    TriangleBreathing(backbuffer, oneOverAspectRatio, timeInSeconds);
}
```

Then, we can trivially do the following,

```C++
void DrawTriangleBreathingCb(const RpsCmdCallbackContext* pContext,
                             rps::UnusedArg               u0,
                             float                        oneOverAspectRatio,
                             float                        timeInSeconds)
{
    // ...
    oneOverAspectRatio *= abs(sin(timeInSeconds));
    pCmdList->SetGraphicsRoot32BitConstant(0, *(const UINT*)&oneOverAspectRatio, 0);
    // ...
}
```

`rps::UnusedArg u0` is meant to align with the `backbuffer` param of the `Triangle` node. RPS provides this special
struct type for unwrap cases like this, where we wont be using particular parameters but still require that the rest of
the parameters are aligned.

The last step is to get the `timeInSeconds` parameter into the `main` entry. We'll update our call to
`rpsRenderGraphUpdate`,

```C++
virtual void OnUpdate(uint32_t frameIndex) override
{
    // ...
    float time = float(RpsAfxCpuTimer::SecondsSinceEpoch().count());
    // ...
    RpsConstant argData[] = {&backBufferDesc, &time};
    // ...
    AssertIfRpsFailed(rpsRenderGraphUpdate(m_rpsRenderGraph, &updateInfo));
    // ...
}
```

And with that, our triangle now appears to be breathing!

![.GIF of a breathing triangle](/docs/assets/rps_tutorial_p3.gif)

Again, the case of interest is when node parameters are resource views. When unwrapping these, they can be unwrapped to
e.g. `D3D12_CPU_DESCRIPTOR_HANDLE`, `ID3D12Resource*`, `VkImage`, `VkImageView`, etc.

Continuing with the hypothetical example from
[two sections above](#retrieve-general-argument-and-node-information-in-the-callback), our `DrawTriangle` function could
be transformed as such,

```C++
void DrawTriangle(const RpsCmdCallbackContext* pContext, rps::UnusedArg u0, D3D12_CPU_DESCRIPTOR_HANDLE srv)
{
    // ...
}
```

More generally, we could consider an auto-unwrap case where the node parameter is an array of resource views, as opposed
to the example above, demonstrating just one view. However, in this case, auto-unwrap is currently not supported.
`rpsCmdGetArgRuntimeResourceArray` can be used instead.

### Writing a custom unwrapper

The various data types such as `D3D12_CPU_DESCRIPTOR_HANDLE`, `ID3D12Resource*`, etc that a node resource view parameter
can be unwrapped to are just the tip of the iceberg when it comes to this kind of convenience unwrapping. It is
trivially possible for a host application to extend the unwrapping to support unwraps to arbitrary data types.

Before diving into how to do this, we'll provide a tiny bit of detail on how the unwrapping works in general. The bulk
of the code to support this is contained within
[rps_cmd_callback_wrapper.hpp](/include/rps/core/rps_cmd_callback_wrapper.hpp). Via some C++ template magic, a
corresponding template specialization of `rps::details::CommandArgUnwrapper` is called for each node parameter
(excluding the command callback context) to deduce what to pass to the node callback. The chosen template is based on
the type of the parameter in the bound callback.

Consider again the `DrawTriangle` function from above,

```C++
void DrawTriangle(const RpsCmdCallbackContext* pContext, rps::UnusedArg u0, D3D12_CPU_DESCRIPTOR_HANDLE srv)
{
    // ...
}
```

For the second function parameter, `rps::details::CommandArgUnwrapper<0, rps::UnusedArg>` is called. For the third
parameter, `rps::details::CommandArgUnwrapper<1, D3D12_CPU_DESCRIPTOR_HANDLE>` is called.

Thus, extending the unwrap functionality is as simple as defining a new template specialization of
`rps::details::CommandArgUnwrapper`.

As an example, let's consider that you want to create an unwrapper that registers resources with your bindless resource
system and returns the offset into the descriptor heap for that resource. This is no problem!

```C++
template <int32_t Index>
struct CommandArgUnwrapper<Index, MyBindlessOffset>
{
    MyBindlessOffset operator()(const RpsCmdCallbackContext* pContext)
    {
        RpsVkImageViewInfo imageViewInfo;
        RpsResult          result = rpsVKGetCmdArgImageViewInfo(pContext, Index, &imageViewInfo);

        if (RPS_FAILED(result))
        {
            rpsCmdCallbackReportError(pContext, result);
        }

        return MyVkRenderer->registerTransientTexture(imageViewInfo.hImageView, imageViewInfo.layout);
    }
};
```
