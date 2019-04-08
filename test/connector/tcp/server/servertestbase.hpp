#pragma once
/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file client.hpp
 * @brief tcp client
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

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

using fd_t = int64_t;

/**
 * @brief class to test server.
 *
 * @tparam _TNetwork the network actor that manages the connection and communication to the network (low level)
 * @tparam _TServerProcess the internal client to spawn to communicate with externals client connecting to this server
 */

template <class _TNetwork, class _TServerProcess> class ServerTestBase : public TcpServer<_TNetwork, _TServerProcess>
{
    uint8_t m_onNewConnection        = 0;
    uint8_t m_onServerProcessDestroy = 0;
    uint8_t m_onListenFailed         = 0;
    uint8_t m_onListenSucceed        = 0;
    uint8_t m_onAcceptFailed         = 0;
    uint8_t m_onListenStopped        = 0;

    uint8_t m_onNewConnectionExpected        = 0;
    uint8_t m_onServerProcessDestroyExpected = 0;
    uint8_t m_onListenFailedExpected         = 0;
    uint8_t m_onListenSucceedExpected        = 0;
    uint8_t m_onAcceptFailedExpected         = 0;
    uint8_t m_onListenStoppedExpected        = 0;

    using parent = TcpServer<_TNetwork, _TServerProcess>;
    public:
    ServerTestBase(uint8_t onNewConnectionExpected, uint8_t onServerProcessDestroyExpected,
                   uint8_t onListenFailedExpected, uint8_t onListenSucceedExpected, uint8_t onAcceptFailedExpected,
                   uint8_t onListenStopped)
        : m_onNewConnectionExpected(onNewConnectionExpected),
          m_onServerProcessDestroyExpected(onServerProcessDestroyExpected),
          m_onListenFailedExpected(onListenFailedExpected), m_onListenSucceedExpected(onListenSucceedExpected),
          m_onAcceptFailedExpected(onAcceptFailedExpected), m_onListenStoppedExpected(onListenStopped)
    {
    }

    ~ServerTestBase() { check(); }

    void check()
    {
        ASSERT_EQ(m_onNewConnectionExpected, m_onNewConnection);
        ASSERT_EQ(m_onServerProcessDestroyExpected, m_onServerProcessDestroy);
        ASSERT_EQ(m_onListenFailedExpected, m_onListenFailed);
        ASSERT_EQ(m_onListenSucceedExpected, m_onListenSucceed);
        ASSERT_EQ(m_onAcceptFailedExpected, m_onAcceptFailed);
        ASSERT_EQ(m_onListenStoppedExpected, m_onListenStopped);
    }

    protected:
    void onServerProcessDestroy(const Actor::ActorId &serverProcessActorId) noexcept override final
    {
#if VERBOSE_TEST > 0
        std::cout << "Server " << parent::getActorId() << " onServerProcessDestroy()" << std::endl;
#endif
        m_onServerProcessDestroy++;
        onServerProcessDestroyTest(serverProcessActorId);
    }

    void onNewConnection(const fd_t fd, const char *clientIp,
                         const Actor::ActorId &serverProcessActorId) noexcept override final
    {
#if VERBOSE_TEST > 0
        std::cout << "Server " << parent::getActorId() << " onNewConnection()" << std::endl;
#endif
        m_onNewConnection++;
        onNewConnectionTest(fd, clientIp, serverProcessActorId);
    }

    void onListenFailed(void) noexcept override final
    {
#if VERBOSE_TEST > 0
        std::cout << "Server " << parent::getActorId() << " onListenFailed()" << std::endl;
#endif
        m_onListenFailed++;
        onListenFailedTest();
    }

    void onListenSucceed(void) noexcept override final
    {
#if VERBOSE_TEST > 0
        std::cout << "Server " << parent::getActorId() << " onListenSucceed()" << std::endl;
#endif
        m_onListenSucceed++;
        onListenSucceedTest();
    }

    void onAcceptFailed(void) noexcept override final
    {
#if VERBOSE_TEST > 0
        std::cout << "Server " << parent::getActorId() << " onAcceptFailed()" << std::endl;
#endif
        m_onAcceptFailed++;
        onAcceptFailedTest();
    }

    void onListenStopped(void) noexcept override final
    {
#if VERBOSE_TEST > 0
        std::cout << "Server " << parent::getActorId() << " onListenStopped()" << std::endl;
#endif
        m_onListenStopped++;
        onListenStoppedTest();
    }

    virtual void onServerProcessDestroyTest(const Actor::ActorId & /*serverProcessActorId*/) noexcept {}

    virtual void onNewConnectionTest(const fd_t /*fd*/, const char * /*clientIp*/,
                                     const Actor::ActorId & /*serverProcessActorId*/) noexcept
    {
    }

    virtual void onListenFailedTest(void) noexcept {}

    virtual void onListenSucceedTest(void) noexcept {}

    virtual void onAcceptFailedTest(void) noexcept {}

    virtual void onListenStoppedTest(void) noexcept {}
};
} // namespace tcp
} // namespace connector
} // namespace tredzone
