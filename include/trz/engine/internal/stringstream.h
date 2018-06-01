/**
 * @file stringstream.h
 * @brief custom stringstream class
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <cstdio>
#include <cstring>
#include <ostream>

#include "trz/engine/platform.h"

namespace tredzone
{

#define TREDZONE_DEFAULT_STREAM_BUFFER_INCREMENT_SIZE 512 /**< Default stream buffer increment size */

/**
 * @brief Tredzone StreamBuffer class
 */
template <typename _Char, class _Allocator, size_t _BufferSizeIncrement = TREDZONE_DEFAULT_STREAM_BUFFER_INCREMENT_SIZE>
class StreamBuffer : public std::basic_streambuf<_Char>
{
  public:
    /** @brief Constructor */
    inline StreamBuffer(const _Allocator &pallocator) noexcept : allocator(pallocator),
                                                                 buffer(defaultBuffer),
                                                                 bufferSize(_BufferSizeIncrement),
                                                                 bufferOffset(0)
    {
    }
    /** @brief Destructor */
    virtual ~StreamBuffer() noexcept
    {
        if (buffer != defaultBuffer)
        {
            allocator.deallocate(buffer, bufferSize * sizeof(_Char));
        }
    }
    /** @brief Get allocator */
    inline const _Allocator &get_allocator() const noexcept { return allocator; }

  protected:
    _Allocator allocator;
    char defaultBuffer[_BufferSizeIncrement];
    _Char *buffer;
    size_t bufferSize;
    size_t bufferOffset;

    /**
     * @brief Writes characters from the array pointed to by s into the controlled output sequence, until either n
     * characters have been written or the end of the output sequence is reached.
     * @param s array pointer
     * @param n number of characters to write
     * @return The number of characters written.
     */
    virtual std::streamsize xsputn(const _Char *s, std::streamsize n)
    {
        if (n > 0)
        {
            reserve((size_t)n);
            memcpy(&buffer[bufferOffset], s, (size_t)n * sizeof(_Char));
            bufferOffset += (size_t)n;
        }
        return n;
    }

    /**
     * @brief write c at current buffer offset and increment bufferOffset
     * @param c character to write
     * @return written character
     */
    virtual int overflow(int c = EOF)
    {
        if (c != EOF)
        {
            reserve(1);
            buffer[bufferOffset] = std::char_traits<_Char>::to_char_type(c);
            ++bufferOffset;
        }
        return c;
    }

    /**
     * @brief Reserve n buffer blocks.
     * This method allocates and copies buffers if needed
     * @param n number of blocks to reserve
     * @throws std::bad_alloc
     */
    inline void reserve(size_t n)
    {
        if (bufferSize - bufferOffset < n + 1)
        { // One more for c_str nul char
            size_t newBufferSize =
                ((n + 1 + bufferOffset + _BufferSizeIncrement - 1) / _BufferSizeIncrement) * _BufferSizeIncrement;
            assert(newBufferSize - bufferOffset >= n + 1);
            _Char *newBuffer = allocator.allocate(newBufferSize);
            assert(bufferSize > 0 && bufferOffset > 0);
            memcpy(newBuffer, buffer, bufferOffset * sizeof(_Char));
            if (buffer != defaultBuffer)
            {
                allocator.deallocate(buffer, bufferSize * sizeof(_Char));
            }
            buffer = newBuffer;
            bufferSize = newBufferSize;
        }
    }
};

/**
 * @brief Tredzone OutputStringStream class
 */
template <typename _Char, class _Allocator, size_t _BufferSizeIncrement = 512>
struct OutputStringStream : private StreamBuffer<_Char, _Allocator, _BufferSizeIncrement>, std::basic_ostream<_Char>
{
    /** @brief Constructor */
    inline OutputStringStream(const _Allocator &allocator) noexcept
        : StreamBuffer<_Char, _Allocator, _BufferSizeIncrement>(allocator),
          std::basic_ostream<_Char>(static_cast<StreamBuffer<_Char, _Allocator, _BufferSizeIncrement> *>(this))
    {
    }
    /** @brief Destructor */
    virtual ~OutputStringStream() noexcept {}
    /** @brief Get allocator */
    inline const _Allocator &get_allocator() const noexcept
    {
        return StreamBuffer<_Char, _Allocator, _BufferSizeIncrement>::get_allocator();
    }
};

/**
 * @brief Tredzone OutputStringStream class
 */
template <class _Allocator, size_t _BufferSizeIncrement>
struct OutputStringStream<char, _Allocator, _BufferSizeIncrement>
    : private StreamBuffer<char, _Allocator, _BufferSizeIncrement>, std::basic_ostream<char>
{
    typedef std::basic_string<char, std::char_traits<char>, _Allocator> string_type;

    /** @brief Constructor */
    inline OutputStringStream(const _Allocator &allocator) noexcept
        : StreamBuffer<char, _Allocator, _BufferSizeIncrement>(allocator),
          std::basic_ostream<char>(static_cast<StreamBuffer<char, _Allocator, _BufferSizeIncrement> *>(this))
    {
    }
    /** @brief Destructor */
    virtual ~OutputStringStream() noexcept {}
    /**
     * @brief Returns a pointer to an array that contains a null-terminated sequence of characters (i.e., a C-string)
     * representing the current value of the string object. This array includes the same sequence of characters that
     * make up the value of the string object plus an additional terminating null-character ('\0') at the end.
     * @return A pointer to the c-string representation of the string object's value.
     */
    inline const char *c_str() const noexcept
    {
        assert(this->bufferSize > this->bufferOffset);
        this->buffer[this->bufferOffset] = '\0';
        return this->buffer;
    }
    /**
     * @brief Returns the length of the string
     * @return String length
     */
    inline size_t size() const noexcept { return this->bufferOffset; }
    /**
     * @brief Returns a string_type instance of the current string
     * @return string_type instance of current string
     */
    inline string_type str() const
    { // throw (std::bad_alloc)
        return string_type(c_str(), get_allocator());
    }
    /**
     * @brief Get allocator
     * @return allocator
     */
    inline const _Allocator &get_allocator() const noexcept
    {
        return StreamBuffer<char, _Allocator, _BufferSizeIncrement>::get_allocator();
    }
};

} // namespace