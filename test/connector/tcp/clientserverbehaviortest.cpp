/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file clientserverbehaviortest.cpp
 * @brief client and server test initializer: run the client and server tests
 * (without mocking the network)
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <cstdint>

#define VERBOSE_TEST 0

#include "clientserver/connectionsuccessclient.hpp"
#include "clientserver/connectionsuccessserver.hpp"
#include "clientserver/messagepassingclient.hpp"
#include "clientserver/messagepassingserver.hpp"
#include "clientserver/servertestactor.hpp"
#include "clientserver/validatoractor.hpp"

using namespace std;
using namespace tredzone;
using namespace tredzone::connector::tcp;

atomic_bool engineStartedFlag;

template <typename _ServerTestClassType, typename _ClientTestClassType>
void runTest(const int64_t timeoutMs, const int64_t shouldTimeOut = false)
{
    g_finished         = false;
    g_expectingTimeout = shouldTimeOut;
    g_failOnTimeout    = false;

    typename _ServerTestClassType::ServerParam serverParam;
    serverParam.m_messageHeaderSize = 1;
    serverParam.m_addressFamily     = AF_INET;
    serverParam.m_addressType       = INADDR_ANY;
    serverParam.m_port              = 8080;

    typename _ClientTestClassType::ConnectParam clientParam;
    clientParam.m_messageHeaderSize = 1;
    clientParam.m_ipAddress         = "127.0.0.1";
    clientParam.m_addressFamily     = AF_INET;
    clientParam.m_port              = 8080;
    clientParam.m_timeoutUSec       = 5000000;
    clientParam.m_ipAddressSource   = "";

    Engine::StartSequence startSequence;
    startSequence.addServiceActor<ValidatorService, ValidatorActor>(3, timeoutMs);
    startSequence.addActor<_ServerTestClassType>(1, serverParam);
    startSequence.addActor<_ClientTestClassType>(2, clientParam);
    Engine             engine(startSequence);
    unique_lock<mutex> lock(g_mutex);
    g_conditionVariable.wait(lock, [] { return g_finished; });
    lock.unlock();

    if (g_failOnTimeout)
    {
        ASSERT_FALSE("Timed Out");
    }
}

constexpr int64_t defaultTimeoutMs = 30;  // 30 ms
constexpr int64_t longTimeoutMs    = 300; // 300 ms => 3 sec
constexpr int64_t shortTimeoutMs   = 3;   // 3 ms
constexpr bool    shouldTimeOut    = true;

TEST(tcp_clientserver, ConnectionSuccess)
{
    runTest<tredzone::connector::tcp::ConnectionSuccess::ServerActor,
            tredzone::connector::tcp::ConnectionSuccess::ClientActor>(defaultTimeoutMs);
}

TEST(tcp_clientserver, MessagePassing)
{
    runTest<tredzone::connector::tcp::MessagePassing::ServerActor,
            tredzone::connector::tcp::MessagePassing::ClientActor>(shortTimeoutMs, shouldTimeOut);
}
