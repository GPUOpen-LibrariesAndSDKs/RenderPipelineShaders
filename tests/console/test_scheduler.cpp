// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <stdarg.h>

#include "rps/rps.h"
#include "utils/rps_test_common.h"

#include <random>
#include <numeric>

RPS_DECLARE_RPSL_ENTRY(test_scheduler, program_order);
RPS_DECLARE_RPSL_ENTRY(test_scheduler, memory_saving);
RPS_DECLARE_RPSL_ENTRY(test_scheduler, random_order);
RPS_DECLARE_RPSL_ENTRY(test_scheduler, dead_code_elimination);
RPS_DECLARE_RPSL_ENTRY(test_scheduler, gfx_comp_batching);
RPS_DECLARE_RPSL_ENTRY(test_scheduler, subres_data_lifetime);

struct NodeOrderChecker
{
public:
    NodeOrderChecker(RpsDevice hDevice)
        : m_device(hDevice)
        , m_mt19937(std::random_device()())
    {
    }

    void CreateRenderGraph(RpsRpslEntry hEntry)
    {
        DestroyRenderGraph();

        RpsRenderGraphCreateInfo renderGraphCreateInfo            = {};
        renderGraphCreateInfo.mainEntryCreateInfo.hRpslEntryPoint = hEntry;

        REQUIRE_RPS_OK(rpsRenderGraphCreate(m_device, &renderGraphCreateInfo, &m_renderGraph));

        auto hMainEntry = rpsRenderGraphGetMainEntry(m_renderGraph);
        REQUIRE_RPS_OK(rpsProgramBindNode(hMainEntry, nullptr, &NodeOrderChecker::CmdCallback, this));
    }

    void DestroyRenderGraph()
    {
        rpsRenderGraphDestroy(m_renderGraph);
        m_renderGraph = RPS_NULL_HANDLE;
    }

    void Execute(const RpsConstant*                                ppArgs,
                 uint32_t                                          numArgs,
                 RpsScheduleFlags                                  scheduleFlags,
                 RpsDiagnosticFlags                                diagnosticFlags = RPS_DIAGNOSTIC_ENABLE_ALL,
                 std::function<void(const std::vector<uint32_t>&)> customAssertion = {},
                 std::function<void(const RpsCmdCallbackContext* pContext, uint32_t id)> customNodeCallback = {})
    {
        RpsRandomNumberGenerator randGen = {};
        randGen.pContext                 = this;
        randGen.pfnRandomUniformInt      = &RandGen;

        RpsRenderGraphUpdateInfo updateInfo = {};
        updateInfo.frameIndex               = 0;
        updateInfo.gpuCompletedFrameIndex   = RPS_GPU_COMPLETED_FRAME_INDEX_NONE;
        updateInfo.numArgs                  = numArgs;
        updateInfo.ppArgs                   = ppArgs;
        updateInfo.ppArgResources           = nullptr;
        updateInfo.diagnosticFlags          = diagnosticFlags;
        updateInfo.scheduleFlags            = scheduleFlags;
        updateInfo.pRandomNumberGenerator   = &randGen;

        RpsRenderGraphRecordCommandInfo recordInfo  = {};
        RpsRenderGraphBatchLayout       batchLayout = {};

        REQUIRE_RPS_OK(rpsRenderGraphUpdate(m_renderGraph, &updateInfo));

        REQUIRE_RPS_OK(rpsRenderGraphGetBatchLayout(m_renderGraph, &batchLayout));

        m_customNodeCallback = customNodeCallback;

        for (uint32_t iBatch = 0; iBatch < batchLayout.numCmdBatches; iBatch++)
        {
            REQUIRE(batchLayout.numCmdBatches == 1);

            recordInfo.cmdBeginIndex = batchLayout.pCmdBatches[iBatch].cmdBegin;
            recordInfo.numCmds       = batchLayout.pCmdBatches[iBatch].numCmds;

            REQUIRE_RPS_OK(rpsRenderGraphRecordCommands(m_renderGraph, &recordInfo));
        }

        if (customAssertion)
        {
            customAssertion(m_actualSequence);
            ResetSequences();
        }
        else
        {
            AssertAndResetSequences();
        }
    }

    void CmdCallback(const RpsCmdCallbackContext* pContext, uint32_t id)
    {
        m_actualSequence.push_back(id);

        if (m_customNodeCallback)
        {
            m_customNodeCallback(pContext, id);
        }
    }

    void PushExpected(uint32_t value)
    {
        m_expectedSequence.push_back(value);
    }

    void PushExpected(std::initializer_list<uint32_t> values)
    {
        m_expectedSequence.insert(m_expectedSequence.end(), values);
    }

