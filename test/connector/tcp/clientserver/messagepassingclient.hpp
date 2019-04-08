/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file connectthendisconnect.hpp
 * @brief test a connection then a disconnection.
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include "clientserver/clienttestactor.hpp"
#include "trz/connector/tcpconnector.hpp"

// #include "trz/connectors/tcp/client/iclient.hpp"
// #include "trz/connectors/tcp/common/networkcalls.hpp"
// #include "test/client/handlerclientbase.hpp"
// #include "test/clientserver/clienttestactor.hpp"
// #include "test/common/testcodec.hpp"

namespace tredzone
{
namespace connector
{
namespace tcp
{
namespace MessagePassing
{
class ClientValue
{
    protected:
    const uint8_t m_onNewMessageToReadExpectedVal         = 0u;
    const uint8_t m_onConnectExpectedVal                  = 1u;
    const uint8_t m_onConnectFailedExpectedVal            = 0u;
    const uint8_t m_onConnectTimedOutExpectedVal          = 0u;
    const uint8_t m_onConnectionLostExpectedVal           = 0u;
    const uint8_t m_onDisconnectExpectedVal               = 1u;
    const uint8_t m_onDataReceivedExpectedVal             = 5u;
    const uint8_t m_onDataSizeReceivedExpectedVal         = 60u;
    const uint8_t m_onOverflowDataReceivedExpectedVal     = 0u;
    const uint8_t m_onOverflowDataSizeReceivedExpectedVal = 0u;
};

using NetworktTest = Network<255, 255, 255, 255>;

class ClientActor : public ClientValue, public ClientTestActor<NetworktTest, 1024, 1024>
{
    using parent = ClientTestActor<NetworktTest, 1024, 1024>;

    public:
    ClientActor(const IClient::ConnectParam &param)
        : ClientValue(), parent(m_onNewMessageToReadExpectedVal, m_onConnectExpectedVal, m_onConnectFailedExpectedVal,
                                m_onConnectTimedOutExpectedVal, m_onConnectionLostExpectedVal,
                                m_onDisconnectExpectedVal, m_onDataReceivedExpectedVal, m_onDataSizeReceivedExpectedVal,
                                m_onOverflowDataReceivedExpectedVal, m_onOverflowDataSizeReceivedExpectedVal),
          m_echoCount(0)
    {
        setConnectParam(param.m_ipAddress, param.m_addressFamily, param.m_port, param.m_timeoutUSec,
                        param.m_messageHeaderSize, param.m_ipAddressSource);
    }

    // doing it in callback (AKA next loop) to avoid race condition :let time to the server to listen
    void startTest() noexcept override { registerConnect(); }

    void onConnectTest() noexcept override
    {
        char   text[]   = "TestEchoing"; // 10 characters including header
        size_t sizeText = sizeof(text) - 1;
        send(static_cast<uint8_t>(sizeText + 1)); // header : size of data header + the payload
        send(text, sizeText);                     // payload
    }

    void onDataReceivedTest(const uint8_t *data, size_t dataSize) noexcept override
    {
        if (++m_echoCount == 5)
            validateTest();
        else
        {
            // sending;
            send(data, dataSize);
        }
    }
    int m_echoCount = 0;
};

} // namespace MessagePassing
} // namespace tcp
} // namespace connector
} // namespace tredzone