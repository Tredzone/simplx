
#pragma once

#include "clientserver/eventandservicetag.hpp"
#include "common/testvariables.hpp"
#include "simplx.h"
#include "gtest/gtest.h"
#include <memory>

namespace tredzone
{
namespace connector
{
namespace tcp
{

class ValidatorActor : public Actor
{
    enum class VALIDATION_SIDE
    {
        SERVER,
        CLIENT
    };

    public:
    ValidatorActor(uint64_t timeout) : m_runCallback(*this, timeout)
    {
        registerEventHandler<ServerSideSuccessEvent>(*this);
        registerEventHandler<ClientSideSuccessEvent>(*this);
        registerEventHandler<ServerSideFaillureEvent>(*this);
        registerEventHandler<ClientSideFaillureEvent>(*this);
    }

    void onEvent(const ServerSideSuccessEvent &)
    {
        if (validateTest(VALIDATION_SIDE::SERVER))
            endTest();
    }

    void onEvent(const ClientSideSuccessEvent &)
    {
        if (validateTest(VALIDATION_SIDE::CLIENT))
            endTest();
    }

    void onEvent(const ServerSideFaillureEvent &)
    {
        ASSERT_FALSE("TcpServer sent a faillure event");
        endTest();
    }

    void onEvent(const ClientSideFaillureEvent &)
    {
        ASSERT_FALSE("TcpClient sent a faillure event");
        endTest();
    }

    private:
    bool validateTest(const VALIDATION_SIDE validation_side)
    {
        if (VALIDATION_SIDE::SERVER == validation_side)
            m_serverSuccessFlag = true;
        else if (VALIDATION_SIDE::CLIENT == validation_side)
            m_clientSuccessFlag = true;

        return (m_serverSuccessFlag && m_clientSuccessFlag);
    }

    class RunCallback : public Callback
    {
        public:
        ValidatorActor &myactor;
        RunCallback(ValidatorActor &actor, const uint64_t timeout)
            : myactor(actor), m_timeout(::std::chrono::milliseconds(timeout)),
              m_start(::std::chrono::steady_clock::now())
        {
            myactor.registerCallback(*this);
        }

        void onCallback(void)
        {
            const auto now = ::std::chrono::steady_clock::now();

            const auto &testDuration = now - m_start;
            if (m_timeout < testDuration)
            {
                myactor.endTest(true);
            }
            else
            {
                myactor.registerCallback(*this);
            }
        }
        ::std::chrono::milliseconds             m_timeout;
        ::std::chrono::steady_clock::time_point m_start;
    };

    void endTest(bool timedOut = false)
    {
        if (timedOut && !g_expectingTimeout)
        {
            g_failOnTimeout = true;
        }
        ::std::lock_guard<::std::mutex> lk(g_mutex);
        g_finished = true;
        g_conditionVariable.notify_one();
    }
    RunCallback m_runCallback;

    bool m_serverSuccessFlag = false;
    bool m_clientSuccessFlag = false;
};
} // namespace tcp
} // namespace connector
} // namespace tredzone