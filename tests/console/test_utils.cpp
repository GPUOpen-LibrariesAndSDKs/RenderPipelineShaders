// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <stdarg.h>

#include "core/rps_util.hpp"

#include "utils/rps_test_common.h"

#include <limits>
#include <array>
#include <cfloat>

template<typename T, size_t N>
void CheckMinMax(T (&arr)[N])
{
    for (auto i = std::cbegin(arr); i != std::cend(arr); ++i)
    {
        for (auto j = std::cbegin(arr); j != std::cend(arr); ++j)
        {
            REQUIRE(std::min(*i, *j) == rpsMin(*i, *j));
            REQUIRE(std::max(*i, *j) == rpsMax(*i, *j));
        }
    }
}

TEST_CASE("MinMaxUtils")
{
    int32_t valuesS32[] = {
        INT32_MIN,
        INT32_MIN + 1,
        -42,
        -2,
        -1,
        0,
        1,
        3,
        101,
        INT32_MAX - 1,
        INT32_MAX,
    };
    uint32_t valuesU32[] = {
        0,
        1,
        2,
        3,
        42,
        UINT32_MAX - 1,
        UINT32_MAX,
    };
    int64_t valuesS64[] = {
        INT64_MIN,
        INT64_MIN + 1,
        INT32_MIN,
        INT32_MIN + 1,
        -42,
        -2,
        -1,
        0,
        1,
        3,
        101,
        INT32_MAX - 1,
        INT32_MAX,
        INT64_MAX - 1,
        INT64_MAX,
    };
    uint64_t valuesU64[] = {
        0,
        1,
        2,
        3,
        42,
        UINT32_MAX - 1,
        UINT32_MAX,
        UINT64_MAX - 1,
        UINT64_MAX,
    };
    float valuesF32[] = {
        -std::numeric_limits<float>::infinity(),
        -FLT_MAX,
        -1E9f,
        -42.0f,
        -3.1415927f,
        -2.0f,
        -1.0f,
        -0.5f,
        -FLT_EPSILON,
        -FLT_MIN,
        -std::numeric_limits<float>::denorm_min(),
        0.5f,
        1.0f,
        2.0f,
        3.1415927f,
        42.0f,
        1E9f,
        std::numeric_limits<float>::denorm_min(),
        FLT_MIN,
        FLT_EPSILON,
        FLT_MAX,
        std::numeric_limits<float>::infinity(),
    };
    size_t valuesSize[] = {
        0,
        1,
        3,
        42,
        SIZE_MAX - 1,
        SIZE_MAX,
    };

    CheckMinMax(valuesS32);
    CheckMinMax(valuesU32);
    CheckMinMax(valuesS64);
    CheckMinMax(valuesU64);
    CheckMinMax(valuesF32);
    CheckMinMax(valuesSize);
}


TEST_CASE("BitUtils")
{
    REQUIRE(rpsFirstBitHigh(0u) == 32);
    REQUIRE(rpsFirstBitHigh(1u) == 31);
    REQUIRE(rpsFirstBitHigh(UINT32_MAX) == 0);
    REQUIRE(rpsFirstBitHigh(0x80000000u) == 0);
    REQUIRE(rpsFirstBitHigh(0x7FFFFFFFu) == 1);
    REQUIRE(rpsFirstBitHigh(0xFFFFu) == 16);
    REQUIRE(rpsFirstBitHigh(0x3Fu) == 26);

    REQUIRE(rpsFirstBitLow(0u) == 32);
    REQUIRE(rpsFirstBitLow(1u) == 0);
    REQUIRE(rpsFirstBitLow(UINT32_MAX) == 0);
    REQUIRE(rpsFirstBitLow(0x80000000u) == 31);
    REQUIRE(rpsFirstBitLow(0x0FFFFFFEu) == 1);
    REQUIRE(rpsFirstBitLow(0xFF0000u) == 16);
    REQUIRE(rpsFirstBitLow(0xFC00u) == 10);

    uint32_t values[] = {
        0,
        UINT32_MAX,
        1,
        2,
        4,
        31,
        42,
        0x80000000u,
        UINT32_MAX - 1,
        UINT16_MAX,
        UINT16_MAX + 1,
        UINT16_MAX + 42,
        0x12345678,
        0xABCD1234,
        0xDEADBEEF,
    };

    for (auto i = std::cbegin(values); i != std::cend(values); ++i)
    {
        const auto rpsReverseBits32Result = rpsReverseBits32(*i);
        for (uint32_t b = 0; b < 32; b++)
        {
            REQUIRE(rpsAnyBitsSet(*i, 1u << b) == rpsAnyBitsSet(rpsReverseBits32Result, 1u << (31 - b)));
        }

        REQUIRE(*i == rpsReverseBits32(rpsReverseBits32Result));

        if (*i > 0x80000000u)
            continue;

        const auto rpsRoundUpToPowerOfTwoResult = rpsRoundUpToPowerOfTwo(*i);

        REQUIRE(rpsIsPowerOfTwo(rpsRoundUpToPowerOfTwoResult));

        REQUIRE(rpsRoundUpToPowerOfTwoResult >= *i);
        REQUIRE(((*i == 0) || ((rpsRoundUpToPowerOfTwoResult >> 1u) < *i)));
    }
}

