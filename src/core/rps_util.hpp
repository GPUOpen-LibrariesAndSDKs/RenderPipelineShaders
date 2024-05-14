// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_UTIL_HPP
#define RPS_UTIL_HPP

#include "rps_core.hpp"

template <typename T>
static constexpr T rpsMin(const T lhs, const T rhs)
{
    return (lhs < rhs) ? lhs : rhs;
}

template <typename T>
static constexpr T rpsMax(const T lhs, const T rhs)
{
    return (lhs < rhs) ? rhs : lhs;
}

template <typename T>
static constexpr T rpsClamp(const T val, const T minVal, const T maxVal)
{
    return rpsMin(rpsMax(val, minVal), maxVal);
}

static inline uint32_t rpsCountBits(uint32_t value)
{
#ifdef RPS_HAS_POPCNT
    return __popcnt(value);
#elif RPS_HAS_BUILTIN_POP_COUNT
    return (uint32_t)__builtin_popcount(value);
#else
    uint32_t a = (value & 0x55555555u) + ((value >> 1u) & 0x55555555u);
    uint32_t b = (a & 0x33333333u) + ((a >> 2u) & 0x33333333u);
    uint32_t c = (b & 0x0F0F0F0Fu) + ((b >> 4u) & 0x0F0F0F0Fu);
    uint32_t d = (c & 0x00FF00FFu) + ((c >> 8u) & 0x00FF00FFu);
    uint32_t e = (d & 0xFFFFu) + (d >> 16u);
    return e;
#endif
}

static inline uint32_t rpsFirstBitHigh(uint32_t value)
{
#ifdef RPS_HAS_BITSCAN
    unsigned long idx;
    (void)_BitScanReverse(&idx, value);
    return value ? (31 - idx) : 32;
#elif defined(RPS_HAS_BUILTIN_CLZ_CTZ)
    return value ? __builtin_clz(value) : 32;
#else
    for (uint32_t i = 0; i < 32; i++)
    {
        if (value & 0x80000000u)
            return i;
        value = value << 1u;
    }
    return 32;
#endif
}

