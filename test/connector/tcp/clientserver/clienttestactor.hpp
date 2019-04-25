/**
 * @file clienttestactor.cpp
 * @brief this class is the manager that start NetworkClient,
 * TcpClient, Handler and Codec (client side) and help them
 * communicate.
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include "client/clienttestbase.hpp"
#include "clientserver/eventandservicetag.hpp"
#include "trz/connector/tcpconnector.hpp"

namespace tredzone
{
namespace connector
{
namespace tcp
{

template <class _TNetwork, size_t _SendBufferSize, size_t _ReceiveBufferSize>
class ClientTestActor : public ClientTestBase<_TNetwork, _SendBufferSize, _ReceiveBufferSize>
{
    using parent = ClientTestBase<_TNetwork, _SendBufferSize, _ReceiveBufferSize>;

    public:
    class ConnectParam : public IClient<_TNetwork>::ConnectParam
    {
    };

    ClientTestActor(uint8_t onNewMessageToReadExpected, uint8_t onConnectExpected, uint8_t onConnectFailedExpected,
                    uint8_t onConnectTimedOutExpected, uint8_t onConnectionLostExpected, uint8_t onDisconnectExpected,
                    uint8_t onDataReceivedExpectedVal, uint8_t onDataSizeReceivedExpectedVal,
                    uint8_t onOverflowDataReceivedExpectedVal, uint8_t onOverflowDataSizeReceivedExpectedVal)
        : parent(onNewMessageToReadExpected, onConnectExpected, onConnectFailedExpected, onConnectTimedOutExpected,
                 onConnectionLostExpected, onDisconnectExpected, onDataReceivedExpectedVal,
                 onDataSizeReceivedExpectedVal, onOverflowDataReceivedExpectedVal,
                 onOverflowDataSizeReceivedExpectedVal),
          m_pipe(*this, parent::getEngine().getServiceIndex().template getServiceActorId<ValidatorService>()),
          m_testStarter(*this)
    {
    }

    ~ClientTestActor(void) {}

    void validateTest()
    {
        m_pipe.push<ClientSideSuccessEvent>();
        parent::disconnect();
    }

    void invalidateTest()
    {
        m_pipe.push<ClientSideFaillureEvent>();
        parent::disconnect();
    }

    virtual void startTest() noexcept {}

private:
    /**
     * @brief instantiates all network actors before starting test
     *
     */
    class TestStarter : public Actor::Callback
    {
        public:
        ClientTestActor &myactor;
        TestStarter(ClientTestActor &actor) : myactor(actor) { myactor.registerCallback(*this); }

        void onCallback(void) { myactor.startTest(); }
    };

    Actor::Event::Pipe m_pipe;
    TestStarter        m_testStarter;
};
} // namespace tcp
} // namespace connector
} // namespace tredzone