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

namespace tredzone
{
namespace connector
{
namespace tcp
{
namespace ConnectionSuccess
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
    const uint8_t m_onDataReceivedExpectedVal             = 0u;
    const uint8_t m_onDataSizeReceivedExpectedVal         = 0u;
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
                                m_onOverflowDataReceivedExpectedVal, m_onOverflowDataSizeReceivedExpectedVal)
    {
        setConnectParam(param.m_ipAddress, param.m_addressFamily, param.m_port, param.m_timeoutUSec,
                        param.m_messageHeaderSize, param.m_ipAddressSource);
    }

    // doing it in callback (AKA next loop) to avoid race condition :let time to the server to listen
    void startTest() noexcept override { registerConnect(); }

    void onConnectTest() noexcept override
    {
        disconnect();
        validateTest();
    }
};

} // namespace ConnectionSuccess
} // namespace tcp
} // namespace connector
} // namespace tredzone
