// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_PERSISTENT_INDEX_GENERATOR_HPP
#define RPS_PERSISTENT_INDEX_GENERATOR_HPP

#include "core/rps_core.hpp"
#include "core/rps_util.hpp"

namespace rps
{
    template <uint32_t NumResourceKinds>
    class PersistentIdGenerator
    {
        struct BlockInfo
        {
            uint32_t numResources[NumResourceKinds];
            uint32_t localIndex;
            uint32_t numChildren;

            BlockInfo()
                : localIndex(RPS_INDEX_NONE_U32)
                , numChildren(0)
            {
            }

            bool IsInitialized()
            {
                return localIndex != RPS_INDEX_NONE_U32;
            }
        };

        struct BlockInstance
        {
            uint32_t isReached : 1;
            uint32_t blockId : 31;
            uint32_t nextIteration;
            uint32_t offsets[NumResourceKinds];

            BlockInstance()
                : isReached(RPS_FALSE)
                , blockId(0)
                , nextIteration(RPS_INDEX_NONE_U32)
                , offsets{}
            {
            }
        };

    public:
        PersistentIdGenerator(Arena& allocator)
            : m_numIndicesTotal{}
            , m_blocks(&allocator)
            , m_blockStack(&allocator)
            , m_blockInstances(&allocator)
        {
        }

        RpsResult EnterFunction(uint32_t                blockId,
                                ConstArrayRef<uint32_t> resourceCounts,
                                uint32_t                localLoopIndex,
                                uint32_t                numChildren)
        {
            RPS_ASSERT(m_blockStack.empty());  // Currently everything is inlined.

            InitBlockInfo(blockId, resourceCounts, localLoopIndex, numChildren);

            RPS_ASSERT(m_blockInstances.empty() || (m_blockInstances.front().blockId == 0));

            uint32_t rootBlockInstanceId = 0;

            if (m_blockInstances.empty())
            {
                rootBlockInstanceId = AllocBlockInstances(1 + numChildren);
                RPS_RETURN_ERROR_IF(rootBlockInstanceId == RPS_INDEX_NONE_U32, RPS_ERROR_OUT_OF_MEMORY);

                RPS_ASSERT(rootBlockInstanceId == 0);  // Currently everything is inlined.
            }

            InitBlockInstance(blockId, rootBlockInstanceId);

            m_currentBlockInstanceId = rootBlockInstanceId;

            return RPS_OK;
        }

        RpsResult EnterLoop(uint32_t                blockId,
                            ConstArrayRef<uint32_t> resourceCounts,
                            uint32_t                localLoopIndex,
                            uint32_t                numChildren)
        {
            RPS_V_RETURN(InitBlockInfo(blockId, resourceCounts, localLoopIndex, numChildren));

            RPS_CHECK_ALLOC(m_blockStack.push_back(m_currentBlockInstanceId));

            return RPS_OK;
        }

        RpsResult ExitLoop(uint32_t blockId)
        {
            const uint32_t parentBlockInstance = m_blockStack.back();

            m_blockStack.pop_back();

            m_currentBlockInstanceId = parentBlockInstance;

            return RPS_OK;
        }

        RpsResult LoopIteration(uint32_t blockId)
        {
            RPS_ASSERT(!m_blockStack.empty());

            const uint32_t parentId        = m_blockStack.back();
            const bool     bFirstIteration = (parentId == m_currentBlockInstanceId);

            const BlockInfo& currBlockInfo = m_blocks[blockId];

            uint32_t currBlockInstanceId = RPS_INDEX_NONE_U32;
            uint32_t prevBlockInstanceId = m_currentBlockInstanceId;

            BlockInstance* pPrevBlock = &m_blockInstances[prevBlockInstanceId];

            if (bFirstIteration)
            {
                prevBlockInstanceId = m_currentBlockInstanceId + 1 + currBlockInfo.localIndex;
                pPrevBlock          = &m_blockInstances[prevBlockInstanceId];
            }
            else
            {
                RPS_RETURN_ERROR_IF(blockId != pPrevBlock->blockId, RPS_ERROR_INVALID_PROGRAM);
            }

            if (pPrevBlock->nextIteration == UINT32_MAX)
            {
                uint32_t newRangeOffset = AllocBlockInstances(1 + currBlockInfo.numChildren);
                RPS_RETURN_ERROR_IF(newRangeOffset == RPS_INDEX_NONE_U32, RPS_ERROR_OUT_OF_MEMORY);

                pPrevBlock                = &m_blockInstances[prevBlockInstanceId];
                pPrevBlock->nextIteration = newRangeOffset;
            }

            currBlockInstanceId = pPrevBlock->nextIteration;

            InitBlockInstance(blockId, currBlockInstanceId);
            m_currentBlockInstanceId = currBlockInstanceId;

            return RPS_OK;
        }