TEST_CASE("AlignmentUtils")
{
    REQUIRE(rpsDivRoundUp(0, 8) == 0);
    REQUIRE(rpsDivRoundUp(1, 8) == 1);
    REQUIRE(rpsDivRoundUp(7, 8) == 1);
    REQUIRE(rpsDivRoundUp(8, 8) == 1);
    REQUIRE(rpsDivRoundUp(9, 8) == 2);

    REQUIRE(rpsAlignUp<uint32_t>(0, 4) == 0);
    REQUIRE(rpsAlignUp<uint32_t>(1, 4) == 4);
    REQUIRE(rpsAlignUp<uint32_t>(3, 4) == 4);
    REQUIRE(rpsAlignUp<uint32_t>(4, 4) == 4);
    REQUIRE(rpsAlignUp<uint32_t>(5, 4) == 8);

    REQUIRE(rpsAlignUp<uint64_t>(0, 4) == 0);
    REQUIRE(rpsAlignUp<uint64_t>(1, 4) == 4);
    REQUIRE(rpsAlignUp<uint64_t>(3, 4) == 4);
    REQUIRE(rpsAlignUp<uint64_t>(4, 4) == 4);
    REQUIRE(rpsAlignUp<uint64_t>(5, 4) == 8);

    uint8_t  foo[1024];
    uint8_t* pFoo = foo;

    for (size_t i = 0; i < 64; i++)
    {
        for (size_t alignmt = 1; alignmt <= 512; alignmt = alignmt << 1u)
        {
            size_t       sz                       = RPS_COUNTOF(foo);
            void*        pRef                     = pFoo + i;
            const size_t rpsPaddingSizeResult     = rpsPaddingSize(pRef, alignmt);
            void*        rpsAlignUpConstPtrResult = rpsAlignUpPtr(pRef, alignmt);

            REQUIRE(rpsBytePtrInc(pRef, rpsPaddingSizeResult) == rpsAlignUpConstPtrResult);
            REQUIRE(rpsAlignUpConstPtrResult == std::align(alignmt, 1, pRef, sz));
        }
    }
}


