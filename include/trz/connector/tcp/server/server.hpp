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
 * @brief server class to listen for connection to a specific service through a network
 *
 * @tparam _TNetwork the network actor that manages the connection and communication to the network (low level)
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
     * @brief Set the Message Header Size
     *
     * @param messageHeaderSize
     */
    void setMessageHeaderSize(size_t messageHeaderSize) { m_messageHeaderSize = messageHeaderSize; }

    protected:
    friend _TNetwork;

    /**
     * @brief callback called when server process actor (handling a communication) send a notification of it's
     * destruction
     *
     * @param serverProcessActorId the actor id of the server process actor
     */
    virtual void onServerProcessDestroy(const ActorId & /*serverProcessActorId*/) noexcept override {}

    /**
     * @brief callback called after a new client did connect and the connection has been established & handled in an
     * actor
     *
     * @param fd socket of the new communication(used to close the connection)
     * @param clientIp ip of the client that just connect (used to filter)
     * @param serverProcessActorId id of the actor handling the communication
     */
    virtual void onNewConnection(const fd_t /*fd*/, const char * /*clientIp*/,
                                 const ActorId & /*serverProcessActorId*/) noexcept override
    {
    }

    /**
     * @brief callback called after listen failed
     *
     */
    virtual void onListenFailed(void) noexcept override {}

    /**
     * @brief callback called after listen succeed
     *
     */
    virtual void onListenSucceed(void) noexcept override {}

    /**
     * @brief callback called after accept (on incomming connection request) failed
     *
     */
    virtual void onAcceptFailed(void) noexcept override {}

    /**
     * @brief callback called after the server stoped listening
     *
     */
    virtual void onListenStopped(void) noexcept override {}

    /**
     * @brief Get the Listen parameters object
     *
     * @return const ListenParam& the params
     */
    virtual const ListenParam &getListenParam(void) const noexcept override { return m_listenParam; }

    private:
    /**
     * @brief technical callback called just after a new client as been accepted.
     *
     * @param fd socket of the new communication(used to close the connection)
     * @param clientIp ip of the client that just connect (used to filter)
     *
     */
    void onNewConnectionBase(const fd_t fd, const char *serverProcessIp) noexcept override
    {
        (void)serverProcessIp;
        const ActorId &serverProcessActorId =
            newUnreferencedActor<_TServerProcess>(); // define the actor handling the communication
        m_serverProcesses.insert(serverProcessActorId);

        {
            ActorReference<_TServerProcess> ref = referenceLocalActor<_TServerProcess>(serverProcessActorId);
            ref->registerDestroyNotification(getActorId());
            ref->setFd(fd);
            ref->setMessageHeaderSize(m_messageHeaderSize);
        }
        onNewConnection(fd, serverProcessIp, serverProcessActorId); // call the higher level callback onNewConnection
    }

    /**
     * @brief technical callback called after listen failed.
     *
     */
    void onListenFailedBase() noexcept override final
    {
        // set internal flags/variable
        m_registeredListen = false;
        onListenFailed(); // call the higher level callback onListenFailed
    }

    /**
     * @brief technical callback called after listen succeed.
     *
     * @param fd the new listening socket
     */
    void onListenSucceedBase(fd_t fd) noexcept override final
    {
        // set internal flags/variable
        m_registeredListen = false;
        m_fd               = fd;
        onListenSucceed(); // call the higher level callback onListenSucceed
    }

    /**
     * @brief technical callback called after listen stopped.
     *
     */
    void onListenStoppedBase(void) noexcept override final
    {
        m_fd = FD_DISCONNECTED; // set internal flags/variable
        onListenStopped();      // call the higher level callback onListenStopped
    }

    protected:
    ActorReference<_TNetwork> m_network;
#if __GNUC__ < 6
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