        void Reset()
        {
            std::fill(std::begin(m_numIndicesTotal), std::end(m_numIndicesTotal), 0);

            m_blocks.reset();
            m_blockStack.reset();
            m_blockInstances.reset();

            m_currentBlockInstanceId = RPS_INDEX_NONE_U32;
        }

        void Clear()
        {
            std::fill(std::begin(m_numIndicesTotal), std::end(m_numIndicesTotal), 0);

            m_blocks.clear();
            m_blockStack.clear();
            m_blockInstances.clear();

            m_currentBlockInstanceId = RPS_INDEX_NONE_U32;
        }

        template <uint32_t IndexKind>
        TResult<uint32_t> Generate(uint32_t localIndex)
        {
            const BlockInstance& currBlockInstance = m_blockInstances[m_currentBlockInstanceId];
            if (localIndex >= m_blocks[currBlockInstance.blockId].numResources[IndexKind])
            {
                return MakeResult(RPS_INDEX_NONE_U32, RPS_ERROR_INVALID_PROGRAM);
            }

            const uint32_t resourceIdx = localIndex + currBlockInstance.offsets[IndexKind];
            return resourceIdx;
        }

    private:
        RpsResult InitBlockInfo(uint32_t                blockId,
                                ConstArrayRef<uint32_t> resourceCounts,
                                uint32_t                localLoopIndex,
                                uint32_t                numChildren)
        {
            if (blockId >= m_blocks.size())
            {
                m_blocks.resize(blockId + 1, BlockInfo());
            }

            auto& blockInfo = m_blocks[blockId];

            if (!blockInfo.IsInitialized())
            {
                std::copy(resourceCounts.begin(), resourceCounts.end(), blockInfo.numResources);
                blockInfo.localIndex  = localLoopIndex;
                blockInfo.numChildren = numChildren;
            }
            else
            {
                RPS_RETURN_ERROR_IF(!std::equal(resourceCounts.begin(), resourceCounts.end(), blockInfo.numResources) ||
                                        (blockInfo.localIndex != localLoopIndex) ||
                                        (blockInfo.numChildren != numChildren),
                                    RPS_ERROR_INVALID_PROGRAM);
            }

            return RPS_OK;
        }

        uint32_t AllocBlockInstances(uint32_t count)
        {
            const uint32_t offset = uint32_t(m_blockInstances.size());

            if (m_blockInstances.resize(m_blockInstances.size() + count, BlockInstance()))
            {
                return offset;
            }

            return RPS_INDEX_NONE_U32;
        }

        RpsResult InitBlockInstance(uint32_t blockId, uint32_t instanceId)
        {
            const BlockInfo& blockInfo     = m_blocks[blockId];
            BlockInstance&   blockInstance = m_blockInstances[instanceId];

            if (!blockInstance.isReached)
            {
                blockInstance.isReached     = RPS_TRUE;
                blockInstance.blockId       = blockId;
                blockInstance.nextIteration = UINT32_MAX;

                for (uint32_t iResourceKind = 0; iResourceKind < NumResourceKinds; iResourceKind++)
                {
                    blockInstance.offsets[iResourceKind] = m_numIndicesTotal[iResourceKind];
                    m_numIndicesTotal[iResourceKind] += blockInfo.numResources[iResourceKind];
                }
            }
            else
            {
                RPS_RETURN_ERROR_IF((blockInstance.blockId != blockId), RPS_ERROR_INVALID_PROGRAM);

                for (uint32_t iResourceKind = 0; iResourceKind < NumResourceKinds; iResourceKind++)
                {
                    RPS_RETURN_ERROR_IF((blockInstance.offsets[iResourceKind] + blockInfo.numResources[iResourceKind]) >
                                            m_numIndicesTotal[iResourceKind],
                                        RPS_ERROR_INVALID_PROGRAM);
                }
            }

            return RPS_OK;
        }

    private:
        uint32_t m_numIndicesTotal[NumResourceKinds];

        ArenaVector<BlockInfo>     m_blocks;
        ArenaVector<uint32_t>      m_blockStack;
        ArenaVector<BlockInstance> m_blockInstances;

        uint32_t m_currentBlockInstanceId = RPS_INDEX_NONE_U32;
    };

}  // namespace rps

#endif  //RPS_PERSISTENT_INDEX_GENERATOR_HPP
