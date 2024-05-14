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

RPS_DECLARE_RPSL_ENTRY(test_scheduler_intrinsics, test_schedule_control);
RPS_DECLARE_RPSL_ENTRY(test_scheduler_intrinsics, test_schedule_control_nested_atomic_subgraph);
RPS_DECLARE_RPSL_ENTRY(test_scheduler_intrinsics, test_abort);
RPS_DECLARE_RPSL_ENTRY(test_scheduler_intrinsics, test_flatten_child);
RPS_DECLARE_RPSL_ENTRY(test_scheduler_intrinsics, test_flatten_parent);

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
                 std::function<void(const std::vector<uint32_t>&)> customAssertion = {})
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
        }
        else if (m_pendingValidation)
        {
            AssertSequences();
        }
        ResetSequences();
    }

    void CmdCallback(const RpsCmdCallbackContext* pContext, uint32_t id)
    {
        m_actualSequence.push_back(id);
    }

    void PushExpected(uint32_t value)
    {
        m_expectedSequence.push_back(value);
    }

    void PushExpected(std::initializer_list<uint32_t> values)
    {
        m_expectedSequence.insert(m_expectedSequence.end(), values);
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

        m_pendingValidation = true;
    }

    void DisableValidation()
    {
        m_pendingValidation = false;
    }

    void BindSubprogram(const char* name, RpsSubprogram hChildProgram)
    {
        RpsSubprogram hMainProgram = rpsRenderGraphGetMainEntry(m_renderGraph);

        rpsProgramBindNodeSubprogram(hMainProgram, name, hChildProgram);
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

    bool                  m_pendingValidation = true;
    std::vector<uint32_t> m_actualSequence;
    std::vector<uint32_t> m_expectedSequence;
};

enum TestCases : uint32_t
{
    TEST_CASE_DEFAULT           = 0,
    TEST_CASE_ATOMIC            = 1,
    TEST_CASE_SEQUENTIAL        = 2,
    TEST_CASE_ATOMIC_SEQUENTIAL = 3,
};

