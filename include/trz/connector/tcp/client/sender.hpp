/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file sender.hpp
 * @brief send buffer
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include "trz/connector/tcp/client/circularbuffer.hpp"

#include <cstdint>
#include <tuple>

namespace tredzone
{
namespace connector
{
namespace tcp
{

// import into namespace
using ::std::tuple;

template <size_t _Capacity> class Sender
{
    public:
    /**
     * @brief Construct a new Sender object
     * 
     */
    Sender(void) noexcept : m_buffer() {}

    /**
     * @brief Destroy the Sender object
     * 
     */
    ~Sender() {}

    /**
     * @brief Get the Buffer Content Size
     * 
     * @return size_t 
     */
    size_t getBufferContentSize(void) const noexcept { return m_buffer.size(); }

    /**
     * @brief empty the buffer
     * 
     */
    void clearBuffer(void) noexcept { m_buffer.clear(); }

    /**
     * @brief write add data to buffer
     * 
     * @param data 
     * @param dataSize 
     * @return true data was added
     * @return false no data added
     */
    bool addToBuffer(const uint8_t *data, const size_t dataSize) noexcept
    {
        return m_buffer.writeData(data, dataSize);
    }

    /**
     * @brief Get the buffer content
     * 
     * @return tuple<const uint8_t *, size_t> the buffer as a tuple of dataPtr and data size
     */
    tuple<const uint8_t *, size_t> getDataToSend(void) noexcept { return m_buffer.readData(); }

    /**
     * @brief clear a part of the buffer
     * 
     * @param dataSize the data size to remove
     * @return size_t remaining data size
     */
    size_t shiftBuffer(size_t dataSize) noexcept
    {
        m_buffer.shiftBuffer(dataSize);
        return m_buffer.size();
    }

    private:
    CircularBuffer<_Capacity> m_buffer;
};
} // namespace tcp
} // namespace connector
} // namespace tredzone