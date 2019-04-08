/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file connectionfailure.hpp
 * @brief test that a registered connection correctly fails (unreachable).
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include "trz/connectors/tcp/server/server.hpp"
#include "test/client/handlerclientbase.hpp"
#include "test/common/networkcallsmock.hpp"
#include "test/common/testcodec.hpp"
#include "test/server/handlerserverbase.hpp"
#include "test/server/testactor.hpp"

namespace tredzone
{

namespace ListenSucceed
{

template <typename _Server, typename _Network>
struct HandlerClient : HandlerClientBase<_Network>
{
    _Server &m_testActor;
    template <typename... ExpectedValues>
    HandlerClient(_Server &testActor, ExpectedValues &&... expected)
        : HandlerClientBase<_Network>(expected...), m_testActor(testActor)
    {
    }
};

template <typename _Server, typename _Network>
struct HandlerServer : HandlerServerBase<_Network>
{
    _Server &m_testActor;
    template <typename... ExpectedValues>
    HandlerServer(_Server &testActor, ExpectedValues &&... expected)
        : HandlerServerBase<_Network>(expected...), m_testActor(testActor)
    {
        NetworkCallsMock::epollFdValue      = 2;
        NetworkCallsMock::listenReturnValue = 1;
        g_expectingTimeout                  = true;
        m_testActor.m_server->registerListen();
    }

    using ServerTest =
        TcpServer<_Network, HandlerServer,
                                 HandlerClient<_Server, _Network>, Codec, 1024,
                                 1024>;
};

using NetworkTest =
    Network<NetworkCallsMock, 255, 255, 255, 255, 255, 255>;

using Actor = TestActor<HandlerServer, HandlerClient, NetworkTest, Codec, 0u,
                        0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 0u, 0u>;

} // namespace ListenSucceed

} // namespace tredzone