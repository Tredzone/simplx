/**
 * @file cacheline.h
 * @brief aligned cache & memory buffers
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <cassert>
#include <cstdlib>
#include <limits>

#include "trz/engine/platform.h"

namespace tredzone
{

#define TREDZONE_CACHE_LINE_SIZE 64
static const int CACHE_LINE_SIZE = TREDZONE_CACHE_LINE_SIZE; // architecture dependent
#define TREDZONE_CACHE_LINE_PADDING(x)                                                                                 \
    (::tredzone::CACHE_LINE_SIZE * ((x + ::tredzone::CACHE_LINE_SIZE - 1) / ::tredzone::CACHE_LINE_SIZE) - (x))

class CacheLineAlignedBuffer
{
  public:
    /**
     * throw (std::bad_alloc)
     */
    inline CacheLineAlignedBuffer(size_t sz)
        : ptr(new char[cacheLineAlignedSize(sz)]), alignedPtr(static_cast<char *>(cacheLineAlignedPointer(ptr)))
    {
        assert((uintptr_t)alignedPtr >= (uintptr_t)ptr);
        assert((uintptr_t)alignedPtr - (uintptr_t)ptr < (unsigned)CACHE_LINE_SIZE);
        assert((uintptr_t)alignedPtr % CACHE_LINE_SIZE == 0);
    }
    inline ~CacheLineAlignedBuffer() noexcept { delete[] ptr; }
    template <class T> inline T cast() noexcept { return reinterpret_cast<T>(alignedPtr); }
    template <class T> inline T cast() const noexcept { return reinterpret_cast<T>(alignedPtr); }
    inline static size_t cacheLineAlignedSize(size_t sz) noexcept
    {
        return (CACHE_LINE_SIZE * ((sz + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE)) + CACHE_LINE_SIZE - 1;
    }
    inline static void *cacheLineAlignedPointer(void *ptr) noexcept
    {
        return static_cast<char *>(ptr) + (CACHE_LINE_SIZE - ((uintptr_t)ptr % CACHE_LINE_SIZE)) % CACHE_LINE_SIZE;
    }

  private:
    char *ptr;
    char *alignedPtr;
};

