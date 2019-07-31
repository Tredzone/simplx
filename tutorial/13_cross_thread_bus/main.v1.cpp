#include "trz/pattern/bus/xthreadbus.hpp"
#include "simplx.h"

#include <chrono>
#include <iostream>
#include <thread>

using namespace tredzone;
using namespace xthreadbus;

constexpr std::size_t MAXSIZE      = 262144;
constexpr std::size_t Limit        = 10000000; // 
constexpr std::size_t LogFrequency = 1048576;//1048576; // need a power of 2
constexpr bool        LogFlag      = true;

/**
 * @brief DataType containing Id
 * 
 */
class Echo : public Data
{
    public:
    Echo() : Echo(0) {}
    Echo(std::size_t id) : m_id(id) {}
    std::size_t m_id;
};

/**
 * @brief Thread context benchmark use
 * 
 * @tparam _TChanelIdType 
 * @tparam _BusExchangeBuffersSize 
 */
template <typename _TChanelIdType, std::size_t _BusExchangeBuffersSize> class UserThreadContext
{
    using Bus = XThreadBus<_TChanelIdType, _BusExchangeBuffersSize>;

    public:
    /**
     * @brief Construct a new User Thread Context
     * 
     * @param subscriberId 
     * @param publisherId 
     * @param benchmarker 
     */
    UserThreadContext(_TChanelIdType subscriberId, _TChanelIdType publisherId, bool oneThreadCore = false, bool benchmarker = false)
        : m_subscriber(Bus::get().getSubscriber(subscriberId)), m_publisher(Bus::get().getPublisher(publisherId)), m_oneThreadCore(oneThreadCore),
          m_benchmarker(benchmarker), m_totalDelay(0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        m_subscriber.template subscribe<Echo>(this);
        if (!m_benchmarker)
        {
            m_publisher.publishOne(new Echo());
        }
    }

    /**
     * @brief method to run in new thread
     * 
     */
    void run()
    {
        std::cout << "UserThreadContext running ..." << std::endl;

        while (m_keepLooping)
        {
            while (m_subscriber.readOne())
            {
            }
            if (!m_oneThreadCore) std::this_thread::yield(); // about 1500 nanosec minimum
            //std::cout << "UserThreadContext readOne failed: no more data to read." << std::endl;
        }

        std::cout << "UserThreadContext stopping." << std::endl;
    }

    /**
     * @brief handler for data of type Echo
     * 
     * @param e 
     */
    void onData(const Echo &e)
    {
        std::size_t count = e.m_id;
        if (m_benchmarker)
        {
            doBenchmark(count);
        }

        if (count > Limit)
        {
            m_keepLooping = false;
        }
        if (!m_publisher.publishOne(new Echo(count + (m_benchmarker ? 1 : 0))))
            std::cout << "UserThreadContext publish failed: no more space. [subscriber reads too slowly]" << std::endl;
    }

    /**
     * @brief method to bench the exchange speed
     * 
     * @param count 
     */
    void doBenchmark(std::size_t &count)
    {
        if ((count % LogFrequency) == 0)
        {
            if (!m_firstTime)
            {
                const auto               current = std::chrono::steady_clock::now();
                std::chrono::nanoseconds delay   = current - m_start;
                m_totalDelay                     = m_totalDelay + delay;

                if (LogFlag)
                {
                    std::cout << "UserThreadContext echoed " << count << " time in " << m_totalDelay.count() / 1000000
                              << " millisecond"
                              << " (logging each " << LogFrequency << " and last took " << delay.count() / 1000000
                              << "millisecond) which is an average about " << ((float)m_totalDelay.count() / (float)count)
                              << " nanosecond for a full round" << std::endl;
                }
            }
            else
            {
                count       = 0;
                m_firstTime = false;
            }

            m_start = std::chrono::steady_clock::now(); // slow the run but avoid the loss due to cout (vs m_start = current) 
        }
    }

    private:
    typename Bus::Subscriber &m_subscriber;
    typename Bus::Publisher & m_publisher;

    bool m_firstTime   = true;
    bool m_keepLooping = true;
	bool m_oneThreadCore;
    bool m_benchmarker;


    std::chrono::steady_clock::time_point m_start;
    std::chrono::nanoseconds              m_totalDelay;
};

using MyThread = UserThreadContext<std::string, MAXSIZE>;

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
    MyThread    userTread{"from2to1", "from1to2", true, true};
    std::thread startedThread(&MyThread::run, &userTread);
    setThreadAffinity(startedThread, 1);

    MyThread    userTread2{"from1to2", "from2to1", true};
    std::thread startedThread2(&MyThread::run, &userTread2);
    setThreadAffinity(startedThread2, 2);

    getchar();

    return 0;
}