TEST_CASE("VectorUtils")
{
    RpsAllocator allocatorCb = {
        &CountedMalloc,
        &CountedFree,
        &CountedRealloc,
        nullptr,
    };

    RPS_TEST_MALLOC_CHECKPOINT(0);

    do
    {
        rps::Vector<uint32_t, rps::GeneralAllocator<uint32_t>> u32Vec;

        REQUIRE(u32Vec.size() == 0);
        REQUIRE(u32Vec.empty());
        REQUIRE(u32Vec.capacity() == 0);

        rps::GeneralAllocator<uint32_t> allocator(&allocatorCb);
        u32Vec.reset(allocator);

        REQUIRE(u32Vec.size() == 0);
        REQUIRE(u32Vec.empty());
        REQUIRE(u32Vec.capacity() == 0);

        u32Vec.reserve(5);
        REQUIRE(u32Vec.capacity() >= 5);

        RPS_TEST_MALLOC_CHECKPOINT(1);
        RPS_TEST_MALLOC_COUNTER_COMPARE(0, <, 1);

        u32Vec.resize(3);

        RPS_TEST_MALLOC_COUNTER_EQUAL_CURRENT(1);

        u32Vec.resize(6);
        REQUIRE(u32Vec.capacity() >= 6);

        for (uint32_t i = 0; i < 6; i++)
        {
            u32Vec[i] = 6 - i;
            u32Vec.push_back(i);
        }

        REQUIRE(u32Vec.size() == 12);

        for (uint32_t i = 0; i < 6; i++)
        {
            REQUIRE(u32Vec[i] == 6 - i);
            REQUIRE(u32Vec[6 + i] == i);
        }

        const size_t beforeClearCapcaity = u32Vec.capacity();

        u32Vec.clear();

        REQUIRE(u32Vec.size() == 0);

        RPS_TEST_MALLOC_CHECKPOINT(2);

        u32Vec.resize(beforeClearCapcaity);
        RPS_TEST_MALLOC_COUNTER_EQUAL_CURRENT(2);

        u32Vec.clear();
        u32Vec.shrink_to_fit();

        RPS_TEST_MALLOC_COUNTER_EQUAL_CURRENT(0);

        u32Vec.push_back(3);
        u32Vec.push_back(4);
        u32Vec.push_back(5);
        REQUIRE(u32Vec[0] == 3);
        REQUIRE(u32Vec[1] == 4);
        REQUIRE(u32Vec[2] == 5);
        REQUIRE(u32Vec.front() == 3);
        REQUIRE(u32Vec.back() == 5);
        u32Vec.pop_back();
        REQUIRE(u32Vec.back() == 4);
        u32Vec.insert(1, 6);
        REQUIRE(u32Vec[0] == 3);
        REQUIRE(u32Vec[1] == 6);
        REQUIRE(u32Vec[2] == 4);
        u32Vec.pop_front();
        REQUIRE(u32Vec[0] == 6);
        REQUIRE(u32Vec.back() == 4);
        REQUIRE(u32Vec.size() == 2);

        u32Vec.clear();
        u32Vec.shrink_to_fit();

        RPS_TEST_MALLOC_COUNTER_EQUAL_CURRENT(0);


    } while (false);

    static int fooCount;

    do
    {
        struct Foo
        {
            int value;

            Foo()
                : Foo(-1)
            {
            }

            Foo(int v)
                : value(v)
            {
                fooCount++;
            }

            ~Foo()
            {
                fooCount--;
            }

            Foo(const Foo& other)
                : Foo(other.value)
            {
            }

            Foo(Foo&& other)
                : Foo(other.value)
            {
                other.value = -43;
            }

            Foo& operator=(const Foo& other)
            {
                value = other.value;
                return *this;
            }

            Foo& operator=(Foo&& other)
            {
                value       = other.value;
                other.value = -42;

                return *this;
            }
        };

        rps::Vector<Foo> foos;
        rps::GeneralAllocator<Foo> allocator(&allocatorCb);

        foos.reset(allocator);

        foos.resize(3);
        REQUIRE(fooCount == 3);

        REQUIRE(foos[0].value == -1);
        REQUIRE(foos[1].value == -1);
        REQUIRE(foos[2].value == -1);

        foos.resize(2);
        REQUIRE(fooCount == 2);

        foos.resize(3, Foo(3));
        REQUIRE(fooCount == 3);
        REQUIRE(foos[0].value == -1);
        REQUIRE(foos[1].value == -1);
        REQUIRE(foos[2].value == 3);

        foos.insert(1, Foo(1));
        REQUIRE(fooCount == 4);
        REQUIRE(foos[0].value == -1);
        REQUIRE(foos[1].value == 1);
        REQUIRE(foos[2].value == -1);
        REQUIRE(foos[3].value == 3);

        foos.remove(2);
        REQUIRE(foos.size() == fooCount);
        REQUIRE(fooCount == 3);
        REQUIRE(foos[0].value == -1);
        REQUIRE(foos[1].value == 1);
        REQUIRE(foos[2].value == 3);

        foos.push_back(Foo(4));
        REQUIRE(foos.size() == fooCount);
        REQUIRE(foos[2].value == 3);
        REQUIRE(foos[3].value == 4);

        {
            Foo tmps[4] = {{5}, {6}, {7}, {8}};
            foos.insert(3, tmps, RPS_COUNTOF(tmps));
        }

        REQUIRE(foos.size() == fooCount);
        int values[] = {-1, 1, 3, 5, 6, 7, 8, 4};

        uint32_t i = 0;
        for (auto iter = foos.begin(); iter != foos.end(); ++iter)
        {
            REQUIRE(iter->value == values[i]);
            i++;
        }

        {
            Foo tmps[2] = {{9}, {10}};
            foos.insert(4, tmps, RPS_COUNTOF(tmps));
        }
        REQUIRE(foos.size() == fooCount);
        int values2[] = {-1, 1, 3, 5, 9, 10, 6, 7, 8, 4};

        i = 0;
        for (auto iter = foos.begin(); iter != foos.end(); ++iter)
        {
            REQUIRE(iter->value == values2[i]);
            i++;
        }

    } while (false);

    REQUIRE(fooCount == 0);

    RPS_TEST_MALLOC_COUNTER_EQUAL_CURRENT(0);
}

