# RPS SDK Tutorial

Copyright (c) 2024 Advanced Micro Devices, Inc.

The AMD Render Pipeline Shaders (RPS) SDK is released under the MIT LICENSE. Please see file
[LICENSE.txt](LICENSE.txt) for full license details.

## Introduction

Since the introduction of explicit graphics APIs for the PC platform, the management of resource-level synchronization
and transient memory has remained a hot topic in the real-time rendering community.

The responsibility of what was previously undertaken by the driver stack is now handed over to the application
programmer. This opens up the door for performance gains, but creates new problems to solve when implementing and
transitioning engine technology onto these APIs.

The RPS SDK is a new software layer that sits above the graphics API layer, primarily aiming to reduce the complexity
associated with barriers, transient memory management, and queue synchronization. The premise of RPS is to use a render
graph to specify as much detail as possible about the frame (or workload) upfront, and through this, enable the many key
features of RPS. These are, but are not limited to: barrier insertion; resource creation, placement, and lifetime
management; automatic graphics state setup such as render target binding; and efficient scheduling of multi-queue and
async compute workloads. This is all with built-in support for both DirectX 12 and Vulkan.

To be precise, the command buffer that is ultimately submitted to the queue is built up across a series of graph node
executions, where each node has an associated application defined callback that partially populates this buffer. RPS
takes the role of managing the precise ordering of such node invocations, whilst also inserting memory barriers and
resource state management commands (among other commands) into the buffer.

While this may sound like a step backwards, given that the goal of explicit APIs was to be lower-level, the RPS SDK is
nevertheless a step in the right direction. Similar to how C provides high-level constructs for expressing logic that
maps to optimized machine code, RPS offers a suite of high-level constructs for expressing data flow in graphics
applications. The RPS SDK maps such high-level constructs to an optimized placement of low-level barriers, resource heap
placements, and graphics state boilerplate, just like a well-written, hand-crafted frame. Both the C and RPS constructs
sit almost 1:1 above the constructs beneath them, and are used to express patterns that would otherwise often be written
by hand.

In summary, this SDK takes the render graph idea further by embracing it wholeheartedly as a foundational principle in
the design of a new way to write graphics applications. RPS stands firmly as an engine agnostic, open-source and highly
extensible render graph library for the natural depiction and implementation of componentized renderers.

## Defining the Graph - RPSL

The RPS SDK introduces a new type of shader, the Render Pipeline Shader (or RPS, for short). With this shader type, the
application programmer may use a high-level language to naturally specify the components that constitute their renderer.
Currently, the shader is executed on the CPU and before command buffer recording. Execution of the shader defines the
graph, and afterwards, the graph may be executed so as to draw a single frame, for example.

The shader itself is written in RPSL (Render Pipeline Shading Language) - a new language purpose-built for the compact
and natural specification of a real-time renderer. Formally, it is a domain specific language derived from HLSL. Such
shader files can be compiled via our bundled `rps-hlslc` compiler, which by default outputs C99 source; this is done to
enable support for the broadest of target platforms.

If you elect to not use RPSL, you may build a render graph via our C API, or construct a custom frontend on top of it.

## Tutorial Overview

This tutorial will demonstrate key RPS SDK API usage, provide an informal introduction to RPSL, and explain how the RPS
Render Graph is built and to be understood. It will be broken into a series of parts, presenting piecewise the different
capabilities of the SDK.

The tutorial assumes on behalf of the reader prior experience with writing graphics applications using explicit APIs. As
such, all associated samples leverage the pre-curated [app framework](/tools/app_framework/). The tutorial parts will be
written using primarily DirectX 12, with the occasional Vulkan parlance.

### Tutorial Parts

**Part 1**: [Hello Triangle](/docs/tutorial/rps_tutorial_p1.md).

Summary:

- Create the RPS runtime device.
- Create the RPS render graph.
- Bind nodes to the render graph.
- Update the graph.
- Record the graph commands.
- Destroy the graph.
- Destroy the runtime device.

**Part 2**: [Exploring Render Graphs and RPSL](/docs/tutorial/rps_tutorial_p2.md)

Summary:

- Resources in render graphs
  - Resource classes
- RPS render graph structure
  - Render graph build phases
  - The RPSL explorer tool
  - What the render graph means
  - How the RPS render graph works
  - Render graph node dependencies
  - The render graph data flow model
- Exploring RPSL
  - Access attributes
    - Access attribute arguments
    - Access attributes and ordering guarantees
  - Modular RPSL
  - Tools for scheduling
  - HLSL semantics

**Part 3**: [Interoperation between RPS and the host](/docs/tutorial/rps_tutorial_p3.md)

Summary:

- Data interoperation of the host app and RPS.
  - Binding member callbacks to nodes.
  - Retrieve general argument and node information in the callback.
  - Accessing graphics context bindings in the callback.
    - Creating Vulkan PSOs upfront.
  - Auto unwrapping of params into the node callback.
    - Writing a custom unwrapper.

**Part 4**: [RPS SDK multithreading guide](/docs/tutorial/rps_tutorial_p4.md)

Summary:

- RPS SDK multithreading guide.
  - Setup.
  - Submitting commands from multiple threads.
    - DirectX 12.
    - Extending our sample app.
    - Vulkan.
  - Recording the render graph from multiple threads.