    void PushExpected(const std::vector<uint32_t>& values)
    {
        m_expectedSequence.insert(m_expectedSequence.end(), values.begin(), values.end());
    }

    void PushExpectedRange(uint32_t begin, uint32_t end, int32_t step)
    {
        REQUIRE(begin != end);
        REQUIRE(step != 0);

        if (begin < end)
        {
            for (uint32_t i = begin; i < end; i += step)
            {
                m_expectedSequence.push_back(i);
            }
        }
        else
        {
            for (uint32_t i = end; i < begin; i += step)
            {
                m_expectedSequence.push_back(i);
            }
        }
    }

private:
    void AssertSequences() const
    {
        REQUIRE(m_actualSequence.size() == m_expectedSequence.size());

        for (uint32_t i = 0; i < m_actualSequence.size(); i++)
        {
            REQUIRE(m_actualSequence[i] == m_expectedSequence[i]);
        }
    }

    void AssertAndResetSequences()
    {
        AssertSequences();
        ResetSequences();
    }

    void ResetSequences()
    {
        m_actualSequence.clear();
        m_expectedSequence.clear();
    }

    static int32_t RandGen(void* pContext, int32_t minVal, int32_t maxVal)
    {
        NodeOrderChecker* pThis = static_cast<NodeOrderChecker*>(pContext);
        return std::uniform_int_distribution<>(minVal, maxVal)(pThis->m_mt19937);
    }

private:
    RpsDevice      m_device = {};
    std::mt19937   m_mt19937;
    RpsRenderGraph m_renderGraph = {};

    std::vector<uint32_t> m_actualSequence;
    std::vector<uint32_t> m_expectedSequence;
    std::function<void(const RpsCmdCallbackContext* pContext, uint32_t id)> m_customNodeCallback;
};

