/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file network.hpp
 * @brief tcp network actor; listen (by system call) client(s) to accept and
 * openconnection
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
 * @brief actor that manage the connection and communication to the network
 *
 * @tparam _MaxReceivePerLoop the maximum read to do each loop
 * @tparam _MaxSendPerLoop the maximum write to do each loop
 * @tparam _MaxDataPerRead the maximum data to get each read
 * @tparam _MaxPendingIncomingConnectionRequest the maximum pending excternal connection
 */
template <size_t _MaxReceivePerLoop, size_t _MaxSendPerLoop, size_t _MaxDataPerRead,
          size_t _MaxPendingIncomingConnectionRequest, class _TNetworkCalls = NetworkCalls>
class Network : public Actor
{
    using TcpClient = IClient<Network>;
    using TcpServer = IServer<Network>;

    public:
    /**
     * @brief the exception is trown when the creation of epoll file description fails
     *
     */
    class EpollCreateException : public exception
    {
        public:
        virtual const char *what() const throw() { return "Couldn't create epollSocket"; }
    };

    /**
     * @brief the exception is trown when a direct send fails
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

#if __GNUC__ < 6
    // use the default std allocator
    /**
     * @brief Construct a new Network
     *
     */
    Network(void) noexcept(false)
        : m_clientBySocket(), m_connectionPending(), m_connectQueue(), m_sendQueue(), m_readQueue(),
          m_connectCallback(*this), m_pendingConnectionCallback(*this), m_sendCallback(*this), m_epollCallback(*this),
          m_serverBySocket(), m_acceptingSocket(), m_listenQueue(), m_listenCallback(*this)
    {
        m_epollSocket = _TNetworkCalls::epoll_create1(0);
        if (m_epollSocket == -1)
        {
            throw EpollCreateException();
        }
    }
#else
    /**
     * @brief Construct a new Network
     *
     */
    Network(void) noexcept(false)
        : m_clientBySocket(getAllocator()), m_connectionPending(getAllocator()), m_connectQueue(getAllocator()),
          m_sendQueue(getAllocator()), m_readQueue(getAllocator()), m_connectCallback(*this),
          m_pendingConnectionCallback(*this), m_sendCallback(*this), m_epollCallback(*this),
          m_serverBySocket(getAllocator()), m_acceptingSocket(getAllocator()), m_listenQueue(getAllocator()),
          m_listenCallback(*this)
    {
        m_epollSocket = _TNetworkCalls::epoll_create1(0);
        if (m_epollSocket == -1)
        {
            throw EpollCreateException();
        }
    }
#endif

    /**
     * @brief stop the network
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
        // disconnect all server
        while (!m_serverBySocket.empty())
        {
            const auto it = m_serverBySocket.begin();
            onDisconnected(it->first);
        }
    }

    /**
     * @brief callback called when the actor is requested to destroy itself
     *
     */
    void onDestroyRequest(void) noexcept override
    {
        stopNetwork();
        acceptDestroy();
    }

    /**
     * @brief Destroy the Network
     *
     */
    virtual ~Network(void) noexcept
    {
        stopNetwork();
        _TNetworkCalls::close(m_epollSocket);
    }

    /**
     * @brief register a client to be connected
     * then on its callback the network will connect clients in queue
     *
     * @param client to asynchronously connect
     */
    void registerConnect(TcpClient *client) noexcept
    {
        assert(client);
        if (!client)
            return;
        // add the client to the queue
        m_connectQueue.push(client);
        // register callback where network connect queued client
        registerCallbackOnce(m_connectCallback);
    }

    /**
     * @brief register a new server to listen
     * then on its callback the network will connect servers in queue
     *
     * @param server
     */
    void registerListen(TcpServer *server) noexcept
    {
        assert(server);
        if (!server)
            return;
        // add the server to the queue
        m_listenQueue.push(server);
        // register callback where network connect queued server
        registerCallbackOnce(m_listenCallback);
    }

