// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_LIFETIME_ANALYSIS_HPP
#define RPS_LIFETIME_ANALYSIS_HPP

#include "rps/runtime/common/rps_render_states.h"

#include "runtime/common/rps_render_graph.hpp"

namespace rps
{
    class LifetimeAnalysisPhase : public IRenderGraphPhase
    {
    public:
        virtual RpsResult Run(RenderGraphUpdateContext& context) override final
        {
            RPS_RETURN_OK_IF(context.renderGraph.GetCreateInfo().renderGraphFlags &
                             RPS_RENDER_GRAPH_NO_LIFETIME_ANALYSIS);

            auto        pRuntimeDevice    = RuntimeDevice::Get(context.renderGraph.GetDevice());
            auto        resourceInstances = context.renderGraph.GetResourceInstances().range_all();
            const auto& transitions       = context.renderGraph.GetTransitions();
            const auto  runtimeCmds       = context.renderGraph.GetRuntimeCmdInfos().range_all();
            auto        cmdInfos          = context.renderGraph.GetCmdAccessInfos().range_all();

            ArenaCheckPoint arenaCheckpoint{context.scratchArena};

            ArenaVector<uint32_t> resourceInstanceSubResOffset{resourceInstances.size(), &context.scratchArena};

            const uint32_t lastCmdId = runtimeCmds.empty() ? 0 : uint32_t(runtimeCmds.size() - 1);

            uint32_t totalSubResCount = 0;

            for (size_t iRes = 0; iRes < resourceInstances.size(); iRes++)
            {
                auto& resInst = resourceInstances[iRes];

                const bool bIsPersistent = resInst.IsPersistent();

                resInst.lifetimeBegin = bIsPersistent ? 0 : UINT32_MAX;
                resInst.lifetimeEnd   = bIsPersistent ? lastCmdId : 0;

                resourceInstanceSubResOffset[iRes] = totalSubResCount;
                totalSubResCount += resInst.numSubResources;
            }

            if (runtimeCmds.empty())
            {
                return RPS_OK;
            }

            RPS_ASSERT((runtimeCmds.front().GetTransitionId() == CMD_ID_PREAMBLE) &&
                       (runtimeCmds.back().GetTransitionId() == CMD_ID_POSTAMBLE));

            ArrayRef<SubResState> subResStates = context.scratchArena.NewArrayZeroed<SubResState>(totalSubResCount);

            auto fnUpdateAccessRange = [&](uint32_t resourceIndex, uint32_t runtimeCmdIdx) {
                auto& resInst         = resourceInstances[resourceIndex];
                resInst.lifetimeBegin = rpsMin(resInst.lifetimeBegin, runtimeCmdIdx);
                resInst.lifetimeEnd   = rpsMax(resInst.lifetimeEnd, runtimeCmdIdx);
            };

            auto fnMarkPersistentSubres = [&]() {
                for (uint32_t iRes = 0; iRes < resourceInstances.size(); iRes++)
                {
                    const auto& resInfo = resourceInstances[iRes];

                    if (resInfo.IsPersistent())
                    {
                        const uint32_t subResOffset = resourceInstanceSubResOffset[iRes];
                        std::for_each(subResStates.begin() + subResOffset,
                                      subResStates.begin() + subResOffset + resourceInstances[iRes].numSubResources,
                                      [](SubResState& state) { state.Access(true, 0); });
                    }
                }
            };

            fnMarkPersistentSubres();

            // Forward pass:
            for (uint32_t runtimeCmdIdx = 1; runtimeCmdIdx < (runtimeCmds.size() - 1); runtimeCmdIdx++)
            {
                const auto& runtimeCmd = runtimeCmds[runtimeCmdIdx];
                if (runtimeCmd.isTransition)
                {
                    const auto& transitionInfo = transitions[runtimeCmd.GetTransitionId()];
                    fnUpdateAccessRange(transitionInfo.access.resourceId, runtimeCmdIdx);
                }
                else
                {
                    auto accessInfos = context.renderGraph.GetCmdInfo(runtimeCmd.GetCmdId())->accesses.Get(cmdInfos);

                    for (auto& accessInfo : accessInfos)
                    {
                        if (accessInfo.resourceId != RPS_RESOURCE_ID_INVALID)
                        {
                            fnUpdateAccessRange(accessInfo.resourceId, runtimeCmdIdx);

                            CheckAndUpdateSubresourceActiveMasks<ForwardPass>(pRuntimeDevice,
                                                                              runtimeCmdIdx,
                                                                              accessInfo,
                                                                              resourceInstances[accessInfo.resourceId],
                                                                              resourceInstanceSubResOffset.crange_all(),
                                                                              subResStates);
                        }
                    }
                }
            }

            // Reverse pass:
            std::fill(subResStates.begin(), subResStates.end(), SubResState{});

            fnMarkPersistentSubres();

            for (uint32_t runtimeCmdIdx = uint32_t(runtimeCmds.size() - 2); runtimeCmdIdx > 0; runtimeCmdIdx--)
            {
                const auto& runtimeCmd = runtimeCmds[runtimeCmdIdx];
                if (!runtimeCmd.isTransition)
                {
                    auto accessInfos = context.renderGraph.GetCmdInfo(runtimeCmd.GetCmdId())->accesses.Get(cmdInfos);

                    for (auto& accessInfo : accessInfos)
                    {
                        if (accessInfo.resourceId != RPS_RESOURCE_ID_INVALID)
                        {
                            CheckAndUpdateSubresourceActiveMasks<ReversePass>(pRuntimeDevice,
                                                                              runtimeCmdIdx,
                                                                              accessInfo,
                                                                              resourceInstances[accessInfo.resourceId],
                                                                              resourceInstanceSubResOffset.crange_all(),
                                                                              subResStates);
                        }
                    }
                }
            }

            return RPS_OK;
        }

