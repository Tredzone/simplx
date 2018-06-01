/**
 * @file serialbufferchain.h
 * @brief serialized buffer list
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <algorithm>
#include <cassert>
#include <cstring>
#include <memory>

#include "trz/engine/internal/intrinsics.h"

#include "trz/engine/platform.h"

namespace tredzone
{

template <class _Allocator = std::allocator<void>> class SerialBufferChain
{
  public:
    typedef typename _Allocator::template rebind<char>::other allocator_type;
    class WriteMark
    {
      private:
        friend class SerialBufferChain;
        void *buffer;
        size_t offset;
        inline WriteMark(void *buffer, size_t offset) noexcept : buffer(buffer), offset(offset) {}
    };
    const size_t bufferSize;

    inline SerialBufferChain(allocator_type allocator = allocator_type(),
                             size_t bufferSize = 4096 - sizeof(void *))
        : // throw(std::bad_alloc)
          bufferSize(std::max<size_t>(1, bufferSize)),
          allocator(allocator), contentSize(0), readOffset(0), writeOffset(0), readBuffer(newBuffer()),
          writeBuffer(readBuffer), unusedBuffers(0)
#ifndef NDEBUG
          ,
          debugBufferCount(1)
#endif
    {
#ifndef NDEBUG
        debugCheckBuffers();
#endif
    }
    inline ~SerialBufferChain() noexcept
    {
#ifndef NDEBUG
        debugCheckBuffers();
#endif
        deleteBufferChain(unusedBuffers);
        deleteBufferChain(readBuffer);
        assert(debugBufferCount == 0);
    }
    inline const void *getCurrentReadBuffer() const noexcept
    {
        assert(readBuffer != 0);
        assert(readOffset < bufferSize);
        return static_cast<char *>(readBuffer) + readOffset;
    }
    inline size_t getCurrentReadBufferSize() const noexcept
    {
        assert(readOffset < bufferSize);
        return bufferSize - readOffset;
    }
    inline void decreaseCurrentReadBufferSize(size_t sz) noexcept
    {
        assert(sz <= contentSize);
        contentSize -= sz;
        assert(readOffset < bufferSize);
        if (sz < bufferSize - readOffset)
        {
            readOffset += sz;
            assert(readOffset < bufferSize);
        }
        else
        {
            for (readOffset = sz + readOffset; readOffset >= bufferSize; readOffset -= bufferSize)
            {
                void *buffer = readBuffer;
                assert(getNextBuffer(readBuffer) != 0);
                readBuffer = getNextBuffer(readBuffer);
                setNextBuffer(buffer, unusedBuffers);
                unusedBuffers = buffer;
            }
        }
#ifndef NDEBUG
        debugCheckBuffers();
#endif
    }
    inline void *getCurrentWriteBuffer() const noexcept
    {
        assert(writeBuffer != 0);
        assert(writeOffset < bufferSize);
        return static_cast<char *>(writeBuffer) + writeOffset;
    }
    inline size_t getCurrentWriteBufferSize() const noexcept
    {
        assert(writeOffset < bufferSize);
        return bufferSize - writeOffset;
    }
    inline WriteMark getCurrentWriteMark() const noexcept { return WriteMark(writeBuffer, writeOffset); }
    inline void *getWriteMarkBuffer(const WriteMark &mark) const noexcept
    {
#ifndef NDEBUG
        debugCheckWriteMark(mark);
#endif
        assert(mark.buffer != 0);
        assert(mark.offset < bufferSize);
        return static_cast<char *>(mark.buffer) + mark.offset;
    }
    inline size_t getWriteMarkBufferSize(const WriteMark &mark) const noexcept
    {
#ifndef NDEBUG
        debugCheckWriteMark(mark);
#endif
        assert(mark.offset < bufferSize);
        return bufferSize - mark.offset;
    }
    inline void increaseCurrentWriteBufferSize(size_t sz)
    { // throw(std::bad_alloc)
        assert(writeOffset < bufferSize);
        if (sz < bufferSize - writeOffset)
        {
            contentSize += sz;
            writeOffset += sz;
            assert(writeOffset < bufferSize);
        }
        else
        {
            void *tailBuffer = writeBuffer;
            size_t remSz = sz + writeOffset;
            for (; remSz >= bufferSize; remSz -= bufferSize)
            {
                try
                {
                    void *buffer = unusedBuffers;
                    if (buffer == 0)
                    {
                        buffer = newBuffer();
                    }
                    else
                    {
                        unusedBuffers = getNextBuffer(buffer);
                        setNextBuffer(buffer, 0);
                    }
                    assert(getNextBuffer(buffer) == 0);
                    assert(getNextBuffer(tailBuffer) == 0);
                    setNextBuffer(tailBuffer, buffer);
                    tailBuffer = buffer;
                }
                catch (...)
                {
                    if (tailBuffer != writeBuffer)
                    {
                        assert(getNextBuffer(tailBuffer) == 0);
                        setNextBuffer(tailBuffer, unusedBuffers);
                        assert(getNextBuffer(writeBuffer) != 0);
                        unusedBuffers = getNextBuffer(writeBuffer);
                        setNextBuffer(writeBuffer, 0);
                    }
                    throw;
                }
            }
            writeBuffer = tailBuffer;
            contentSize += sz;
            writeOffset = remSz;
        }
#ifndef NDEBUG
        debugCheckBuffers();
#endif
    }
    inline bool empty() const noexcept { return contentSize == 0; }
    inline size_t size() const noexcept { return contentSize; }
    inline size_t getUnusedBuffersCount() const noexcept { return countBufferChain(unusedBuffers); }
    inline void deleteUnusedBuffers() noexcept
    {
        deleteBufferChain(unusedBuffers);
        unusedBuffers = 0;
    }
    inline void copyReadBuffer(void *destBuffer, size_t destBufferSz) const noexcept
    {
        assert(destBufferSz <= contentSize);
        void *buffer = readBuffer;
        for (size_t sz = 0, offset = readOffset, n = bufferSize - offset; sz < destBufferSz;
             buffer = getNextBuffer(buffer), sz += n, offset = 0, n = bufferSize)
        {
            memcpy(static_cast<char *>(destBuffer) + sz, static_cast<char *>(buffer) + offset,
                   std::min((size_t)(destBufferSz - sz), n));
        }
    }
    inline void copyWriteBuffer(const void *srcBuffer, size_t srcBufferSz, const WriteMark &mark) const noexcept
    {
        assert(debugCheckWriteMark(mark) + srcBufferSz <= contentSize);
        void *buffer = mark.buffer;
        for (size_t sz = 0, offset = mark.offset, n = bufferSize - offset; sz < srcBufferSz;
             buffer = getNextBuffer(buffer), sz += n, offset = 0, n = bufferSize)
        {
            memcpy(static_cast<char *>(buffer) + offset, static_cast<const char *>(srcBuffer) + sz,
                   std::min((size_t)(srcBufferSz - sz), n));
        }
    }
    inline void clear() noexcept
    {
        setNextBuffer(writeBuffer, unusedBuffers);
        unusedBuffers = getNextBuffer(readBuffer);
        setNextBuffer(readBuffer, 0);
        writeBuffer = readBuffer;
        readOffset = writeOffset = contentSize = 0;
#ifndef NDEBUG
        debugCheckBuffers();
#endif
    }

  private:
    allocator_type allocator;
    size_t contentSize;
    size_t readOffset;
    size_t writeOffset;
    void *readBuffer;
    void *writeBuffer;
    void *unusedBuffers;
#ifndef NDEBUG
    size_t debugBufferCount;
#endif

    inline void *newBuffer()
    {
#ifndef NDEBUG
        try
        {
            ++debugBufferCount;
#endif
            void *buffer = allocator.allocate(bufferSize + sizeof(void *));
            setNextBuffer(buffer, 0);
            return buffer;
#ifndef NDEBUG
        }
        catch (...)
        {
            --debugBufferCount;
            throw;
        }
#endif
    }
    inline void deleteBuffer(void *buffer) noexcept
    {
#ifndef NDEBUG
        assert(debugBufferCount > 0);
        --debugBufferCount;
#endif
        allocator.deallocate(static_cast<char *>(buffer), bufferSize + sizeof(void *));
    }
    inline void *getNextBuffer(void *buffer) const noexcept
    {
        assert(buffer != 0);
        return *reinterpret_cast<void **>(static_cast<char *>(buffer) + bufferSize);
    }
    inline void setNextBuffer(void *buffer, void *nextBuffer) const noexcept
    {
        assert(buffer != 0);
        *reinterpret_cast<void **>(static_cast<char *>(buffer) + bufferSize) = nextBuffer;
    }
    inline size_t countBufferChain(void *b) const noexcept
    {
        size_t bufferCount = 0;
        for (; b != 0; b = getNextBuffer(b), ++bufferCount)
        {
        }
        return bufferCount;
    }
    inline void deleteBufferChain(void *b) noexcept
    {
        while (b != 0)
        {
            void *bNext = getNextBuffer(b);
            deleteBuffer(b);
            b = bNext;
        }
    }
#ifndef NDEBUG
    inline size_t debugCheckWriteMark(const WriteMark &mark) const
    {
        debugCheckBuffers();
        assert(mark.offset < bufferSize);
        if (mark.buffer == readBuffer)
        {
            assert(mark.offset >= readOffset);
        }
        if (mark.buffer == writeBuffer)
        {
            assert(mark.offset <= writeOffset);
        }
        size_t sz = 0;
        for (void *buffer = readBuffer; buffer != 0; buffer = getNextBuffer(buffer), sz += bufferSize)
        {
            if (buffer == mark.buffer)
            {
                sz += mark.offset;
                sz -= readOffset;
                assert(sz <= contentSize);
                return sz;
            }
        }
        TRZ_DEBUG_BREAK();

        return 0;
    }
    inline void debugCheckBuffers() const
    {
        assert(bufferSize > 0);
        size_t bufferCount = countBufferChain(unusedBuffers);
        assert((bufferCount == 0 && unusedBuffers == 0) || (bufferCount > 0 && unusedBuffers != 0));
        bufferCount += countBufferChain(readBuffer);
        assert(bufferCount == debugBufferCount);
        assert((readBuffer == writeBuffer && readOffset <= writeOffset) || (readBuffer != writeBuffer));
        assert(readBuffer != 0);
        assert(writeBuffer != 0);
        size_t localContentSize = 0;
        for (void *b = readBuffer; b != writeBuffer; b = getNextBuffer(b), localContentSize += bufferSize)
        {
            assert(b != 0);
        }
        localContentSize += writeOffset;
        localContentSize -= readOffset;
        assert(contentSize == localContentSize);
        assert(readOffset < bufferSize);
        assert(writeOffset < bufferSize);
    }
#endif
};

} // namespace