TEST_CASE("BitVector")
{
    RpsAllocator allocatorCb = {
        &CountedMalloc,
        &CountedFree,
        &CountedRealloc,
        nullptr,
    };

    RPS_TEST_MALLOC_CHECKPOINT(0);

    do
    {
        rps::BitVector<> bitVec{&allocatorCb};
        REQUIRE(bitVec.size() == 0);
        REQUIRE(rps::BitVector<>::ELEMENT_NUM_BITS == 64);

        const size_t size1 = 17; 
        bitVec.Resize(size1);
        REQUIRE(bitVec.size() == size1);

        REQUIRE(bitVec.GetVector().size() == rpsDivRoundUp(bitVec.size(), size_t(rps::BitVector<>::ELEMENT_NUM_BITS)));

        for (uint32_t i = 0; i < bitVec.size(); i++)
        {
            bitVec.SetBit(i, (i % 3) == 0);
        }

        for (uint32_t i = 0; i < bitVec.size(); i++)
        {
            REQUIRE(bitVec.GetBit(i) == ((i % 3) == 0));
        }

        bitVec.Resize(bitVec.size() + 55, true);
        const size_t size2 = bitVec.size();
        REQUIRE(size2 == size1 + 55);

        bitVec.Resize(bitVec.size() + 77, false);
        const size_t size3 = bitVec.size();
        REQUIRE(size3 == size2 + 77);

        for (size_t i = 0; i < size1; i++)
        {
            REQUIRE(bitVec.GetBit(i) == ((i % 3) == 0));
        }

        for (size_t i = size1; i < size2; i++)
        {
            REQUIRE(bitVec.GetBit(i) == true);
        }

        for (size_t i = size2; i < size3; i++)
        {
            REQUIRE(bitVec.GetBit(i) == false);
        }

        bitVec.Fill(10, size3 - 11, false);

        for (size_t i = 0; i < 10; i++)
        {
            REQUIRE(bitVec.GetBit(i) == ((i % 3) == 0));
        }

        for (size_t i = 10; i < size3 - 11; i++)
        {
            REQUIRE(bitVec.GetBit(i) == false);
        }

        bitVec.Fill(13, size3 - 14, true);

        for (size_t i = 13; i < size3 - 14; i++)
        {
            REQUIRE(bitVec.GetBit(i) == true);
        }

        for (size_t i = size3 - 14; i < bitVec.size(); i++)
        {
            REQUIRE(bitVec.GetBit(i) == false);
        }

        bitVec.Resize(133);
        bitVec.Fill(false);

        REQUIRE(true == bitVec.CompareRange(0, bitVec.size(), false));
        REQUIRE(true == bitVec.CompareAndSetRange(0, bitVec.size(), false, true));
        for (size_t i = 0; i < bitVec.size(); i++)
        {
            REQUIRE(bitVec.GetBit(i) == true);
        }

        REQUIRE(true == bitVec.CompareRange(0, bitVec.size(), true));
        REQUIRE(true == bitVec.CompareAndSetRange(0, bitVec.size(), true, false));
        for (size_t i = 0; i < bitVec.size(); i++)
        {
            REQUIRE(bitVec.GetBit(i) == false);
        }

        auto setAndCheckRange = [&](size_t begin, size_t end, bool refVal, bool setVal) {

            rps::BitVector<> copy{&allocatorCb};
            rps::BitVector<> copyForSetRange{&allocatorCb};

            bitVec.Clone(copy);
            bitVec.Clone(copyForSetRange);

            bool bExpectedEq = true;

            for (size_t i = 0; i < bitVec.size(); i++)
            {
                REQUIRE(bitVec.GetBit(i) == copy.GetBit(i));

                if ((i >= begin) && (i < end))
                {
                    bExpectedEq &= (bitVec.GetBit(i) == refVal);
                }
            }

            REQUIRE(bExpectedEq == bitVec.CompareRange(begin, end, refVal));
            REQUIRE(bExpectedEq == bitVec.CompareAndSetRange(begin, end, refVal, setVal));
            copyForSetRange.SetRange(begin, end, setVal);

            for (size_t i = 0; i < begin; i++)
            {
                REQUIRE(bitVec.GetBit(i) == copy.GetBit(i));
                REQUIRE(copyForSetRange.GetBit(i) == copy.GetBit(i));
            }

            for (size_t i = begin; i < end; i++)
            {
                REQUIRE(bitVec.GetBit(i) == setVal);
                REQUIRE(copyForSetRange.GetBit(i) == setVal);
            }

            for (size_t i = end; i < bitVec.size(); i++)
            {
                REQUIRE(bitVec.GetBit(i) == copy.GetBit(i));
                REQUIRE(copyForSetRange.GetBit(i) == copy.GetBit(i));
            }
        };

        setAndCheckRange(13, 27, false, true);
        setAndCheckRange(15, 20, true, false);
        setAndCheckRange(18, 130, false, true);
        setAndCheckRange(64, 128, true, false);
        setAndCheckRange(129, bitVec.size(), false, true);
        setAndCheckRange(19, 127, true, true);
        setAndCheckRange(33, 85, false, false);
        setAndCheckRange(22, 22, false, true);
        setAndCheckRange(64, 64, true, false);
        setAndCheckRange(14, 128, false, true);
        setAndCheckRange(64, bitVec.size(), true, false);

        for (uint32_t i = 0; i < 100; i++)
        {
            size_t begin = (size_t(rand()) % bitVec.size());
            size_t end = (size_t(rand()) % (bitVec.size() + 1));
            if (end < begin)
            {
                std::swap(begin, end);
            }
            bool refVal = !!(rand() & 1);
            bool setVal = !!(rand() & 1);

            setAndCheckRange(begin, end, refVal, setVal);
        }

    } while (false);

    RPS_TEST_MALLOC_COUNTER_EQUAL_CURRENT(0);
}