    /**
     * @brief synchronously close the socket then remove the client/server associated
     *
     * @param socket to disconnect
     * @return true when socket was connected
     * @return false when socket wasn't connected
     */
    bool disconnect(const fd_t socket) noexcept
    {
        // close the socket
        const bool wasConnected =
            (_TNetworkCalls::shutdown(socket, _TNetworkCalls::Shut_rdwr) == 0 && _TNetworkCalls::close(socket) == 0);

        // remove client or server bound to the socket
        m_clientBySocket.erase(socket);
        m_serverBySocket.erase(socket);

        // cancel callback not animore needed
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
     * @brief add a client (identified by socket) to the send queue
     * then on its callback the network will dequeue by getting the clients' buffers
     * and writing it on the socket
     *
     * @param socket (identifier of the client) to put in queue
     */
    void addToSendQueue(const fd_t socket) noexcept
    {
        m_sendQueue.push(socket);
        registerCallbackOnce(m_sendCallback);
    }

    /**
     * @brief synchronously write "datasize" amount of "data" on the "socket" in a maximum of "retryNumber" attempts.
     * As oppose to blockingDirectSend, here the write opperation is non-blocking.
     * /!\ can importantly decrease performance for multi client/server use on the same core, prefere addToSendQueue.
     *
     * @param socket to send on
     * @param data to send
     * @param dataSize the amount ot data to send
     * @param retryNumber the amount of attempt to send
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
     * @brief synchronously write "datasize" amount of "data" on the "socket" in a maximum of "retryNumber" attempts.
     * As oppose to directSend, here the write opperation is blocking.
     * /!\ can importantly decrease performance for multi client/server use the on same core, prefere addToSendQueue.
     *
     * @param socket to send on
     * @param data to send
     * @param dataSize the amount ot data to send
     * @param retryNumber the amount of attempt to send
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
     * @brief bind a client to a socket
     * when a server spawn a client (server process) to handle an accepted socket
     *
     * @param client the handler
     * @param fd
     */
    void bindClientHandler(TcpClient *client, const fd_t fd)
    {
        assert(client);
        if (!client)
        {
        }
        m_clientBySocket.emplace(fd, client);
        m_connectionPending.emplace(fd, make_tuple(steady_clock::now(), client->getTimeout()));
        registerCallbackOnce(m_pendingConnectionCallback);

        if (!m_clientBySocket.empty() || !m_serverBySocket.empty())
            registerCallbackOnce(m_epollCallback);
    }

    /**
     * @brief read the socket using a non-blocking receive operation
     * must be called directly by the client with auto read desable
     *
     * @param socket to read
     */
    void readSocket(const fd_t socket) noexcept
    {
        array<uint8_t, _MaxDataPerRead> dataToReceive;

        // read _MaxDataPerRead amount of data on the socket
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
                // if all data has not been read queue the socket for a further read
                if (bytes_read == _MaxDataPerRead)
                    addToReadQueue(socket);
            }
        }
        else
        {
            // if the socket was not ready queue the socket for a further read
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
     * @brief callback called just after a timed out of a socket connection occurs
     * notify handler of the state changes through its callbacks, then remove it
     *
     * @param socket that was trying to connect
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
     * @brief callback called just after a connection of a socket has been lost
     * notify handler of the state changes through its callbacks, then remove it
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
     * @brief callback called just after a disconnection of a socket
     * notify handler of the state changes through its callbacks, then remove it
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
     * @brief callback called just after a internal socket successfully connect
     * notify handler of the state changes through its callbacks
     * then register to the epoll callback
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
     * @brief network actorCallback that gets clients from queue (FIFO) and connects them
     * get client in queue, get param in client, create the socket, connect it
     * the connect opperation is non-blocking
     *
     */
    void onConnectCallback(void) noexcept
    {
        while (!m_connectQueue.empty())
        {
            // get client from queue
            TcpClient *client = m_connectQueue.front();
            m_connectQueue.pop();
            assert(client);
            if (!client)
                continue;

            // get param in client
            const auto     params        = client->getConnectParam();
            const uint64_t addressFamily = params.m_addressFamily;
            const string & ipAddress     = params.m_ipAddress;
            const int16_t  port          = params.m_port;

            typename _TNetworkCalls::Sockaddr_in serv_addr;
            // store char 0 and not 0x00
            ::memset(&serv_addr, '0', sizeof(serv_addr));
            serv_addr.sin_family = addressFamily;
            serv_addr.sin_port   = _TNetworkCalls::wrapped_htons(port);

            // Convert IP addresses from text to binary form
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

            // trying to bind to a specific network interface (and optionally a specific local port)
            if (!params.m_ipAddressSource.empty())
            {
                typename _TNetworkCalls::Sockaddr_in localaddr;
                localaddr.sin_family      = addressFamily;
                localaddr.sin_addr.s_addr = _TNetworkCalls::inet_addr(params.m_ipAddressSource.c_str());
                localaddr.sin_port        = 0; // local port 0 for any
                _TNetworkCalls::bind(newSocket, &localaddr, sizeof(localaddr));
            }

            // set socket non-blocking for connection
            if (!_TNetworkCalls::setSocketNonBlocking(newSocket))
            {
                _TNetworkCalls::close(newSocket);
                client->onConnectFailedBase();
                continue;
            }

            // add socket to the epoll
            typename _TNetworkCalls::Epoll_event epollEvent;
            epollEvent.events  = _TNetworkCalls::Epollout | _TNetworkCalls::Epollin | _TNetworkCalls::Epollet;
            epollEvent.data.fd = newSocket;

            if (_TNetworkCalls::epoll_ctl(m_epollSocket, _TNetworkCalls::Epoll_ctl_add, newSocket, &epollEvent))
            {
                _TNetworkCalls::close(newSocket);
                client->onConnectFailedBase();
                continue;
            }

            // associate the socket to the handler (client actor)
            m_clientBySocket.emplace(newSocket, client);
            m_connectionPending.emplace(newSocket, make_tuple(client->getRegistrationTime(), client->getTimeout()));

            // connect (non-blocking)
            const int64_t connectResult = _TNetworkCalls::connect(newSocket, &serv_addr, sizeof(serv_addr));

            if (connectResult < 0 && errno != _TNetworkCalls::Einprogress)
            {
                onDisconnected(newSocket);
                continue;
            }

            // register actor callbacks (to complete connection or to timed out)
            registerCallbackOnce(m_epollCallback);
            registerCallbackOnce(m_pendingConnectionCallback);

            // connection ready to be completed
            if (connectResult >= 0)
            {
                onCompleteConnection(newSocket);
            }
        }
    }

    /**
     * @brief network actorCallback checking all pending connection for timed out
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
     * @brief network actorCallback getting server from queue (FIFO) and make them listening
     * get server in queue, get param in server, create the socket, start listening
     * the listening socket is set to non-blocking
     *
     */
    void onListenCallback(void) noexcept
    {
        while (!m_listenQueue.empty())
        {
            // get server
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

            // trying to bind to a specific network interface (and optionally a specific local port)
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

            // add socket to the epoll
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

            // associate the socket to the handler (client actor)
            m_serverBySocket.emplace(serverSocket, server);
            server->onListenSucceedBase(serverSocket);
        }
        if (!m_serverBySocket.empty())
            registerCallbackOnce(m_epollCallback);
    }

    /**
     * @brief callback called when a listening socket receive a new connection from a client
     *
     * @param serverSocket the listening socket
     * @param server the actor handling this listening socket
     */
    void onNewConnection(fd_t serverSocket, TcpServer *server) noexcept
    {
        assert(server);
        if (!server)
            return;
        // accept the connection to get infos about the client
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
            // the non-blocking server socket is not yet ready to accept just return until a further try succeed
            return;
        }

        // adding the new socket to the epoll
        typename _TNetworkCalls::Epoll_event event;
        event.events  = _TNetworkCalls::Epollout;
        event.data.fd = fd;
        if (_TNetworkCalls::epoll_ctl(m_epollSocket, _TNetworkCalls::Epoll_ctl_add, fd, &event))
        {
            _TNetworkCalls::close(fd);
            server->onAcceptFailed();
        }

        // callback the server to  handle this new connection
        server->onNewConnectionBase(fd, clientIp);
    }