    private:

        struct SubResState
        {
            uint32_t currActive : 1;
            uint32_t prevActive : 1;
            uint32_t currCmd : 30;

            // Update sub-resource access active state.
            // Returns if the access was active before current cmd.
            bool Access(bool bActive, uint32_t cmdIndex)
            {
                if (currCmd != cmdIndex)
                {
                    // The first time current cmd accesses this sub-resource
                    // Preserve previous active state in case the same sub-resource
                    // is accessed by other arguments of the current cmd.
                    prevActive = currActive;
                    currActive = bActive;
                    currCmd    = cmdIndex;
                }
                else
                {
                    // Only deactivate a subresource if all accesses to it in current cmd
                    // has discard flags.
                    currActive |= bActive;
                }

                return prevActive;
            }
        };

        static constexpr bool ReversePass = true;
        static constexpr bool ForwardPass = false;

        template <bool bReverseScan>
        static void CheckAndUpdateSubresourceActiveMasks(const RuntimeDevice*    pRuntimeDevice,
                                                         uint32_t                currCmdIdx,
                                                         CmdAccessInfo&          accessInfo,
                                                         const ResourceInstance& resInfo,
                                                         ConstArrayRef<uint32_t> resourceInstanceSubResOffset,
                                                         ArrayRef<SubResState>   subResStates)
        {
            static constexpr RpsAccessFlags DeactivatingAccessMask =
                (bReverseScan ? RPS_ACCESS_DISCARD_DATA_BEFORE_BIT : RPS_ACCESS_DISCARD_DATA_AFTER_BIT);
            static constexpr RpsAccessFlags ToBeDiscardedFlag =
                (bReverseScan ? RPS_ACCESS_DISCARD_DATA_AFTER_BIT : RPS_ACCESS_DISCARD_DATA_BEFORE_BIT);
            static constexpr RpsAccessFlags StencilDeactivatingAccessMask =
                (bReverseScan ? RPS_ACCESS_STENCIL_DISCARD_DATA_BEFORE_BIT : RPS_ACCESS_STENCIL_DISCARD_DATA_AFTER_BIT);
            static constexpr RpsAccessFlags StencilToBeDiscardedFlag =
                (bReverseScan ? RPS_ACCESS_STENCIL_DISCARD_DATA_AFTER_BIT : RPS_ACCESS_STENCIL_DISCARD_DATA_BEFORE_BIT);

            if (resInfo.numSubResources == 1)
            {
                const uint32_t subResOffset = resourceInstanceSubResOffset[accessInfo.resourceId];

                // Current access specifies that the data can be discarded afterwards, mark data inactive for next access.
                const bool bActiveAfter = !(accessInfo.access.accessFlags & DeactivatingAccessMask);

                const bool bActiveBefore = subResStates[subResOffset].Access(bActiveAfter, currCmdIdx);

                if (!bActiveBefore)
                {
                    accessInfo.access.accessFlags |= ToBeDiscardedFlag;
                }
            }
            else
            {
                RPS_ASSERT(resInfo.desc.IsImage());

                const uint32_t resAspectMask    = resInfo.fullSubresourceRange.aspectMask;
                const uint32_t rangeAspectMask  = accessInfo.range.aspectMask;
                const uint32_t rangeMipLevels   = accessInfo.range.GetMipLevelCount();
                const uint32_t maxAspectMaskBit = 32 - rpsFirstBitHigh(rangeAspectMask);
                const uint32_t resArraySlices   = resInfo.fullSubresourceRange.GetArrayLayerCount();
                const uint32_t resMipLevels     = resInfo.fullSubresourceRange.GetMipLevelCount();
                const uint32_t subResPerAspect  = resArraySlices * resMipLevels;

                const RpsImageAspectUsageFlags allAspectUsages = pRuntimeDevice->GetImageAspectUsages(rangeAspectMask);
                bool bNonStencilSubResInactiveBefore = rpsAnyBitsSet(allAspectUsages, ~RPS_IMAGE_ASPECT_STENCIL);

                uint32_t aspectSubResOffset = resourceInstanceSubResOffset[accessInfo.resourceId];
                for (uint32_t aspectBitIdx = 0; aspectBitIdx < maxAspectMaskBit; aspectBitIdx++)
                {
                    const uint32_t currAspectBit = (1u << aspectBitIdx);

                    if (resAspectMask & currAspectBit)
                    {
                        if (rangeAspectMask & currAspectBit)
                        {
                            bool bAspectInactive = true;

                            const RpsImageAspectUsageFlags currAspectUsage =
                                pRuntimeDevice->GetImageAspectUsages(currAspectBit);
                            const bool bIsStencil = rpsAnyBitsSet(currAspectUsage, RPS_IMAGE_ASPECT_STENCIL);

                            // Current access specifies that the data can be discarded afterwards, mark data inactive for next access.
                            const bool bActiveAfter =
                                !(accessInfo.access.accessFlags &
                                  (bIsStencil ? StencilDeactivatingAccessMask : DeactivatingAccessMask));

                            const uint32_t arraySliceSubResOffset =
                                aspectSubResOffset + accessInfo.range.baseArrayLayer * resMipLevels;

                            uint32_t mipLevelSubResOffset = arraySliceSubResOffset + accessInfo.range.baseMipLevel;

                            for (uint32_t iArray = accessInfo.range.baseArrayLayer;
                                 iArray < accessInfo.range.arrayLayerEnd;
                                 iArray++)
                            {
                                std::for_each(subResStates.begin() + mipLevelSubResOffset,
                                              subResStates.begin() + mipLevelSubResOffset + rangeMipLevels,
                                              [&](SubResState& subResState) {
                                                  bAspectInactive &= !subResState.Access(bActiveAfter, currCmdIdx);
                                              });

                                mipLevelSubResOffset += resMipLevels;
                            }

                            if (bIsStencil)
                            {
                                if (bAspectInactive)
                                {
                                    accessInfo.access.accessFlags |= StencilToBeDiscardedFlag;
                                }
                            }
                            else
                            {
                                bNonStencilSubResInactiveBefore &= bAspectInactive;
                            }
                        }

                        aspectSubResOffset += subResPerAspect;
                    }
                }

                if (bNonStencilSubResInactiveBefore)
                {
                    accessInfo.access.accessFlags |= ToBeDiscardedFlag;
                }
            }
        }
    };
}  // namespace rps

#endif  //RPS_LIFETIME_ANALYSIS_HPP
