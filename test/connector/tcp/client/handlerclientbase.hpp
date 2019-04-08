/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file base.hpp
 * @brief standard client handler for test.
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once


#include "trz/connector/tcpconnector.hpp"
#include "trz/connector/tcp/client/iclient.hpp"
#include "trz/connector/tcp/common/network.hpp"
#include "common/networkcallsmock.hpp"
#include "common/testvariables.hpp"
#include "gtest/gtest.h"
#include <cstdint>

namespace tredzone
{

//using _Network = Network<255, 255, 255, 255, NetworkCallsMock>;

template <class _Network>
struct ClientTestBase
{
    using _Client = tredzone::connector::tcp::IClient<_Network>;

    uint8_t m_onConnect                     = 0;
    uint8_t m_onConnectFailed               = 0;
    uint8_t m_onConnectTimedOut             = 0;
    uint8_t m_onConnectionLost              = 0;
    uint8_t m_onDisconnect                  = 0;
    uint8_t m_onDataReceived                = 0;
    uint8_t m_onOverflowDataReceived = 0;

    uint8_t m_onConnectExpected                     = 0;
    uint8_t m_onConnectFailedExpected               = 0;
    uint8_t m_onConnectTimedOutExpected             = 0;
    uint8_t m_onConnectionLostExpected              = 0;
    uint8_t m_onDisconnectExpected                  = 0;
    uint8_t m_onDataReceivedExpected                = 0;
    uint8_t m_onOverflowDataReceivedExpected = 0;

    HandlerClientBase(uint8_t onConnectExpected,
                uint8_t onConnectFailedExpected,
                uint8_t onConnectTimedOutExpected,
                uint8_t onConnectionLostExpected,
                uint8_t onDisconnectExpected,
                uint8_t onDataReceivedExpected,
                uint8_t onOverflowDataReceivedExpected)
        : m_onConnectExpected(onConnectExpected),
          m_onConnectFailedExpected(
              onConnectFailedExpected),
          m_onConnectTimedOutExpected(
              onConnectTimedOutExpected),
          m_onConnectionLostExpected(
              onConnectionLostExpected),
          m_onDisconnectExpected(onDisconnectExpected),
          m_onDataReceivedExpected(
              onDataReceivedExpected),
          m_onOverflowDataReceivedExpected(
              onOverflowDataReceivedExpected)
    {
    }
    
    virtual void onConnect(std::shared_ptr<_Client> client)
    {
        (void)client;
        ++m_onConnect;
    }

    virtual void onConnectFailed(std::shared_ptr<_Client> client)
    {
        (void)client;
        ++m_onConnectFailed;
    }

    virtual void
    onConnectTimedOut(std::shared_ptr<_Client> client)
    {
        (void)client;
        ++m_onConnectTimedOut;
    }

    virtual void
    onConnectionLost(std::shared_ptr<_Client> client)
    {
        (void)client;
        ++m_onConnectionLost;
    }

    virtual void onDisconnect(std::shared_ptr<_Client> client)
    {
        (void)client;
        ++m_onDisconnect;
    }

    virtual void onDataReceived(std::shared_ptr<_Client> client,
                                const uint8_t * /*data*/,
                                size_t /*dataQuantity*/)
    {
        (void)client;
        ++m_onDataReceived;
    }

    virtual void onOverflowDataReceived(
        std::shared_ptr<_Client> client,
        const uint8_t * /*data*/, size_t /*dataQuantity*/)
    {
        (void)client;
        ++m_onOverflowDataReceived;
    }

    virtual void beforeReleaseClient(std::shared_ptr<_Client> client)
    {
        (void)client;
    }

    ~HandlerClientBase() { check(); }
    void check()
    {
        ASSERT_EQ(m_onConnectExpected, m_onConnect);
        ASSERT_EQ(m_onDisconnectExpected, m_onDisconnect);
        ASSERT_EQ(m_onConnectFailedExpected,
                  m_onConnectFailed);
        ASSERT_EQ(m_onConnectTimedOutExpected,
                  m_onConnectTimedOut);
        ASSERT_EQ(m_onDataReceivedExpected,
                  m_onDataReceived);
        ASSERT_EQ(m_onOverflowDataReceivedExpected,
                  m_onOverflowDataReceived);
        ASSERT_EQ(m_onConnectionLostExpected,
                  m_onConnectionLost);
    }
};
} // namespace tredzone