template <typename T> class CacheLineAlignedArray
{
  public:
    inline explicit CacheLineAlignedArray(size_t n)
        : // throw (std::bad_alloc, ...)
          ptr(new char[n * ALIGNED_T_SZ + CACHE_LINE_SIZE - 1]),
          alignedPtr(static_cast<char *>(CacheLineAlignedBuffer::cacheLineAlignedPointer(ptr))),
          sz((assert(n <= std::numeric_limits<uint32_t>::max()), (uint32_t)n))
    {
        assert((uintptr_t)alignedPtr >= (uintptr_t)ptr);
        assert((uintptr_t)alignedPtr - (uintptr_t)ptr < (unsigned)CACHE_LINE_SIZE);
        assert((uintptr_t)alignedPtr % CACHE_LINE_SIZE == 0);
        assert(ALIGNED_T_SZ % CACHE_LINE_SIZE == 0);
        assert((unsigned)ALIGNED_T_SZ >= sizeof(T));
        for (size_t i = 0; i < sz; ++i)
        {
            new (&alignedPtr[i * ALIGNED_T_SZ]) T();
        }
    }
    template <class _Init>
    inline explicit CacheLineAlignedArray(size_t n,
                                          const _Init &init)
        : // throw (std::bad_alloc, ...)
          ptr(new char[n * ALIGNED_T_SZ + CACHE_LINE_SIZE - 1]),
          alignedPtr(static_cast<char *>(CacheLineAlignedBuffer::cacheLineAlignedPointer(ptr))),
          sz((assert(n <= std::numeric_limits<uint32_t>::max()), (uint32_t)n))
    {
        assert((uintptr_t)alignedPtr >= (uintptr_t)ptr);
        assert((uintptr_t)alignedPtr - (uintptr_t)ptr < (unsigned)CACHE_LINE_SIZE);
        assert((uintptr_t)alignedPtr % CACHE_LINE_SIZE == 0);
        assert(ALIGNED_T_SZ % CACHE_LINE_SIZE == 0);
        assert((unsigned)ALIGNED_T_SZ >= sizeof(T));
        for (size_t i = 0; i < sz; ++i)
        {
            new (&alignedPtr[i * ALIGNED_T_SZ]) T(init);
        }
    }
    inline ~CacheLineAlignedArray() noexcept
    {
        for (size_t i = 0; i < sz; ++i)
        {
            reinterpret_cast<T &>(alignedPtr[i * ALIGNED_T_SZ]).~T();
        }
        delete[] ptr;
    }
    inline T &operator[](size_t i) noexcept
    {
        assert(i < sz);
        return reinterpret_cast<T &>(alignedPtr[i * ALIGNED_T_SZ]);
    }
    inline const T &operator[](size_t i) const noexcept
    {
        assert(i < sz);
        return reinterpret_cast<const T &>(alignedPtr[i * ALIGNED_T_SZ]);
    }
    inline size_t size() const noexcept { return sz; }

  private:
    static const int ALIGNED_T_SZ = CACHE_LINE_SIZE * (((int)sizeof(T) + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE);
    char *ptr;
    char *alignedPtr;
    const uint32_t sz;
};

template <typename T> struct CacheLineAlignedObject : private CacheLineAlignedBuffer
{
    inline CacheLineAlignedObject()
        : // throw (std::bad_alloc, ...)
          CacheLineAlignedBuffer(sizeof(T))
    {
        new (cast<T *>()) T();
    }
    template <class _Init>
    inline CacheLineAlignedObject(const _Init &init)
        : // throw (std::bad_alloc, ...)
          CacheLineAlignedBuffer(sizeof(T))
    {
        new (cast<T *>()) T(init);
    }
    inline ~CacheLineAlignedObject() { cast<T *>()->~T(); }
    inline T &operator*() noexcept { return *cast<T *>(); }
    inline const T &operator*() const noexcept { return *cast<const T *>(); }
    inline T *operator->() noexcept { return cast<T *>(); }
    inline const T *operator->() const noexcept { return cast<const T *>(); }
};

class CacheLineAlignedBufferContainer
{
  public:
    inline CacheLineAlignedBufferContainer() noexcept : head(0) { assert((unsigned)CACHE_LINE_SIZE >= sizeof(void *)); }
    inline ~CacheLineAlignedBufferContainer() noexcept
    {
        for (void *p = head; p != 0;)
        {
            void *nextp = *static_cast<void **>(CacheLineAlignedBuffer::cacheLineAlignedPointer(p));
            std::free(p);
            p = nextp;
        }
    }
    /**
     * throw (std::bad_alloc)
     */
    inline void *insert(size_t sz)
    {
        void *p = std::malloc(CacheLineAlignedBuffer::cacheLineAlignedSize(sz + CACHE_LINE_SIZE));
        if (p == 0)
        {
            throw std::bad_alloc();
        }
        *static_cast<void **>(CacheLineAlignedBuffer::cacheLineAlignedPointer(p)) = head;
        head = p;
        return static_cast<char *>(CacheLineAlignedBuffer::cacheLineAlignedPointer(p)) + CACHE_LINE_SIZE;
    }

  private:
    void *head;
};

template <class _Allocator> class CacheLineAlignedAllocator
{
  public:
    typedef typename _Allocator::value_type value_type;
    typedef typename _Allocator::size_type size_type;
    typedef typename _Allocator::difference_type difference_type;
    typedef typename _Allocator::pointer pointer;
    typedef typename _Allocator::const_pointer const_pointer;
    typedef typename _Allocator::reference reference;
    typedef typename _Allocator::const_reference const_reference;

    template <class U> struct rebind
    {
        typedef CacheLineAlignedAllocator<typename _Allocator::template rebind<U>::other> other;
    };

    inline CacheLineAlignedAllocator() {}
    inline CacheLineAlignedAllocator(const _Allocator &pallocator) : allocator(pallocator) {}
    template <class _OtherAllocator>
    inline CacheLineAlignedAllocator(const CacheLineAlignedAllocator<_OtherAllocator> &other) noexcept
        : allocator(other.allocator)
    {
    }
    inline pointer address(reference r) const { return &r; }
    inline const_pointer address(const_reference r) const { return &r; }
    /**
     * throw (std::bad_alloc)
     */
    inline pointer allocate(size_type n, const void * = 0)
    {
        char *p = allocator.allocate(n * sizeof(value_type) + sizeof(void *) + CACHE_LINE_SIZE - 1);
        pointer ret = static_cast<pointer>(CacheLineAlignedBuffer::cacheLineAlignedPointer(p + sizeof(void *)));
        assert((uintptr_t)ret - (uintptr_t)p <= sizeof(void *) + CACHE_LINE_SIZE - 1);
        *(static_cast<char **>((void *)ret) - 1) = p;
        return ret;
    }
    inline void deallocate(pointer p, size_type n) noexcept
    {
        allocator.deallocate(*(static_cast<char **>((void *)p) - 1),
                             n * sizeof(value_type) + sizeof(void *) + CACHE_LINE_SIZE - 1);
    }
    template <class _Init> inline void construct(pointer p, const _Init &init) { new (p) value_type(init); }
    inline void destroy(pointer p) { p->~value_type(); }
    inline size_type max_size() const noexcept { return allocator.max_size(); }

  private:
    typename _Allocator::template rebind<char>::other allocator;
};

} // namespace