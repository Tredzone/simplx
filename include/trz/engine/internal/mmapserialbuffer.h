/**
 * @file serialbufferchain.h
 * @brief serialized buffer list
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <algorithm>
#include <cassert>
#include <cstring>
#include <memory>
#include <iostream>

#include <sys/mman.h>
#include <errno.h>

#include "trz/engine/platform.h"
#include "trz/engine/internal/stringstream.h"

namespace tredzone
{
// import into namespace
using std::string;

const size_t    MAX_SERIAL_STRING_SIZE = 32766;

class mmapSerialBuffer
{
public:

    using WriteMark = size_t;
    
    // ctor
    inline
    mmapSerialBuffer()
        : m_PageSize(::sysconf(_SC_PAGE_SIZE)), m_NumPages((1ull<< 32) / m_PageSize), m_BufferSize(m_NumPages * m_PageSize),
          contentSize(0), readOffset(0), writeOffset(0),
          m_Buffer(newBuffer())
    {
        assert(m_Buffer);        
    }
    
    // dtor
    ~mmapSerialBuffer() noexcept
    {
        assert(m_Buffer);
        
        // unmap
        const int   err = ::munmap(m_Buffer, m_BufferSize);
        assert(!err);
        (void) err; // unused-variable
    }
    
    inline
    const void *getCurrentReadBuffer() const noexcept
    {
        assert(readOffset < m_BufferSize);                          // always true?
        return static_cast<const char *>(m_Buffer) + readOffset;
    }
    
    inline
    size_t getCurrentReadBufferSize() const noexcept
    {
        assert(readOffset < m_BufferSize);                          // always true?
        return m_BufferSize - readOffset;
    }
    
    inline
    void decreaseCurrentReadBufferSize(size_t sz) noexcept
    {
        assert(sz <= contentSize);
        contentSize -= sz;
        assert(readOffset < m_BufferSize);                          // always true?
        assert(sz < m_BufferSize - readOffset);
        
        readOffset += sz;
        assert(readOffset < m_BufferSize);                          // always true?
    }
    
    inline
    void *getCurrentWriteBuffer(void) const noexcept
    {
        assert(writeOffset < m_BufferSize);                          // always true?
        return static_cast<char *>(m_Buffer) + writeOffset;
    }
    
    inline
    size_t getCurrentWriteBufferSize() const noexcept
    {
        assert(writeOffset < m_BufferSize);                          // always true?
        return m_BufferSize - writeOffset;
    }
    
    inline
    WriteMark getCurrentWriteMark() const noexcept
    {
        return WriteMark(writeOffset);
    }
    
    inline
    void *getWriteMarkBuffer(const WriteMark &mark) const noexcept
    {
        assert(mark < m_BufferSize);                          // always true?
        return static_cast<char *>(m_Buffer) + mark;
    }
    
    inline
    size_t getWriteMarkBufferSize(const WriteMark &mark) const noexcept             // is infinity?
    {
        assert(mark < m_BufferSize);
        return m_BufferSize - mark;
    }

    void*   GetRawBuffer(void) const noexcept   { return m_Buffer;}
    
//---- Increase Write Buffer Size ----------------------------------------------

    inline
    void increaseCurrentWriteBufferSize(size_t sz)
    {   
        assert(writeOffset < m_BufferSize);
        assert(sz < m_BufferSize - writeOffset);
        
        // update indices
        contentSize += sz;
        writeOffset += sz;
        assert(writeOffset < m_BufferSize);
    }
    
    inline
    bool empty(void) const noexcept      { return contentSize == 0; }
    
    inline
    size_t size(void) const noexcept     { return contentSize; }    
    
//---- Copy from Read Buffer ---------------------------------------------------

    inline
    void copyReadBuffer(void *destBuffer, size_t destBufferSz) const noexcept
    {
        // should never be called?
        ::memcpy(static_cast<char *>(destBuffer), static_cast<const char *>(m_Buffer) + readOffset, destBufferSz);
    }
    
//---- Copy to Write Buffer ----------------------------------------------------

    inline
    void copyWriteBuffer(const void *srcBuffer, size_t srcBufferSz, const WriteMark &mark) const noexcept
    {
        assert(mark + srcBufferSz <= contentSize);
        
        ::memcpy(static_cast<char *>(m_Buffer) + mark, static_cast<const char *>(srcBuffer), srcBufferSz);
    }
    
    inline
    void clear(void) noexcept
    {
        readOffset = writeOffset = contentSize = 0;
    }
    
    // pipe out (NOTE: _T must have a vanilla ctor!)
    template<typename _T>
    mmapSerialBuffer& operator <<(const _T &t)
    {
        ::memcpy(getCurrentWriteBuffer(), &t, sizeof(t));
		increaseCurrentWriteBufferSize(sizeof(t));
        return *this;
    }
    
    // pipe out (string specialization)
    mmapSerialBuffer& operator <<(const string &s)
    {
        const uint32_t    sz = (uint32_t) s.length();
        assert(sz < MAX_SERIAL_STRING_SIZE);
        
        this->operator <<(sz);
        
        ::memcpy(getCurrentWriteBuffer(), s.c_str(), sz);
		increaseCurrentWriteBufferSize(sz);
        return *this;
    }
    
    // pipe in
    template<typename _T>
    mmapSerialBuffer& operator >>( _T &t)
    {
        t = *(reinterpret_cast<const _T*>(getCurrentReadBuffer()));
		decreaseCurrentReadBufferSize(sizeof(t));
                
        return *this;
    }
    
    // pipe in (string specialization)
    mmapSerialBuffer& operator >>(string &s)
    {
        uint32_t    sz;
        
        this->operator >>(sz/*&*/);
        
        assert(sz < MAX_SERIAL_STRING_SIZE);
        
        s = string(reinterpret_cast<const char*>(getCurrentReadBuffer()), (size_t)sz);
		decreaseCurrentReadBufferSize(sz);
        
        return *this;
    }
    
private:
  
    void*   newBuffer(void)
    {
        assert(m_BufferSize > 0);
        assert((m_BufferSize & (m_PageSize -1)) == 0);          // is page-aligned?
        
        void *p = ::mmap(0, m_BufferSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1/*fd*/, 0/*file offset*/);
        assert(p);
        assert(MAP_FAILED != p);
       // cout << "mmap err " << errno << endl;
        
        return p;
    }

    const size_t    m_PageSize;
    const size_t    m_NumPages;
    const size_t    m_BufferSize;           // ~= infinity
    size_t          contentSize;
    size_t          readOffset;
    size_t          writeOffset;
    void            *m_Buffer;
    
};


} // namespace tredzone