static inline uint32_t rpsFirstBitHigh(uint64_t value)
{
#ifdef RPS_HAS_BITSCAN
    unsigned long idx;
    (void)_BitScanReverse64(&idx, value);
    return value ? (63 - idx) : 64;
#elif defined(RPS_HAS_BUILTIN_CLZ_CTZ)
    return value ? __builtin_clzll(value) : 64;
#else
    for (uint32_t i = 0; i < 64; i++)
    {
        if (value & 0x8000'0000'0000'0000u)
            return i;
        value = value << 1u;
    }
    return 64;
#endif
}

static inline uint32_t rpsFirstBitLow(uint32_t value)
{
#ifdef RPS_HAS_BITSCAN
    unsigned long idx;
    (void)_BitScanForward(&idx, value);
    return value ? idx : 32;
#elif defined(RPS_HAS_BUILTIN_CLZ_CTZ)
    return value ? __builtin_ctz(value) : 32;
#else
    for (uint32_t i = 0; i < 32; i++)
    {
        if (value & 1u)
            return i;
        value = value >> 1u;
    }
    return 32;
#endif
}

static inline uint32_t rpsFirstBitLow(uint64_t value)
{
#ifdef RPS_HAS_BITSCAN
    unsigned long idx;
    (void)_BitScanForward64(&idx, value);
    return value ? idx : 64;
#elif defined(RPS_HAS_BUILTIN_CLZ_CTZ)
    return value ? __builtin_ctzll(value) : 64;
#else
    for (uint32_t i = 0; i < 64; i++)
    {
        if (value & 1ull)
            return i;
        value = value >> 1u;
    }
    return 64;
#endif
}

static constexpr uint32_t rpsReverseBits32(uint32_t v)
{
    v = ((v >> 1) & 0x55555555u) | ((v & 0x55555555u) << 1);
    v = ((v >> 2) & 0x33333333u) | ((v & 0x33333333u) << 2);
    v = ((v >> 4) & 0x0f0f0f0fu) | ((v & 0x0f0f0f0fu) << 4);
    v = ((v >> 8) & 0x00ff00ffu) | ((v & 0x00ff00ffu) << 8);
    return ((v >> 16) & 0xffffu) | ((v & 0xffffu) << 16);
}

static constexpr uint32_t rpsRoundUpToPowerOfTwo(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v;
}

static constexpr uint64_t rpsRoundUpToPowerOfTwo(uint64_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v++;

    return v;
}

template <typename T,
          typename = typename std::enable_if<std::is_integral<T>::value && std::is_unsigned<T>::value>::type>
static constexpr bool rpsIsPowerOfTwo(T v)
{
    return (v & (v - 1u)) == 0;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
static constexpr T rpsDivRoundUp(T dividend, T divisor)
{
    return (dividend + divisor - 1) / divisor;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
static constexpr T rpsAlignUp(T offset, T alignment)
{
    return alignment ? (offset + (alignment - T(1))) & ~(alignment - T(1)) : offset;
}

static inline void* rpsAlignUpPtr(void* ptr, size_t alignment)
{
    return alignment ? reinterpret_cast<void*>(((uintptr_t)ptr + (alignment - (size_t)1)) & ~(alignment - (size_t)1))
                     : ptr;
}

static inline const void* rpsAlignUpConstPtr(const void* ptr, size_t alignment)
{
    return alignment ? reinterpret_cast<void*>(((uintptr_t)ptr + (alignment - (size_t)1)) & ~(alignment - (size_t)1))
                     : ptr;
}

static inline size_t rpsPaddingSize(void* ptr, size_t alignment)
{
    if (alignment == 0)
        return 0;

    const uintptr_t mask = uintptr_t(alignment - size_t(1));
    return size_t((alignment - (uintptr_t(ptr) & mask)) & mask);
}

static constexpr RpsBool rpsIsPointerAlignedTo(const void* ptr, size_t alignment)
{
    return (alignment == 0) || (0 == (((uintptr_t)ptr) & ((uintptr_t)alignment - 1)));
}

static constexpr uint32_t rpsDataSize(const size_t elementSize, const uint32_t elementCount)
{
    return rpsAlignUp((uint32_t)elementSize * elementCount, 8u);
}

static constexpr void* rpsBytePtrInc(void* ptr, size_t size)
{
    return (uint8_t*)ptr + size;
}

static constexpr const void* rpsBytePtrInc(const void* ptr, size_t size)
{
    return (const uint8_t*)ptr + size;
}

template <typename T>
static constexpr T* rpsBytePtrInc(void* ptr, size_t size)
{
    return static_cast<T*>(rpsBytePtrInc(ptr, size));
}

template <typename T,
          typename U,
          typename = typename std::enable_if<(std::is_integral<T>::value || std::is_enum<T>::value) &&
                                             (std::is_integral<U>::value || std::is_enum<U>::value)>::type>
constexpr bool rpsAllBitsSet(T value, U bits)
{
    return (((value) & static_cast<T>(bits)) == static_cast<T>(bits));
}

template <typename T,
          typename U,
          typename = typename std::enable_if<(std::is_integral<T>::value || std::is_enum<T>::value) &&
                                             (std::is_integral<U>::value || std::is_enum<U>::value)>::type>
constexpr bool rpsAnyBitsSet(T value, U bits)
{
    return ((value) & static_cast<T>(bits)) != T(0);
}

namespace rps
{

#if RPS_DEBUG && !RPS_DISABLE_MEMORY_DEBUG_FILL
#define RPS_DEBUG_FILL_MEMORY_ON_ALLOC(Ptr, Size)      ((void)((Ptr) ? memset((Ptr), 0xBE, (Size)) : nullptr))
#define RPS_DEBUG_FILL_MEMORY_ON_POOL_ALLOC(Ptr, Size) ((void)((Ptr) ? memset((Ptr), 0xB0, (Size)) : nullptr))
#define RPS_DEBUG_FILL_MEMORY_ON_POOL_FREE(Ptr, Size)  ((void)((Ptr) ? memset((Ptr), 0xF0, (Size)) : nullptr))
#define RPS_DEBUG_FILL_MEMORY_ON_FREE(Ptr, Size)       ((void)((Ptr) ? memset((Ptr), 0xFE, (Size)) : nullptr))
#else  //RPS_DEBUG
#define RPS_DEBUG_FILL_MEMORY_ON_ALLOC(Ptr, Size)
#define RPS_DEBUG_FILL_MEMORY_ON_POOL_ALLOC(Ptr, Size)
#define RPS_DEBUG_FILL_MEMORY_ON_POOL_FREE(Ptr, Size)
#define RPS_DEBUG_FILL_MEMORY_ON_FREE(Ptr, Size)
#endif  //RPS_DEBUG

    struct AllocInfo : public RpsAllocInfo
    {
    public:
        AllocInfo()
            : AllocInfo(0, 0)
        {
        }

        AllocInfo(size_t size_, size_t alignment_)
            : RpsAllocInfo{size_, alignment_}
        {
        }

        AllocInfo(const RpsAllocInfo& allocInfo)
            : RpsAllocInfo{allocInfo}
        {
        }

        size_t Append(const AllocInfo& allocInfo)
        {
            return Append(allocInfo.size, allocInfo.alignment);
        }

        size_t Append(size_t size_, size_t alignment_)
        {
            const size_t offset = rpsAlignUp(size, alignment_);
            size                = offset + size_;
            alignment           = rpsMax(alignment, alignment_);

            return offset;
        }

        size_t Append(size_t elementSize, size_t numElements, size_t alignment_)
        {
            return Append(elementSize * numElements, alignment_);
        }

        template <typename T>
        size_t Append(size_t count = 1)
        {
            return Append(sizeof(T) * count, alignof(T));
        }

        template <typename T>
        static AllocInfo FromType(size_t count = 1)
        {
            return AllocInfo(sizeof(T) * count, alignof(T));
        }
    };

    template <typename T, typename SizeType = size_t>
    class ArrayRef
    {
    public:
        typedef T        value_type;
        typedef T*       iterator;
        typedef const T* const_iterator;

        typedef std::reverse_iterator<iterator>       reverse_iterator;
        typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

        constexpr ArrayRef() = default;

        ArrayRef(T* pData, SizeType size)
            : m_pData(pData)
            , m_Size(size)
        {
        }

        template <size_t N>
        ArrayRef(T (&arr)[N])
            : m_pData(arr)
            , m_Size(N)
        {
        }

        template <typename U = typename std::remove_const<T>::type>
        ArrayRef(std::initializer_list<U> initList)
            : ArrayRef(initList.begin(), SizeType(initList.size()))
        {
        }

        ArrayRef(const ArrayRef& other)            = default;
        ArrayRef& operator=(const ArrayRef& other) = default;

        template <typename OtherSizeType>
        operator ArrayRef<T, OtherSizeType>() const
        {
            return ArrayRef<T, OtherSizeType>{
                data(),
                static_cast<OtherSizeType>(size()),
            };
        }

        template <
            typename U,
            typename = typename std::enable_if<std::is_assignable<T*&, U*>::value && !std::is_same<T, U>::value>::type>
        ArrayRef(const ArrayRef<U, SizeType>& otherNonConst)
            : m_pData(otherNonConst.data())
            , m_Size(otherNonConst.size())
        {
        }

        template <
            typename U,
            typename = typename std::enable_if<std::is_assignable<T*&, U*>::value && !std::is_same<T, U>::value>::type>
        ArrayRef& operator=(const ArrayRef<U>& otherNonConst)
        {
            Set(otherNonConst.data(), otherNonConst.size());
            return *this;
        }

        void Set(T* pData, SizeType size)
        {
            m_pData = pData;
            m_Size  = size;
        }

        template <SizeType N>
        void Set(T (&pArray)[N])
        {
            Set(pArray, N);
        }

        T& operator[](SizeType index)
        {
            RPS_ASSERT(index < size());
            return m_pData[index];
        }

        const T& operator[](SizeType index) const
        {
            RPS_ASSERT(index < size());
            return m_pData[index];
        }

        void clear()
        {
            Set(nullptr, 0);
        }

        bool empty() const
        {
            return size() == 0;
        }

        T* data() const
        {
            return m_pData;
        }

        SizeType size() const
        {
            return m_Size;
        }

        iterator begin() const
        {
            return data();
        }

        iterator end() const
        {
            return data() + size();
        }

        const_iterator cbegin() const
        {
            return data();
        }

        const_iterator cend() const
        {
            return data() + size();
        }

        reverse_iterator rbegin() const
        {
            return reverse_iterator(end());
        }

        reverse_iterator rend() const
        {
            return reverse_iterator(begin());
        }

        const_reverse_iterator crbegin() const
        {
            return const_reverse_iterator(cend());
        }

        const_reverse_iterator crend() const
        {
            return const_reverse_iterator(cbegin());
        }

        const T& front() const
        {
            return *begin();
        }

        const T& back() const
        {
            return *rbegin();
        }

        T& front()
        {
            return *begin();
        }

        T& back()
        {
            return *rbegin();
        }

        ArrayRef<T, SizeType> range(SizeType offset, SizeType count)
        {
            RPS_ASSERT(offset + count <= size());
            return {data() + offset, count};
        }

        ArrayRef<T, SizeType> range(SizeType offset)
        {
            RPS_ASSERT(offset <= size());
            return {data() + offset, size() - rpsMin(size(), offset)};
        }

        ArrayRef<const T, SizeType> range(SizeType offset, SizeType count) const
        {
            RPS_ASSERT(offset + count <= size());
            return {data() + offset, count};
        }

        ArrayRef<const T, SizeType> range(SizeType offset) const
        {
            RPS_ASSERT(offset <= size());
            return {data() + offset, size() - rpsMin(size(), offset)};
        }

    private:
        T*       m_pData = nullptr;
        SizeType m_Size  = 0;
    };

    template <typename T, typename SizeType = size_t>
    using ConstArrayRef = ArrayRef<const T, SizeType>;

    template <typename T>
    class GeneralAllocator;

    template <typename T, typename AllocatorT = GeneralAllocator<T>>
    class Vector
    {
        RPS_CLASS_NO_COPY(Vector);

    public:
        typedef T        value_type;
        typedef T*       iterator;
        typedef const T* const_iterator;

        typedef std::reverse_iterator<iterator>       reverse_iterator;
        typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

        static constexpr bool ElementTrivialCopyable     = std::is_trivially_copyable<T>::value;
        static constexpr bool ElementTrivialDestructible = std::is_trivially_destructible<T>::value;

        Vector() = default;

        Vector(size_t count, const AllocatorT& allocator)
            : m_Allocator(allocator)
            , m_Count(count)
            , m_Capacity(count)
        {
            m_pArray = AllocNoConstruct(count);
            ConstructElements(m_pArray, count);
        }

        Vector(const AllocatorT& allocator)
            : Vector(0, allocator)
        {
        }

        Vector(size_t count, const T& value, const AllocatorT& allocator)
            : Vector(count, allocator)
        {
        }

        Vector(ConstArrayRef<T> values, const AllocatorT& allocator)
            : Vector(values.size(), allocator)
        {
            if (!values.empty() && data())
            {
                std::copy(values.begin(), values.end(), data());
            }
        }

    protected:
        Vector(size_t count, size_t capacity, T* pInitialInplaceData, const AllocatorT& allocator)
            : m_Allocator(allocator)
            , m_Count(count)
            , m_Capacity(rpsMax(count, capacity))
        {
            m_pArray = (count > capacity) ? AllocNoConstruct(count) : pInitialInplaceData;
            ConstructElements(m_pArray, count);
        }

    public:
        ~Vector()
        {
            CleanUp();
        }

        Vector(Vector&& other) = default;

        Vector& operator=(Vector&& other) = default;

        Vector Clone() const
        {
            return Vector(range_all(), *m_Allocator);
        }

        template <typename... TArgs>
        void emplace_back(TArgs&&... args)
        {
            ResizeNoConstruct(m_Count + 1);
            ConstructElements(&m_pArray[m_Count - 1], 1, std::forward<TArgs>(args)...);
        }

        bool empty() const
        {
            return m_Count == 0;
        }

        size_t size() const
        {
            return m_Count;
        }

        size_t capacity() const
        {
            return m_Capacity;
        }

        T* data()
        {
            return m_pArray;
        }

        T& front()
        {
            RPS_ASSERT(m_Count > 0);
            return m_pArray[0];
        }

        T& back()
        {
            RPS_ASSERT(m_Count > 0);
            return m_pArray[m_Count - 1];
        }

        const T* data() const
        {
            return m_pArray;
        }

        const T& front() const
        {
            RPS_ASSERT(m_Count > 0);
            return m_pArray[0];
        }

        const T& back() const
        {
            RPS_ASSERT(m_Count > 0);
            return m_pArray[m_Count - 1];
        }

        iterator begin()
        {
            return data();
        }
        iterator end()
        {
            return data() + size();
        }
        const_iterator cbegin() const
        {
            return data();
        }
        const_iterator cend() const
        {
            return data() + size();
        }
        const_iterator begin() const
        {
            return cbegin();
        }
        const_iterator end() const
        {
            return cend();
        }

        reverse_iterator rbegin()
        {
            return reverse_iterator(end());
        }

        reverse_iterator rend()
        {
            return reverse_iterator(begin());
        }

        const_reverse_iterator rbegin() const
        {
            return const_reverse_iterator(end());
        }

        const_reverse_iterator rend() const
        {
            return const_reverse_iterator(begin());
        }

        const_reverse_iterator crbegin() const
        {
            return const_reverse_iterator(cend());
        }

        const_reverse_iterator crend() const
        {
            return const_reverse_iterator(cbegin());
        }

        ArrayRef<T> range(size_t startIndex, size_t count)
        {
            return ArrayRef<T>(data() + startIndex, count);
        }

        ConstArrayRef<T> range(size_t startIndex, size_t count) const
        {
            return ConstArrayRef<T>(data() + startIndex, count);
        }

        ArrayRef<T> range_all()
        {
            return ArrayRef<T>(data(), size());
        }

        ConstArrayRef<T> range_all() const
        {
            return crange_all();
        }

        ConstArrayRef<T> crange_all() const
        {
            return ConstArrayRef<T>(data(), size());
        }

        void pop_front()
        {
            RPS_ASSERT(m_Count > 0);
            remove(0);
        }
        void pop_back()
        {
            RPS_ASSERT(m_Count > 0);
            resize(size() - 1);
        }
        void push_front(const T& src)
        {
            insert(0, src);
        }
        void push_front(T&& src)
        {
            insert(0, std::forward<T>(src));
        }

        bool push_back(const T& src)
        {
            const size_t newIndex = size();
            if (!ResizeNoConstruct(newIndex + 1))
                return false;

            ConstructElements(&m_pArray[newIndex], 1, src);
            return true;
        }

        bool push_back(T&& src)
        {
            const size_t newIndex = size();
            if (!ResizeNoConstruct(newIndex + 1))
                return false;

            ConstructElements(&m_pArray[newIndex], 1, std::move(src));
            return true;
        }

        bool reserve(size_t newCapacity)
        {
            if (newCapacity > m_Capacity)
            {
                T* const newArray = newCapacity ? m_Allocator.allocate(newCapacity) : nullptr;

                if (newCapacity && !newArray)
                {
                    RPS_DIAG(RPS_DIAG_ERROR, "Allocation failed");
                    return false;
                }

                if (m_Count != 0)
                {
                    std::copy(m_pArray, m_pArray + m_Count, newArray);
                }
                const size_t prevCount = m_Count;
                CleanUp();
                Init(newArray, newCapacity, prevCount);
            }

            return true;
        }

        bool reserve_additional(size_t sizeIncrease)
        {
            return reserve(size() + sizeIncrease);
        }

        T* grow(size_t growCount)
        {
            if (resize(m_Count + growCount))
            {
                return &m_pArray[m_Count - growCount];
            }
            return nullptr;
        }

        T* grow(size_t growCount, const T& fill)
        {
            size_t oldCount = m_Count;
            if (!ResizeNoConstruct(oldCount + growCount))
            {
                return nullptr;
            }

            ConstructElements(m_pArray + oldCount, growCount, fill);

            return m_pArray + oldCount;
        }

        bool resize(size_t newCount, const T& fill)
        {
            size_t oldCount = size();
            bool   result   = ResizeNoConstruct(newCount);

            if (result && (newCount > oldCount))
            {
                ConstructElements(m_pArray + oldCount, newCount - oldCount, fill);
            }

            return result;
        }

        bool resize(size_t newCount)
        {
            size_t oldCount = m_Count;

            if (!ResizeNoConstruct(newCount))
                return false;

            if (oldCount < newCount)
            {
                ConstructElements(m_pArray + oldCount, newCount - oldCount);
            }

            return true;
        }

        void clear()
        {
            resize(0);
        }

        void reset()
        {
            clear();
            CleanUp();
        }

        void reset(const AllocatorT& newAllocator)
        {
            clear();
            CleanUp();

            m_Allocator = newAllocator;
        }

        bool reset_keep_capacity(const AllocatorT& newAllocator)
        {
            const auto oldCapacity = capacity();
            reset(newAllocator);
            return reserve(oldCapacity);
        }

        bool shrink_to_fit()
        {
            if (m_Capacity > m_Count)
            {
                T* newArray = nullptr;
                if (m_Count > 0)
                {
                    newArray = m_Allocator.allocate(m_Count);

                    if (!newArray)
                    {
                        RPS_DIAG(RPS_DIAG_ERROR, "Allocation failed");
                        return false;
                    }

                    if (newArray)
                    {
                        std::copy(m_pArray, m_pArray + m_Count, newArray);
                    }
                }
                const size_t currCount = m_Count;
                CleanUp();
                Init(newArray, currCount, currCount);
            }

            return true;
        }

        bool insert(size_t index, const T& src)
        {
            RPS_ASSERT(index <= m_Count);
            const size_t oldCount = size();
            if (!ResizeNoConstruct(oldCount + 1))
                return false;

            if (index < oldCount)
            {
                // Copy construct the last element
                ConstructElements(m_pArray + oldCount, 1, m_pArray[oldCount - 1]);
                // Move the rest by 1
                std::move_backward(m_pArray + index, m_pArray + oldCount - 1, m_pArray + oldCount);
                m_pArray[index] = src;
            }
            else
            {
                ConstructElements(m_pArray + oldCount, 1, src);
            }

            return true;
        }

        bool insert(size_t index, T&& src)
        {
            RPS_ASSERT(index <= m_Count);
            const size_t oldCount = size();
            if (!ResizeNoConstruct(oldCount + 1))
                return false;

            if (index < oldCount)
            {
                // Move construct the last element
                ConstructElements(m_pArray + oldCount, 1, std::forward<T>(m_pArray[oldCount - 1]));
                // Move the rest by 1
                std::move_backward(m_pArray + index, m_pArray + oldCount - 1, m_pArray + oldCount);
                m_pArray[index] = std::move(src);
            }
            else
            {
                ConstructElements(m_pArray + oldCount, 1, std::forward<T>(src));
            }

            return true;
        }

        bool insert(size_t index, const T* pSrcs, size_t numSrcs)
        {
            RPS_ASSERT(index <= m_Count);
            const size_t oldCount = size();
            if (!ResizeNoConstruct(oldCount + numSrcs))
                return false;

            if (index < oldCount)
            {
                const size_t constructStart     = rpsMax(oldCount, index + numSrcs);
                const size_t numToMoveConstruct = oldCount + numSrcs - constructStart;

                // Construct tail new elements
                for (auto iDst = rbegin(), dstEnd = rbegin() + numToMoveConstruct, iSrc = rbegin() + numSrcs;
                     iDst != dstEnd;
                     ++iDst, ++iSrc)
                {
                    ConstructElements(&*iDst, 1, *iSrc);
                }

                std::move_backward(begin() + index, begin() + constructStart - numSrcs, begin() + constructStart);
            }

            if (pSrcs)
            {
                const size_t numCopy = rpsMin(oldCount - index, numSrcs);

                std::copy(pSrcs, pSrcs + numCopy, begin() + index);

                auto iSrc = pSrcs + numCopy;
                for (auto iDst = begin() + index + numCopy, dstEnd = begin() + index + numSrcs; iDst != dstEnd;
                     ++iDst, ++iSrc)
                {
                    ConstructElements(iDst, 1, *iSrc);
                }
            }

            return true;
        }

        T* get_or_grow(size_t index)
        {
            if ((index >= size()) && !resize(index + 1))
            {
                return nullptr;
            }

            return data() + index;
        }

        T* get_or_grow(size_t index, const T& fill)
        {
            if ((index >= size()) && !resize(index + 1, fill))
            {
                return nullptr;
            }

            return data() + index;
        }

        void remove(size_t index)
        {
            RPS_ASSERT(index < m_Count);
            const size_t oldCount = size();
            if (index + 1 < oldCount)
            {
                std::move(m_pArray + (index + 1), m_pArray + oldCount, m_pArray + index);
            }
            resize(oldCount - 1);
        }

        void remove_unordered(size_t index)
        {
            RPS_ASSERT(index < m_Count);
            const size_t oldCount = size();
            if (index < oldCount - 1)
            {
                std::swap(m_pArray[index], m_pArray[oldCount - 1]);
            }
            resize(oldCount - 1);
        }

        T& operator[](size_t index)
        {
            RPS_ASSERT(index < m_Count);
            return m_pArray[index];
        }

        const T& operator[](size_t index) const
        {
            RPS_ASSERT(index < m_Count);
            return m_pArray[index];
        }

    private:
        void Init(T* pArray, size_t capacity, size_t count)
        {
            m_pArray   = pArray;
            m_Count    = count;
            m_Capacity = capacity;
        }

        void CleanUp()
        {
            if (m_pArray)
            {
                DestructElements(m_pArray, m_Count);

                m_Allocator.deallocate(m_pArray, m_Capacity);
            }
            m_pArray   = nullptr;
            m_Count    = 0;
            m_Capacity = 0;
        }

        bool ResizeNoConstruct(size_t newCount)
        {
            if (newCount < m_Count)
            {
                DestructElements(m_pArray + newCount, m_Count - newCount);
            }

            size_t newCapacity = m_Capacity;
            if (newCount > m_Capacity)
            {
                newCapacity = rpsMax(newCount, rpsMax(m_Capacity * 3 / 2, (size_t)8));
            }

            if (newCapacity != m_Capacity)
            {
                T* const newArray = newCapacity ? m_Allocator.allocate(newCapacity) : nullptr;

                if (newCapacity && !newArray)
                {
                    RPS_DIAG(RPS_DIAG_ERROR, "Allocation failed");
                    return false;
                }

                const size_t elementsToCopy = rpsMin(m_Count, newCount);
                if (elementsToCopy != 0)
                {
                    auto iSrc = m_pArray;
                    for (auto iDst = newArray, dstEnd = newArray + elementsToCopy; iDst != dstEnd; ++iDst, ++iSrc)
                    {
                        ConstructElements(iDst, 1, std::forward<T>(*iSrc));
                    }
                }

                CleanUp();
                Init(newArray, newCapacity, newCount);
            }

            m_Count = newCount;
            return true;
        }

        T* AllocNoConstruct(size_t count)
        {
            return count ? m_Allocator.allocate(count) : nullptr;
        }

        template <typename... Args>
        void ConstructElements(T* pElements, size_t count, Args&&... args)
        {
            std::for_each(pElements, pElements + count, [&](auto& elem) {
                std::allocator_traits<AllocatorT>::construct(m_Allocator, &elem, std::forward<Args>(args)...);
            });
        }

        void DestructElements(T* pElements, size_t count)
        {
            if (!ElementTrivialDestructible)
            {
                std::for_each(pElements, pElements + count, [&](auto& elem) {
                    std::allocator_traits<AllocatorT>::destroy(m_Allocator, &elem);
                });
            }
        }

        void CopyElements(T* pDst, const T* pSrc, size_t count)
        {
            std::copy(pSrc, pSrc + count, pDst);
        }

    private:
        AllocatorT m_Allocator = {};
        T*         m_pArray    = nullptr;
        size_t     m_Count     = 0;
        size_t     m_Capacity  = 0;
    };

    // Bit Vector
    template <typename TElement = uint64_t, typename TAllocator = GeneralAllocator<TElement>>
    class BitVector
    {
    public:
        static constexpr uint32_t ELEMENT_NUM_BITS = sizeof(TElement) * (CHAR_BIT);

    public:
        struct BitIndex
        {
            uint32_t element;
            uint32_t bit;
        };

        BitVector()
        {
        }

        BitVector(const TAllocator& allocator)
            : m_bitVector(allocator)
        {
        }

        void reset(const TAllocator& allocator)
        {
            m_bitVector.reset(allocator);
        }

        const Vector<TElement, TAllocator>& GetVector() const
        {
            return m_bitVector;
        }

        size_t size() const
        {
            return m_BitSize;
        }

        bool Resize(size_t numBits)
        {
            if (m_bitVector.resize((numBits + ELEMENT_NUM_BITS - 1) / ELEMENT_NUM_BITS))
            {
                m_BitSize = numBits;
                return true;
            }

            return false;
        }

        bool Resize(size_t numBits, bool bSet)
        {
            const size_t oldBits = m_BitSize;

            if (!Resize(numBits))
                return false;

            Fill(oldBits, numBits, bSet);

            return true;
        }

        void Fill(bool bSet)
        {
            std::fill(m_bitVector.begin(), m_bitVector.end(), bSet ? ~TElement(0) : 0);
        }

        void Fill(size_t beginBitIndex, size_t endBitIndex, bool bSet)
        {
            const size_t fullElemBegin = rpsDivRoundUp(beginBitIndex, size_t(ELEMENT_NUM_BITS));
            const size_t fullElemEnd   = endBitIndex / ELEMENT_NUM_BITS;

            const size_t beginSubElemBits = (beginBitIndex % ELEMENT_NUM_BITS);
            if (beginSubElemBits != 0)
            {
                const TElement keepMask  = (TElement(1) << beginSubElemBits) - 1;
                TElement&      beginElem = m_bitVector[fullElemBegin - 1];
                beginElem                = bSet ? (beginElem | ~keepMask) : (beginElem & keepMask);
            }

            std::fill(m_bitVector.begin() + fullElemBegin, m_bitVector.begin() + fullElemEnd, bSet ? ~TElement(0) : 0);

            const size_t endSubElemBits = endBitIndex % ELEMENT_NUM_BITS;
            if (endSubElemBits != 0)
            {
                const TElement fillMask = (TElement(1) << endSubElemBits) - 1;
                TElement&      endElem  = m_bitVector[fullElemEnd];
                endElem                 = bSet ? (endElem | fillMask) : (endElem & ~fillMask);
            }
        }

        constexpr bool GetBit(size_t index) const
        {
            RPS_ASSERT(index < m_BitSize);

            const size_t elementIdx = index / ELEMENT_NUM_BITS;
            const size_t bitIndex   = index % ELEMENT_NUM_BITS;

            return !!(m_bitVector[elementIdx] & (TElement(1) << bitIndex));
        }

        constexpr bool ExchangeBit(size_t index, bool newValue)
        {
            RPS_ASSERT(index < m_BitSize);

            const size_t elementIdx = index / ELEMENT_NUM_BITS;
            const size_t bitIndex   = index % ELEMENT_NUM_BITS;

            TElement& dstElement = m_bitVector[elementIdx];
            TElement  mask       = (TElement(1) << bitIndex);

            bool oldValue = !!(dstElement & mask);
            dstElement    = newValue ? (dstElement | mask) : (dstElement & ~mask);

            return oldValue;
        }

        // Set the [beginIndex, endIndex) bit range to newValue.
        // Returns if original bits in the range were all equal to the comparand.
        bool CompareAndSetRange(size_t beginIndex, size_t endIndex, bool comparand, bool newValue)
        {
            bool bResult = true;

            IterateRange(
                *this, beginIndex, endIndex, [&bResult, comparand, newValue](TElement& dstElement, TElement mask) {
                    bResult &= ((dstElement & mask) == (comparand ? mask : 0));
                    dstElement = newValue ? (dstElement | mask) : (dstElement & ~mask);
                });

            return bResult;
        }

        bool CompareRange(size_t beginIndex, size_t endIndex, bool comparand) const
        {
            bool bResult = true;

            IterateRange(*this, beginIndex, endIndex, [&bResult, comparand](const TElement& dstElement, TElement mask) {
                bResult &= ((dstElement & mask) == (comparand ? mask : 0));
            });

            return bResult;
        }

        void SetRange(size_t beginIndex, size_t endIndex, bool newValue)
        {
            IterateRange(*this, beginIndex, endIndex, [newValue](TElement& dstElement, TElement mask) {
                dstElement = newValue ? (dstElement | mask) : (dstElement & ~mask);
            });
        }

        void SetBit(size_t index, bool value)
        {
            RPS_ASSERT(index < m_BitSize);

            const size_t elementIdx = index / ELEMENT_NUM_BITS;
            const size_t bitIndex   = index % ELEMENT_NUM_BITS;

            TElement& dstElement = m_bitVector[elementIdx];
            TElement  mask       = (TElement(1) << bitIndex);
            dstElement           = value ? (dstElement | mask) : (dstElement & ~mask);
        }

        void SetBit(BitIndex index, bool value)
        {
            RPS_ASSERT(index < m_BitSize);

            TElement& dstElement = m_bitVector[index.element];
            TElement  mask       = (TElement(1) << index.bit);
            dstElement           = value ? (dstElement | mask) : (dstElement & ~mask);
        }

        BitIndex FindFirstBitLow(size_t startElement) const
        {
            for (size_t i = startElement, end = m_bitVector.size(); i < end; i++)
            {
                if (m_bitVector[i])
                {
                    return BitIndex{i, rpsFirstBitLow(m_bitVector[i])};
                }
            }

            return BitIndex{RPS_INDEX_NONE_U32, RPS_INDEX_NONE_U32};
        }

        void Clone(BitVector<TElement>& other) const
        {
            other.Resize(size());
            std::copy(m_bitVector.begin(), m_bitVector.end(), other.m_bitVector.begin());
        }

    private:

        template <typename TFunc, typename TSelf>
        static void IterateRange(TSelf& self, size_t beginIndex, size_t endIndex, TFunc elementHandler)
        {
            RPS_ASSERT(beginIndex < self.m_BitSize);
            RPS_ASSERT(endIndex <= self.m_BitSize);
            RPS_ASSERT(beginIndex <= endIndex);

            const size_t beginElementIdx = beginIndex / ELEMENT_NUM_BITS;
            const size_t beginBitIndex   = beginIndex % ELEMENT_NUM_BITS;
            const size_t endElementIdx   = endIndex / ELEMENT_NUM_BITS;
            const size_t endBitIndex     = endIndex % ELEMENT_NUM_BITS;

            bool bResult = true;

            if (beginIndex != endIndex)
            {
                auto&    dstElement = self.m_bitVector[beginElementIdx];
                TElement mask       = (~((TElement(1) << beginBitIndex) - 1));

                if (beginElementIdx == endElementIdx)
                {
                    mask = mask & ((TElement(1) << endBitIndex) - 1);
                }

                elementHandler(dstElement, mask);
            }

            for (size_t i = beginElementIdx + 1; i < endElementIdx; i++)
            {
                auto& dstElement = self.m_bitVector[i];

                elementHandler(dstElement, ~(TElement(0)));
            }

            if ((beginElementIdx != endElementIdx) && (endElementIdx < self.m_bitVector.size()))
            {
                auto&    dstElement = self.m_bitVector[endElementIdx];
                TElement mask       = ((TElement(1) << endBitIndex) - 1);

                elementHandler(dstElement, mask);
            }
        }

    private:
        Vector<TElement, TAllocator> m_bitVector;
        size_t                       m_BitSize = 0;
    };

    template <typename TElement, typename TAllocator>
    constexpr uint32_t BitVector<TElement, TAllocator>::ELEMENT_NUM_BITS;

    struct StrRef
    {
        const char* str;
        size_t      len;

        constexpr StrRef()
            : str(nullptr)
            , len(0)
        {
        }

        constexpr StrRef(const char* cStr)
            : str(cStr)
            , len(cStr ? strlen(cStr) : 0)
        {
        }

        constexpr StrRef(const char* inStr, size_t inLen)
            : str(inStr)
            , len(inLen)
        {
        }

        template <size_t Len>
        static constexpr StrRef From(const char (&charArray)[Len])
        {
            return StrRef{charArray, Len - 1};
        }

        bool empty() const
        {
            return len == 0;
        }

        operator bool() const
        {
            return !empty();
        }

        void Print(const RpsPrinter& printer) const
        {
            printer.pfnPrintf(printer.pContext, "%.*s", len, str);
        }

        bool ToCStr(char* dstBuf, size_t bufSize) const
        {
            size_t copyLen = (bufSize > 0) ? rpsMin(bufSize - 1, len) : 0;

            if (copyLen)
            {
                memcpy(dstBuf, str, copyLen);
            }

            if (bufSize > 0)
            {
                dstBuf[copyLen] = '\0';
            }

            return (copyLen == len);
        }

        bool operator==(const StrRef& other) const
        {
            return (len == other.len) && (0 == strncmp(str, other.str, len));
        }

        bool operator!=(const StrRef& other) const
        {
            return !(*this == other);
        }
    };

    // TODO:
    // - add more string types for append and construct.
    // - add ability to resize over current terminating behaviour.
    // - add a str() function returning StrRef, or allow implicit cast to StrRef.
    template <size_t Capacity = RPS_NAME_MAX_LEN>
    class StrBuilder
    {
        char   m_buf[Capacity];
        size_t m_spaceLeft;  // includes the terminator of the existing string

    public:
        constexpr StrBuilder()
            : m_buf{}
            , m_spaceLeft(Capacity)
        {
        }

        explicit StrBuilder(const char* str)
            : StrBuilder()
        {
            Append(str);
        }

        StrBuilder& Reset()
        {
            m_buf[0]    = '\0';
            m_spaceLeft = Capacity;

            return *this;
        }

        StrBuilder& AppendFormatV(const char* fmt, va_list vl)
        {
            if (m_spaceLeft <= 1)
                return *this;

            const int written = vsnprintf(m_buf + Length(), m_spaceLeft, fmt, vl);

            if (written >= 0)
            {
                m_spaceLeft = (size_t(written) > (m_spaceLeft - 1)) ? 1 : (m_spaceLeft - written);
            }

            return *this;
        }

        StrBuilder& AppendFormat(const char* fmt, ...)
        {
            if (m_spaceLeft <= 1)
                return *this;

            va_list vl;
            va_start(vl, fmt);
            AppendFormatV(fmt, vl);

            va_end(vl);

            return *this;
        }

        StrBuilder& Append(StrRef strRef)
        {
            if (m_spaceLeft <= 1)
                return *this;

            const size_t copyLen = rpsMin(m_spaceLeft - 1, strRef.len);
            memcpy(m_buf + Length(), strRef.str, copyLen);
            m_buf[Length() + copyLen] = '\0';
            m_spaceLeft               = (strRef.len > (m_spaceLeft - 1)) ? 1 : (m_spaceLeft - strRef.len);

            return *this;
        }

        StrBuilder& PopBack(size_t lengthToPop)
        {
            m_spaceLeft     = rpsMin(m_spaceLeft + lengthToPop, Capacity);
            m_buf[Length()] = '\0';
            return *this;
        }

        StrBuilder& operator+=(const char* str)
        {
            return Append(str);
        }

        const char* c_str() const
        {
            return m_buf;
        }

        StrRef GetStr() const
        {
            return StrRef{m_buf, Length()};
        }

        size_t Length() const
        {
            return Capacity - m_spaceLeft;
        }

        RpsPrinter AsPrinter()
        {
            RpsPrinter printer = {};

            printer.pContext   = this;
            printer.pfnPrintf  = &PrintfCb;
            printer.pfnVPrintf = &VPrintfCb;

            return printer;
        }

        StrBuilder& operator=(const StrBuilder& other)
        {
            return Reset().Append(other.GetStr());
        }

    private:
        typedef StrBuilder<Capacity> Self;

        static void PrintfCb(void* pUserContext, const char* format, ...)
        {
            Self* pThis = static_cast<Self*>(pUserContext);
            if (pThis)
            {
                va_list vl;
                va_start(vl, format);
                pThis->AppendFormatV(format, vl);
                va_end(vl);
            }
        }

        static void VPrintfCb(void* pUserContext, const char* format, va_list vl)
        {
            Self* pThis = static_cast<Self*>(pUserContext);
            if (pThis)
            {
                pThis->AppendFormatV(format, vl);
            }
        }
    };

    template <typename T, typename SizeType = uint32_t>
    class Span
    {
    public:
        using size_type = SizeType;

        constexpr Span()
            : Span(0, 0)
        {
        }

        constexpr Span(SizeType offset, SizeType count)
            : m_offset(offset)
            , m_count(count)
        {
        }

        template <typename TCollection>
        ArrayRef<T, SizeType> Get(TCollection& collection) const
        {
            return ArrayRef<T, SizeType>{collection.begin() + m_offset, m_count};
        }

        template <typename TCollection>
        ConstArrayRef<T, SizeType> GetConstRef(TCollection& collection) const
        {
            return ConstArrayRef<T, SizeType>{collection.begin() + m_offset, m_count};
        }

        template <typename TCollection>
        ConstArrayRef<T, SizeType> Get(const TCollection& collection) const
        {
            return ConstArrayRef<T, SizeType>{collection.cbegin() + m_offset, m_count};
        }

        void SetRange(SizeType offset, SizeType count)
        {
            m_offset = offset;
            m_count  = count;
        }

        void SetCount(SizeType count)
        {
            m_count = count;
        }

        void SetEnd(SizeType endIndex)
        {
            RPS_ASSERT(endIndex >= m_offset);
            SetCount(endIndex - m_offset);
        }

        SizeType GetBegin() const
        {
            return m_offset;
        }

        SizeType GetEnd() const
        {
            return m_offset + m_count;
        }

        SizeType size() const
        {
            return m_count;
        }

        bool empty() const
        {
            return m_count == 0;
        }

    private:
        SizeType m_offset;
        SizeType m_count;
    };

    template <typename T,
              typename TContainer = rps::Vector<T>,
              typename =
                  typename std::enable_if<sizeof(T) >= sizeof(uint32_t) && std::is_trivially_destructible<T>::value &&
                                          std::is_trivially_copyable<T>::value>::type>
    class SpanPool
    {
    public:
        SpanPool(TContainer& container)
            : m_container(container)
        {
            reset();
        }

        void reset()
        {
            std::fill(std::begin(m_freeLists), std::end(m_freeLists), UINT32_MAX);
        }

        void alloc_span(Span<T>& span, uint32_t count)
        {
            if (count != 0)
                span = AllocSpan(rpsRoundUpToPowerOfTwo(count));
            else
                span = {0, 0};
        }

        void push_to_span(Span<T>& span, const T& newElement)
        {
            // Implicit capacity is the next pow of 2
            if (rpsIsPowerOfTwo(span.size()))
            {
                uint32_t newSize = span.size() ? (span.size() << 1) : 1;

                auto newSpan = AllocSpan(newSize);
                auto oldData = span.Get(m_container);
                std::copy(oldData.begin(), oldData.end(), newSpan.Get(m_container).begin());

                free_span(span);

                span = {newSpan.GetBegin(), oldData.size()};
            }

            span.SetCount(span.size() + 1);
            span.Get(m_container).back() = newElement;
        }

        void free_span(Span<T>& span)
        {
            if (!span.empty())
            {
                const uint32_t freeListIndex = rpsFirstBitLow(span.size());

                *reinterpret_cast<uint32_t*>(&span.Get(m_container)[0]) = m_freeLists[freeListIndex];

                m_freeLists[freeListIndex] = span.GetBegin();
            }
            span = {0, 0};
        }

    private:
        Span<T> AllocSpan(uint32_t size)
        {
            const uint32_t freeListIndex = rpsFirstBitLow(size);
            const uint32_t freeLoc       = m_freeLists[freeListIndex];

            if (freeLoc != UINT32_MAX)
            {
                const uint32_t nextFree    = *reinterpret_cast<uint32_t*>(&m_container[freeLoc]);
                m_freeLists[freeListIndex] = nextFree;
                return {freeLoc, size};
            }
            else if (m_container.grow(size))
            {
                return {uint32_t(m_container.size() - size), size};
            }
            return {0, 0};
        }

    private:
        TContainer& m_container;
        uint32_t    m_freeLists[32];
    };

    class Arena
    {
        RPS_CLASS_NO_COPY_MOVE(Arena);

        struct Block
        {
            struct Block* pNext;
            size_t        size;
        };

        static const size_t DEFAULT_BLOCK_SIZE = 65500;
        static const size_t DEFAULT_ALIGNMENT  = alignof(max_align_t);

        size_t             m_blockSize      = DEFAULT_BLOCK_SIZE;
        Block*             m_pBlocks        = {};
        void*              m_pCurrBufferPos = {};
        size_t             m_currBufferSize = 0;
        Block*             m_pFreeBlocks    = {};
        const RpsAllocator m_allocCbs       = {};

    public:
        struct CheckPoint
        {
            void*  pBlock;
            size_t remainingSize;
        };

    public:
        Arena(const RpsAllocator& parentAllocator, size_t defaultBlockSize = 0)
            : m_blockSize(defaultBlockSize ? defaultBlockSize : DEFAULT_BLOCK_SIZE)
            , m_allocCbs(parentAllocator)
        {
        }

        ~Arena()
        {
            FreeBlockList(m_pBlocks);
            FreeBlockList(m_pFreeBlocks);
        }

        void* Alloc(size_t size)
        {
            return AlignedAlloc(size, DEFAULT_ALIGNMENT);
        }

        void* AlignedAlloc(size_t size, size_t alignment)
        {
            size_t paddingSize = rpsPaddingSize(m_pCurrBufferPos, alignment);

            if (paddingSize + size > m_currBufferSize)
            {
                if (RPS_SUCCEEDED(AllocBlock(size + alignment)))
                {
                    paddingSize = rpsPaddingSize(m_pCurrBufferPos, alignment);
                }
                else
                {
                    return nullptr;
                }
            }

            void* pAllocated = rpsBytePtrInc(m_pCurrBufferPos, paddingSize);

            RPS_DEBUG_FILL_MEMORY_ON_POOL_ALLOC(pAllocated, size);

            m_pCurrBufferPos = rpsBytePtrInc(pAllocated, size);
            m_currBufferSize -= paddingSize + size;

            return pAllocated;
        }

        void* Realloc(void* ptr, size_t oldSize, size_t newSize)
        {
            return AlignedRealloc(ptr, oldSize, newSize, DEFAULT_ALIGNMENT);
        }

        void* AlignedRealloc(void* pOldBuffer, size_t oldSize, size_t newSize, size_t alignment)
        {
            RPS_ASSERT(rpsIsPointerAlignedTo(pOldBuffer, alignment));

            // ptr is the last allocation, try extending without realloc
            if (pOldBuffer && (rpsBytePtrInc(pOldBuffer, oldSize) == m_pCurrBufferPos) &&
                (newSize <= (oldSize + m_currBufferSize)))
            {
                RPS_DEBUG_FILL_MEMORY_ON_POOL_ALLOC(rpsBytePtrInc(pOldBuffer, oldSize),
                                                    (newSize > oldSize) ? (newSize - oldSize) : 0);
                RPS_DEBUG_FILL_MEMORY_ON_POOL_FREE(rpsBytePtrInc(pOldBuffer, newSize),
                                                   (oldSize > newSize) ? (oldSize - newSize) : 0);

                m_pCurrBufferPos = rpsBytePtrInc(pOldBuffer, newSize);
                m_currBufferSize = m_currBufferSize + oldSize - newSize;

                return pOldBuffer;
            }
            else if (newSize < oldSize)
            {
                RPS_DEBUG_FILL_MEMORY_ON_POOL_FREE(rpsBytePtrInc(pOldBuffer, oldSize), oldSize - newSize);

                // Not the last allocation, but can reuse old buffer for now.
                return pOldBuffer;
            }

            void* pNewBuffer = AlignedAlloc(newSize, alignment);

            RPS_DEBUG_FILL_MEMORY_ON_POOL_ALLOC(pNewBuffer, newSize);
            RPS_DEBUG_FILL_MEMORY_ON_POOL_ALLOC(pOldBuffer, oldSize);

            if (pOldBuffer && pNewBuffer)
            {
                memcpy(pNewBuffer, pOldBuffer, oldSize);
            }

            return pNewBuffer;
        }

        void* AllocZeroed(size_t size)
        {
            return AlignedAllocZeroed(size, DEFAULT_ALIGNMENT);
        }

        void* AlignedAllocZeroed(size_t size, size_t alignment)
        {
            void* pAllocated = AlignedAlloc(size, alignment);
            if (pAllocated)
            {
                memset(pAllocated, 0, size);
            }
            return pAllocated;
        }

        CheckPoint GetCheckPoint() const
        {
            return CheckPoint{m_pBlocks, m_currBufferSize};
        }

        void ResetToCheckPoint(const CheckPoint& checkPoint)
        {
            Block* pFreeBlocks = m_pFreeBlocks;
            Block* pBlock      = m_pBlocks;

            while ((pBlock != nullptr) && (pBlock != checkPoint.pBlock))
            {
                Block* pNextBlock = pBlock->pNext;

                RPS_DEBUG_FILL_MEMORY_ON_POOL_FREE(rpsBytePtrInc(pBlock, sizeof(Block)),
                                                   (pBlock->size - sizeof(Block)));

                pBlock->pNext = pFreeBlocks;
                pFreeBlocks   = pBlock;
                pBlock        = pNextBlock;
            }

            m_pFreeBlocks = pFreeBlocks;
            m_pBlocks     = pBlock;

            if (pBlock)
            {
                m_pCurrBufferPos = rpsBytePtrInc(pBlock, pBlock->size - checkPoint.remainingSize);
                m_currBufferSize = checkPoint.remainingSize;

                RPS_DEBUG_FILL_MEMORY_ON_POOL_FREE(m_pCurrBufferPos, m_currBufferSize);
            }
            else
            {
                m_pCurrBufferPos = nullptr;
                m_currBufferSize = 0;
            }
        }

        void Reset()
        {
            ResetToCheckPoint({nullptr, 0});
        }

        StrRef StoreCStr(const char* s)
        {
            if (!s)
            {
                return StrRef{};
            }

            const size_t len = strlen(s);
            return StoreStr(StrRef{s, len});
        }

        StrRef StoreStr(const StrRef& s)
        {
            if (s.empty())
            {
                return s;
            }

            char* const pDst = static_cast<char*>(AlignedAlloc(s.len + 1, 1));

            if (pDst)
            {
                memcpy(pDst, s.str, s.len);
                pDst[s.len] = '\0';
            }

            return StrRef{pDst, s.len};
        }

        const void* StoreData(const void* pData, size_t size)
        {
            return StoreDataAligned(pData, size, DEFAULT_ALIGNMENT);
        }

        const void* StoreDataAligned(const void* pData, size_t size, size_t alignment)
        {
            char* pDst = (char*)AlignedAlloc(size, DEFAULT_ALIGNMENT);

            if (pDst)
            {
                memcpy(pDst, pData, size);
            }

            return pDst;
        }

        bool HasFreeBlocks() const
        {
            return m_pFreeBlocks != nullptr;
        }

        template <typename T, typename = typename std::enable_if<std::is_trivial<T>::value>::type>
        T* New()
        {
            return static_cast<T*>(AlignedAlloc(sizeof(T), alignof(T)));
        }

        template <typename T>
        void* Alloc()
        {
            return AlignedAlloc(sizeof(T), alignof(T));
        }

        template <typename T, typename = typename std::enable_if<std::is_trivial<T>::value>::type>
        ArrayRef<T> NewArray(size_t count)
        {
            if (count > 0)
            {
                T* p = static_cast<T*>(AlignedAlloc(count * sizeof(T), alignof(T)));
                return {p, p ? count : 0};
            }

            return {nullptr, 0};
        }

        template <typename T, typename = typename std::enable_if<std::is_trivial<T>::value>::type>
        ArrayRef<T> NewArrayZeroed(size_t count)
        {
            auto result = NewArray<T>(count);
            if (!result.empty())
            {
                memset(result.data(), 0, result.size() * sizeof(T));
            }
            return result;
        }

        template <typename T, typename... TCtorArgs>
        T* New(TCtorArgs&&... args)
        {
            void* pBuffer = AlignedAlloc(sizeof(T), alignof(T));
            new (pBuffer) T(args...);
            return static_cast<T*>(pBuffer);
        }

        template <typename T>
        void Delete(T* p)
        {
            p->~T();
        }

        template <typename T, typename... TCtorArgs>
        ArrayRef<T> NewArray(
            typename std::enable_if<std::is_trivially_destructible<T>::value && !std::is_trivial<T>::value,
                                    size_t>::type count,
            TCtorArgs&&... args)
        {
            T* pBuffer = static_cast<T*>(AlignedAlloc(count * sizeof(T), alignof(T)));
            for (T *pIter = pBuffer, *pEnd = (pBuffer + count); pIter != pEnd; ++pIter)
            {
                new (pIter) T(args...);
            }
            return {pBuffer, pBuffer ? count : 0};
        }

        template <typename T, typename TCtor>
        ArrayRef<T> NewArray(
            TCtor                                 ctor,
            typename std::enable_if<std::is_trivially_destructible<T>::value && !std::is_trivial<T>::value,
                                    size_t>::type count)
        {
            T* pBuffer = static_cast<T*>(AlignedAlloc(count * sizeof(T), alignof(T)));
            for (size_t idx = 0; idx < count; ++idx)
            {
                ctor(idx, pBuffer + idx);
            }
            return {pBuffer, pBuffer ? count : 0};
        }

        RpsAllocator AsRpsAllocator()
        {
            RpsAllocator result;
            result.pfnAlloc   = &AllocatorAllocCb;
            result.pfnFree    = &AllocatorFreeCb;
            result.pfnRealloc = &AllocatorReallocCb;
            result.pContext   = this;

            return result;
        }

    private:
        static void* AllocatorAllocCb(void* pUserContext, size_t size, size_t alignment)
        {
            return static_cast<Arena*>(pUserContext)->AlignedAlloc(size, alignment);
        }

        static void* AllocatorReallocCb(
            void* pUserContext, void* oldBuffer, size_t oldSize, size_t newSize, size_t alignment)
        {
            return static_cast<Arena*>(pUserContext)->AlignedRealloc(oldBuffer, oldSize, newSize, alignment);
        }

        static void AllocatorFreeCb(void* pUserContext, void* buffer)
        {
        }

    private:
        RpsResult AllocBlock(size_t minSize)
        {
            const size_t requiredBlockSize = minSize + sizeof(Block);

            m_blockSize = rpsMax(m_blockSize, requiredBlockSize);

            Block* pNewBlock = nullptr;

            if (m_pFreeBlocks && (m_pFreeBlocks->size >= requiredBlockSize))
            {
                pNewBlock     = m_pFreeBlocks;
                m_pFreeBlocks = pNewBlock->pNext;
            }
            else
            {
                void* pNewBuffer = m_allocCbs.pfnAlloc(m_allocCbs.pContext, m_blockSize, alignof(Block));

                RPS_CHECK_ALLOC(pNewBuffer);

                RPS_DEBUG_FILL_MEMORY_ON_ALLOC(pNewBuffer, m_blockSize);

                pNewBlock       = static_cast<Block*>(pNewBuffer);
                pNewBlock->size = m_blockSize;
            }

            pNewBlock->pNext = m_pBlocks;
            m_pBlocks        = pNewBlock;

            m_pCurrBufferPos = rpsBytePtrInc(pNewBlock, sizeof(Block));
            m_currBufferSize = pNewBlock->size - sizeof(Block);

            return RPS_OK;
        }

        void FreeBlockList(Block* pBlockList)
        {
            Block* pNext = nullptr;
            for (Block* pBlock = pBlockList; pBlock != NULL; pBlock = pNext)
            {
                pNext = pBlock->pNext;

                RPS_DEBUG_FILL_MEMORY_ON_FREE(pBlock, pBlock->size);

                m_allocCbs.pfnFree(m_allocCbs.pContext, pBlock);
            }
        }
    };

    class RPS_NO_DISCARD ArenaCheckPoint
    {
        RPS_CLASS_NO_COPY_MOVE(ArenaCheckPoint);

        Arena*            m_arena      = nullptr;
        Arena::CheckPoint m_checkpoint = {};

    public:
        ArenaCheckPoint(Arena& arena)
            : m_arena(&arena)
            , m_checkpoint(m_arena->GetCheckPoint())
        {
        }

        ~ArenaCheckPoint()
        {
            if (m_arena)
            {
                m_arena->ResetToCheckPoint(m_checkpoint);
            }
        }
    };

    template <typename T>
    class ArenaAllocator
    {
    public:
        using value_type = T;

        ArenaAllocator() = default;

        template <class U>
        ArenaAllocator(ArenaAllocator<U> const& other) noexcept
            : m_pArena(other.m_pArena)
        {
        }

        ArenaAllocator(Arena* pArena)
            : m_pArena(pArena)
        {
        }

        value_type* allocate(size_t n)
        {
            return static_cast<T*>(m_pArena->AlignedAlloc(n * sizeof(T), alignof(T)));
        }

        value_type* allocate(size_t n, const void* hint)
        {
            return allocate(n);
        }

        void deallocate(value_type* p, size_t n)
        {
        }

        template <class U>
        bool operator==(ArenaAllocator<U> const& rhs) const noexcept
        {
            return m_pArena == rhs.m_pArena;
        }

        template <class U>
        bool operator!=(ArenaAllocator<U> const& rhs) const noexcept
        {
            return !(*this == rhs);
        }

    private:
        Arena* m_pArena = {};
    };

    template <typename T>
    class GeneralAllocator
    {
    public:
        using value_type = T;

        GeneralAllocator() = default;

        template <class U>
        GeneralAllocator(GeneralAllocator<U> const& other) noexcept
            : m_pCallbacks(other.m_pCallbacks)
        {
        }

        GeneralAllocator(const RpsAllocator* pCallbacks)
            : m_pCallbacks(pCallbacks)
        {
        }

        template <typename U>
        GeneralAllocator& operator=(GeneralAllocator<U> const& other) noexcept
        {
            m_pCallbacks = other.m_pCallbacks;
            return *this;
        }

        value_type* allocate(size_t n)
        {
            return static_cast<T*>(m_pCallbacks->pfnAlloc(m_pCallbacks->pContext, n * sizeof(T), alignof(T)));
        }

        value_type* allocate(size_t n, const void* hint)
        {
            return allocate(n);
        }

        void deallocate(value_type* p, size_t n)
        {
            m_pCallbacks->pfnFree(m_pCallbacks->pContext, p);
        }

        template <class U>
        bool operator==(GeneralAllocator<U> const& rhs) const noexcept
        {
            return m_pCallbacks == rhs.m_pCallbacks;
        }

        template <class U>
        bool operator!=(GeneralAllocator<U> const& rhs) const noexcept
        {
            return !(*this == rhs);
        }

    private:
        const RpsAllocator* m_pCallbacks = {};
    };

    template <typename T>
    using ArenaVector = Vector<T, ArenaAllocator<T>>;

    template <typename T = uint64_t>
    using ArenaBitVector = BitVector<T, ArenaAllocator<T>>;

    template <typename T>
    class InplaceAllocator
    {
    public:
        using value_type = T;

        InplaceAllocator() = default;

        template <class U>
        InplaceAllocator(InplaceAllocator<U> const& other) noexcept
            : m_dynamicAllocator(other.m_dynamicAllocator)
            , m_pStaticArray(other.m_pStaticArray)
        {
        }

        template <typename TDynamicAllocator>
        InplaceAllocator(const TDynamicAllocator& dynamicAllocator, const T* pInplaceArray)
            : m_dynamicAllocator(dynamicAllocator)
            , m_pStaticArray(pInplaceArray)
        {
        }

        template <typename U>
        InplaceAllocator& operator=(InplaceAllocator<U> const& other) noexcept
        {
            m_dynamicAllocator = other.m_dynamicAllocator;
            m_pStaticArray     = other.m_pStaticArray;
            return *this;
        }

        value_type* allocate(size_t n)
        {
            return m_dynamicAllocator.allocate(n);
        }

        value_type* allocate(size_t n, const void* hint)
        {
            return allocate(n);
        }

        void deallocate(value_type* p, size_t n)
        {
            if (p != m_pStaticArray)
            {
                m_dynamicAllocator.deallocate(p, n);
            }
        }

        template <class U>
        bool operator==(InplaceAllocator<U> const& rhs) const noexcept
        {
            return (m_dynamicAllocator == rhs.m_dynamicAllocator) && (m_pStaticArray == rhs.m_pStaticArray);
        }

        template <class U>
        bool operator!=(InplaceAllocator<U> const& rhs) const noexcept
        {
            return !(*this == rhs);
        }

    private:
        GeneralAllocator<T> m_dynamicAllocator;
        const T*            m_pStaticArray = nullptr;
    };

    template <typename T, size_t InplaceSize, typename AllocatorT = GeneralAllocator<T>>
    class InplaceVector : public Vector<T, InplaceAllocator<T>>
    {
        RPS_CLASS_NO_COPY(InplaceVector);

    public:
        InplaceVector() = default;

        InplaceVector(size_t count, const AllocatorT& allocator)
            : Vector<T, InplaceAllocator<T>>(count,
                                             InplaceSize,
                                             reinterpret_cast<T*>(m_staticData),
                                             InplaceAllocator<T>(allocator, reinterpret_cast<T*>(m_staticData)))
        {
        }

        InplaceVector(const AllocatorT& allocator)
            : InplaceVector(0, allocator)
        {
        }

        InplaceVector(size_t count, const T& value, const AllocatorT& allocator)
            : InplaceVector(count, allocator)
        {
        }

        InplaceVector(ConstArrayRef<T> values, const AllocatorT& allocator)
            : InplaceVector(values.size(), allocator)
        {
        }

    private:
        alignas(T) uint8_t m_staticData[InplaceSize * sizeof(T)];
    };

    namespace details
    {
        template <typename T>
        union FreeListPoolSlot
        {
            T        value;
            uint32_t nextFree;

            FreeListPoolSlot()
            {
                new (&value) T();
            }
        };
    }  // namespace details

    template <typename T,
              typename AllocatorT = GeneralAllocator<details::FreeListPoolSlot<T>>,
              typename            = typename std::enable_if<std::is_trivially_destructible<T>::value, T>::type>
    class FreeListPool
    {
    public:
        FreeListPool() = default;

        FreeListPool(const AllocatorT& allocator)
            : m_vector(allocator)
        {
        }

        uint32_t AllocSlot(T** ppValue = nullptr)
        {
            uint32_t result = m_freeList;

            if (result != UINT32_MAX)
            {
                if (ppValue)
                {
                    *ppValue = &m_vector[result].value;
                }
                m_freeList = m_vector[result].nextFree;

                new (&m_vector[result].value) T();
            }
            else
            {
                size_t slotIndex = m_vector.size();
                auto*  pSlot     = m_vector.grow(1);

                if (pSlot)
                {
                    result = uint32_t(slotIndex);

                    if (ppValue)
                    {
                        *ppValue = &pSlot->value;
                    }
                }
            }

            return result;
        }

        void FreeSlot(uint32_t slot)
        {
            m_vector[slot].nextFree = m_freeList;
            m_freeList              = slot;
        }

        T* GetSlot(uint32_t slot)
        {
            return &m_vector[slot].value;
        }

        const T* GetSlot(uint32_t slot) const
        {
            return &m_vector[slot].value;
        }

        void Reset(const AllocatorT& allocator)
        {
            m_vector.reset(allocator);
            m_freeList = UINT32_MAX;
        }

    private:
        Vector<details::FreeListPoolSlot<T>, AllocatorT> m_vector;

        uint32_t m_freeList = UINT32_MAX;
    };

    template <typename T>
    using ArenaFreeListPool = rps::FreeListPool<T, ArenaAllocator<details::FreeListPoolSlot<T>>>;

    namespace details
    {
        template <typename T>
        struct FieldInfo
        {
        };

        template <typename T>
        struct FieldInfo<T**>
        {
            T**    ppData = nullptr;
            size_t offset = 0;

            void Init(AllocInfo& allocInfo, T** pPtr)
            {
                ppData = pPtr;
                offset = allocInfo.Append<T>();
            }

            void Resolve(void* pMemory)
            {
                *ppData = static_cast<T*>(rpsBytePtrInc(pMemory, offset));
            }
        };

        template <typename T>
        struct FieldInfo<std::pair<T**, AllocInfo>>
        {
            T**    ppData = nullptr;
            size_t offset = 0;

            void Init(AllocInfo& allocInfo, std::pair<T**, AllocInfo> arg)
            {
                ppData = arg.first;
                offset = allocInfo.Append(arg.second);
            }

            void Resolve(void* pMemory)
            {
                *ppData = rpsBytePtrInc<T>(pMemory, offset);
            }
        };

        template <typename T, typename TSize>
        struct FieldInfo<std::pair<ArrayRef<T>*, TSize>>
        {
            ArrayRef<T>* pArray = nullptr;
            size_t       offset = 0;
            size_t       count  = 0;

            void Init(AllocInfo& allocInfo, std::pair<ArrayRef<T>*, TSize> arg)
            {
                pArray = arg.first;
                count  = arg.second;
                offset = allocInfo.Append<T>(arg.second);
            }

            void Resolve(void* pMemory)
            {
                pArray->Set(rpsBytePtrInc<T>(pMemory, offset), count);
            }
        };

        template <typename... TArgs>
        struct CompoundAllocInfo : AllocInfo
        {
            std::tuple<FieldInfo<TArgs>...> fields;

            CompoundAllocInfo(TArgs... args)
            {
                Init<0>(args...);
            }

            void Resolve(void* pMemory)
            {
                Resolve<0, TArgs...>(pMemory);
            }

        private:
            template <int32_t I, typename TArg0, typename... TArgX>
            void Init(TArg0 arg0, TArgX... argx)
            {
                std::get<I>(fields).Init(*this, arg0);

                Init<I + 1>(argx...);
            }

            template <int32_t I>
            void Init()
            {
            }

            template <int32_t I, typename TArg0, typename... TArgX>
            void Resolve(void* pMemory)
            {
                std::get<I>(fields).Resolve(pMemory);

                Resolve<I + 1, TArgX...>(pMemory);
            }

            template <int32_t I>
            void Resolve(void* pMemory)
            {
            }
        };
    }  // namespace details

    template <typename T>
    std::pair<T**, AllocInfo> CompoundEntry(T** ppEntry)
    {
        return std::make_pair(ppEntry, AllocInfo::FromType<T>());
    }

    template <typename T>
    std::pair<T**, AllocInfo> CompoundEntry(T** ppEntry, const AllocInfo& allocInfo)
    {
        return std::make_pair(ppEntry, allocInfo);
    }

    template <typename T, typename TSize>
    std::pair<ArrayRef<T>*, TSize> CompoundEntry(ArrayRef<T, TSize>* pArray, size_t count = 1)
    {
        return std::make_pair(pArray, count);
    }

    template <typename... TArgs>
    void* AllocateCompound(const RpsAllocator& allocator, TArgs... args)
    {
        details::CompoundAllocInfo<TArgs...> layout(args...);

        void* pMemory = allocator.pfnAlloc(allocator.pContext, layout.size, layout.alignment);

        if (pMemory)
        {
            layout.Resolve(pMemory);
        }

        return pMemory;
    }

    static inline void* Allocate(const RpsAllocator& allocator, const AllocInfo& allocInfo)
    {
        return allocator.pfnAlloc(allocator.pContext, allocInfo.size, allocInfo.alignment);
    }

    static inline void Free(const RpsAllocator& allocator, void* pMemory)
    {
        allocator.pfnFree(allocator.pContext, pMemory);
    }

    template <typename T>
    struct TResult
    {
        RpsResult code = RPS_ERROR_UNSPECIFIED;
        T         data = {};

        TResult()
        {
        }

        TResult(RpsResult inCode, T inData = {})
            : code(inCode)
            , data(inData)
        {
        }

        TResult(T value)
            : code(RPS_OK)
            , data(value)
        {
        }

        bool IsSucceeded() const
        {
            return RPS_SUCCEEDED(code);
        }

        operator bool() const
        {
            return IsSucceeded();
        }

        operator const T&() const
        {
            return data;
        }

        RpsResult Result() const
        {
            return code;
        }

        template <typename U>
        TResult<U> StaticCast() const
        {
            return TResult<U>(code, static_cast<U>(data));
        }
    };

    template <typename T>
    static inline TResult<T> MakeResult(T value, RpsResult errorCode)
    {
        return TResult<T>(errorCode, value);
    }

    template <typename TValue>
    struct NameValuePair
    {
        StrRef name;
        TValue value;
    };

#define RPS_INIT_NAME_VALUE_PAIR(Value) \
    {                                   \
        StrRef(#Value), Value           \
    }

#define RPS_INIT_NAME_VALUE_PAIR_PREFIXED(ValuePrefix, Name) \
    {                                                        \
        StrRef(#Name), ValuePrefix##Name                     \
    }

#define RPS_INIT_NAME_VALUE_PAIR_PREFIXED_POSTFIXED(ValuePrefix, Name, ValuePostfix) \
    {                                                                                \
        StrRef(#Name), ValuePrefix##Name##ValuePostfix                               \
    }

    class PrinterRef
    {
    public:
        PrinterRef(const RpsPrinter& printer)
            : m_printer(printer)
        {
        }

        operator const RpsPrinter&() const
        {
            return m_printer;
        }

        template <typename... T>
        const PrinterRef& operator()(const char* fmt, T... args) const
        {
            m_printer.pfnPrintf(m_printer.pContext, fmt, args...);
            return *this;
        }

        const PrinterRef& operator()(const StrRef str) const
        {
            m_printer.pfnPrintf(m_printer.pContext, "%.*s", str.len, str.str);
            return *this;
        }

        template <typename TValue, size_t Count>
        const PrinterRef& PrintFlags(TValue value,
                                     const NameValuePair<TValue> (&names)[Count],
                                     StrRef separator     = " | ",
                                     StrRef defaultString = "NONE") const
        {
            return PrintFlags(value, ConstArrayRef<NameValuePair<TValue>>(names), separator, defaultString);
        }

        template <typename TValue>
        const PrinterRef& PrintFlags(TValue                               value,
                                     ConstArrayRef<NameValuePair<TValue>> names,
                                     StrRef                               separator     = " | ",
                                     StrRef                               defaultString = "NONE") const
        {
            bool bPrintedAnyFlag = false;

            for (auto& nameValuePair : names)
            {
                if (value == nameValuePair.value)
                {
                    (*this)(nameValuePair.name);
                    bPrintedAnyFlag = true;
                    break;
                }
                else if (value & nameValuePair.value)
                {
                    if (bPrintedAnyFlag)
                    {
                        (*this)(separator);
                    }
                    (*this)(nameValuePair.name);

                    bPrintedAnyFlag = true;
                }
            }

            if (!bPrintedAnyFlag)
            {
                (*this)(defaultString);
            }

            return *this;
        }

        template <typename TValue, size_t Count>
        const PrinterRef& PrintValueName(TValue value, const NameValuePair<TValue> (&names)[Count]) const
        {
            return PrintValueName(value, ConstArrayRef<NameValuePair<TValue>>(names));
        }

        template <typename TValue>
        const PrinterRef& PrintValueName(TValue value, ConstArrayRef<NameValuePair<TValue>> names) const
        {
            for (auto& nameValuePair : names)
            {
                if (value == nameValuePair.value)
                {
                    (*this)(nameValuePair.name);
                    break;
                }
            }

            return *this;
        }

    private:
        const RpsPrinter& m_printer;
    };

    /// @brief Util for setting a pointer to a given value temporarily, and restore it when going out of scope with RAAI
    template <typename T, typename = typename std::enable_if<std::is_trivially_copy_assignable<T>::value>::type>
    struct ScopedContext
    {
        ScopedContext(T* pPrev, T curr)
            : m_pPrev(pPrev)
            , m_prevValue(*pPrev)
        {
            *pPrev = curr;
        }

        ~ScopedContext()
        {
            *m_pPrev = m_prevValue;
        }

    private:
        T* const m_pPrev;
        T        m_prevValue;

        RPS_CLASS_NO_COPY_MOVE(ScopedContext);
    };

}  // namespace rps

#endif  // RPS_UTIL_HPP
