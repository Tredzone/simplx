/**
 * @author erian Vives <valerian.vives@tredzone.com>
 * @file client.hpp
 * @brief tcp client
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include "trz/connector/tcpconnector.hpp"

#include "gtest/gtest.h"

#include "simplx.h"

#include <cstdint>

namespace tredzone
{
namespace connector
{
namespace tcp
{

using ::std::size_t;

/**
 * @brief class to test client.
 *
 * @tparam _TNetwork the network actor that manages the connection and communication to the network (low level)
 * @tparam _SendBufferSize
 * @tparam _ReceiveBufferSize
 */

template <class _TNetwork, size_t _SendBufferSize, size_t _ReceiveBufferSize>
class ClientTestBase : public TcpClient<_TNetwork, _SendBufferSize, _ReceiveBufferSize>
{
    uint8_t m_onNewMessageToRead         = 0;
    uint8_t m_onConnect                  = 0;
    uint8_t m_onConnectFailed            = 0;
    uint8_t m_onConnectTimedOut          = 0;
    uint8_t m_onConnectionLost           = 0;
    uint8_t m_onDisconnect               = 0;
    uint8_t m_onDataReceived             = 0;
    uint8_t m_onDataSizeReceived         = 0;
    uint8_t m_onOverflowDataReceived     = 0;
    uint8_t m_onOverflowDataSizeReceived = 0;

    uint8_t m_onNewMessageToReadExpected         = 0;
    uint8_t m_onConnectExpected                  = 0;
    uint8_t m_onConnectFailedExpected            = 0;
    uint8_t m_onConnectTimedOutExpected          = 0;
    uint8_t m_onConnectionLostExpected           = 0;
    uint8_t m_onDisconnectExpected               = 0;
    uint8_t m_onDataReceivedExpected             = 0;
    uint8_t m_onDataSizeReceivedExpected         = 0;
    uint8_t m_onOverflowDataReceivedExpected     = 0;
    uint8_t m_onOverflowDataSizeReceivedExpected = 0;

    using parent = TcpClient<_TNetwork, _SendBufferSize, _ReceiveBufferSize>;

    public:
    ClientTestBase(uint8_t onNewMessageToReadExpected, uint8_t onConnectExpected, uint8_t onConnectFailedExpected,
                   uint8_t onConnectTimedOutExpected, uint8_t onConnectionLostExpected, uint8_t onDisconnectExpected,
                   uint8_t onDataReceivedExpected, uint8_t onDataSizeReceivedExpected,
                   uint8_t onOverflowDataReceivedExpected, uint8_t onOverflowDataSizeReceivedExpected)
        : m_onNewMessageToReadExpected(onNewMessageToReadExpected), m_onConnectExpected(onConnectExpected),
          m_onConnectFailedExpected(onConnectFailedExpected), m_onConnectTimedOutExpected(onConnectTimedOutExpected),
          m_onConnectionLostExpected(onConnectionLostExpected), m_onDisconnectExpected(onDisconnectExpected),
          m_onDataReceivedExpected(onDataReceivedExpected), m_onDataSizeReceivedExpected(onDataSizeReceivedExpected),
          m_onOverflowDataReceivedExpected(onOverflowDataReceivedExpected),
          m_onOverflowDataSizeReceivedExpected(onOverflowDataSizeReceivedExpected)
    {
    }

    ~ClientTestBase() { check(); }
    void check()
    {
        ASSERT_EQ(m_onNewMessageToReadExpected, m_onNewMessageToRead);
        ASSERT_EQ(m_onConnectExpected, m_onConnect);
        ASSERT_EQ(m_onDisconnectExpected, m_onDisconnect);
        ASSERT_EQ(m_onConnectFailedExpected, m_onConnectFailed);
        ASSERT_EQ(m_onConnectTimedOutExpected, m_onConnectTimedOut);
        ASSERT_EQ(m_onConnectionLostExpected, m_onConnectionLost);
        ASSERT_EQ(m_onDataReceivedExpected, m_onDataReceived);
        ASSERT_EQ(m_onDataSizeReceivedExpected, m_onDataSizeReceived);
        ASSERT_EQ(m_onOverflowDataReceivedExpected, m_onOverflowDataReceived);
        ASSERT_EQ(m_onOverflowDataSizeReceivedExpected, m_onOverflowDataSizeReceived);
    }

    protected:
    void onNewMessageToRead(void) noexcept override final
    {
#if VERBOSE_TEST > 0
        std::cout << parent::getActorId() << " onNewMessageToRead()" << std::endl;
#endif
        m_onNewMessageToRead++;
        onNewMessageToReadTest();
    }

    void onConnectFailed(void) noexcept override final
    {
#if VERBOSE_TEST > 0
        std::cout << parent::getActorId() << " onConnectFailed()" << std::endl;
#endif
        m_onConnectFailed++;
        onConnectFailedTest();
    }

    void onConnectTimedOut(void) noexcept override final
    {
#if VERBOSE_TEST > 0
        std::cout << parent::getActorId() << " onConnectTimedOut()" << std::endl;
#endif
        m_onConnectTimedOut++;
        onConnectTimedOutTest();
    }

    void onConnectionLost(void) noexcept override final
    {
#if VERBOSE_TEST > 0
        std::cout << parent::getActorId() << " onConnectionLost()" << std::endl;
#endif
        m_onConnectionLost++;
        onConnectionLostTest();
    }

    void onConnect(void) noexcept override final
    {
#if VERBOSE_TEST > 0
        std::cout << parent::getActorId() << " onConnect()" << std::endl;
#endif
        m_onConnect++;
        onConnectTest();
    }

    void onDisconnect(void) noexcept override final
    {
#if VERBOSE_TEST > 0
        std::cout << parent::getActorId() << " onDisconnect()" << std::endl;
#endif
        m_onDisconnect++;
        onDisconnectTest();
    }

    void onDataReceived(const uint8_t *data, size_t dataSize) noexcept override final
    {
#if VERBOSE_TEST > 0
        std::cout << parent::getActorId() << " onDataReceived()" << std::endl;
        std::cout << parent::getActorId() << " receiving : " << reinterpret_cast<const char *>(data) << std::endl;
#endif
        m_onDataReceived++;
        m_onDataSizeReceived += dataSize;
        onDataReceivedTest(data, dataSize);
    }

    void onOverflowDataReceived(const uint8_t *data, size_t dataSize) noexcept override final
    {
#if VERBOSE_TEST > 0
        std::cout << parent::getActorId() << " onOverflowDataReceived()" << std::endl;
        std::cout << parent::getActorId() << " receiving : " << reinterpret_cast<const char *>(data) << std::endl;
#endif
        m_onOverflowDataReceived++;
        m_onOverflowDataSizeReceived += dataSize;
        onOverflowDataReceivedTest(data, dataSize);
    }

    virtual void onNewMessageToReadTest(void) noexcept {}

    virtual void onConnectFailedTest(void) noexcept {}

    virtual void onConnectTimedOutTest(void) noexcept {}

    virtual void onConnectionLostTest(void) noexcept {}

    virtual void onConnectTest(void) noexcept {}

    virtual void onDisconnectTest(void) noexcept {}

    virtual void onDataReceivedTest(const uint8_t * /*data*/, size_t /*dataSize*/) noexcept {}

    virtual void onOverflowDataReceivedTest(const uint8_t * /*data*/, size_t /*dataSize*/) noexcept {}
};
} // namespace tcp
} // namespace connector
} // namespace tredzone
