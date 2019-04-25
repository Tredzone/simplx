/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file server.hpp
 * @brief tcp server
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include "trz/connector/tcp/server/iserver.hpp"
#include <chrono>
#include <cstdint>
#include <exception>
#include <list>
#include <memory>
#include <ostream>
#include <tuple>
#include <unordered_set>

namespace tredzone
{
namespace connector
{
namespace tcp
{
using fd_t = int64_t;

// import into namespace
using ::std::equal_to;
using ::std::exception;
using ::std::hash;
using ::std::list;
using ::std::make_tuple;
using ::std::ostringstream;
using ::std::shared_ptr;
using ::std::tuple;
using ::std::unordered_set;

/**
 * @brief server class listening for connection to a specific service through a network
 *
 * @tparam _TNetwork network actor managing connection to and communication with network (low level)
 * @tparam _TServerProcess the internal client to spawn to communicate with externals client connecting to this server
 */
template <class _TNetwork, class _TServerProcess> class TcpServer : public Actor, public IServer<_TNetwork>
{
    // static_assert(std::is_convertible<_TServerProcess, TcpServerProcess>::value,
    //              "template parameter 2 [_TServerProcess] must be of type TcpServerProcess");

    using typename IServer<_TNetwork>::ListenParam;

    public:
    static constexpr int64_t FD_DISCONNECTED = -1;

    /**
     * @brief Construct a new Tcp Server
     * if no _TNetwork exist on this core, instanciate it
     *
     * @throw _TNetwork::EpollCreateException
     */
#if __GNUC__ < 6
    TcpServer() noexcept(false) : m_network(newReferencedSingletonActor<_TNetwork>()), m_serverProcesses()
    {
        registerEventHandler<typename _TServerProcess::DestroyNotificationEvent>(*this);
    }
#else
    TcpServer() noexcept(false) : m_network(newReferencedSingletonActor<_TNetwork>()), m_serverProcesses(getAllocator())
    {
        registerEventHandler<typename _TServerProcess::DestroyNotificationEvent>(*this);
    }
#endif

    /**
     * @brief callback called when the DestroyNotificationEvent event is received
     * remove the ServerProcess from the map of active ServerProcess
     * then call the higher level callback onServerProcessDestroy
     *
     * @param e the notification event
     */
    void onEvent(const typename _TServerProcess::DestroyNotificationEvent &e)
    {
        m_serverProcesses.erase(e.getSourceActorId());
        onServerProcessDestroy(e.getSourceActorId());
    }

    /**
     * @brief callback called when the server is asked to destroy itself
     *
     */
    void onDestroyRequest(void) noexcept override
    {
        stopListening();        // stop listenning
        stopAllServerProcess(); // stop all communication
        acceptDestroy();
    }

    /**
     * @brief stop all open communication for this server
     */
    void stopAllServerProcess(void) override
    {
        for (ActorId id : m_serverProcesses)
        {
            try
            {
                ActorReference<_TServerProcess> ref = referenceLocalActor<_TServerProcess>(id);
                ref->destroy();
            }
            catch (const ReferenceLocalActorException &)
            {
            }
        }
    }

    /**
     * @brief register this service to start listening
     *
     * @param addressFamily ip address type AF_INET(IPv4) - AF_INET6(IPv6)
     * @param addressType listenning adress ( *.*.*.* for any)
     * @param port to bind the listenning socket (0 to any)
     *
     */
    void registerListen(const int64_t addressFamily, const int64_t addressType, const int64_t port) override final
    {
        if (m_registeredListen || m_fd != FD_DISCONNECTED)
            throw typename IServer<_TNetwork>::AlreadyListeningException(getListenParam());

        m_listenParam.m_addressFamily = addressFamily;
        m_listenParam.m_addressType   = addressType;
        m_listenParam.m_port          = port;
        m_network->registerListen(this); // add to network queue to be registered next time (async)
        m_registeredListen = true;
    }

    /**
     * @brief stop the listen
     *
     */
    void stopListening(void) noexcept override final
    {
        m_network->disconnect(m_fd); //
        m_registeredListen = false;
        m_fd               = FD_DISCONNECTED;
    }

    /**
     * @brief Sets Message Header Size
     *
     * @param messageHeaderSize
     */
    void setMessageHeaderSize(size_t messageHeaderSize) { m_messageHeaderSize = messageHeaderSize; }

    protected:
    friend _TNetwork;

    /**
     * @brief callback triggered when server process actor (handling the communication) sent a notification of its
     * destruction
     *
     * @param serverProcessActorId actor id of server process actor
     */
    virtual void onServerProcessDestroy(const ActorId & /*serverProcessActorId*/) noexcept override {}

    /**
     * @brief callback triggered after a new client connected and its connection has been established & handled by an
     * actor
     *
     * @param fd new socket of accepted connection (later used to close connection)
     * @param clientIp ip of client that just connected
     * @param serverProcessActorId id of actor handling said socket
     */
    virtual void onNewConnection(const fd_t /*fd*/, const char * /*clientIp*/,
                                 const ActorId & /*serverProcessActorId*/) noexcept override
    {
    }

    /**
     * @brief callback triggered upon failed listen attempt
     *
     */
    virtual void onListenFailed(void) noexcept override {}

    /**
     * @brief callback triggered upon successful listen attempt
     *
     */
    virtual void onListenSucceed(void) noexcept override {}

    /**
     * @brief callback triggered upon failed accept (on incomming connection request)
     *
     */
    virtual void onAcceptFailed(void) noexcept override {}

    /**
     * @brief callback triggered upon server listening stop
     *
     */
    virtual void onListenStopped(void) noexcept override {}

    /**
     * @brief Get Listen parameters
     *
     * @return const ListenParam& parameters
     */
    virtual const ListenParam &getListenParam(void) const noexcept override { return m_listenParam; }

    private:
    /**
     * @brief low-level callback triggered upon accepted new client
     *
     * @param fd new socket of accepted connection (later used to close connection)
     * @param clientIp ip of client that just connected
     *
     */
    void onNewConnectionBase(const fd_t fd, const char *serverProcessIp) noexcept override
    {
        (void)serverProcessIp;
        const ActorId &serverProcessActorId =
            newUnreferencedActor<_TServerProcess>(); // instantiate actor handling said socket
        m_serverProcesses.insert(serverProcessActorId);

        {
            ActorReference<_TServerProcess> ref = referenceLocalActor<_TServerProcess>(serverProcessActorId);
            ref->registerDestroyNotification(getActorId());
            ref->setFd(fd);
            ref->setMessageHeaderSize(m_messageHeaderSize);
        }
        onNewConnection(fd, serverProcessIp, serverProcessActorId);         // call higher-level onNewConnection callback
    }

    /**
     * @brief low-level callback triggered upon failed listen
     *
     */
    void onListenFailedBase() noexcept override final
    {
        m_registeredListen = false;
        onListenFailed(); // call higher-level onListenFailed
    }

    /**
     * @brief low-level callback triggered upon successful listen
     *
     * @param fd new listening socket
     */
    void onListenSucceedBase(fd_t fd) noexcept override final
    {
        m_registeredListen = false;
        m_fd               = fd;
        onListenSucceed(); // call higher-level onListenSucceed
    }

    /**
     * @brief low-level callback triggered upon stopped listen
     *
     */
    void onListenStoppedBase(void) noexcept override final
    {
        m_fd = FD_DISCONNECTED;
        onListenStopped();      // call higher-level onListenStopped
    }

    protected:
    ActorReference<_TNetwork> m_network;
#if __GNUC__ < 6
    // previous gcc versions don't support custom allocator for unordered_set/map
    unordered_set<ActorId> m_serverProcesses;
#else
    unordered_set<ActorId, hash<ActorId>, equal_to<ActorId>, Allocator<ActorId>> m_serverProcesses;
#endif
    ListenParam m_listenParam;

    fd_t m_fd = FD_DISCONNECTED;

    bool m_registeredListen = false;

    size_t m_messageHeaderSize = 0;
};

} // namespace tcp

} // namespace connector

} // namespace tredzone