TEST_CASE("ArenaUtils")
{
    RpsAllocator allocator = {
        &CountedMalloc,
        &CountedFree,
        &CountedRealloc,
        nullptr,
    };

    RPS_TEST_MALLOC_CHECKPOINT(0);

    do
    {
        rps::Arena arena(allocator, 4096 - 32);

        void* pAllocated = arena.Alloc(1);
        REQUIRE(pAllocated != nullptr);

        for (uint32_t i = 1; i < 512; i++)
        {
            pAllocated = arena.Alloc(i);
            REQUIRE(pAllocated != nullptr);
        }

        for (uint32_t i = 1; i < 32; i++)
        {
            const size_t alignmt = size_t(1ull << (rand() % 8));
            pAllocated           = arena.AlignedAlloc(rand() % (1024 * 64 * 2), alignmt);
            REQUIRE(pAllocated != nullptr);
            REQUIRE(rpsIsPointerAlignedTo(pAllocated, alignmt));
        }

        RPS_TEST_MALLOC_CHECKPOINT(1);

        // Test realloc:
        // First alloc
        pAllocated = arena.Alloc(42);

        // Shrinking
        void* pReallocatedWithSmallerSize = arena.Realloc(pAllocated, 42, 36);
        REQUIRE(pAllocated == pReallocatedWithSmallerSize);

        // Extending within range of previous allocs
        void* pReallocatedWithLargerSize = arena.Realloc(pAllocated, 36, 40);
        REQUIRE(pAllocated == pReallocatedWithLargerSize);

        // Insert a new alloc
        REQUIRE(0 != arena.Alloc(3));

        // Shrinking
        void* pReallocatedNotLast = arena.Realloc(pAllocated, 40, 31);
        REQUIRE(pAllocated == pReallocatedNotLast);

        // Extending
        pReallocatedNotLast = arena.Realloc(pAllocated, 31, 48);
        REQUIRE(pAllocated != pReallocatedNotLast);

        arena.Reset();

        RPS_TEST_MALLOC_COUNTER_EQUAL_CURRENT(1);

        while (arena.HasFreeBlocks())
        {
            pAllocated = arena.Alloc(42);
            REQUIRE(pAllocated != nullptr);
            RPS_TEST_MALLOC_COUNTER_EQUAL_CURRENT(1);
        }
    } while (false);

    RPS_TEST_MALLOC_COUNTER_EQUAL_CURRENT(0);
}

