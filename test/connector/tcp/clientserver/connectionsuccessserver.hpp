/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file connectionfailure.hpp
 * @brief test that a registered connection correctly fails (unreachable).
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include "clientserver/clienttestactor.hpp"
#include "clientserver/servertestactor.hpp"
#include "trz/connector/tcpconnector.hpp"

namespace tredzone
{

namespace connector
{
namespace tcp
{
namespace ConnectionSuccess
{
using NetworktTest = Network<255, 255, 255, 255>;

class ServerProcessValues
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

class ServerValues
{
    protected:
    const uint8_t m_onNewConnectionExpectedVal        = 1;
    const uint8_t m_onServerProcessDestroyExpectedVal = 1;
    const uint8_t m_onListenFailedExpectedVal         = 0;
    const uint8_t m_onListenSucceedExpectedVal        = 1;
    const uint8_t m_onAcceptFailedExpectedVal         = 0;
    const uint8_t m_onListenStoppedExpectedVal        = 0;
};

class ServerProcessActor : public ServerProcessValues, public ClientTestActor<NetworktTest, 1024, 1024>
{
    using parent = ClientTestActor<NetworktTest, 1024, 1024>;

    public:
    ServerProcessActor()
        : ServerProcessValues(),
          parent(m_onNewMessageToReadExpectedVal, m_onConnectExpectedVal, m_onConnectFailedExpectedVal,
                 m_onConnectTimedOutExpectedVal, m_onConnectionLostExpectedVal, m_onDisconnectExpectedVal,
                 m_onDataReceivedExpectedVal, m_onDataSizeReceivedExpectedVal, m_onOverflowDataReceivedExpectedVal,
                 m_onOverflowDataSizeReceivedExpectedVal)
    {
    }

    void onConnectTest() noexcept override 
    {
        disconnect();
    }
};

class ServerActor : public ServerValues, public ServerTestActor<NetworktTest, ServerProcessActor>
{
    using parent = ServerTestActor<NetworktTest, ServerProcessActor>;

    public:
    ServerActor(const ServerTestActor::ServerParam &param)
        : ServerValues(),
          parent(m_onNewConnectionExpectedVal, m_onServerProcessDestroyExpectedVal, m_onListenFailedExpectedVal,
                 m_onListenSucceedExpectedVal, m_onAcceptFailedExpectedVal, m_onListenStoppedExpectedVal)
    {
        registerListen(param.m_addressFamily, param.m_addressType, param.m_port);
    }

    protected:
    void onServerProcessDestroyTest(const Actor::ActorId &/*serverProcessActorId*/) noexcept override final
    {
        validateTest();
    }
};


} // namespace ConnectionSuccess

} // namespace tcp
} // namespace connector
} // namespace tredzone
