# RPS Tutorial Part 1 - Hello Triangle

Copyright (c) 2024 Advanced Micro Devices, Inc.

The AMD Render Pipeline Shaders (RPS) SDK is released under the MIT LICENSE. Please see file
[LICENSE.txt](LICENSE.txt) for full license details.

## Introduction

This tutorial guides the reader through basic RPS API usage by transforming an existing hello triangle example into an
RPS one. In the final tutorial source, the initial example can be toggled back on by setting `m_useRps=false`. The
source can be found here: [hello_triangle.cpp](/examples/hello_triangle.cpp). Also, please ignore the parts within
hello_triangle.cpp that are marked as for the third tutorial part only.

As mentioned in the introduction, all associated tutorial samples leverage the pre-curated
[app framework](/tools/app_framework). It provides (among other functions) an already created d3d12 device; management
of the swapchain, command queues, and fences; and utility functions such as `AcquireCmdList` and `RecycleCmdList`.

### Overview

RPS has minimal surface area in order to get a basic example working:

[On init](#rps-initialization):

- [Create the RPS runtime device](#creating-the-runtime-device).
- [Create the RPS render graph](#setting-up-the-render-graph).
- [Bind nodes to the render graph](#binding-render-graph-node-callbacks).

[Every frame](#rps-per-frame-logic):

- [Update the graph](#updating-the-render-graph).
- [Record the graph commands](#recording-render-graph-commands).

[On cleanup](#rps-cleanup):

- Destroy the graph.
- Destroy the runtime device.

In summary, usage of the RPS SDK boils down to building the render graph; providing RPS with a command buffer; letting
RPS record into that command buffer; and finally submitting this command buffer along with the appropriate signalling.

## RPS initialization

With the preamble out of the way, we're ready to transform this example into an RPS one!

### Creating the runtime device

We'll begin by creating the runtime device,

```C++
virtual void OnInit(ID3D12GraphicsCommandList*         pInitCmdList,
                    std::vector<ComPtr<ID3D12Object>>& tempResources) override
{
    // ...
    // Create RPS runtime device.
    RpsD3D12RuntimeDeviceCreateInfo runtimeDeviceCreateInfo = {};
    runtimeDeviceCreateInfo.pD3D12Device                    = m_device.Get();
    AssertIfRpsFailed(rpsD3D12RuntimeDeviceCreate(&runtimeDeviceCreateInfo, &m_rpsDevice));
    // ...
}
```

Our call to `rpsD3D12RuntimeDeviceCreate` creates two objects: the RPS device, and the RPS runtime. The device is used
by RPS to manage host application memory allocation and diagnostic logging, together with other functions. The runtime
device largely implements per-backend graphics API runtime calls such as resource creation and destruction.

The call to `rpsD3D12RuntimeDeviceCreate` returns the created device by writing through a pointer to a variable of type
`RpsDevice`. Conversely, the handle to the runtime device is not returned. The runtime device handle is stored within
the device since the host application will not require to reference it.

`RpsDevice`, and other handle types such as `RpsRenderGraph`, are a common pattern in this SDK. These types are defined
in the headers via `RPS_DEFINE_HANDLE`, and are used as part of the signature for many API calls when an SDK object
instance is to be referenced. The headers also define a secondary handle type, and these are defined with
`RPS_DEFINE_OPAQUE_HANDLE`. There's no surprises here; these handle types are similar to those found in other APIs. The
first handle type is a reference to an SDK internal type whose data representation is not exposed in the API. The second
handle type, contrary to the regular handles, references a type whose data representation can be dynamic at runtime.

### Setting up the render graph

Next, we'll setup the render graph. There are two methods to do this. The first was mentioned in the repository README -
to implicitly define it via an RPSL shader. The second method is to use the RPS C API. For the sake of simplicity and to
introduce RPSL, we'll use the former.

#### Introduction to RPSL

For a deeper documentation of RPSL, please see [rpsl.h](/tools/rps_hlslc/rpsl/rpsl.h).

To begin, let's recap what we know about RPSL:

- RPSL is an HLSL derivative language.
- An RPSL "shader" is executed on the CPU and is invoked by the RPS runtime.
- Execution of an RPSL shader builds a render graph.
- Execution of a render graph invokes its graph node implementations, each of which constitute a part of a componentized renderer.

For this tutorial, our application will use the following .rpsl file,

```rpsl
graphics node Triangle([readwrite(rendertarget)] texture renderTarget : SV_Target0);

export void main([readonly(present)] texture backbuffer)
{
    // clear and then render geometry to backbuffer
    clear(backbuffer, float4(0.0, 0.2, 0.4, 1.0));
    Triangle(backbuffer);
}
```

The component renderer that the above RPSL source defines is quite simple; it clears the backbuffer, then executes the
`Triangle` node, which as the application programmer, we understand will render a triangle to the backbuffer.

While the above example demonstrates just a single RPSL program, an RPSL shader can contain many different programs,
each of which if executed would build a separate graph. These programs are identified by their entry nodes, which are
declared with the keyword `export`. For the snippet above, the entry point is `main`.

But, once the program above is executed, what render graph does it build? And, in general, what is the structure of a
render graph?

The execution model of RPSL is akin to executing a program on the CPU - during its execution it will make "calls" to the
nodes. These aren't actual "calls" but instead a "touch" - the nodes are noted to be executed and the execution data
that was passed into them is saved. Once the RPSL program has completed execution, all touched nodes define the nodes
that make up the render graph, which takes the form of a linear, ordered sequence of nodes to call.

And so, when invoking the `main` entry point in the RPSL program above, it ends up touching two nodes: `clear` and
`Triangle`. The render graph is thus made up of these two nodes. The `clear` node is a built-in node - its
implementation is already provided by RPS. As for the `Triangle` node - this is a user-defined node, or, more
appropriately, a node declaration. We'll need to hook an implementation callback to this declaration before executing
the render graph.

Let's not forget one of the key functions of the RPS SDK - to optimize the execution of work on the GPU. To this end,
after the render graph is built and before graph execution, the nodes in the graph are reordered by the RPS runtime
scheduler. This occurs specifically during the call to `rpsRenderGraphUpdate` as one of the phases used to build the
render graph. As such, the RPSL program author should keep this in mind when writing their RPSL shader. It can be an
arbitrary reordering as long as it preserves pipeline correctness.

You may ask, then, what rules are used during scheduling and how can a developer use them to reason about the authoring
of a render graph?

It's all about resource views and access attributes. Resource views are the working flux of RPS. A resource view
contains a reference to an underlying resource as well as several view attributes, such as the subresource range over
which the resource is viewed. In RPSL, the types of `texture` and `buffer` are both resource views. As for access
attributes, these are the decoration on-top. They specify how a node accesses resources through its resource view
parameters. It is through this specification that implicit ordering dependencies are defined, to which the RPS scheduler
must guarantee.

It would be a fair assumption to make that the `clear` node is scheduled before the `Triangle` node, and indeed, that
would be correct. But why is that?

Let's consider the node definitions. We'll start with the `clear` node (there are many overloads, so this is just one):

```rpsl
node clear( [writeonly(rendertarget, clear)] texture dst, float4 clearValue );
```

From this definition, we can say that the first access of the backbuffer in the frame is with an access attribute of
`writeonly`. This means that the first access can only write to the backbuffer, not read from it, and that the
previous data can be discarded. The subsequent `Triangle` node accesses the backbuffer with the access attribute of
`readwrite`. This means that the node can both read and write to the backbuffer. The program order access sequence
thus goes as: writeonly (WO) -> readwrite (RW). Such a sequence _must not_ be reordered. Placing the WO after the RW
discards whatever the `Triangle` node has generated, functionally altering the specified pipeline. Since the re-ordering
would be invalid, the scheduler must keep the program order.

Right, but, what is going on with the `rendertarget` part in the `Triangle` node? This is an argument to the access
attribute, and it plays a special role in the specification of the pipeline as well. The `rendertarget` argument means
that the resource view node parameter it decorates is accessible as a render target view. This has different
implications depending on the runtime backend in use. For example, one of the requirements that a DX12 runtime backend
fulfils is to ensure the resource is in the `D3D12_RESOURCE_STATE_RENDER_TARGET` state before the node callback is
invoked. A render target view is very common, as are various other accesses. Thus, RPS defines a set of convenience
macros; e.g., `rtv` is an alias for `[readwrite(rendertarget)] texture`.

The last part to break down in the .rpsl above is the access attribute of the entry node as this has different semantics
than accesses by regular node declarations. For the entry nodes, the access attributes do not apply within the node
scope, but rather outside the node scope. This establishes an enter/exit state for the resources accessed by the entry.
The input parameter into `main` is accessed with `[readonly(present)]`. This is what we want - after using the render
graph to render the frame, we present the backbuffer to the swapchain.

To bring it all together, we'll reflect on what we've just built. We've built a component renderer that clears the
backbuffer via the built-in `clear` node, and then executes the `Triangle` node implementation (user-defined),
ultimately making a draw call with whatever graphics API that happens to be in use. Finally, at the frame end, the
backbuffer is transitioned into the `D3D12_RESOURCE_STATE_PRESENT` state.

#### Compiling and linking the RPSL module

Referring once again back to our brief RPS introduction, the .rpsl shader document must be compiled. This is achieved
via the bundled RPSL compiler, `rps-hlslc`.

To compile an RPSL file, simply execute,

```bash
rps-hlslc.exe rps_tutorial_p1.rpsl -O0
```

The result of this compilation, as priorly stated, is a .c file. However, it is also possible to compile our module into
a DLL, or even load it via the RPSL-JIT compiler. For now, we will do the simplest possible thing - statically link with
some .c file.

To do so, it begins with a call to a function-like macro - no .h is required, nor is it generated by the RPSL
compiler. For our case, that looks like this:

```C++
// ...
RPS_DECLARE_RPSL_ENTRY(rps_tutorial_p1, main);
// ...
```

With this macro, we can forward declare some entry found in a .c RPSL module and later specify in
`RpsRenderGraphCreateInfo` that this function is the main entry point. The macro is here to deal with the implementation
details of the RPSL .c backend output and the RPS runtime linkage to that, which is permitted to be different than the
conceptual model of "linking with a function". It's important to make sure that you have the correct module and entry
name in your macro, as incorrect information may lead to a complicated debugging process.

Alright, we're finally ready to make the call to `rpsRenderGraphCreate`,

```C++
virtual void OnInit(ID3D12GraphicsCommandList*         pInitCmdList,
                    std::vector<ComPtr<ID3D12Object>>& tempResources) override
{
    // ...
    // Create RPS render graph.
    RpsRenderGraphCreateInfo renderGraphInfo = {};
    RpsQueueFlags            queueFlags[]  = {RPS_QUEUE_FLAG_GRAPHICS, RPS_QUEUE_FLAG_COMPUTE, RPS_QUEUE_FLAG_COPY};
    renderGraphInfo.scheduleInfo.numQueues = 3;
    renderGraphInfo.scheduleInfo.pQueueInfos            = queueFlags;
    renderGraphInfo.mainEntryCreateInfo.hRpslEntryPoint = RPS_ENTRY_REF(hello_triangle, main);
    AssertIfRpsFailed(rpsRenderGraphCreate(m_rpsDevice, &renderGraphInfo, &m_rpsRenderGraph));
    // ...
}
```

Calling `rpsRenderGraphCreate` does not execute the main RPSL module; it merely sets up the `RenderGraph` object. 

If this call returns with `RPS_OK`, you're good to go! Otherwise, one thing to note about this call is the
`scheduleInfo` structure member.

Here, we specify to RPS the available queues and their index layout. This mapping of indices to queues becomes relevant
later, when RPS will communicate a piece of work to be scheduled on some queue index. In the snippet above, we associate
queue indices 0, 1, and 2 to the application managed Graphics, Compute, and Copy queues. These managed queues and their
index ordering are particular to our app framework. The queue indices are used in our hello triangle example to index
into an array storing the queue pointers. In general, an application should communicate to RPS the correct queue indices
given the application defined index layout of the available queues.

### Binding render graph node callbacks

After creating the render graph, we can bind callbacks to the nodes declared in our RPSL module. In this simple example,
we have just one node, `Triangle`.

```C++
virtual void OnInit(ID3D12GraphicsCommandList*         pInitCmdList,
                    std::vector<ComPtr<ID3D12Object>>& tempResources) override
{
    // ...
    // Bind nodes.
    AssertIfRpsFailed(
        rpsProgramBindNode(rpsRenderGraphGetMainEntry(m_rpsRenderGraph), "Triangle", &DrawTriangleCb, this));
    // ...
}
// ...
static void DrawTriangleCb(const RpsCmdCallbackContext* pContext)
{
    HelloTriangle* pThis = static_cast<HelloTriangle*>(pContext->pCmdCallbackContext);
    pThis->DrawTriangle(rpsD3D12CommandListFromHandle(pContext->hCommandBuffer));
}
// ...
void DrawTriangle(ID3D12GraphicsCommandList* pCmdList)
{
    pCmdList->SetGraphicsRootSignature(m_rootSignature.Get());
    pCmdList->SetPipelineState(m_pipelineState.Get());
    pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pCmdList->DrawInstanced(3, 1, 0, 0);
}
```

It is actually okay to leave nodes unbound. Such nodes, if scheduled and then iterated over during graph record time,
simply do nothing when invoked. That being said, if the render graph has a default callback, it will be invoked for any
unbound nodes. To setup the default callback, call `rpsProgramBindNode` with the name parameter set to a null pointer.

Within `rps_runtime.h`, there are actually multiple overloads of `rpsProgramBindNode`; these are for binding both free
and member functions. For now, we're calling the only non-templated one.

The first parameter to `rpsProgramBindNode` is a handle with type `RpsSubprogram`. An `RpsSubprogram` is exactly as it
sounds; it represents an RPSL program (an entry point). Our call to `rpsRenderGraphGetMainEntry` that was made earlier
returns the subprogram associated with our statically linked RPSL `main` entry. The call to `rpsProgramBindNode` then
takes this subprogram handle as the program that contains the node to bind a callback to. For more detail on the power
of subprograms, please see [Modular RPSL](/docs/tutorial/rps_tutorial_p2.md#modular-rpsl).

The final parameter is a user defined context to be passed to the callback. The `this` pointer is passed as we'll need
to resolve the class instance for calling our member function `DrawTriangle`. The user context is accessible from within
the callback via the `pCmdCallbackContext` member of `RpsCmdCallbackContext`.

While this section is within the "on initialization" one, binding node callbacks is _not_ a one-time thing. As long as
it is done before updating the render graph, it is no problem to dynamically bind the callbacks. Binding callbacks is
discussed in this section merely because we expect the typical app usage to be binding callbacks just once for every
node.

## RPS per-frame logic

Since we haven't yet changed our `OnRender` callback function, `DrawTriangleCb` will never be called. This leads us to
the next step - to modify our `OnRender` callback for submitting RPS generated commands to our D3D12 command list. This
is a multi-step process, as we must first update the render graph.

### Updating the render graph

Updating the render graph implies execution of the render graph phases, thus rebuilding (or creating for the first time)
the graph. It's at this point that the RPSL module will be executed. Therefore, when updating the render graph, it is
our duty to pass in the arguments that the main entry point consumes.

In the case of our specific .rpsl module, the main entry point takes in a single texture, the backbuffer.

Let's take a step back to recall what we are doing at a high-level; our ultimate goal is to render a triangle to the
appropriate swapchain image for each frame. Since the render graph takes just a single texture, we might reason that
there must be some host-app-side logic required to deduce which swapchain image view to pass in.

However, this will not be required as RPS has a helpful mechanism called temporal resources that can help with this.

#### Temporal resources

Temporal resources are comprised of slices. For any given frame, a temporal resource will resolve to one of its slices
(which is itself a resource). The selected slice to resolve to is computed via the `frameIndex` member within
`RpsRenderGraphUpdateInfo` as well as the `TemporalLayer` member of the resource view.

The computation for resolving temporal slices is as follows,

```C++
uint32_t temporalSliceIdx = (frameIndex - std::min(TemporalLayer, frameIndex)) % numTemporalLayers;
```

By default, resource views have `TemporalLayer` set to zero, which causes the resolved slice to be that for the current
frame.

While the temporal resource construct is useful for the specific problem of resolving to the correct render target for
the frame, the power of this construct is of course much more - most generically, temporal resources allow for easy
historical data access. Setting `TemporalLayer` can be used to access historical temporal slices. To set the temporal
layer on the resource view, a derived view must be created. In RPSL, this can be achieved via something like
`resA.temporal(1)`. This is a view that would access the slice of some resA that a default view of resA last frame would
have accessed.

#### Providing input arguments to the render graph

In the snippet below, we update the render graph and provide our swapchain images / temporal slices as an array of
`ID3D12Resource` handles to the RPSL program via `ppArgResources`.

```C++
virtual void OnUpdate(uint32_t frameIndex) override
{
    if (m_rpsRenderGraph != RPS_NULL_HANDLE)
    {
        RpsRuntimeResource backBufferResources[DXGI_MAX_SWAP_CHAIN_BUFFERS];

        for (uint32_t i = 0; i < m_backBuffers.size(); i++)
        {
            // RpsRuntimeResource is a RPS_OPAQUE_HANDLE.
            // this type has a single elem "void* ptr".
            backBufferResources[i].ptr = m_backBuffers[i].Get();
        }

        RpsResourceDesc backBufferDesc   = {};
        backBufferDesc.type              = RPS_RESOURCE_TYPE_IMAGE_2D;
        backBufferDesc.temporalLayers    = uint32_t(m_backBuffers.size());
        backBufferDesc.image.width       = m_width;
        backBufferDesc.image.height      = m_height;
        backBufferDesc.image.arrayLayers = 1;
        backBufferDesc.image.mipLevels   = 1;
        backBufferDesc.image.format      = RPS_FORMAT_R8G8B8A8_UNORM;
        backBufferDesc.image.sampleCount = 1;

        RpsConstant               argData[]      = {&backBufferDesc};
        const RpsRuntimeResource* argResources[] = {backBufferResources};

        uint32_t argDataCount = 1;
        // ...

        // RpsAfx always waits for presentation before rendering to a swapchain image again,
        // therefore the guaranteed last completed frame by the GPU is m_backBufferCount frames ago.
        //
        // RPS_GPU_COMPLETED_FRAME_INDEX_NONE means no frames are known to have completed yet;
        // we use this during the initial frames.
        const uint64_t completedFrameIndex =
            (frameIndex > m_backBufferCount) ? frameIndex - m_backBufferCount : RPS_GPU_COMPLETED_FRAME_INDEX_NONE;

        RpsRenderGraphUpdateInfo updateInfo = {};
        updateInfo.frameIndex               = frameIndex;
        updateInfo.gpuCompletedFrameIndex   = completedFrameIndex;
        updateInfo.numArgs                  = argDataCount;
        updateInfo.ppArgs                   = argData;
        updateInfo.ppArgResources           = argResources;

        assert(_countof(argData) == _countof(argResources));

        AssertIfRpsFailed(rpsRenderGraphUpdate(m_rpsRenderGraph, &updateInfo));
    }
}
```

In general, arguments are provided to the main entry point of an RPSL program by setting the `ppArgResources` and
`ppArgs` members of `RpsRenderGraphUpdateInfo`. These members are both arrays with an element type of pointer. For every
pointer within `ppArgs`, there is a corresponding pointer to a resource handle (each of type
`const RpsRuntimeResource *`) in `ppArgResources`. For temporal resources, the corresponding pointer to a resource
handle points to an entire array of resource handles, and indeed, this is what is done in the `OnUpdate` function above.

It is also legal that a pointer in `ppArgs` points merely to a built-in type such as `int` or `float`, with the
corresponding pointer in `ppArgsResources` being `nullptr`. In general, any C struct / POD C++ class may be passed so
long as the arguments at each index match in memory size and layout between the host app and HLSL. Please note that
since HLSL booleans are 32 bit, having an element in `ppArgs` with type `bool` is undefined behavior. To this end, we
provide a type `typedef int32_t RpsBool;`.

We note further the `gpuCompletedFrameIndex` member. The host application should populate this to a value that
communicates, at the very least, a frame index whose associated command buffers are guaranteed to have completed on the
GPU. RPS uses this value to manage frame resources. If the application continually provides the value of
`RPS_GPU_COMPLETED_FRAME_INDEX_NONE`, RPS may encounter an internal memory error, as there is a limit to the amount of
frame resources that may be buffered.

Finally, we note that while a graph might be built correctly, it could be semantically invalid. Currently, at least some
semantic verifications that the RPS SDK performs are done online. In such cases, the SDK errors out with
`RPS_ERROR_INVALID_PROGRAM`.

### Recording render graph commands

Now that we have updated the render graph, we may investigate an updated `OnRender` callback for recording graph
commands.

But, what exactly is a "graph command"?

It isn't an API command such as e.g. DrawInstanced; rather, it is a more abstract grouping of work to be recorded to the
API command buffer. The most common type of graph command is a node callback to invoke. In fact, such graph commands are
a result of the scheduler phase - the reordered sequence of "touched" nodes that make up the render graph.

Recording commands begins with a call to `rpsRenderGraphGetBatchLayout`,

```C++
virtual void OnRender(uint32_t frameIndex) override
{
    if (m_useRps)
    {
        RpsRenderGraphBatchLayout batchLayout = {};
        AssertIfRpsFailed(rpsRenderGraphGetBatchLayout(m_rpsRenderGraph, &batchLayout));
        // ...
    }
    // ...
}
```

This will return the batch layout from a render graph. RPS specifies commands to submit in batches, where each batch is
a chunk of work to submit to some queue. Before recording each batch, we may need to queue a set of GPU-side waits for
some amount of signal values to occur on some amount of queues.

```C++
virtual void OnRender(uint32_t frameIndex) override
{
    // ...
    m_fenceSignalInfos.resize(batchLayout.numFenceSignals);

    for (uint32_t ib = 0; ib < batchLayout.numCmdBatches; ib++)
    {
        const RpsCommandBatch& batch = batchLayout.pCmdBatches[ib];

        for (uint32_t iw = batch.waitFencesBegin; iw < batch.waitFencesBegin + batch.numWaitFences; ++iw)
        {
            const FenceSignalInfo& sInfo = m_fenceSignalInfos[batchLayout.pWaitFenceIndices[iw]];
            AssertIfFailed(m_queues[batch.queueIndex]->Wait(m_fences[sInfo.queueIndex].Get(), sInfo.value));
        }
        // ...
    }
    // ...
}
```

After queuing the waits, we may record the graph and submit the resulting API commands. The `queueIndex` member of the
`RpsCommandBatch` structure associates a batch directly with some queue to submit to.

```C++
virtual void OnRender(uint32_t frameIndex) override
{
    // ...
    ID3D12CommandQueue* pQueue  = GetCmdQueue(RpsAfxQueueIndices(batch.queueIndex));
    ActiveCommandList   cmdList = AcquireCmdList(RpsAfxQueueIndices(batch.queueIndex));

    RpsRenderGraphRecordCommandInfo recordInfo = {};
    recordInfo.hCmdBuffer                      = rpsD3D12CommandListToHandle(cmdList.cmdList.Get());
    recordInfo.pUserContext                    = this;
    recordInfo.frameIndex                      = frameIndex;
    recordInfo.cmdBeginIndex                   = batch.cmdBegin;
    recordInfo.numCmds                         = batch.numCmds;

    AssertIfRpsFailed(rpsRenderGraphRecordCommands(m_rpsRenderGraph, &recordInfo));

    CloseCmdList(cmdList);

    ID3D12CommandList* pCmdLists[] = {cmdList.cmdList.Get()};
    pQueue->ExecuteCommandLists(1, pCmdLists);

    RecycleCmdList(cmdList);
    // ...
}
```

Aside from what is explicitly recorded by the node callbacks, what API commands does RPS generate?

As you may recall, it chiefly generates resource state management commands (barrier insertion); however, RPS also has in
its jurisdiction the ability to insert commands such as binding RTs, clearing RTs, and setting up viewport and scissor
rects. Generally, we might call these "render pass setup" commands. These occur before RPS hands over control to the
host application callback.

These setup commands are controlled via the semantics of the node declaration. For example, the RTs to bind are
specified by parameters annotated with the semantic `SV_Target[n]`.

Recall the Triangle node within our .rpsl,

```rpsl
graphics node Triangle([readwrite(rendertarget)] texture renderTarget : SV_Target0);
```

Since the Triangle node is declared with the render target semantic, RPS will automatically bind the passed resource
parameter as a render target - hence the lack of such binding in the callback. We also see that the viewport / scissor
rects are not setup either. RPS will setup a default viewport and scissor rect with the width and height extent set to
the minimum extent of the set of RTs bound. The default viewport Z extent is set between [0.0, 1.0].

The default viewport / scissor rects can be overridden via node parameters that have the HLSL semantics of
`SV_Viewport[n]` and `SV_ScissorRect[n]`.

In general, HLSL semantics as a method for controlling graphics state setup are explained in further detail in the
[next tutorial part](/docs/tutorial//rps_tutorial_p2.md#hlsl-semantics).

The automatic binding of RTs, as well as the viewport and scissor rects, can be optionally disabled via the
`RPS_CMD_CALLBACK_CUSTOM_VIEWPORT_SCISSOR_BIT` and/or `RPS_CMD_CALLBACK_CUSTOM_RENDER_TARGETS_BIT` flags. These are
passed when calling `rpsProgramBindNode`.

#### Post-record logic

Even with the render graph commands recorded, there's still some work to do for our `OnRender` function! The batch may
also specify that after submission we must signal some value on the queue,

```C++
virtual void OnRender(uint32_t frameIndex) override
{
    // ...
    if (batch.signalFenceIndex != RPS_INDEX_NONE_U32)
    {
        m_fenceValue++;
        FenceSignalInfo& sInfo = m_fenceSignalInfos[batch.signalFenceIndex];
        sInfo.queueIndex       = batch.queueIndex;
        sInfo.value            = m_fenceValue;
        AssertIfFailed(m_queues[batch.queueIndex]->Signal(m_fences[sInfo.queueIndex].Get(), sInfo.value));
    }
    // ...
}
```

In fact, it is this exact signal scheduling moment that produces those fence signal wait calls prior to submission of
the batch. While it might have been obvious already, we'll mention that RPS elects to schedule commands like this (in
batches) for the purpose of multi-queue and asynchronous compute scenarios.

## RPS cleanup

And that's it! RPS is doing its magic, and our triangle is rendering again. The last step is to ensure proper cleanup of
the RPS runtime.

```C++
virtual void OnCleanUp() override
{
    rpsRenderGraphDestroy(m_rpsRenderGraph);
    rpsDeviceDestroy(m_rpsDevice);
    // ...
}
```

... Voila! Here's a screenshot of the triangle that we just rendered.

![a screenshot of the triangle that we just rendered](/docs/assets/rps_tutorial_p1.PNG)

Congratulations on completing the tutorial! You should now have a basic understanding of how to set up a simple
rendering pipeline using RPS.

As you continue to work with RPS, you may have some further questions. Maybe you are thinking:

- How can I retrieve parameters from RPSL nodes in the callback?
- How does RPS help manage transient memory?
- What is the structure of the render graph?
- How does the scheduler work?
- How can I record commands from multiple threads?

Don't worry! In the next few parts of the tutorial, we will answer all these questions and more. By the end of this
tutorial series, you will have a solid foundation in using RPS to create optimized pipelines.