TEST_CASE("CompoundAlloc")
{
    RpsAllocator allocator = {
        &CountedMalloc,
        &CountedFree,
        &CountedRealloc,
        nullptr,
    };

    RPS_TEST_MALLOC_CHECKPOINT(0);

    uint32_t* pUInt;

    void* pMemory = rps::AllocateCompound(allocator, &pUInt);

    REQUIRE(pMemory);
    REQUIRE(pMemory == pUInt);

    allocator.pfnFree(allocator.pContext, pMemory);

    RPS_TEST_MALLOC_COUNTER_EQUAL_CURRENT(0);

    rps::ArrayRef<uint32_t> arrUInts;
    rps::AllocInfo          field2Info;
    rps::AllocInfo          field3Info;
    uint16_t*               pField2;
    uint64_t*               pField3;

    field2Info.Append<uint16_t>();
    field3Info.Append<uint64_t>(23);

    pMemory = rps::AllocateCompound(allocator,
                                    &pUInt,
                                    rps::CompoundEntry(&arrUInts, 42),
                                    rps::CompoundEntry(&pField2, field2Info),
                                    rps::CompoundEntry(&pField3, field3Info));

    rps::AllocInfo checker;

    size_t field0Offset = checker.Append<uint32_t>();
    size_t field1Offset = checker.Append<uint32_t>(42);
    size_t field2Offset = checker.Append<uint16_t>();
    size_t field3Offset = checker.Append<uint64_t>(23);

    REQUIRE(pMemory);
    REQUIRE(pMemory == pUInt);

    REQUIRE(field0Offset == 0);

    REQUIRE(rpsBytePtrInc(pMemory, field0Offset) == pUInt);
    REQUIRE(rpsBytePtrInc(pMemory, field1Offset) == arrUInts.data());
    REQUIRE(rpsBytePtrInc(pMemory, field2Offset) == pField2);
    REQUIRE(rpsBytePtrInc(pMemory, field3Offset) == pField3);
}

TEST_CASE("Span")
{
    RpsAllocator allocator = {
        &CountedMalloc,
        &CountedFree,
        &CountedRealloc,
        nullptr,
    };

    RPS_TEST_MALLOC_CHECKPOINT(0);

    //rps::Vector<uint16_t> u16Vec(0, &allocator);
    //rps::SpanPool<uint16_t> MustNotCompile(u16Vec);

    rps::Vector<uint32_t>   u32Vec(0, &allocator);
    rps::SpanPool<uint32_t> spanPool(u32Vec);

    rps::Span<uint32_t> span;
    REQUIRE(span.size() == 0);

    const size_t initVecSize = u32Vec.size();

    for (uint32_t i = 0; i < 130; i++)
    {
        uint32_t oldOffset = span.GetBegin();
        spanPool.push_to_span(span, 42 + i);

        REQUIRE(span.size() == i + 1);
        REQUIRE(span.Get(u32Vec).back() == 42 + i);

        if (rpsIsPowerOfTwo(i))
        {
            REQUIRE(span.GetBegin() == oldOffset + rpsRoundUpToPowerOfTwo(i));
        }
        else
        {
            REQUIRE(span.GetBegin() == oldOffset);
        }
    }

    rps::Span<uint32_t> span1;

    const size_t sizeBeforeReuse = u32Vec.size();

    for (uint32_t i = 0; i < 128; i++)
    {
        uint32_t oldOffset = span1.GetBegin();

        spanPool.push_to_span(span1, 242 + i);

        REQUIRE(span1.size() == i + 1);
        REQUIRE(span1.Get(u32Vec).back() == 242 + i);
        REQUIRE(span1.GetBegin() < span.GetBegin());

        if (rpsIsPowerOfTwo(i))
        {
            REQUIRE(span1.GetBegin() == oldOffset + rpsRoundUpToPowerOfTwo(i));
        }
        else
        {
            REQUIRE(span1.GetBegin() == oldOffset);
        }

        REQUIRE(sizeBeforeReuse == u32Vec.size());
    }

    spanPool.push_to_span(span1, 999);
    REQUIRE(span1.GetBegin() > span.GetBegin());
    REQUIRE(sizeBeforeReuse < u32Vec.size());

    u32Vec.reset();

    RPS_TEST_MALLOC_COUNTER_EQUAL_CURRENT(0);
}