TEST_CASE("TestSchedulerIntrinsics_Subgraph")
{
    RpsDevice device = rpsTestUtilCreateDevice([](auto pCreateInfo, auto phDevice) {
        RpsNullRuntimeDeviceCreateInfo nullDeviceCreateInfo = {};
        nullDeviceCreateInfo.pDeviceCreateInfo              = pCreateInfo;
        return rpsNullRuntimeDeviceCreate(&nullDeviceCreateInfo, phDevice);
    });

    RPS_TEST_MALLOC_CHECKPOINT(PostCreateDevice);

    NodeOrderChecker orderChecker(device);

    rps::ResourceDesc resourceDesc(RPS_RESOURCE_TYPE_IMAGE_2D, RPS_FORMAT_R8G8B8A8_UNORM, 1920, 1080, 1);
    uint32_t          testCase      = TEST_CASE_DEFAULT;
    RpsBool           useSchBarrier = RPS_FALSE;
    RpsConstant       args[]        = {&resourceDesc, &testCase, &useSchBarrier};

    orderChecker.CreateRenderGraph(rpsTestLoadRpslEntry(test_scheduler_intrinsics, test_schedule_control));

    auto makeId = [](uint32_t callId, uint32_t localId) { return (callId << 16) | localId; };

    // Asserts the explicit dependency (3 -> 7) is obeyed:
    auto assertExplicitDependency = [](const std::vector<uint32_t>& actualSeq) {
        auto iter = std::find(actualSeq.begin(), actualSeq.end(), 3);
        REQUIRE(iter != actualSeq.end());

        iter = std::find(iter, actualSeq.end(), 7);
        REQUIRE(iter != actualSeq.end());
    };

    // Asserts the given range is atomic subgraph (can be reordered within the subgroup but not with nodes outside the subgroup):
    auto assertAtomicRange = [](const std::vector<uint32_t>& actualSeq, uint32_t lowerBound, uint32_t upperBound) {
        auto rangeStartIter = std::find_if(
            actualSeq.begin(), actualSeq.end(), [=](uint32_t i) { return (lowerBound <= i) && (i < upperBound); });
        REQUIRE(rangeStartIter != actualSeq.end());

        std::vector<bool> seenFlags;
        seenFlags.resize(upperBound - lowerBound, false);

        for (uint32_t i = 0; i < (upperBound - lowerBound); i++, ++rangeStartIter)
        {
            REQUIRE(rangeStartIter != actualSeq.end());
            REQUIRE(*rangeStartIter >= lowerBound);
            REQUIRE(*rangeStartIter < upperBound);

            const uint32_t idx = *rangeStartIter - lowerBound;
            REQUIRE(!seenFlags[idx]);
            seenFlags[idx] = true;
        }
    };

    // Asserts the given range is sequential (can not be reordered within the group, but external nodes can be inserted inbetween).
    auto assertSequentialRange = [](const std::vector<uint32_t>& actualSeq, uint32_t lowerBound, uint32_t upperBound) {
        auto rangeStartIter = std::find(actualSeq.begin(), actualSeq.end(), lowerBound);
        REQUIRE(rangeStartIter != actualSeq.end());

        uint32_t expected = lowerBound;

        for (; rangeStartIter != actualSeq.end(); ++rangeStartIter)
        {
            if ((lowerBound <= *rangeStartIter) && (*rangeStartIter < upperBound))
            {
                REQUIRE(*rangeStartIter == expected);
                expected++;

                if (expected == upperBound)
                {
                    break;
                }
            }
        }

        REQUIRE(expected == upperBound);
    };

    // Asserts given range is sequential and not reordered with any external nodes.
    auto assertEqualRange = [](const std::vector<uint32_t>& actualSeq, uint32_t lowerBound, uint32_t upperBound) {
        auto rangeStartIter = std::find(actualSeq.begin(), actualSeq.end(), lowerBound);
        REQUIRE(rangeStartIter != actualSeq.end());

        for (uint32_t expected = lowerBound; expected < upperBound; ++expected)
        {
            REQUIRE(rangeStartIter != actualSeq.end());
            REQUIRE(*rangeStartIter == expected);
            ++rangeStartIter;
        }
    };

    // Default case, only check the explicit dependency:
    orderChecker.Execute(
        args, 3, RPS_SCHEDULE_DISABLE_DEAD_CODE_ELIMINATION_BIT, RPS_DIAGNOSTIC_ENABLE_ALL, assertExplicitDependency);

    // Atomic subgraph
    testCase = TEST_CASE_ATOMIC;
    orderChecker.Execute(args,
                         3,
                         RPS_SCHEDULE_DISABLE_DEAD_CODE_ELIMINATION_BIT,
                         RPS_DIAGNOSTIC_ENABLE_ALL,
                         [&](const std::vector<uint32_t>& actualSeq) {
                             assertAtomicRange(actualSeq, makeId(2, 0), makeId(2, 12));
                             assertExplicitDependency(actualSeq);
                         });

    // Sequential subgraph
    testCase = TEST_CASE_SEQUENTIAL;
    orderChecker.Execute(args,
                         3,
                         RPS_SCHEDULE_DISABLE_DEAD_CODE_ELIMINATION_BIT,
                         RPS_DIAGNOSTIC_ENABLE_ALL,
                         [&](const std::vector<uint32_t>& actualSeq) {
                             assertSequentialRange(actualSeq, makeId(2, 0), makeId(2, 12));
                             assertExplicitDependency(actualSeq);
                         });

    // Atomic Sequential subgraph
    testCase = TEST_CASE_ATOMIC_SEQUENTIAL;
    orderChecker.Execute(args,
                         3,
                         RPS_SCHEDULE_DISABLE_DEAD_CODE_ELIMINATION_BIT,
                         RPS_DIAGNOSTIC_ENABLE_ALL,
                         [&](const std::vector<uint32_t>& actualSeq) {
                             assertEqualRange(actualSeq, makeId(2, 0), makeId(2, 12));
                             assertExplicitDependency(actualSeq);
                         });

    // Repeat with sch_barrier intrinsics enabled.
    useSchBarrier = RPS_TRUE;
    testCase      = TEST_CASE_DEFAULT;
    orderChecker.Execute(args,
                         3,
                         RPS_SCHEDULE_DISABLE_DEAD_CODE_ELIMINATION_BIT,
                         RPS_DIAGNOSTIC_ENABLE_ALL,
                         [&](const std::vector<uint32_t>& actualSeq) {
                             assertAtomicRange(actualSeq, makeId(2, 0), makeId(2, 4));
                             assertAtomicRange(actualSeq, makeId(2, 4), makeId(2, 8));
                             assertAtomicRange(actualSeq, makeId(2, 8), makeId(2, 12));
                             assertExplicitDependency(actualSeq);
                         });

    testCase = TEST_CASE_ATOMIC;
    orderChecker.Execute(args,
                         3,
                         RPS_SCHEDULE_DISABLE_DEAD_CODE_ELIMINATION_BIT,
                         RPS_DIAGNOSTIC_ENABLE_ALL,
                         [&](const std::vector<uint32_t>& actualSeq) {
                             assertAtomicRange(actualSeq, makeId(2, 0), makeId(2, 12));
                             assertAtomicRange(actualSeq, makeId(2, 0), makeId(2, 4));
                             assertAtomicRange(actualSeq, makeId(2, 4), makeId(2, 8));
                             assertAtomicRange(actualSeq, makeId(2, 8), makeId(2, 12));
                             assertExplicitDependency(actualSeq);
                         });

    testCase = TEST_CASE_SEQUENTIAL;
    orderChecker.Execute(args,
                         3,
                         RPS_SCHEDULE_DISABLE_DEAD_CODE_ELIMINATION_BIT,
                         RPS_DIAGNOSTIC_ENABLE_ALL,
                         [&](const std::vector<uint32_t>& actualSeq) {
                             assertSequentialRange(actualSeq, makeId(2, 0), makeId(2, 12));
                             assertAtomicRange(actualSeq, makeId(2, 0), makeId(2, 4));
                             assertAtomicRange(actualSeq, makeId(2, 4), makeId(2, 8));
                             assertAtomicRange(actualSeq, makeId(2, 8), makeId(2, 12));
                             assertExplicitDependency(actualSeq);
                         });

    testCase = TEST_CASE_ATOMIC_SEQUENTIAL;
    orderChecker.Execute(args,
                         3,
                         RPS_SCHEDULE_DISABLE_DEAD_CODE_ELIMINATION_BIT,
                         RPS_DIAGNOSTIC_ENABLE_ALL,
                         [&](const std::vector<uint32_t>& actualSeq) {
                             assertEqualRange(actualSeq, makeId(2, 0), makeId(2, 12));
                             // No need to validate individual atomic range anymore as EqualRange is more restrictive.
                             assertExplicitDependency(actualSeq);
                         });

    // Check nested graphs:

    orderChecker.CreateRenderGraph(
        rpsTestLoadRpslEntry(test_scheduler_intrinsics, test_schedule_control_nested_atomic_subgraph));

    orderChecker.Execute(args,
                         1,
                         RPS_SCHEDULE_DISABLE_DEAD_CODE_ELIMINATION_BIT,
                         RPS_DIAGNOSTIC_ENABLE_ALL,
                         [&](const std::vector<uint32_t>& actualSeq) {
                             assertAtomicRange(actualSeq, 5, 29);
                             assertAtomicRange(actualSeq, 9, 13);
                             assertAtomicRange(actualSeq, 13, 18);
                             assertAtomicRange(actualSeq, 18, 22);
                             assertEqualRange(actualSeq, 25, 29);
                         });

    // Checks if nodes from different subgraphs are scheduled across subgraph boundaries
    orderChecker.CreateRenderGraph(rpsTestLoadRpslEntry(test_scheduler_intrinsics, test_flatten_parent));

    RpsRenderGraphCreateInfo renderGraphCreateInfo            = {};
    renderGraphCreateInfo.mainEntryCreateInfo.hRpslEntryPoint =
        rpsTestLoadRpslEntry(test_scheduler_intrinsics, test_flatten_child);
    
    RpsRenderGraph subgraph = RPS_NULL_HANDLE;

    REQUIRE_RPS_OK(rpsRenderGraphCreate(device, &renderGraphCreateInfo, &subgraph));

    auto hSubEntry = rpsRenderGraphGetMainEntry(subgraph);
    REQUIRE_RPS_OK(rpsProgramBindNode(hSubEntry, nullptr, &NodeOrderChecker::CmdCallback, &orderChecker));

    orderChecker.BindSubprogram("N_Subprogram", hSubEntry);

    orderChecker.Execute(args,
                         1,
                         RPS_SCHEDULE_DISABLE_DEAD_CODE_ELIMINATION_BIT,
                         RPS_DIAGNOSTIC_ENABLE_ALL,
                         [&](const std::vector<uint32_t>& actualSeq) {
                              
                             //0, initial access in subgraph 0, a_0
                             //1, second  access in subgraph 0, b_0
                             //2, initial access in subgraph 1, a_1
                             //3, second  access in subgraph 1, b_1
                             
                             //Reordering the initial order [a_0, b_0, a_1, b_1] into [a_0, a_1, b_0, b_1] shows how we are reordering for transition batching even across subgraph boundaries.

                             REQUIRE(actualSeq == std::vector<uint32_t>({0, 2, 1, 3}));
                         });

    orderChecker.DestroyRenderGraph();
    rpsRenderGraphDestroy(subgraph);

    RPS_TEST_MALLOC_COUNTER_EQUAL_CURRENT(PostCreateDevice);

    // Clean up
    rpsTestUtilDestroyDevice(device);
}

