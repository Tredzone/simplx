/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file receiver.hpp
 * @brief receive buffer
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include "trz/connector/tcp/client/circularbuffer.hpp"
#include "trz/connector/tcp/client/iclient.hpp"

#include "simplx.h"

#include <exception>
#include <iostream>

namespace tredzone
{
namespace connector
{
namespace tcp
{
// import into namespace
using ::std::cout;
using ::std::endl;
using ::std::get;
using ::std::min;
using ::std::runtime_error;

template <typename _TClient, size_t _Capacity> class Receiver
{
    static_assert((_Capacity > 8), "Receive buffer can't be lower than 8 bytes");

    enum class ORIGIN_T
    {
        BUFFER,
        NEWMESSAGE
    };

    public:
    /**
     * @brief Construct a new Receiver object
     * 
     * @param client 
     */
    Receiver(_TClient &client) noexcept(false) : m_client(client), m_buffer() {}

    /**
     * @brief Destroy the Receiver object
     * 
     */
    ~Receiver() {}

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
     * @brief process incoming data when the buffer is not empty
     * complete the buffer with the missing part of the data (according to size read in header)
     * 
     * @param data the incoming data to add
     * @param size the size of the incoming data
     */
    void doCompleteMsg(const uint8_t *data, size_t size) noexcept
    {
        // buffer does not contains a complete minimum header
        if (m_buffer.size() < m_messageHeaderMinimumSize) 
        {
            // incoming part can complete the minimum header
            if ((size + m_buffer.size()) > m_messageHeaderMinimumSize) 
            {
                // complete the minimum header then process the rest of the data
                const size_t quantityDataToCompleteHeader = m_messageHeaderMinimumSize - m_buffer.size();
                m_buffer.writeData(data, quantityDataToCompleteHeader);
                data += quantityDataToCompleteHeader;
                size -= quantityDataToCompleteHeader;
            }
            // incoming part can NOT complete the minimum header
            else 
            {
                // add the incoming part to the buffer then ends there
                m_buffer.writeData(data, size); 
                return;
            }
        }
        //get from the minimum header the size of the current header
        size_t currentHeaderSize = m_client.getHeaderSize(get<0>(m_buffer.readData()));

        // buffer does not contains the complete current header
        if (m_buffer.size() < currentHeaderSize) 
        {
            const size_t quantityDataToCompleteHeader = currentHeaderSize - m_buffer.size();

            // incoming part can complete the current header
            if (size > quantityDataToCompleteHeader)
            {
                // complete the current header then process the rest of the data
                m_buffer.writeData(data, quantityDataToCompleteHeader);
                data += quantityDataToCompleteHeader;
                size -= quantityDataToCompleteHeader;
            }
            // incoming part can NOT complete the current header
            else
            {
                // add the incoming part to the buffer then ends there
                m_buffer.writeData(data, size);
                return;
            }
        }

        uint64_t dataSize = m_client.getDataSize(get<0>(m_buffer.readData()), currentHeaderSize);

        size_t msgSize = currentHeaderSize + dataSize;

        // error : buffer too small for data size
        if (msgSize > m_buffer.max_size())
        {
            // process all content to onOverflowDataReceived
            const uint8_t *dataToDrop = get<0>(m_buffer.readData());
            size_t dataSizeToDrop = m_buffer.size();
            size_t remainingData = dropData(dataToDrop/*by ref*/, dataSizeToDrop/*by ref*/);
            assert(remainingData == 0);
            (void)remainingData;
            m_overflowSize = msgSize - m_buffer.size();
            m_buffer.clear();
            processData(data, size, ORIGIN_T::BUFFER);
            return;
        }

        // incomplete msg : keep fragment in buffer
        if (msgSize > (m_buffer.size() + size))
        {
            m_buffer.writeData(data, size);
            return;
        }

        // buffer big enough to store data then process it
        if (m_buffer.availableSpace() > size)
        {
            m_buffer.writeData(data, size);
            const size_t   initialSize = m_buffer.size();
            size_t         bufferSize  = m_buffer.size();
            const uint8_t *bufferMsg   = get<0>(m_buffer.readData());
            processData(bufferMsg, bufferSize/*by ref*/, ORIGIN_T::BUFFER);
            m_buffer.shiftBuffer(initialSize - bufferSize);
            return;
        }

        // complete buffer with data regarding dataSize
        const int sizeToComplete = dataSize - m_buffer.size();
        m_buffer.writeData(data, sizeToComplete);
        data += sizeToComplete;
        size -= sizeToComplete;

        // processing buffer 
        size_t         bufferSize = m_buffer.size();
        const uint8_t *bufferMsg  = std::get<0>(m_buffer.readData());
        setOtherCompleteMessageFlag(data, size);
        processData(bufferMsg, bufferSize /*by ref*/, ORIGIN_T::BUFFER);
        m_buffer.clear();

        // processing the rest of the incoming data
        processData(data, size/*by ref*/, ORIGIN_T::NEWMESSAGE);
    }

    /**
     * @brief process incoming data when buffer is empty
     * 
     * @param data the incomming data
     * @param size the size 
     * @param origin 
     */
    void processData(const uint8_t *data, size_t &size, const ORIGIN_T origin) noexcept
    {
        // the incoming data are part of data bloc too big for the buffer
        if (m_overflowSize > 0 && !dropData(data/*by ref*/, size/*by ref*/)) 
            // after sending all the overflowing data bloc no incoming data is left 
            return;

        // the incoming data is smaller than the minimum header size
        if (size < m_messageHeaderMinimumSize)
        {
            // add data to buffer 
            m_buffer.writeData(data, size);
            return;
        }
        // process all message in data 
        while (size >= m_messageHeaderMinimumSize)
        {
            size_t currentHeaderSize = m_client.getHeaderSize(data);

            // no header size in header : sending data directly
            if (currentHeaderSize == 0)
            {
                m_otherCompleteMessageFlag = false;
                m_client.onDataReceived(data, size);
                return;
            }

            // the current header is not complete
            if (size < currentHeaderSize)
            {
                m_buffer.writeData(data, size);
                return;
            }

            const uint64_t dataSize = m_client.getDataSize(data, currentHeaderSize);

            const uint64_t msgSize = currentHeaderSize + dataSize;

            // no data size in header : sending data directly
            if (dataSize == 0)
            {
                m_otherCompleteMessageFlag = false;
                m_client.onDataReceived(data, size);
                return;
            }

            // error : buffer to small for data length
            if (msgSize > m_buffer.max_size())
            {
                // process content to onOverflowDataReceived
                m_overflowSize = msgSize - min((int64_t)size, (int64_t)msgSize);
                processData(data, size/*by ref*/, origin);
                return;
            }

            // incomplete msg : keep fragment in buffer
            if (msgSize > size)
            {
                if (ORIGIN_T::NEWMESSAGE == origin)
                {
                    m_buffer.writeData(data, size);
                    return;
                }
                else
                {
                    return;
                }
            }

            // data complete : sending to handler
            size -= currentHeaderSize + dataSize;
            setOtherCompleteMessageFlag(data + currentHeaderSize + dataSize, size);
            m_client.onDataReceived(data, currentHeaderSize + dataSize);
            data += currentHeaderSize + dataSize;
        }
    }

    /**
     * @brief Set a Flag to true if an other message is available after treating current message
     * the flag is set before calling the client callback therefor in onDataReceived client can check if it is the last message of the incomings data
     * 
     * @param data the data left in incoming data
     * @param size the size of the data left in incoming data
     */
    void setOtherCompleteMessageFlag(const uint8_t *data, size_t size)
    {
        // remaining data are incomplete for minimum header or minimum header size is 0
        if (size < m_messageHeaderMinimumSize || m_messageHeaderMinimumSize == 0)
        {
            m_otherCompleteMessageFlag = false;
            return;
        }
        const size_t currentHeaderSize = m_client.getHeaderSize(data);
        // remaining data are incomplete for current header or current header size is 0
        if (size < currentHeaderSize || currentHeaderSize == 0)
        {
            m_otherCompleteMessageFlag = false;
            return;
        }
        const uint64_t dataSize = m_client.getDataSize(data, currentHeaderSize);
        // remaining data are incomplete
        if (size < (currentHeaderSize + dataSize) || dataSize == 0)
        {
            m_otherCompleteMessageFlag = false;
            return;
        }
        // an other message is complete
        m_otherCompleteMessageFlag = true;
    }

    /**
     * @brief Get flag is Other Complete Message 
     * 
     * @return true yes
     * @return false no
     */
    bool isOtherCompleteMessage() const { return m_otherCompleteMessageFlag; }


    /**
     * @brief send data without buffering it due to buffer max size to small 
     * 
     * @param data to send
     * @param dataSize size of data to send
     * @return true remaining data to process
     * @return false NO data remaining
     */
    bool dropData(const uint8_t *&data, size_t &dataSize) noexcept
    {
        size_t droppingData = min((int64_t)dataSize, (int64_t)m_overflowSize);

        m_overflowSize -= droppingData;
        m_client.onOverflowDataReceived(data, droppingData);

        data += droppingData;
        dataSize -= droppingData;
        return m_overflowSize == 0;
    }

    /**
     * @brief Get if there Is Previous Incomplete Msg object
     * 
     * @return true yes
     * @return false no
     */
    bool isPreviousIncompleteMsg(void) noexcept { return !m_buffer.isEmpty(); }

    /**
     * @brief Set the minimum Header Size 
     * 
     * @param messageHeaderSize the new minimum header size 
     */
    void setHeaderSize(size_t messageHeaderSize) { m_messageHeaderMinimumSize = messageHeaderSize; }

    /**
     * @brief callback called when new data just been received from the client socket
     * 
     * @param data the data received
     * @param dataSize the size of data received
     */
    void onDataReceived(const uint8_t *data, size_t dataSize) noexcept
    {
        if (dataSize == 0)
        {
            return;
        }
        if (m_messageHeaderMinimumSize == 0)
        {
            m_otherCompleteMessageFlag = false;
            m_client.onDataReceived(data, dataSize);
            return;
        }

        if (isPreviousIncompleteMsg())
            doCompleteMsg(data, dataSize);
        else
            processData(data, dataSize, ORIGIN_T::NEWMESSAGE);
    }

    private:
    _TClient &                m_client;
    CircularBuffer<_Capacity> m_buffer;
    size_t                    m_overflowSize             = 0;
    size_t                    m_messageHeaderMinimumSize = 0;
    bool                      m_otherCompleteMessageFlag = false;
};
} // namespace tcp
} // namespace connector
} // namespace tredzone