template <size_t T>
static void StrBuilderCheck(const rps::StrBuilder<T>& builder, const char *str)
{
    REQUIRE(builder.c_str() != nullptr);
    REQUIRE(builder.Length() == strlen(builder.c_str()));
    REQUIRE(strlen(str) == builder.Length());
    REQUIRE(strcmp(str, builder.c_str()) == 0);
}

TEST_CASE("StrBuilder") {

    auto builder = rps::StrBuilder<10>();

    StrBuilderCheck(builder, "");
    builder.Append("hello");
    StrBuilderCheck(builder, "hello");
    builder.Append("hi");
    StrBuilderCheck(builder, "hellohi");
    builder.Append("_test!");
    StrBuilderCheck(builder, "hellohi_t");
    builder.Append("_test!");
    StrBuilderCheck(builder, "hellohi_t");
    builder.PopBack(4);
    StrBuilderCheck(builder, "hello");
    builder.Append("_t");
    StrBuilderCheck(builder, "hello_t");
    builder.PopBack(2);
    StrBuilderCheck(builder, "hello");
    builder.Append("hi_test!");
    StrBuilderCheck(builder, "hellohi_t");

    builder.Reset();
    StrBuilderCheck(builder, "");

    builder = rps::StrBuilder<10>("ab");
    StrBuilderCheck(builder, "ab");
    builder.Append("3434343434343434");
    StrBuilderCheck(builder, "ab3434343");
    builder.Append("3434343434343434");
    StrBuilderCheck(builder, "ab3434343");

    auto builder2 = rps::StrBuilder<5>("abcdefgh");
    StrBuilderCheck(builder2, "abcd");
    builder2.Append("3434343434343434");
    StrBuilderCheck(builder2, "abcd");

    auto builder3 = rps::StrBuilder<5>("abc");
    StrBuilderCheck(builder3, "abc");
    builder3.Append("a");
    StrBuilderCheck(builder3, "abca");
    builder3.Append("a");
    StrBuilderCheck(builder3, "abca");

    auto builder4 = rps::StrBuilder<11>("abc%d");
    StrBuilderCheck(builder4, "abc%d");
    builder4.Append("abc%d");
    StrBuilderCheck(builder4, "abc%dabc%d");

    char testName[256];
    std::fill(testName, testName + 255, 'a');
    testName[255] = '\0';
    auto builder5 = rps::StrBuilder<>();
    builder5.Append(testName);
    StrBuilderCheck(builder5, testName);

    auto builder6 = rps::StrBuilder<>();
    builder6.AppendFormat("%d", 666);
    StrBuilderCheck(builder6, "666");

    auto builder7 = rps::StrBuilder<>("test");
    StrBuilderCheck(builder7, "test");
    builder7 += "test";
    StrBuilderCheck(builder7, "testtest");

    auto builder8 = rps::StrBuilder<>("a");
    builder8.Append("b").Append("c").Append("d");
    StrBuilderCheck(builder8, "abcd");

    rps::StrBuilder<> builder9("qwer");
    builder9 = builder8;
    StrBuilderCheck(builder8, "abcd");
    StrBuilderCheck(builder9, "abcd");

    const rps::StrBuilder<> builder10("qwer");
    builder9 = builder10;
    StrBuilderCheck(builder9, "qwer");
}

TEST_CASE("StrRef")
{
    using rps::StrRef;

    StrRef s;
    REQUIRE(!s);
    REQUIRE(s.empty());

    s = "asdf";

    REQUIRE(s.len == 4);

    char buf[6];

    memset(buf, 0xfe, sizeof(buf));
    REQUIRE(!s.ToCStr(buf, 3));
    REQUIRE(0 == strcmp(buf, "as"));

    memset(buf, 0xfe, sizeof(buf));
    REQUIRE(!s.ToCStr(buf, 4));
    REQUIRE(0 == strcmp(buf, "asd"));

    memset(buf, 0xfe, sizeof(buf));
    REQUIRE(s.ToCStr(buf, 5));
    REQUIRE(0 == strcmp(buf, "asdf"));

    memset(buf, 0xfe, sizeof(buf));
    REQUIRE(s.ToCStr(buf, 6));
    REQUIRE(0 == strcmp(buf, "asdf"));

    REQUIRE(StrRef(buf, 3) == StrRef("asd"));
    REQUIRE(StrRef(buf, 3) == StrRef("asdX", 3));
    REQUIRE(StrRef(buf, 3) != StrRef(buf, 2));
}