void DummyCmdCallback(const RpsCmdCallbackContext* pContext)
{
}

TEST_CASE("TestAbort")
{
    RpsDevice device = rpsTestUtilCreateDevice([](auto pCreateInfo, auto phDevice) {
        RpsNullRuntimeDeviceCreateInfo nullDeviceCreateInfo = {};
        nullDeviceCreateInfo.pDeviceCreateInfo              = pCreateInfo;
        return rpsNullRuntimeDeviceCreate(&nullDeviceCreateInfo, phDevice);
    });

    RPS_TEST_MALLOC_CHECKPOINT(PostCreateDevice);

    rps::ResourceDesc resourceDesc(RPS_RESOURCE_TYPE_IMAGE_2D, RPS_FORMAT_R8G8B8A8_UNORM, 1920, 1080, 1);
    int32_t           errorCode = 0;
    RpsConstant       args[]    = {&resourceDesc, &errorCode};

    RpsRenderGraphCreateInfo renderGraphCreateInfo = {};
    renderGraphCreateInfo.mainEntryCreateInfo.hRpslEntryPoint =
        rpsTestLoadRpslEntry(test_scheduler_intrinsics, test_abort);

    RpsRenderGraph renderGraph;
    REQUIRE_RPS_OK(rpsRenderGraphCreate(device, &renderGraphCreateInfo, &renderGraph));

    auto hMainEntry = rpsRenderGraphGetMainEntry(renderGraph);

    for (int32_t iter = 0; iter < 100; iter++)
    {
        errorCode = (iter >> 1) * ((iter & 1) ? 1 : -1);

        RpsRenderGraphUpdateInfo updateInfo = {};
        updateInfo.frameIndex               = 0;
        updateInfo.gpuCompletedFrameIndex   = RPS_GPU_COMPLETED_FRAME_INDEX_NONE;
        updateInfo.numArgs                  = RPS_TEST_COUNTOF(args);
        updateInfo.ppArgs                   = args;
        updateInfo.diagnosticFlags          = RPS_DIAGNOSTIC_ENABLE_ALL;

        const RpsResult updateResult = rpsRenderGraphUpdate(renderGraph, &updateInfo);

        if (errorCode < 0)
        {
            REQUIRE(RPS_FAILED(updateResult));
        }
        else
        {
            REQUIRE_RPS_OK(updateResult);
        }
    }

    rpsRenderGraphDestroy(renderGraph);

    RPS_TEST_MALLOC_COUNTER_EQUAL_CURRENT(PostCreateDevice);

    // Clean up
    rpsTestUtilDestroyDevice(device);
}