    /**
     * @brief network actorCallback checking any change on sockets (server and client) it manages
     * check the epoll and if an event appened on it, execute the good behavior.
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
            // error on socket
            if (m_events[i].events & _TNetworkCalls::Epollerr)
            {
                // normal disconnection
                if (m_events[i].events & _TNetworkCalls::Epollhup)
                {
                    onDisconnected(m_events[i].data.fd);
                }
                // unplaned disconnection
                else
                {
                    onConnectionLost(m_events[i].data.fd);
                }
            }
            // outgoing event (connection)
            else if (m_events[i].events & _TNetworkCalls::Epollout)
            {
                // check pending connection
                const auto it1 = m_connectionPending.find(m_events[i].data.fd);
                if (it1 != m_connectionPending.end())
                {
                    m_connectionPending.erase(it1);
                    // complete connection connecting
                    if (!onCompleteConnection(m_events[i].data.fd))
                        continue;

                    // switching socket event in epoll to communication
                    typename _TNetworkCalls::Epoll_event event;
                    event.events  = _TNetworkCalls::Epollin | _TNetworkCalls::Epollet;
                    event.data.fd = m_events[i].data.fd;
                    if (_TNetworkCalls::epoll_ctl(m_epollSocket, _TNetworkCalls::Epoll_ctl_mod, m_events[i].data.fd,
                                                  &event) < 0)
                    {
                        // when failing, complete connection disconnecting
                        auto it2 = m_clientBySocket.find(m_events[i].data.fd);
                        if (it2 != m_clientBySocket.end())
                        {
                            onConnectionLost(m_events[i].data.fd);
                        }
                    }
                }
            }
            // incoming event (communication: new msg / incoming)
            else if (m_events[i].events & _TNetworkCalls::Epollin)
            {
                // if the socket is a server one, this is a connection request
                const auto it = m_serverBySocket.find(m_events[i].data.fd);
                if (it != m_serverBySocket.end())
                {
                    onNewConnection(it->first, it->second);
                }
                // else this is a new message
                else
                {
                    // treat socket having message to read
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
     * @brief add a socket having message to the read queue if its handler is set to auto read
     * otherwise signal to the client that a new message arrived
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
     * @brief run the read of all clients in queue until _MaxReceivePerLoop is reach
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
     * @brief network actorCallback that gets clients (by sockets) from queue (FIFO) and make them send data
     * get client in queue, get data in client, send data, queue for sending the remaining data
     * the send opperation is non-blocking then can be partial
     *
     */
    void onSendCallback(void) noexcept
    {
        size_t sendCount = _MaxSendPerLoop;
        while (!m_sendQueue.empty() && sendCount > 0)
        {
            // getting the client from queue
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
                    // queue socket for sending the remaining data
                    m_sendQueue.push(socket);
                }
                if (--sendCount == 0)
                    break;
            }
        }
    }

    /**
     * @brief technical method to register actorCallback only once and not change the actorCallback order
     *
     * @tparam _Callback the actorCallback type to register
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
     * @brief the actorCallback used to connect registered clients
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
     * @brief the actorCallback used to check the timeout of pending connections
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
     * @brief the actorCallback used to send data of registered clients
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
     * @brief the actorCallback used to start listening of registered servers
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
     * @brief the actorCallback used to check any event on the sockets managed by this network
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

    // beffore gcc 6 the tredzone allocator seems not working: disabling it
#if __GNUC__ < 6
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
}; // namespace tcp
} // namespace tcp
} // namespace connector
} // namespace tredzone