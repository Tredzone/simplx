#pragma once
/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file servertestactor.cpp
 * @brief this class is the manager that start Network,
 * TcpServer, Handlers and Codec (server side) and help them
 * communicate.
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include "clientserver/eventandservicetag.hpp"
#include "server/servertestbase.hpp"
#include "trz/connector/tcp/server/iserver.hpp"

namespace tredzone
{
namespace connector
{
namespace tcp
{

template <class _TNetwork, class _TServerProcess>
class ServerTestActor : public ServerTestBase<_TNetwork, _TServerProcess>
{
    using parent = ServerTestBase<_TNetwork, _TServerProcess>;

    public:
    class ServerParam : public IServer<_TNetwork>::ListenParam
    {
        public:
        size_t m_messageHeaderSize;
    };

    ServerTestActor(uint8_t onNewConnectionExpected, uint8_t onServerProcessDestroyExpected,
                    uint8_t onListenFailedExpected, uint8_t onListenSucceedExpected, uint8_t onAcceptFailedExpected,
                    uint8_t onListenStopped)
        : parent(onNewConnectionExpected, onServerProcessDestroyExpected, onListenFailedExpected,
                 onListenSucceedExpected, onAcceptFailedExpected, onListenStopped),
          m_pipe(*this, parent::getEngine().getServiceIndex().template getServiceActorId<ValidatorService>()),
          m_testStarter(*this)
    {
    }

    ~ServerTestActor(void) {}

    void validateTest()
    {
        m_pipe.push<ServerSideSuccessEvent>();
        parent::stopListening();
    }

    void invalidateTest()
    {
        m_pipe.push<ServerSideFaillureEvent>();
        parent::stopListening();
    }

    virtual void startTest() noexcept {}

    private:
    /**
     * @brief callback to let all networks actors start before start it
     *
     */
    class TestStarter : public Actor::Callback
    {
        public:
        ServerTestActor &myactor;
        TestStarter(ServerTestActor &actor) : myactor(actor) { myactor.registerCallback(*this); }

        void onCallback(void) { myactor.startTest(); }
    };

    private:
    Actor::Event::Pipe m_pipe;
    TestStarter        m_testStarter;
};
} // namespace tcp
} // namespace connector
} // namespace tredzone