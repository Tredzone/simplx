#include "trz/pattern/bus/xthreadbus.hpp"
#include "simplx.h"

#include <iostream>
#include <thread>

using namespace tredzone;
using namespace xthreadbus;

constexpr std::size_t MAXSIZE = 262144;
constexpr int         Limit   = 20;
constexpr bool        logFlag = false;

class Message : public Data
{
    public:
    Message(const std::string &message) : m_message(message) {}
    const std::string m_message;
};

class Echo : public Data
{
    public:
    std::size_t m_id;
};

class UserThreadContext
{
    public:
    UserThreadContext()
        : m_subscriber(XThreadBus<std::size_t, MAXSIZE>::get().getSubscriber(2)),
          m_publisher(XThreadBus<std::size_t, MAXSIZE>::get().getPublisher(1))
    {
        m_subscriber.subscribe<Data>(this);
        m_subscriber.subscribe<Message>(this);
        m_subscriber.subscribe<Echo>(this);

        m_publisher.prepare(new Message("Hello"));
        m_publisher.prepare(new Message("from"));
        m_publisher.prepare(new Message("Thread"));
    }

    void run()
    {
        std::cout << "UserThreadContext running ..." << std::endl;

        while (m_keepLooping)
        {
            m_subscriber.readAll();
            m_publisher.publishAll();
        }

        std::cout << "UserThreadContext stopping." << std::endl;
    }

    void onData(const Data &e) { std::cout << "UserThreadContext::onData(const Data & e)" << std::endl; }

    void onData(const Message &e)
    {
        std::cout << "UserThreadContext::onData(const Message & e) msg=" << e.m_message << std::endl;
    }

    void onData(const Echo &e)
    {
        std::cout << "UserThreadContext::onData(const Echo & e) id=" << e.m_id << std::endl;
        Echo &myThreadEvent = m_publisher.prepare(new Echo());
        myThreadEvent.m_id  = e.m_id + 1;

        if (e.m_id > Limit)
        {
            m_keepLooping = false;
            m_publisher.prepare(new Data());
        }
    }

    private:
    bool m_keepLooping = true;

    XThreadBus<std::size_t, MAXSIZE>::Subscriber &m_subscriber;
    XThreadBus<std::size_t, MAXSIZE>::Publisher & m_publisher;
};

class ThreadInterfaceActor : public Actor, public Actor::Callback
{
    public:
    ThreadInterfaceActor()
        : m_subscriber(XThreadBus<std::size_t, MAXSIZE>::get().getSubscriber(1)),
          m_publisher(XThreadBus<std::size_t, MAXSIZE>::get().getPublisher(2))
    {

        m_subscriber.subscribe<Data>(this);
        m_subscriber.subscribe<Message>(this);
        m_subscriber.subscribe<Echo>(this);

        std::cout << "Actor simplx running ..." << std::endl;

        m_publisher.prepare(new Echo());
        m_publisher.prepare(new Message("Hello"));
        m_publisher.prepare(new Message("from"));
        m_publisher.prepare(new Message("Actor"));

        registerCallback(*this);
    }

    void onCallback()
    {
        m_subscriber.readAll();
        m_publisher.publishAll();

        // exit poc
        if (!m_keepLooping)
            std::cout << "Actor simplx stop running ..." << std::endl;
        else
            registerCallback(*this);
    }

    void onData(const Data &e) { std::cout << "Actor::onData(const Data & e)" << std::endl; }

    void onData(const Message &e)
    {
        std::cout << "Actor::onData(const Message & e) msg=" << e.m_message << std::endl;
    }

    void onData(const Echo &e)
    {
        std::cout << "Actor::onData(const Echo & e) id=" << e.m_id << std::endl;
        Echo &myThreadEvent = m_publisher.prepare(new Echo());
        myThreadEvent.m_id  = e.m_id + 1;

        if (e.m_id > Limit)
        {
            m_keepLooping = false;
            m_publisher.prepare(new Data());
        }
    }

    private:
    bool m_keepLooping = true;

    XThreadBus<std::size_t, MAXSIZE>::Subscriber &m_subscriber;
    XThreadBus<std::size_t, MAXSIZE>::Publisher & m_publisher;
};

/**
 * @brief stick thread to specific core
 * 
 * @param thread 
 * @param core 
 */
void setThreadAffinity(std::thread &thread, std::size_t core)
{
    // Create a cpu_set_t object representing a set of CPUs. Clear it and mark
    // only CPU i as set.
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    int rc = pthread_setaffinity_np(thread.native_handle(),
                                    sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
      std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
    }
}

int main(int argc, char **argv)
{
    UserThreadContext  userTread{};
    std::thread startedThread(&UserThreadContext::run, &userTread);
    setThreadAffinity(startedThread, 2);

    Engine::StartSequence startSequence;

    startSequence.addActor<ThreadInterfaceActor>(1);

    Engine engine(startSequence); // Start all the actors

    getchar();
    // sleep(300); 5 minutes

    return 0;
}