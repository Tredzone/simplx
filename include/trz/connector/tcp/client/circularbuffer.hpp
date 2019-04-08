/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file circularbuffer.hpp
 * @brief circular buffer
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <array>
#include <assert.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <tuple>

namespace tredzone
{
namespace connector
{
namespace tcp
{
// import into namespace
using ::std::array;
using ::std::min;
using ::std::ostringstream;
using ::std::string;
using ::std::tuple;

/**
 * @brief a circular buffer class to store data as uint8_t
 * 
 * @tparam _Capacity 
 */
template <size_t _Capacity> class CircularBuffer
{
    public:
    /**
     * @brief Construct a new Circular Buffer
     * 
     */
    CircularBuffer(void) noexcept : m_buffer(), m_readBuffer() 
    {
        m_buffer.fill(0);
        m_readBuffer.fill(0);
     }

    /**
     * @brief Destroy the Circular Buffer 
     * 
     */
    ~CircularBuffer() {}

    /**
     * @brief get the size of the buffer content
     * 
     * @return size_t 
     */
    size_t size(void) const noexcept { return m_contentSize; }

    /**
     * @brief get the max size of the buffer
     * 
     * @return size_t max size
     */
    size_t max_size(void) const noexcept { return _Capacity; }

    /**
     * @brief get the available space in buffer
     * 
     * @return size_t available space
     */
    size_t availableSpace(void) const noexcept { return m_freeSpace; }

    /**
     * @brief Get if the buffer Is Empty
     * 
     * @return true yes
     * @return false no
     */
    bool isEmpty(void) const noexcept { return m_emptyFlag; }

    /**
     * @brief debug method to get a buffer content representation and some debug info as a string
     * 
     * @return string 
     */
    string displayBuffer(void) noexcept
    {
        readData();
        ostringstream buff;

        buff << "\nbufferRaw : ";
        for (uint8_t byte : m_buffer)
            if (byte == 0)
                buff << '_';
            else
                buff << static_cast<char>(byte);

        buff << "\n            ";
        for (size_t i = 0; i < _Capacity; i++)
            if (i == m_readerIndex || i == m_writerIndex)
                buff << '^';
            else
                buff << ' ';

        buff << "\n            ";
        if (m_readerIndex == m_writerIndex)
            buff << "W/R point same byte (buffer is empty or full)";
        else
            for (size_t i = 0; i < _Capacity; i++)

                if (i == m_readerIndex)
                    buff << 'R';
                else if (i == m_writerIndex)
                    buff << 'W';
                else
                    buff << ' ';

        buff << "\n\nbufferRead: ";
        for (uint8_t byte : m_readBuffer)
            if (byte == 0)
                buff << '_';
            else
                buff << static_cast<char>(byte);
        buff << '\n';

        return buff.str();
    }

    /**
     * @brief write in buffer
     * 
     * @param data data to write
     * @param size data size
     * @return true data written
     * @return false nothing written
     */
    bool writeData(const uint8_t *data, size_t size) noexcept
    {
        if (m_freeSpace < size || size < 1)
            return false;

        // aligned section: writer to reader
        if (m_readerIndex > m_writerIndex)
        {
            // [#W_______R#####]
            memcpy(&m_buffer[m_writerIndex], data, size);
            m_writerIndex += size;
            m_writerIndex = m_writerIndex % _Capacity;
            m_contentSize += size;
            m_freeSpace -= size;
            assert(m_writerIndex < _Capacity);
            // [#####W___R#####]
        }

        // aligned section: writer to end
        else
        {
            // [___R####W_____]
            assert(m_writerIndex >= 0);
            assert(m_writerIndex <= _Capacity);
            const size_t spaceAtEnd   = _Capacity - m_writerIndex;
            const size_t toWriteAtEnd = min(size, spaceAtEnd);

            if (toWriteAtEnd > 0)
            {
                memcpy(&m_buffer[m_writerIndex], data, toWriteAtEnd);
                m_writerIndex += toWriteAtEnd;
                m_writerIndex = m_writerIndex % _Capacity;
                assert(m_writerIndex <= _Capacity);
                assert(toWriteAtEnd <= m_freeSpace);
                m_contentSize += toWriteAtEnd;
                m_freeSpace -= toWriteAtEnd;
            }
            // [___R#######W__]

            // circular section: begin to Reader
            if (toWriteAtEnd < size)
            {
                // [___R#########W]
                const size_t toWriteAtBegin = size - toWriteAtEnd;
                memcpy(&m_buffer[0], data + toWriteAtEnd, toWriteAtBegin);
                m_writerIndex = toWriteAtBegin;
                m_writerIndex = m_writerIndex % _Capacity;
                m_contentSize += toWriteAtBegin;
                m_freeSpace -= toWriteAtBegin;
                assert(m_writerIndex < _Capacity);
                // [##W_R#########]
            }
        }
        m_emptyFlag   = false;
        m_newDataFlag = true;
        return true;
    }

    /*
     * @brief Get the Data in the buffer
     * if the buffer has changed since last read
     * copy the data from circular buffer to a contiguous buffer
     * the return the contiguous buffer 
     * 
     * @return tuple<const uint8_t *, size_t> the buffer as a tuple of data pointer and the size of the data
     */
    const tuple<uint8_t *, size_t> readData(void) noexcept
    {
        if (m_newDataFlag)
        {
            copyData(&m_readBuffer[0]);
            m_newDataFlag = false;
        }
        return std::make_tuple(&m_readBuffer[0], m_contentSize);
    }

    /**
     * @brief remove data
     * move the read pointer in the circular buffer and set sizes
     * 
     * @param size the size of data to remove
     */
    void shiftBuffer(const size_t size) noexcept
    {
        if (size == 0 || m_emptyFlag)
            return;

        if (size == m_contentSize)
        {
            clear();
            return;
        }

        if (m_readerIndex < m_writerIndex)
        {
            // aligned section: reader -> writer
            assert(size <= m_writerIndex);

            memset(&m_buffer[0], 0, size);
            m_readerIndex += size;
            assert(m_readerIndex <= _Capacity);
            m_contentSize -= size;
            m_freeSpace += size;
        }

        else
        {
            // aligned section: reader -> end
            const size_t spaceAtEnd  = _Capacity - m_readerIndex;
            const size_t toReadAtEnd = min(size, spaceAtEnd);
            memset(&m_buffer[m_readerIndex], 0, toReadAtEnd);
            m_readerIndex += toReadAtEnd;
            assert(m_readerIndex <= _Capacity);
            m_contentSize -= toReadAtEnd;
            m_freeSpace += toReadAtEnd;
            if (toReadAtEnd != size)
            {
                // circular section: begin -> writer
                const size_t toReadAtBegin = size - toReadAtEnd;
                memset(&m_buffer[0], 0, toReadAtBegin);
                m_readerIndex              = toReadAtBegin;
                assert(m_readerIndex <= _Capacity);
                m_contentSize -= toReadAtBegin;
                m_freeSpace += toReadAtBegin;
            }
        }
        if (m_readerIndex == m_writerIndex && m_freeSpace == _Capacity)
        {
            // no data left in buffer, set indexes to 0
            clear();
        }
        m_newDataFlag = true;
        memset(&m_readBuffer[0], 0, _Capacity);
    }

    /**
     * @brief empty the buffer
     * move the read and write cursor to begining and set sizes
     * 
     */
    void clear(void) noexcept
    {
        m_freeSpace   = _Capacity;
        m_readerIndex = 0;
        m_writerIndex = 0;
        m_contentSize = 0;
        m_emptyFlag   = true;
        m_newDataFlag = true;
        memset(&m_readBuffer[0], 0, _Capacity);
        memset(&m_buffer[0], 0, _Capacity);
    }

    private:
    /**
     * @brief copy data from the buffer to a contiguous buffer
     * 
     * @param data 
     */
    void copyData(uint8_t *data) noexcept
    {
        if (m_emptyFlag)
            return;
        size_t size = m_contentSize;
        // aligned section
        if (m_readerIndex < m_writerIndex)
            memcpy(data, &m_buffer[m_readerIndex], size);
        else
        {
            // aligned section
            assert(m_readerIndex >= 0);
            assert(m_readerIndex <= _Capacity);
            const size_t spaceAtEnd  = _Capacity - m_readerIndex;
            const size_t toReadAtEnd = min(size, spaceAtEnd);
            memcpy(data, &m_buffer[m_readerIndex], toReadAtEnd);
            if (toReadAtEnd != size)
            {
                // circular section
                const size_t toReadAtBegin = size - toReadAtEnd;
                memcpy(data + toReadAtEnd, &m_buffer[0], toReadAtBegin);
            }
        }
    }

    using Buffer = array<uint8_t, _Capacity>;

    Buffer   m_buffer;
    Buffer   m_readBuffer;
    size_t   m_freeSpace   = _Capacity;
    size_t   m_contentSize = 0;
    uint64_t m_writerIndex = 0;
    uint64_t m_readerIndex = 0;
    bool     m_emptyFlag   = true;
    bool     m_newDataFlag = false;
};
} // namespace tcp
} // namespace connector
} // namespace tredzone
