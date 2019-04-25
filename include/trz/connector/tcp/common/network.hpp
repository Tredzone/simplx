/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file network.hpp
 * @brief tcp network actor
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include "trz/connector/tcp/client/iclient.hpp"
#include "trz/connector/tcp/common/networkcalls.hpp"
#include "trz/connector/tcp/server/iserver.hpp"

#include "simplx.h"

#include <chrono>
#include <cstdint>
#include <exception>
#include <memory>
#include <queue>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace tredzone
{
namespace connector
{
namespace tcp
{

// import into namespace
using ::std::array;
using ::std::deque;
using ::std::equal_to;
using ::std::get;
using ::std::hash;
using ::std::make_tuple;
using ::std::pair;
using ::std::queue;
using ::std::runtime_error;
using ::std::shared_ptr;
using ::std::tuple;
using ::std::unordered_map;
using ::std::unordered_set;
using ::std::chrono::microseconds;
using ::std::chrono::steady_clock;
using fd_t = int64_t;

/**
 * @brief actor managing network connection and communication
 *
 * @tparam _MaxReceivePerLoop maximum read operations per loop
 * @tparam _MaxSendPerLoop maximum write operations per loop
 * @tparam _MaxDataPerRead the maximum data to get each read
 * @tparam _MaxPendingIncomingConnectionRequest maximum pending external connections per service. Additional connections will be rejected
 */
template <size_t _MaxReceivePerLoop, size_t _MaxSendPerLoop, size_t _MaxDataPerRead,
          size_t _MaxPendingIncomingConnectionRequest, class _TNetworkCalls = NetworkCalls>
class Network : public Actor
{
    using TcpClient = IClient<Network>;
    using TcpServer = IServer<Network>;

    public:
    /**
     * @brief exception thrown when epoll file descriptor creation fails
     *
     */
    class EpollCreateException : public exception
    {
        public:
        virtual const char *what() const throw() { return "Couldn't create epollSocket"; }
    };

    /**
     * @brief exception trown when a directSend(...) call fails
     *
     */
    class DirectSendException : public exception
    {
        public:
        DirectSendException(const string &err) : m_err(err) { m_msg << "Failed to direct send message: " << m_err; }

        DirectSendException(const DirectSendException &other) : m_err(other.m_err)
        {
            m_msg << "Failed to direct send message: " << m_err;
        }

        virtual const char *what() const throw() { return m_msg.str().c_str(); }

        private:
        ostringstream m_msg;
        const string &m_err;
    };

    /**
     * @brief instantiate a new Network
     *
     */
#if __GNUC__ < 6
    // with default std allocator
    Network(void) noexcept(false)
        : m_clientBySocket(), m_connectionPending(), m_connectQueue(), m_sendQueue(), m_readQueue(),
          m_connectCallback(*this), m_pendingConnectionCallback(*this), m_sendCallback(*this), m_epollCallback(*this),
          m_serverBySocket(), m_acceptingSocket(), m_listenQueue(), m_listenCallback(*this)
#else
    // with custom allocator
    Network(void) noexcept(false)
        : m_clientBySocket(getAllocator()), m_connectionPending(getAllocator()), m_connectQueue(getAllocator()),
          m_sendQueue(getAllocator()), m_readQueue(getAllocator()), m_connectCallback(*this),
          m_pendingConnectionCallback(*this), m_sendCallback(*this), m_epollCallback(*this),
          m_serverBySocket(getAllocator()), m_acceptingSocket(getAllocator()), m_listenQueue(getAllocator()),
          m_listenCallback(*this)
#endif
    {
        m_epollSocket = _TNetworkCalls::epoll_create1(0);
        if (m_epollSocket == -1)
        {
            throw EpollCreateException();
        }
    }

    /**
     * @brief close all network sockets
     *
     */
    void stopNetwork(void) noexcept
    {
        // disconnect all clients
        while (!m_clientBySocket.empty())
        {
            const auto it = m_clientBySocket.begin();
            onDisconnected(it->first);
        }
        // disconnect all servers
        while (!m_serverBySocket.empty())
        {
            const auto it = m_serverBySocket.begin();
            onDisconnected(it->first);
        }
    }

    /**
     * @brief callback triggered when actor receives a destruction request
     *
     */
    void onDestroyRequest(void) noexcept override
    {
        stopNetwork();
        acceptDestroy();
    }

    /**
     * @brief Destructor
     *
     */
    virtual ~Network(void) noexcept
    {
        stopNetwork();
        _TNetworkCalls::close(m_epollSocket);
    }

    /**
     * @brief register a client actor for later, asynchronous (on next network handler loop) connection
     *
     * @param client actor
     */
    void registerConnect(TcpClient *client) noexcept
    {
        assert(client);
        if (!client)
            return;
        // add client to connection queue
        m_connectQueue.push(client);
        // register callback that'll be triggered when network connected said client
        registerCallbackOnce(m_connectCallback);
    }

    /**
     * @brief queue a server actor for later, asynchronous (on next network handler loop) listening
     *
     * @param server actor
     */
    void registerListen(TcpServer *server) noexcept
    {
        assert(server);
        if (!server)
            return;
        // add server to listen queue
        m_listenQueue.push(server);
        // register callback that'll be triggered when network connected said server
        registerCallbackOnce(m_listenCallback);
    }

    /**
     * @brief synchronously close socket and remove any associated client or server actor
     *
     * @param socket to disconnect
     * @return true when socket was connected
     * @return false when socket was not connected
     */
    bool disconnect(const fd_t socket) noexcept
    {
        // close socket
        const bool wasConnected =
            (_TNetworkCalls::shutdown(socket, _TNetworkCalls::Shut_rdwr) == 0 && _TNetworkCalls::close(socket) == 0);

        // remove client or server bound to said socket
        m_clientBySocket.erase(socket);
        m_serverBySocket.erase(socket);

        // cancel any existing callback, as is no longer needed
        if (m_clientBySocket.empty())
        {
            m_sendCallback.unregister();
            if (m_serverBySocket.empty())
            {
                m_epollCallback.unregister();
            }
        }
        return wasConnected;
    }

    /**
     * @brief queue a client actor for later, asynchronous (on next network handler loop) 
     * getting and writing its buffer to the socket
     *
     * @param socket identifying client
     */
    void addToSendQueue(const fd_t socket) noexcept
    {
        m_sendQueue.push(socket);
        registerCallbackOnce(m_sendCallback);
    }

    /**
     * @brief synchronously write [datasize] bytes of data to socket in max [retryNumber] attempts
     * Here, the write opperation is non-blocking (as opposed to calling blockingDirectSend())
     * /!\ can significantly degrade performance when multiple clients/servers are used on the same core, so prefer addToSendQueue
     *
     * @param socket to send on
     * @param data to send
     * @param dataSize bytes of data to send
     * @param retryNumber max send attempts
     */
    void directSend(const fd_t socket, const uint8_t *data, const size_t dataSize, ssize_t retryNumber = 10)
    {
        int64_t dataSizeToSend = dataSize;
        while (dataSizeToSend > 0)
        {
            const ssize_t dataSizeSent = _TNetworkCalls::send(socket, data, dataSize, _TNetworkCalls::Msg_dontwait);
            if (dataSizeSent < 0)
            {
                if ((errno != _TNetworkCalls::Eagain && errno != _TNetworkCalls::Ewouldblock) || --retryNumber <= 0)
                {
                    throw DirectSendException(std::strerror(errno));
                }
                usleep(1);
                continue;
            }
            dataSizeToSend -= dataSizeSent;
            data += dataSizeSent;
        }
    }

    /**
     * @brief synchronously write [datasize] bytes of data to the socket in max [retryNumber] attempts
     * Here, the write opperation is blocking, as opposed to calling directSend()
     * /!\ can significantly degrade performance when multiple clients/servers are used on the same core, so prefer addToSendQueue
     *
     * @param socket to send on
     * @param data to send
     * @param dataSize bytes of data to send
     * @param retryNumber max send attempts
     */
    void blockingDirectSend(const fd_t socket, const uint8_t *data, const size_t dataSize, ssize_t retryNumber = 10)
    {
        int64_t dataSizeToSend = dataSize;
        while (dataSizeToSend > 0)
        {
            const ssize_t dataSizeSent = _TNetworkCalls::send(socket, data, dataSize, 0);
            if (dataSizeSent < 0)
            {
                if ((errno != _TNetworkCalls::Eagain && errno != _TNetworkCalls::Ewouldblock) || --retryNumber <= 0)
                {
                    throw DirectSendException(std::strerror(errno));
                }
                continue;
            }
            dataSizeToSend -= dataSizeSent;
            data += dataSizeSent;
        }
    }

    /**
     * @brief bind socket to a server-spawned actor
     *
     * @param serverProcess server-spawned actor
     * @param fd new socket of accepted connection
     */
    void bindClientHandler(TcpClient *serverProcess, const fd_t fd)
    {
        assert(serverProcess);
        if (!serverProcess)
        {
        }
        m_clientBySocket.emplace(fd, serverProcess);
        m_connectionPending.emplace(fd, make_tuple(steady_clock::now(), serverProcess->getTimeout()));
        registerCallbackOnce(m_pendingConnectionCallback);

        if (!m_clientBySocket.empty() || !m_serverBySocket.empty())
            registerCallbackOnce(m_epollCallback);
    }

    /**
     * @brief non-blocking socket read
     * must be called directly by the client (whose auto-read flag is disabled)
     *
     * @param socket to read
     */
    void readSocket(const fd_t socket) noexcept
    {
        array<uint8_t, _MaxDataPerRead> dataToReceive;

        // read _MaxDataPerRead bytes on socket
        const ssize_t bytes_read =
            _TNetworkCalls::recv(socket, dataToReceive.data(), _MaxDataPerRead, _TNetworkCalls::Msg_dontwait);
        if (bytes_read == 0)
            return;
        else if (bytes_read > 0)
        {
            const auto it = m_clientBySocket.find(socket);
            if (it != m_clientBySocket.end())
            {
                assert(it->second);
                if (it->second)
                {
                    it->second->onDataReceivedBase(dataToReceive.data(), static_cast<size_t>(bytes_read));
                }
                // if all data couldn't be read this call, re-queue socket for another read in next loop iteration
                if (bytes_read == _MaxDataPerRead)
                    addToReadQueue(socket);
            }
        }
        else
        {
            // if socket wasn't ready, re-queue it for another read in next loop iteration
            if (errno == _TNetworkCalls::Eagain && errno == _TNetworkCalls::Ewouldblock)
            {
                addToReadQueue(socket);
            }
            else
            {
                std::cout << "Error: errno=" << std::strerror(errno) << std::endl;
            }
        }
    }

    protected:
    /**
     * @brief callback triggered upon socket connection timeout
     * triggers actor callback, then removes socket and associated actor from network handler
     *
     * @param socket that failed to connect
     */
    void onConnectionTimedOut(const fd_t socket) noexcept
    {
        _TNetworkCalls::close(socket);
        const auto it1 = m_clientBySocket.find(socket);
        if (it1 != m_clientBySocket.end())
        {
            assert(it1->second);
            if (it1->second)
            {
                it1->second->onConnectTimedOutBase();
            }
            m_clientBySocket.erase(it1);
        }
        if (m_clientBySocket.empty())
        {
            m_sendCallback.unregister();
            if (m_serverBySocket.empty())
            {
                m_epollCallback.unregister();
            }
        }
    }

    /**
     * @brief callback triggered socket connection loss
     * triggers actor callback, then removes socket and associated actor from network handler
     *
     * @param socket that has lost connection
     */
    void onConnectionLost(const fd_t socket) noexcept
    {
        _TNetworkCalls::close(socket);
        const auto it1 = m_clientBySocket.find(socket);
        if (it1 != m_clientBySocket.end())
        {
            assert(it1->second);
            const auto it2 = m_connectionPending.find(it1->first);
            if (it2 != m_connectionPending.end())
            {
                if (it1->second)
                {
                    it1->second->onConnectFailedBase();
                }
                m_connectionPending.erase(it2);
            }
            else
            {
                if (it1->second)
                {
                    it1->second->onConnectionLostBase();
                }
            }
            m_clientBySocket.erase(it1);
        }
        const auto it3 = m_serverBySocket.find(socket);
        if (it3 != m_serverBySocket.end())
        {
            assert(it3->second);
            if (it3->second)
            {
                it3->second->onListenStoppedBase();
            }
            m_serverBySocket.erase(it3);
        }
        if (m_clientBySocket.empty())
        {
            m_sendCallback.unregister();
            if (m_serverBySocket.empty())
            {
                m_epollCallback.unregister();
            }
        }
    }

    /**
     * @brief callback triggered upon socket disconnection
     * triggers actor callback, then removes socket and associated actor from network handler
     *
     * @param socket that has disconnect
     */
    void onDisconnected(const fd_t socket) noexcept
    {
        _TNetworkCalls::close(socket);
        const auto it1 = m_clientBySocket.find(socket);
        if (it1 != m_clientBySocket.end())
        {
            assert(it1->second);
            const auto it2 = m_connectionPending.find(it1->first);
            if (it2 != m_connectionPending.end())
            {
                if (it1->second)
                {
                    it1->second->onConnectFailedBase();
                }
                m_connectionPending.erase(it2);
            }
            else
            {
                if (it1->second)
                {
                    it1->second->onDisconnectBase();
                }
            }
            m_clientBySocket.erase(it1);
        }

        const auto it2 = m_serverBySocket.find(socket);
        if (it2 != m_serverBySocket.end())
        {
            if (it2->second)
            {
                it2->second->onListenStoppedBase();
            }
            m_serverBySocket.erase(it2);
        }

        if (m_clientBySocket.empty())
        {
            m_sendCallback.unregister();
            if (m_serverBySocket.empty())
            {
                m_epollCallback.unregister();
            }
        }
    }

    /**
     * @brief callback triggered upon successful socket connection
     * triggers actor callback, then registers epoll callback
     *
     * @param socket that just been connected
     * @return true when connection succeed
     * @return false when connection failed
     */
    bool onCompleteConnection(const fd_t socket) noexcept
    {
        const auto it = m_clientBySocket.find(socket);
        if (it != m_clientBySocket.end())
        {
            assert(it->second);
            if (!it->second)
            {
                _TNetworkCalls::close(socket);
                return false;
            }
            if (!_TNetworkCalls::setSocketBlocking(socket))
            {
                _TNetworkCalls::close(socket);
                it->second->onConnectFailedBase();
                return false;
            }
            it->second->onConnectBase(socket);
            registerCallbackOnce(m_epollCallback);
            return true;
        }
        return false;
    }

    /**
     * @brief pops clients from FIFO queue and connects them
     * the connect opperation is non-blocking
     *
     */
    void onConnectCallback(void) noexcept
    {
        while (!m_connectQueue.empty())
        {
            // pop client from queue
            TcpClient *client = m_connectQueue.front();
            m_connectQueue.pop();
            assert(client);
            if (!client)
                continue;

            // get client params
            const auto     params        = client->getConnectParam();
            const uint64_t addressFamily = params.m_addressFamily;
            const string & ipAddress     = params.m_ipAddress;
            const int16_t  port          = params.m_port;

            typename _TNetworkCalls::Sockaddr_in serv_addr;
            // store char 0 and not 0x00
            ::memset(&serv_addr, '0', sizeof(serv_addr));
            serv_addr.sin_family = addressFamily;
            serv_addr.sin_port   = _TNetworkCalls::wrapped_htons(port);

            // Convert IP addresses from text to binary
            if (_TNetworkCalls::inet_pton(addressFamily, ipAddress.c_str(), &serv_addr.sin_addr) < 0)
            {
                client->onConnectFailedBase();
                continue;
            }

            // create socket
            const fd_t newSocket = _TNetworkCalls::socket(addressFamily, _TNetworkCalls::Sock_stream, 0);
            if (newSocket < 0)
            {
                _TNetworkCalls::close(newSocket);
                client->onConnectFailedBase();
                continue;
            }

            // if requested, try binding to specific source/local network interface/port
            if (!params.m_ipAddressSource.empty())
            {
                typename _TNetworkCalls::Sockaddr_in localaddr;
                localaddr.sin_family      = addressFamily;
                localaddr.sin_addr.s_addr = _TNetworkCalls::inet_addr(params.m_ipAddressSource.c_str());
                localaddr.sin_port        = 0; // local port 0 for [any]
                _TNetworkCalls::bind(newSocket, &localaddr, sizeof(localaddr));
            }

            // make socket non-blocking for connection
            if (!_TNetworkCalls::setSocketNonBlocking(newSocket))
            {
                _TNetworkCalls::close(newSocket);
                client->onConnectFailedBase();
                continue;
            }

            // add new socket to the epoll socket
            typename _TNetworkCalls::Epoll_event epollEvent;
            epollEvent.events  = _TNetworkCalls::Epollout | _TNetworkCalls::Epollin | _TNetworkCalls::Epollet;
            epollEvent.data.fd = newSocket;

            if (_TNetworkCalls::epoll_ctl(m_epollSocket, _TNetworkCalls::Epoll_ctl_add, newSocket, &epollEvent))
            {
                _TNetworkCalls::close(newSocket);
                client->onConnectFailedBase();
                continue;
            }

            // associate socket to (client actor) handler
            m_clientBySocket.emplace(newSocket, client);
            m_connectionPending.emplace(newSocket, make_tuple(client->getRegistrationTime(), client->getTimeout()));

            // connect (non-blocking)
            const int64_t connectResult = _TNetworkCalls::connect(newSocket, &serv_addr, sizeof(serv_addr));

            if (connectResult < 0 && errno != _TNetworkCalls::Einprogress)
            {
                onDisconnected(newSocket);
                continue;
            }

            // register actor callbacks (triggered upon connection completion or timeout)
            registerCallbackOnce(m_epollCallback);
            registerCallbackOnce(m_pendingConnectionCallback);

            if (connectResult >= 0)
            {   
                // successful socket connection
                onCompleteConnection(newSocket);
            }
        }
    }

    /**
     * @brief network actorCallback, checks all pending connection for timeout
     *
     */
    void onPendingConnectionCallback(void) noexcept
    {
        const auto now = steady_clock::now();
        for (auto it = m_connectionPending.begin(); it != m_connectionPending.end();)
        {
            const auto &connectionDuration = now - get<0>(it->second);
            const auto &timeout            = get<1>(it->second);

            if (timeout > m_zeroMicrosecond && timeout < connectionDuration)
            {
                // reached timeout
                onConnectionTimedOut(it->first);
                it = m_connectionPending.erase(it);
            }
            else
            {
                // skip
                it++;
            }
        }
        if (!m_connectionPending.empty())
            registerCallbackOnce(m_pendingConnectionCallback);
    }

    /**
     * @brief network actorCallback, pops server from queue (FIFO) and puts it in listening mode
     * get server in queue, get param in server, create the socket, start listening
     * the listening socket is set to non-blocking
     *
     */
    void onListenCallback(void) noexcept
    {
        while (!m_listenQueue.empty())
        {
            // pop server
            TcpServer *server = m_listenQueue.front();
            m_listenQueue.pop();
            assert(server);
            if (!server)
                continue;

            // get params
            const auto    params        = server->getListenParam();
            const int64_t addressFamily = params.m_addressFamily;
            const int64_t addressType   = params.m_addressType;
            const int16_t port          = params.m_port;

            // create socket
            const fd_t serverSocket = _TNetworkCalls::socket(addressFamily, _TNetworkCalls::Sock_stream, 0);
            if (serverSocket < 0)
            {
                server->onListenFailedBase();
                continue;
            }

            int opt = 1;
            if (_TNetworkCalls::setsockopt(serverSocket, _TNetworkCalls::Sol_Socket,
                                           _TNetworkCalls::So_reuseaddr | _TNetworkCalls::So_reuseport, &opt,
                                           sizeof(opt)))
            {
                server->onListenFailedBase();
                continue;
            }

            // set socket non-blocking
            if (!_TNetworkCalls::setSocketNonBlocking(serverSocket))
            {
                _TNetworkCalls::close(serverSocket);
                server->onListenFailedBase();
                continue;
            }

            // if rquested, try binding to a specific network interface/port
            typename _TNetworkCalls::Sockaddr_in address;
            address.sin_family      = addressFamily;
            address.sin_addr.s_addr = addressType;
            address.sin_port        = _TNetworkCalls::wrapped_htons(port);
            if (_TNetworkCalls::bind(serverSocket, &address, sizeof(address)) < 0)
            {
                _TNetworkCalls::close(serverSocket);
                server->onListenFailedBase();
                continue;
            }

            // add new socket to epoll socket
            typename _TNetworkCalls::Epoll_event event;
            event.data.fd = serverSocket;
            event.events  = _TNetworkCalls::Epollin;
            if (_TNetworkCalls::epoll_ctl(m_epollSocket, _TNetworkCalls::Epoll_ctl_add, serverSocket, &event) < 0)
            {
                _TNetworkCalls::close(serverSocket);
                server->onListenFailedBase();
                continue;
            }

            // start listening
            if (_TNetworkCalls::listen(serverSocket, _MaxPendingIncomingConnectionRequest) < 0)
            {
                _TNetworkCalls::close(serverSocket);
                server->onListenFailedBase();
                continue;
            }

            // associate socket with (client actor) handler
            m_serverBySocket.emplace(serverSocket, server);
            server->onListenSucceedBase(serverSocket);
        }
        if (!m_serverBySocket.empty())
            registerCallbackOnce(m_epollCallback);
    }

    /**
     * @brief callback triggered when a listening socket receives new client connection
     *
     * @param serverSocket listening socket
     * @param server actor handling this listening socket
     */
    void onNewConnection(fd_t serverSocket, TcpServer *server) noexcept
    {
        assert(server);
        if (!server)
            return;
        // accept connection and get client info
        char       clientIp[_TNetworkCalls::Inet6_addrstrlen];
        const fd_t fd = _TNetworkCalls::accept4(serverSocket, clientIp, _TNetworkCalls::Sock_nonblock);
        if (fd < 0 && errno != _TNetworkCalls::Eagain && errno != _TNetworkCalls::Ewouldblock)
        {
            _TNetworkCalls::close(fd);
            server->onAcceptFailed();
            return;
        }
        else if (fd < 0)
        {
            // the non-blocking server socket isn't yet ready to accept, so wait to reprocess at next loop iteration
            return;
        }

        // add new socket to the epoll socket
        typename _TNetworkCalls::Epoll_event event;
        event.events  = _TNetworkCalls::Epollout;
        event.data.fd = fd;
        if (_TNetworkCalls::epoll_ctl(m_epollSocket, _TNetworkCalls::Epoll_ctl_add, fd, &event))
        {
            _TNetworkCalls::close(fd);
            server->onAcceptFailed();
        }

        // tell server to  handle this new connection
        server->onNewConnectionBase(fd, clientIp);
    }

    /**
     * @brief network actorCallback, checks any managed socket changes (server and client)
     * check epoll and dispatch any event
     *
     */
    void onEpollCallback(void) noexcept
    {
        // check the epoll
        const int64_t event_count = _TNetworkCalls::epoll_wait(m_epollSocket, m_events, _MaxReceivePerLoop, 0);
        if (event_count == -1)
        {
            return;
        }

        for (int64_t i = 0; i < event_count; i++)
        {
            // socket error
            if (m_events[i].events & _TNetworkCalls::Epollerr)
            {
                if (m_events[i].events & _TNetworkCalls::Epollhup)
                {
                    // normal disconnection
                    onDisconnected(m_events[i].data.fd);
                }
                else
                {
                    // unexpected disconnection
                    onConnectionLost(m_events[i].data.fd);
                }
            }
            else if (m_events[i].events & _TNetworkCalls::Epollout)
            {
                // outgoing event (connection)

                // check pending connection
                const auto it1 = m_connectionPending.find(m_events[i].data.fd);
                if (it1 != m_connectionPending.end())
                {
                    // successful socket connection
                    m_connectionPending.erase(it1);
                    
                    if (!onCompleteConnection(m_events[i].data.fd))
                        continue;

                    // toggle socket event flags in epoll
                    typename _TNetworkCalls::Epoll_event event;
                    event.events  = _TNetworkCalls::Epollin | _TNetworkCalls::Epollet;
                    event.data.fd = m_events[i].data.fd;
                    if (_TNetworkCalls::epoll_ctl(m_epollSocket, _TNetworkCalls::Epoll_ctl_mod, m_events[i].data.fd,
                                                  &event) < 0)
                    {
                        // socket event toggling failed
                        auto it2 = m_clientBySocket.find(m_events[i].data.fd);
                        if (it2 != m_clientBySocket.end())
                        {   
                            onConnectionLost(m_events[i].data.fd);
                        }
                    }
                }
            }
            else if (m_events[i].events & _TNetworkCalls::Epollin)
            {
                // incoming event
                const auto it = m_serverBySocket.find(m_events[i].data.fd);
                if (it != m_serverBySocket.end())
                {
                    // server socket = incoming client
                    onNewConnection(it->first, it->second);
                }
                else
                {   // client socket = message
                    addToReadQueue(m_events[i].data.fd);
                }
            }
            else
            {
                (void)m_events[i].data.fd;
                (void)m_events[i].events;
            }
        }
        // read sockets in queue
        if (!m_readQueue.empty())
            readSockets();
        if (!m_clientBySocket.empty() || !m_serverBySocket.empty())
            registerCallbackOnce(m_epollCallback);
    }

    /**
     * @brief queue a client actor for later, asynchronous (on next network handler loop) reading
     * if AutoRead flag false: merely signal client that a new message arrived
     *
     * @param fd socket having message to read
     */
    void addToReadQueue(const fd_t fd)
    {
        auto it = m_clientBySocket.find(fd);
        if (it != m_clientBySocket.end())
        {
            assert(it->second);
            if (!it->second)
                return;
            if (it->second->getAutoReadFlag())
            {
                m_readQueue.push(fd);
            }
            else
            {
                it->second->onNewMessageToReadBase();
            }
        }
    }

    /**
     * @brief repeatedly read from all clients in queue, up to [_MaxReceivePerLoop] times
     *
     */
    void readSockets(void) noexcept
    {
        size_t receiveCount = _MaxReceivePerLoop;
        while (!m_readQueue.empty() && receiveCount-- > 0)
        {
            const fd_t socket = m_readQueue.front();
            m_readQueue.pop();
            readSocket(socket);
        }
    }

    /**
     * @brief network actorCallback, pops clients (by sockets) from queue (FIFO) and makes them send data
     * get client in queue, get data in client, send data, re-queue to send any remaining data at next loop iteration
     * the send opperation is non-blocking and can be segmented
     *
     */
    void onSendCallback(void) noexcept
    {
        size_t sendCount = _MaxSendPerLoop;
        while (!m_sendQueue.empty() && sendCount > 0)
        {
            // pop client from queue
            const fd_t socket = m_sendQueue.front();
            m_sendQueue.pop();
            const auto it1 = m_clientBySocket.find(socket);
            if (it1 == m_clientBySocket.end())
                continue;
            assert(it1->second);
            if (!it1->second)
                continue;
            // get data to send
            const auto     messageToSend = it1->second->getDataToSend();
            const uint8_t *dataToSend    = get<0>(messageToSend);
            const size_t   dataSize      = get<1>(messageToSend);
            if (dataSize > 0)
            {
                // send data
                const size_t sentDataSize    = _TNetworkCalls::send(socket, dataToSend, dataSize, 0);
                const bool   isRemainingData = it1->second->shiftBuffer(sentDataSize) > 0;
                if (isRemainingData)
                {
                    // re-queue socket to send remaining data at next loop iteration
                    m_sendQueue.push(socket);
                }
                if (--sendCount == 0)
                    break;
            }
        }
    }

    /**
     * @brief low-level method to register actorCallback only once without affecting the existing actorCallback order
     *
     * @tparam _Callback actorCallback type to register
     * @param callback to register
     */
    template <class _Callback> void registerCallbackOnce(_Callback &callback)
    {
        if (!callback.m_isRegisteredFlag)
        {
            registerCallback(callback);
            callback.m_isRegisteredFlag = true;
        }
    }

    /**
     * @brief actorCallback used for connecting registered clients
     *
     */
    struct ConnectCallback : public Actor::Callback
    {
        bool     m_isRegisteredFlag = false;
        Network &m_network;
        ConnectCallback(Network &network) : m_network(network) {}
        void onCallback(void)
        {
            m_isRegisteredFlag = false;
            m_network.onConnectCallback();
        }
    };

    /**
     * @brief actorCallback used for checking a pending connection's timeout
     *
     */
    struct PendingConnectionCallback : public Actor::Callback
    {
        bool     m_isRegisteredFlag = false;
        Network &m_network;
        PendingConnectionCallback(Network &network) : m_network(network) {}
        void onCallback(void)
        {
            m_isRegisteredFlag = false;
            m_network.onPendingConnectionCallback();
        }
    };

    /**
     * @brief actorCallback used to send data of registered clients
     *
     */
    struct SendCallback : public Actor::Callback
    {
        bool     m_isRegisteredFlag = false;
        Network &m_network;
        SendCallback(Network &network) : m_network(network) {}
        void onCallback(void)
        {
            m_isRegisteredFlag = false;
            m_network.onSendCallback();
        }
    };

    /**
     * @brief actorCallback putting registered servers in listening state
     *
     */
    struct ListenCallback : public Actor::Callback
    {
        bool     m_isRegisteredFlag = false;
        Network &m_network;
        ListenCallback(Network &network) : m_network(network) {}
        void onCallback(void)
        {
            m_isRegisteredFlag = false;
            m_network.onListenCallback();
        }
    };

    /**
     * @brief actorCallback used for evenet-polling sockets managed by this network
     *
     */
    struct EpollCallback : public Actor::Callback
    {
        bool     m_isRegisteredFlag = false;
        Network &m_network;
        EpollCallback(Network &network) : m_network(network) {}
        void onCallback(void)
        {
            m_isRegisteredFlag = false;
            m_network.onEpollCallback();
            m_network.registerCallbackOnce(*this);
        }
    };

#if __GNUC__ < 6
    // custom allocators don't work prior to gcc 6; use std allocator instead
    unordered_map<fd_t, TcpClient *>                                   m_clientBySocket;
    unordered_map<fd_t, tuple<steady_clock::time_point, microseconds>> m_connectionPending;

    queue<TcpClient *> m_connectQueue;
    queue<fd_t>        m_sendQueue;
    queue<fd_t>        m_readQueue;

    ConnectCallback           m_connectCallback;
    PendingConnectionCallback m_pendingConnectionCallback;
    SendCallback              m_sendCallback;
    EpollCallback             m_epollCallback;

    typename _TNetworkCalls::Epoll_event m_events[_MaxReceivePerLoop];
    unordered_map<fd_t, TcpServer *>     m_serverBySocket;
    unordered_set<fd_t>                  m_acceptingSocket;
    queue<TcpServer *>                   m_listenQueue;
#else
    // custom allocator
    unordered_map<fd_t, TcpClient *, hash<fd_t>, equal_to<fd_t>, Actor::Allocator<pair<const fd_t, TcpClient *>>>
        m_clientBySocket;
    unordered_map<fd_t, tuple<steady_clock::time_point, microseconds>, hash<fd_t>, equal_to<fd_t>,
                  Actor::Allocator<pair<const fd_t, tuple<steady_clock::time_point, microseconds>>>>
        m_connectionPending;

    queue<TcpClient *, deque<TcpClient *, Actor::Allocator<TcpClient *>>> m_connectQueue;
    queue<fd_t, deque<fd_t, Actor::Allocator<fd_t>>>                      m_sendQueue;
    queue<fd_t, deque<fd_t, Actor::Allocator<fd_t>>>                      m_readQueue;

    ConnectCallback           m_connectCallback;
    PendingConnectionCallback m_pendingConnectionCallback;
    SendCallback              m_sendCallback;
    EpollCallback             m_epollCallback;

    typename _TNetworkCalls::Epoll_event m_events[_MaxReceivePerLoop];
    unordered_map<fd_t, TcpServer *, hash<fd_t>, equal_to<fd_t>, Actor::Allocator<pair<const fd_t, TcpServer *>>>
                                                                            m_serverBySocket;
    unordered_set<fd_t, hash<fd_t>, equal_to<fd_t>, Actor::Allocator<fd_t>> m_acceptingSocket;
    queue<TcpServer *, deque<TcpServer *, Actor::Allocator<TcpServer *>>>   m_listenQueue;

#endif
    fd_t               m_epollSocket      = -1;
    const microseconds m_zeroMicrosecond  = microseconds(0);
    bool               m_coutCallbackFlag = true;
    ListenCallback     m_listenCallback;
};

} // namespace tcp

} // namespace connector

} // namespace tredzone