TEST_CASE("TestScheduler")
{
    RpsDevice device = rpsTestUtilCreateDevice([](auto pCreateInfo, auto phDevice) {
        RpsNullRuntimeDeviceCreateInfo nullDeviceCreateInfo = {};
        nullDeviceCreateInfo.pDeviceCreateInfo              = pCreateInfo;
        return rpsNullRuntimeDeviceCreate(&nullDeviceCreateInfo, phDevice);
    });

    RPS_TEST_MALLOC_CHECKPOINT(PostCreateDevice);

    NodeOrderChecker orderChecker(device);

    rps::ResourceDesc resourceDesc(RPS_RESOURCE_TYPE_IMAGE_2D, RPS_FORMAT_R8G8B8A8_UNORM, 1920, 1080, 1);

    RpsConstant args[] = {&resourceDesc, nullptr, nullptr};

    // Default scheduling:

    orderChecker.CreateRenderGraph(rpsTestLoadRpslEntry(test_scheduler, program_order));

    // Expect interleaved Draw / Blt to be rescheduled and grouped together ( Draw x 6 + Blt x 6 + Draw x 6 + Blt x 6 ):
    orderChecker.PushExpectedRange(0, 12, 1);
    orderChecker.PushExpectedRange(12, 24, 2);
    orderChecker.PushExpectedRange(13, 25, 2);
    orderChecker.Execute(args, 1, RPS_SCHEDULE_DEFAULT);

    // Force Program-Order scheduling:

    // Expect program order ( Draw x 6 + Blt x 6 + ( Draw + Blt ) x 6 ):
    orderChecker.PushExpectedRange(0, 24, 1);
    orderChecker.Execute(args, 1, RPS_SCHEDULE_KEEP_PROGRAM_ORDER_BIT);

    // Prefer memory-saving scheduling:
    orderChecker.CreateRenderGraph(rpsTestLoadRpslEntry(test_scheduler, memory_saving));

    // Expect default order ( Draw x 6 + Blt x 6 )
    orderChecker.PushExpectedRange(0, 12, 1);
    orderChecker.Execute(args, 1, RPS_SCHEDULE_DEFAULT);

    // Expect memory-saving order ( (Draw + Blt) x 6 )
    for (uint32_t i = 0; i < 6; i++)
    {
        orderChecker.PushExpectedRange(i, i + 6 + 1, 6);
    }
    orderChecker.Execute(args, 1, RPS_SCHEDULE_PREFER_MEMORY_SAVING_BIT);

    // Random ordering

    orderChecker.CreateRenderGraph(rpsTestLoadRpslEntry(test_scheduler, random_order));

    constexpr uint32_t NumIndependentNodes  = 12;
    constexpr uint32_t NumIterations        = 100;
    constexpr int32_t  ExpectedSum          = (NumIndependentNodes * (NumIndependentNodes - 1) / 2) * NumIterations;
    constexpr int32_t  ExpectedAvgSumPerCmd = ExpectedSum / NumIndependentNodes;

    orderChecker.PushExpectedRange(0, NumIndependentNodes + 1, 1);
    orderChecker.Execute(args, 1, RPS_SCHEDULE_KEEP_PROGRAM_ORDER_BIT);

    uint32_t sumsPerCmd[NumIndependentNodes] = {};

    for (uint32_t iFrame = 0; iFrame < NumIterations; iFrame++)
    {
        orderChecker.Execute(args,
                             1,
                             RPS_SCHEDULE_RANDOM_ORDER_BIT,
                             (iFrame < 5) ? RPS_DIAGNOSTIC_ENABLE_POST_SCHEDULE_DUMP : RPS_DIAGNOSTIC_NONE,
                             [&](auto& sequence) {
                                 REQUIRE(sequence.size() == (NumIndependentNodes + 1));
                                 REQUIRE(sequence.back() == NumIndependentNodes);

                                 for (uint32_t i = 0; i < NumIndependentNodes; i++)
                                 {
                                     sumsPerCmd[i] += sequence[i];
                                 }
                             });
    }

    uint32_t totalSum = 0;
    for (uint32_t i = 0; i < NumIndependentNodes; i++)
    {
        totalSum += sumsPerCmd[i];

        //TODO find a better solution that does not occasionally fail like this
        REQUIRE(abs(int32_t(sumsPerCmd[i]) - ExpectedAvgSumPerCmd) < (ExpectedAvgSumPerCmd / 2));
    }

    REQUIRE(totalSum == ExpectedSum);

    // Dead code elimination:

    orderChecker.CreateRenderGraph(rpsTestLoadRpslEntry(test_scheduler, dead_code_elimination));

    const RpsBool bBltEnable = RPS_TRUE, bBltDisable = RPS_FALSE;

    args[1] = &bBltEnable;
    args[2] = &bBltEnable;
    orderChecker.PushExpected({0, 1, 2, 3});
    orderChecker.Execute(args, 3, RPS_SCHEDULE_DEFAULT, RPS_DIAGNOSTIC_ENABLE_ALL);
    orderChecker.PushExpected({0, 1, 2, 3});
    orderChecker.Execute(args, 3, RPS_SCHEDULE_DISABLE_DEAD_CODE_ELIMINATION_BIT, RPS_DIAGNOSTIC_ENABLE_ALL);

    auto unorderedEqual = [](const std::vector<uint32_t>& vec1, const std::vector<uint32_t>& vec2) {
        if (vec1.size() != vec2.size())
            return false;

        std::vector<uint32_t> counter;
        counter.reserve(vec1.size());

        uint32_t slotCount = 0;
        for (uint32_t elem : vec1)
        {
            if (elem >= counter.size())
                counter.resize(elem + 1, 0);

            slotCount += (0 == (counter[elem]++)) ? 1 : 0;
        }

        uint32_t reachingZeroCount = 0;

        for (uint32_t elem : vec2)
        {
            if (elem >= counter.size())
                return false;
            reachingZeroCount += (0 == --(counter[elem])) ? 1 : 0;
        }

        return slotCount == reachingZeroCount;
    };

    args[1] = &bBltDisable;
    args[2] = &bBltEnable;
    orderChecker.PushExpected({1, 3});
    orderChecker.Execute(args, 3, RPS_SCHEDULE_DEFAULT, RPS_DIAGNOSTIC_ENABLE_ALL);
    orderChecker.Execute(
        args, 3, RPS_SCHEDULE_DISABLE_DEAD_CODE_ELIMINATION_BIT, RPS_DIAGNOSTIC_ENABLE_ALL, [=](auto& actualSequence) {
            REQUIRE(unorderedEqual({0, 1, 3}, actualSequence));
        });

    args[1] = &bBltEnable;
    args[2] = &bBltDisable;
    orderChecker.PushExpected({0, 2});
    orderChecker.Execute(args, 3, RPS_SCHEDULE_DEFAULT, RPS_DIAGNOSTIC_ENABLE_ALL);
    orderChecker.Execute(
        args, 3, RPS_SCHEDULE_DISABLE_DEAD_CODE_ELIMINATION_BIT, RPS_DIAGNOSTIC_ENABLE_ALL, [=](auto& actualSequence) {
            REQUIRE(unorderedEqual({0, 1, 2}, actualSequence));
        });

    args[1] = &bBltDisable;
    args[2] = &bBltDisable;
    orderChecker.PushExpected({});
    orderChecker.Execute(args, 3, RPS_SCHEDULE_DEFAULT, RPS_DIAGNOSTIC_ENABLE_ALL);
    orderChecker.Execute(
        args, 3, RPS_SCHEDULE_DISABLE_DEAD_CODE_ELIMINATION_BIT, RPS_DIAGNOSTIC_ENABLE_ALL, [=](auto& actualSequence) {
            REQUIRE(unorderedEqual({0, 1}, actualSequence));
        });

    // Graphics / Compute interleave

    orderChecker.CreateRenderGraph(rpsTestLoadRpslEntry(test_scheduler, gfx_comp_batching));

    // Default behavior, expect Gfx & Compute are interleaved:
    orderChecker.PushExpected({0, 1, 3, 2, 4, 5, 6, 7, 8, 9, 10, 11});
    orderChecker.Execute(args, 1, RPS_SCHEDULE_DEFAULT, RPS_DIAGNOSTIC_ENABLE_ALL);

    // Aggressive pipelining, expect compute to be moved before gfx while interleaved:
    orderChecker.PushExpected({1, 0, 2, 3, 5, 4, 6, 7, 8, 9, 10, 11});
    orderChecker.Execute(args, 1, RPS_SCHEDULE_WORKLOAD_TYPE_PIPELINING_AGGRESSIVE_BIT, RPS_DIAGNOSTIC_ENABLE_ALL);

    // Prefer minimize compute & gfx switching:
    orderChecker.PushExpected({0, 3, 4, 1, 2, 5, 6, 7, 8, 9, 10, 11});
    orderChecker.Execute(args, 1, RPS_SCHEDULE_MINIMIZE_COMPUTE_GFX_SWITCH_BIT, RPS_DIAGNOSTIC_ENABLE_ALL);

    orderChecker.DestroyRenderGraph();

    // Check lifetime / Discard flags

    orderChecker.CreateRenderGraph(rpsTestLoadRpslEntry(test_scheduler, subres_data_lifetime));

    // Default behavior, expect Gfx & Compute are interleaved:

    auto checkDiscardFlags = [](const RpsCmdCallbackContext* pContext, uint32_t nodeId) {
        // clang-format off
        static struct
        {
            uint32_t                              nodeId;
            std::initializer_list<RpsAccessFlags> expectedDiscardAccesses;
        } expected [] = {
            {0, {RPS_ACCESS_DISCARD_DATA_BEFORE_BIT}},
            {1, {RPS_ACCESS_DISCARD_DATA_BEFORE_BIT, 0}},
            {2, {RPS_ACCESS_DISCARD_DATA_BEFORE_BIT | RPS_ACCESS_DISCARD_DATA_AFTER_BIT}},
            {3, {RPS_ACCESS_DISCARD_DATA_BEFORE_BIT, RPS_ACCESS_DISCARD_DATA_BEFORE_BIT}},
            {4, {0}}, // Not all sub-resources are discarded.
            {5, {RPS_ACCESS_DISCARD_DATA_BEFORE_BIT, RPS_ACCESS_DISCARD_DATA_AFTER_BIT}},
            {6, {0}},
            {7, {RPS_ACCESS_DISCARD_DATA_BEFORE_BIT, RPS_ACCESS_DISCARD_DATA_BEFORE_BIT}},
            {8, {0}},
            {9, {0, RPS_ACCESS_DISCARD_DATA_BEFORE_BIT | RPS_ACCESS_STENCIL_DISCARD_DATA_BEFORE_BIT | RPS_ACCESS_STENCIL_DISCARD_DATA_AFTER_BIT}},
            {10, {0, RPS_ACCESS_DISCARD_DATA_AFTER_BIT | RPS_ACCESS_STENCIL_DISCARD_DATA_BEFORE_BIT}},
            {11, {0, RPS_ACCESS_DISCARD_DATA_BEFORE_BIT | RPS_ACCESS_STENCIL_DISCARD_DATA_AFTER_BIT}},
            {12, {0, RPS_ACCESS_DISCARD_DATA_AFTER_BIT}},
            {13, {0, RPS_ACCESS_DISCARD_DATA_BEFORE_BIT | RPS_ACCESS_STENCIL_DISCARD_DATA_BEFORE_BIT}},
            {14, {0, 0}},
            {15, {0, RPS_ACCESS_DISCARD_DATA_AFTER_BIT | RPS_ACCESS_STENCIL_DISCARD_DATA_AFTER_BIT}},
            {16, {0, RPS_ACCESS_DISCARD_DATA_AFTER_BIT}},
            {17, {0, 0}},

            {18, {RPS_ACCESS_DISCARD_DATA_BEFORE_BIT}},
            {19, {RPS_ACCESS_DISCARD_DATA_BEFORE_BIT}},
            {20, {RPS_ACCESS_DISCARD_DATA_BEFORE_BIT | RPS_ACCESS_DISCARD_DATA_AFTER_BIT}},
            {21, {RPS_ACCESS_DISCARD_DATA_BEFORE_BIT}},
           
            {22, {
                    0,
                    RPS_ACCESS_DISCARD_DATA_BEFORE_BIT | RPS_ACCESS_DISCARD_DATA_AFTER_BIT,
                    RPS_ACCESS_DISCARD_DATA_BEFORE_BIT,
                    0,
                    RPS_ACCESS_DISCARD_DATA_AFTER_BIT,
                    RPS_ACCESS_DISCARD_DATA_AFTER_BIT,
                    RPS_ACCESS_DISCARD_DATA_AFTER_BIT,
                    0, 0,
                    RPS_ACCESS_DISCARD_DATA_BEFORE_BIT | RPS_ACCESS_DISCARD_DATA_AFTER_BIT}},

            {23, {
                    0,
                    RPS_ACCESS_DISCARD_DATA_BEFORE_BIT,
                    RPS_ACCESS_DISCARD_DATA_BEFORE_BIT,
                    RPS_ACCESS_DISCARD_DATA_AFTER_BIT, RPS_ACCESS_DISCARD_DATA_AFTER_BIT,
                    RPS_ACCESS_DISCARD_DATA_AFTER_BIT | RPS_ACCESS_DISCARD_DATA_BEFORE_BIT,
                    RPS_ACCESS_DISCARD_DATA_AFTER_BIT, RPS_ACCESS_DISCARD_DATA_AFTER_BIT, RPS_ACCESS_DISCARD_DATA_AFTER_BIT,
                    RPS_ACCESS_DISCARD_DATA_BEFORE_BIT | RPS_ACCESS_DISCARD_DATA_AFTER_BIT}},

            {24, {0, 0}},
            {25, {0, 0}},
            {26, {0, RPS_ACCESS_DISCARD_DATA_BEFORE_BIT | RPS_ACCESS_DISCARD_DATA_AFTER_BIT}},

            {27, {RPS_ACCESS_DISCARD_DATA_AFTER_BIT, RPS_ACCESS_DISCARD_DATA_BEFORE_BIT}},

            {28, {0, 0}},
            {29, {0, RPS_ACCESS_DISCARD_DATA_BEFORE_BIT}},
            {30, {0, RPS_ACCESS_DISCARD_DATA_AFTER_BIT}},
        };
        // clang-format on

        static constexpr RpsAccessFlags DiscardFlags =
            RPS_ACCESS_DISCARD_DATA_BEFORE_BIT | RPS_ACCESS_DISCARD_DATA_AFTER_BIT |
            RPS_ACCESS_STENCIL_DISCARD_DATA_BEFORE_BIT | RPS_ACCESS_STENCIL_DISCARD_DATA_AFTER_BIT;

        REQUIRE(nodeId < RPS_TEST_COUNTOF(expected));
        REQUIRE(nodeId == expected[nodeId].nodeId);

        for (uint32_t iArg = 1; iArg < pContext->numArgs; iArg++)
        {
            if ((iArg - 1) < expected[nodeId].expectedDiscardAccesses.size())
            {
                RpsResourceAccessInfo accessInfo = {};
                REQUIRE_RPS_OK(rpsCmdGetArgResourceAccessInfo(pContext, iArg, &accessInfo));
                REQUIRE((accessInfo.access.accessFlags & DiscardFlags) ==
                        *(expected[nodeId].expectedDiscardAccesses.begin() + (iArg - 1)));
            }
        }
    };

    auto iotaFn = [](uint32_t begin, uint32_t count) {
        std::vector<uint32_t> result;
        result.resize(count);
        std::iota(result.begin(), result.end(), begin);
        return result;
    };

    orderChecker.PushExpected(iotaFn(0, 31));
    orderChecker.Execute(args,
                         1,
                         RPS_SCHEDULE_KEEP_PROGRAM_ORDER_BIT | RPS_SCHEDULE_DISABLE_DEAD_CODE_ELIMINATION_BIT,
                         RPS_DIAGNOSTIC_ENABLE_ALL,
                         {},
                         checkDiscardFlags);

    orderChecker.DestroyRenderGraph();

    RPS_TEST_MALLOC_COUNTER_EQUAL_CURRENT(PostCreateDevice);

    // Clean up
    rpsTestUtilDestroyDevice(device);
}
