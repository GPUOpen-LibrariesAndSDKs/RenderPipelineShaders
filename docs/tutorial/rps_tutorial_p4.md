# RPS Tutorial Part 4 - RPS SDK multithreading guide

Copyright (c) 2024 Advanced Micro Devices, Inc.

The AMD Render Pipeline Shaders (RPS) SDK is released under the MIT LICENSE. Please see file
[LICENSE.txt](LICENSE.txt) for full license details.

## Introduction

This tutorial guides the reader through how to record graph commands from multiple threads, and how to record API
commands from within a callback in a multithreaded way. Similar to the first tutorial part, the full source code for a
complete app is provided. The app source is modified from the source for part 1, and it can be found here:
[rps_multithreading.cpp](/examples/rps_multithreading.cpp). Unless otherwise mentioned, all snippets in this tutorial
are part of the coupled source.

## Overview

- [Setup](#setup)
- [Going wide from within the callback](#going-wide-from-within-the-callback)
  - [DirectX 12](#directx-12)
    - [Extending our sample app](#extending-our-sample-app)
  - [Vulkan](#vulkan)
- [Recording the render graph from multiple threads](#recording-the-render-graph-from-multiple-threads)

## Setup

To demonstrate an MT use-case, we first require an application that has a need to do so. We'll modify our triangle
example to render many more triangles!

In our RPSL program, we'll define a graph made of a few `GeometryPass` nodes. Each pass will render 1024 triangles to
some viewport of the backbuffer. The viewports will be set with the HLSL `SV_Viewport` semantic.

Our new RPSL shader goes like so:

```rpsl
graphics node GeometryPass(rtv renderTarget
                           : SV_Target0, float oneOverAspectRatio, float timeInSeconds, RpsViewport viewport
                           : SV_Viewport);

export void main([readonly(present)] texture backbuffer, float timeInSeconds)
{
    // ...
    uint32_t numPasses = 10;
    uint32_t triStride = floor(sqrt(numPasses));

    for (uint32_t i = 0; i < numPasses; i++)
    {
        float       sX = texDesc.Width / float(triStride);
        float       sY = texDesc.Height / float(triStride);
        float       x  = (i % triStride) * sX;
        float       y  = (i / triStride) * sY;
        RpsViewport v  = viewport(x, y, sX, sY);

        GeometryPass(backbuffer, oneOverAspectRatio, timeInSeconds, v);
    }
}
```

Of course, the render `X` triangles part happens within the host application callback. This, along with other required
changes, were made on the host app side to support the new render graph. For brevity, we omit those changes here.

## Going wide from within the callback

RPS has surface area of its API dedicated to assist with multithreaded recording from within the callback. The
conceptual model for how this recording is achieved varies depending on the particular runtime backend in use.

We'll begin with the DirectX 12 case.

### DirectX 12

Here, the conceptual model is that each worker thread will be given their own command list. We'll submit the lists to
the GPU in order, as if it saw just one command list instead.

Recall that before launching the graph recording, we provide RPS with a command list that gets passed back to us in the
node callbacks. This is the same command list that RPS inserts barriers into.

Suppose that we naively implement the conceptual model we just described. Here's some pseudocode to ensure we're all on
the same page:

```Python
def node_callback(pContext):
    cmdList = pContext.hCommandBuffer
    for i in range(threads_to_launch):
        thread_cmd_list = acquire_new_cmd_list()
        job = lambda thread_cmd_list : thread_cmd_list.draw_foo_at_the_bar()
        enqueue_job(job)
```

With this naive method, we would observe the behavior that the below diagram describes, i.e., all of the work enqueued
across all of the node callbacks would be submitted linearly after the "main" command list that was passed when we made
the call to `rpsRenderGraphRecordCommands`. We have that every behind-the-scenes command that RPS records will occur
before all of the API commands that our app recorded. This is very wrong!

```text

   cmdList passed at graph record time:              Worker thread command lists:
|----------------------------------------|       |-------|     |-------|     |-------|
|  e.g. all barriers are inserted here.  |------>|       |---->|       |---->|       |----> ...
|----------------------------------------|       |-------|     |-------|     |-------|

```

Hence, we provide a method to override the current command buffer that is being used with a new one. This is done with
an API call to `rpsCmdSetCommandBuffer`. RPS provides this so that host apps can override the buffer at the end of the
node callback, resulting in subsequent node callbacks continuing with a different buffer. This allows the host app to
arbitrate inbetween its worker thread lists at queue submission time.

Keep in mind that RPS does not track historical command buffers. It is entirely in the hands of the host app to track
and ensure the appropriate ordering of the submitted buffers.

#### Extending our sample app

So, we just mentioned that a call to `rpsCmdSetCommandBuffer` should be made at the end of the callback. This was enough
to paint a general picture of how we could accomplish multi-threaded recording in the callback.

We will now modify our sample app towards a complete implementation.

For our DX12 MT use-case, we desire that all the command lists have the same pipeline state setup - the state that the
preamble setup for us.

How can we get RPS to do this for all of our worker thread command lists?

This is accomplished with `rpsCmdBeginRenderPass`. This call does exactly the same thing as the node preamble (which was
explained
[in the 3rd tutorial part](/docs/tutorial/rps_tutorial_p3.md#accessing-graphics-context-bindings-in-the-callback)). The
important bit is that we get to call this on our own time, and that the difference between the explicit invocation the
implicit one is that the flags bound at node bind time do not apply to `rpsCmdBeginRenderPass`. This is important, as we
want to set `RPS_CMD_CALLBACK_CUSTOM_ALL` to skip the automatic preamble, but we do not want this behavior to apply for
the explicit preamble.

Let's make these changes in the app.

First, we set the custom all flag in order to skip the default preamble - we only want to set up graphics state (bind the
render target, etc) on the worker thread command buffers,

```C++
virtual void OnInit(ID3D12GraphicsCommandList*         pInitCmdList,
                        std::vector<ComPtr<ID3D12Object>>& tempResources) override
{
    // ...
    // Bind nodes.
    AssertIfRpsFailed(rpsProgramBindNode(rpsRenderGraphGetMainEntry(m_rpsRenderGraph),
                                        "GeometryPass",
                                        &RpsMultithreading::GeometryPass,
                                        this,
                                        RPS_CMD_CALLBACK_CUSTOM_ALL));
    // ...
}
```

Next, we want to make sure to call `rpsCmdBeginRenderPass` on each command buffer. However, we aren't quite ready to do
so. Consider the signature of the function,

```C++
RpsResult rpsCmdBeginRenderPass(const RpsCmdCallbackContext* pContext, RpsRuntimeRenderPassFlags flags);
```

The first parameter is the callback context. Recall that this context contains a handle to the command buffer that we
passed in when kicking off the render graph recording. If we were to call this function with the context that our
callback gets called with, the graphics state would be setup on the main command buffer, not the worker thread one.

How can we get this function to record to an arbitrary command buffer?

Introducing another API call: `rpsCmdCloneContext`. You provide this call with the context that the callback receives,
and it clones that context. At the same time, you ensure to provide a new handle to a command buffer. Every context maps
to a command buffer.

And so, now we can actually make the call to `rpsCmdBeginRenderPass`.

```C++
void GeometryPass(const RpsCmdCallbackContext* pContext,
                  rps::UnusedArg               u0,
                  float                        oneOverAspectRatio,
                  float                        timeInSeconds)
{
    //...
    for (uint32_t i = 0; i < renderJobs; i++)
    {
        // auto hNewCmdBuf = AcquireNewCommandBuffer( ... );
        const RpsCmdCallbackContext* pLocalContext = {};
        {
            std::lock_guard<std::mutex> lock(m_cmdListsMutex);

            AssertIfRpsFailed(rpsCmdCloneContext(pContext, hNewCmdBuf, &pLocalContext));
        }
        // ...
        auto job = [=]() {
            RpsCmdRenderPassBeginInfo beginInfo = {};
            RpsRuntimeRenderPassFlags rpFlags   = RPS_RUNTIME_RENDER_PASS_FLAG_NONE;
            rpFlags |= (i != 0) ? RPS_RUNTIME_RENDER_PASS_RESUMING : 0;
            rpFlags |= (i != (renderJobs - 1)) ? RPS_RUNTIME_RENDER_PASS_SUSPENDING : 0;
            beginInfo.flags = rpFlags;
            AssertIfRpsFailed(rpsCmdBeginRenderPass(pLocalContext, &beginInfo));
            // render ...
            AssertIfRpsFailed(rpsCmdEndRenderPass(pLocalContext));
        };

        // launch job.
    }
    // ... 
}
```

There's two additional ideas in the snippet above that have not been explained yet. Firstly, a lock is used when calling
`rpsCmdCloneContext`. The cloned context is a data structure managed by the RPS SDK and therefore an allocation occurs
for this call. Undefined behavior _will_ occur if we are trying to call clone context from two threads at once.

The second idea is the mysterious render pass resuming and suspending flags that are passed along with the call to
`rpsCmdBeginRenderPass`. These flags are passed to indicate that a render pass is suspending, and will resume with
another render pass. If a render pass is resuming, any clears that would have otherwise automatically occurred will
not. Likewise, if a render pass is suspending, any resolves will not automatically occur.

And that's it, we're pretty much done! We just need to make the call to `rpsCmdSetCommandBuffer` to override the buffer
to be used for subsequent node callback recording.

```C++
void GeometryPass(const RpsCmdCallbackContext* pContext,
                  rps::UnusedArg               u0,
                  float                        oneOverAspectRatio,
                  float                        timeInSeconds)
{
    //...
    for (uint32_t i = 0; i < renderJobs; i++)
    {
        // ...
    }

    // auto hNewCmdBuf = AcquireNewCommandBuffer( ... );
    AssertIfRpsFailed(rpsCmdSetCommandBuffer(pContext, hNewCmdBuf));
}
```

Finally, recall that RPS does not track historical command buffers. Thus, for our sample app, a special helper function
`AcquireNewCommandBuffer` has been created. This is used to construct a linked list structure of command buffers so that
at queue submission time we know how to submit them in the correct order.

### Vulkan

In the Vulkan case, the current supported path for recording in a multithreaded way is to use a secondary command buffer
for each worker thread. To this end, there is no need to call `rpsCmdSetCommandBuffer`, as we'll simply make a call to
`vkCmdExecuteCommands` in the main command buffer.

MT from within the callback in VK is the same as in DX12 but with some additional changes,

- Begin a render pass on the primary command buffer with the `RPS_RUNTIME_RENDER_PASS_EXECUTE_SECONDARY_COMMAND_BUFFERS` flag.
- Begin a render pass on each secondary buffer with the `RPS_RUNTIME_RENDER_PASS_SECONDARY_COMMAND_BUFFER` flag.
- It is no longer a requirement to use suspending/resuming render pass flags.

Here's an example that should hopefully make things concrete,

```C++
void DrawGeometry(const RpsCmdCallbackContext* pContext)
{
    // Begin RP on primary cmdBuf with expected contents as secondary command buffers.
    RpsCmdRenderPassBeginInfo passBeginInfo = {};
    passBeginInfo.flags                     = RPS_RUNTIME_RENDER_PASS_EXECUTE_SECONDARY_COMMAND_BUFFERS;
    rpsCmdBeginRenderPass(pContext, passBeginInfo);

    VkCommandBuffer vkCmdBufs[MAX_THREADS];

    for (uint32_t i = 0; i < numThreads; i++)
    {
        // retrieve a new command buffer (hNewCmdBuf).

        vkCmdBufs[i] = hNewCmdBuf;

        const RpsCmdCallbackContext* pLocalContext = {};
        {
            std::lock_guard<std::mutex> lock(m_cmdListMutex);
            rpsCmdCloneContext(pContext, rpsVKCommandBufferToHandle(hNewCmdBuf), &pLocalContext);
        }

        auto job = [=]() 
        {
            // Begin RP on secondary cmdBuf. Let RPS know this pass is on a secondary command buffer.
            // This communicates to RPS to not call vkCmdBeginRenderPass. RP is inherited.
            RpsCmdRenderPassBeginInfo passBeginInfo = {};
            passBeginInfo.flags                     = RPS_RUNTIME_RENDER_PASS_SECONDARY_COMMAND_BUFFER;
            rpsCmdBeginRenderPass(pLocalContext, passBeginInfo);
            // record work.
            rpsCmdEndRenderPass(pLocalContext);
        };

        // launch job.
    }

    // wait for jobs.

    VkCommandBuffer cmdBufPrimary = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);
    vkCmdExecuteCommands(cmdBufPrimary, numThreads, vkCmdBufs);

    rpsCmdEndRenderPass(pContext);
}
```

Please note the above snippet is not part of the associated source code for this tutorial. For a functional example of
multithreaded recording with Vulkan, please see [test_multithreading_vk.cpp](/test/gui/test_multithreading_vk.cpp).

## Recording the render graph from multiple threads

Up until now, multithreaded recording has been discussed from the perspective of within a single node callback. However,
it is also possible to do so for the entire graph itself.

When making the call to `rpsRenderGraphRecordCommands`, a range of commands in the linear command stream is provided via
`cmdBeginIndex` and `numCmds`. Thus, multithreaded recording in this scenario means splitting the entire command stream
into a set of subranges and providing a subrange to each worker thread. And since each command callback context is
associated with a command buffer, we'll need to assign a unique buffer for each record info structure.

What does this look like in code?

```C++
virtual void OnRender(uint32_t frameIndex) override
{
    RpsRenderGraphBatchLayout batchLayout = {};
    AssertIfRpsFailed(rpsRenderGraphGetBatchLayout(m_rpsRenderGraph, &batchLayout));

    const uint32_t batchCmdEnd      = batch.cmdBegin + batch.numCmds;
    const uint32_t cmdsPerThread    = DIV_CEIL(batch.numCmds, m_graphThreadsToLaunch);
    const uint32_t numThreadsActual = DIV_CEIL(batch.numCmds, cmdsPerThread);

    uint32_t cmdBegin = 0;

    ScopedThreadLauncher stl;

    for (uint32_t threadIdx = 0; threadIdx < numThreadsActual; threadIdx++)
    {
        const uint32_t cmdEnd = std::min(batchCmdEnd, cmdBegin + cmdsPerThread);

        auto job = [=]() {
            auto buffer = buffers[threadIdx];
            
            RpsRenderGraphRecordCommandInfo recordInfo = {};
            recordInfo.hCmdBuffer                      = buffer;
            recordInfo.cmdBeginIndex                   = cmdBegin;
            recordInfo.numCmds                         = cmdEnd - cmdBegin;

            AssertIfRpsFailed(rpsRenderGraphRecordCommands(m_rpsRenderGraph, &recordInfo));
        };

        stl.launchJob(job);

        cmdBegin = cmdEnd;
    }

    stl.waitForAllJobs();

    // collect all command buffers recorded to and order them appropriately before submitting to API queue ...
}
```

Please note that the above snippet has been modified from the source code associated with this tutorial. This was done
for simplicity. Once again, the full source can be found here:
[rps_multithreading.cpp](/examples/rps_multithreading.cpp).
