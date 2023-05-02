# Known Issues

This document serves the purpose to make the user aware of issues we are still working on finding a suitable solution for. Below, we provide some workarounds until general solutions for these problems are implemented. Please note that these are only temporary.


## Temporal History Accesses

Accessing a previous slice of a temporal resource with access attributes that were not used with the temporal resource yet may cause problems due to missing usage flags at its creation.

For now, please specify any new access attributes used with any previous slices (e.g. temporal(1)) as a dummy node in the initial frames before the first history access. For an example see the [temporal use sample](/tests/gui/test_temporal.rpsl).


## Persistent Resource Data Loss

Right now, persistent resources have to be recreated when a formerly unused access attribute is used on them for the first time.

If this creates problems, make sure to communicate these access attributes to RPS in the first frame they are used. This can be achieved by a similar dummy approach as demonstrated for temporal history accesses.

Other cases causing persistent resource data loss are:

* Any other newly accumulated bit into the allAccesses member of the ResourceInstance (either RpsAccessFlagBits or RpsShaderStageBits)

* Not using an (internal) persistent resource for at least one frame.

* Other newly used forms of accesses like being viewed with a different format than being created or e.g. as a cubemap.



## Non-uniform Temporal Slices

The slices of a temporal resource are allowed to vary in their descriptions. This can be sometimes useful for very dynamic control flow. However, due to some aspects of handling temporal resources, their descriptions (no matter the selected slice) seem to the user as if they originate from the current slice.

If you need to query resource descriptions from previous slices, make sure to query them each frame and cache them for the last n frames where n is the largest amount of frames back from which you need resource descriptions.


## Unused/Unaccessed Resources

While mostly used during development, unused resources should be generally functional as well. For now, please make sure resources are only declared if they are being used in the same frame. We are currently investigating a few issues related to unused resources.

## Node Elimination

By default, nodes that transitively cannot have any influence onto the output of the render graph are eliminated. This is the case when they do not write to a resource that is either persistent or used later in the frame. We are currently investigating a rare issue where barriers near eliminated nodes are batched incorrectly. This leads to graphics API errors indicating wrong usage of barriers.

Should you run into such an issue, make sure to either remove unnecessary node calls yourself or specify `RPS_SCHEDULE_DISABLE_DEAD_CODE_ELIMINATION_BIT` for `RpsRenderGraphUpdateInfo::scheduleFlags` during calls to `rpsRenderGraphUpdate`.