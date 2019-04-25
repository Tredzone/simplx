/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file client.hpp
 * @brief tcp client
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include "trz/connector/tcp/client/iclient.hpp"
#include "trz/connector/tcp/client/receiver.hpp"
#include "trz/connector/tcp/client/sender.hpp"

#include "simplx.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_set>

namespace tredzone
{
namespace connector
{
namespace tcp
{

// import into namespace
using ::std::equal_to;
using ::std::get;
using ::std::hash;
using ::std::make_tuple;
using ::std::ostringstream;
using ::std::size_t;
using ::std::string;
using ::std::tuple;
using ::std::unordered_set;
using ::std::chrono::microseconds;
using ::std::chrono::steady_clock;
using fd_t = int64_t;

/**
 * @brief class communicating with network 
 *
 * @tparam _TNetwork network actor managing network low-level connection and communication
 * @tparam _SendBufferSize
 * @tparam _ReceiveBufferSize
 */
template <class _TNetwork, size_t _SendBufferSize, size_t _ReceiveBufferSize>
class TcpClient : public Actor, public IClient<_TNetwork>
{
    using typename IClient<_TNetwork>::ConnectParam;

    public:
    /**
     * @brief client destruction request
     *
     */
    class DestroyRequestEvent : public Actor::Event
    {
    };

    /**
     * @brief incoming client destruction notification
     *
     */
    class DestroyNotificationEvent : public Actor::Event
    {
    };

    /**
     * @brief request client destruction notification
     *
     */
    class RegisterNotificationEvent : public Actor::Event
    {
    };

    /**
     * @brief instantiate new tcp client
     * if no _TNetwork exists on this core, instanciate it
     *
     * @throw _TNetwork::EpollCreateException
     */
    TcpClient() noexcept(false)
        : m_network(newReferencedSingletonActor<_TNetwork>()), m_receiver(*this), m_destroyCallback(*this),
          m_actorToNotifyOnDestroy(), m_pipe(*this)
    {
        registerEventHandler<DestroyRequestEvent>(*this);
        registerEventHandler<RegisterNotificationEvent>(*this);
    }

    /**
     * @brief callback triggered when client is asked to destroy itself
     *
     * @param e destroy request event
     */
    void onEvent(const DestroyRequestEvent &e)
    {
        (void)e;
        selfDestroy();
    }

    /**
     * @brief Set client socket 
     * used when client is spawned by server
     *
     * @param fd socket
     */
    void setFd(fd_t fd)
    {
        m_fd = fd;
        m_network->bindClientHandler(this, fd);
    }

    /**
     * @brief subscribe to destruction notification
     * used when client is spawned by server
     *
     * @param id actor id to send destruction notification to
     */
    void registerDestroyNotification(const ActorId &id) { m_actorToNotifyOnDestroy.insert(id); }

    /**
     * @brief callback triggered when client is asked to register a subscriber to destroy notification
     *
     * @param e subscriber actor id
     */
    void onEvent(const RegisterNotificationEvent &e) { registerDestroyNotification(e.getSourceActorId()); }

    /**
     * @brief callback triggered by engine to  requested actor destruction
     *
     */
    void onDestroyRequest(void) noexcept override
    {

        if (m_fd != FD_DISCONNECTED)
            disconnectBase();
        acceptDestroy();
    }

    /**
     * @brief register this client to be connected
     * calls setConnectionParam then register connect
     *
     * @param ipAddress ip to connect to
     * @param addressFamily ip address type AF_INET(IPv4), AF_INET6(IPv6)
     * @param port to connect to
     * @param timeoutUSec delay in microseconds before connection process timeout
     * @param messageHeaderSize minimum header size (0 for no header)
     * @param ipAddressSource adress of network interface to connect from ( *.*.*.* for any)
     */
    void registerConnect(const string &ipAddress, int64_t addressFamily, uint64_t port, uint64_t timeoutUSec,
                         size_t messageHeaderSize, const string &ipAddressSource = string()) override
    {
        setConnectParam(ipAddress, addressFamily, port, timeoutUSec, messageHeaderSize, ipAddressSource);
        registerConnect();
    }

    /**
     * @brief Set connection parameters
     *
     * @param ipAddress ip to connect to
     * @param addressFamily ip address type AF_INET(IPv4), AF_INET6(IPv6)
     * @param port to connect to
     * @param timeoutUSec delay in microseconds before connection process timeout
     * @param messageHeaderSize minimum header size (0 for no header)
     * @param ipAddressSource adress of network interface to connect from ( *.*.*.* for any)
     *
     * @throw AlreadyConnectedException
     */
    void setConnectParam(const string &ipAddress, int64_t addressFamily, uint64_t port, uint64_t timeoutUSec,
                         size_t messageHeaderSize, const string &ipAddressSource = string()) override
    {
        if (m_fd != FD_DISCONNECTED || m_connectingFlag)
            throw typename IClient<_TNetwork>::AlreadyConnectedException(getActorId(), m_connectParam);

        m_connectParam.m_ipAddressSource = ipAddressSource;
        m_connectParam.m_ipAddress       = ipAddress;
        m_connectParam.m_addressFamily   = addressFamily;
        m_connectParam.m_port            = port;
        m_connectParam.m_timeoutUSec     = timeoutUSec;
        m_timeout                        = microseconds(timeoutUSec);
        m_receiver.setHeaderSize(messageHeaderSize);
    }

    /**
     * @brief Get connection parameters
     *
     * @return const ConnectParam&
     */
    const ConnectParam &getConnectParam(void) const noexcept override
    {
        m_inConnectQueueFlag = false;
        return m_connectParam;
    }

    /**
     * @brief register client to be connected
     * connection will use parameters set using setConnectParam
     *
     * @throw AlreadyConnectedException
     */
    void registerConnect() override final
    {
        if (m_fd != FD_DISCONNECTED || m_connectingFlag)
            throw typename IClient<_TNetwork>::AlreadyConnectedException(getActorId(), m_connectParam);

        m_registrationTimePoint = steady_clock::now();
        m_connectingFlag        = true;
        if (!m_inConnectQueueFlag)
        {
            m_network->registerConnect(this);
            m_inConnectQueueFlag = true;
        }
    }

    /**
     * @brief close connection (high-level method)
     *
     */
    virtual void disconnect(void) noexcept override
    {
        // calls (low-level) disconnectBase to actually disconnect
        disconnectBase();
    }

    /**
     * @brief add data to send buffer and queue this buffer to be sent asynchronously by network,
     * parameters type conversion method
     *
     * @param data to send
     */
    void send(const uint8_t data) override { return send(&data, 1); }

    /**
     * @brief add data to send buffer and queue this buffer to be sent asynchronously by network,
     * parameters type conversion method
     *
     * @param data to send
     */
    void send(const string &data) override { return send(data.c_str(), data.size()); }

    /**
     * @brief add data to send buffer and queue this buffer to be sent asynchronously by network,
     * parameters type conversion method
     *
     * @param data to send
     * @param dataSize size of data to send
     */
    void send(const char *data, const size_t dataSize) override
    {
        return send(reinterpret_cast<const uint8_t *>(data), dataSize);
    }

    /**
     * @brief add data to send buffer and queue this buffer to be sent asynchronously by network,
     * calls sendBase to actually send data
     *
     * @param data to send
     * @param dataSize size of data to send
     */
    void send(const uint8_t *data, const size_t dataSize) override { sendBase(data, dataSize); }

    /**
     * @brief synchronously send data to network layer,
     * parameters type conversion method
     *
     * @param data to send
     * @param blocking flag, if true use a nonblocking send
     * @param retry number of try to send when socket would block
     */
    void directSend(const uint8_t data, bool blocking = false, ssize_t retry = 10) override
    {
        return directSend(&data, 1, blocking, retry);
    }

    /**     
     * @brief synchronously send data to network layer,
     * parameters type conversion method
     *
     * @param data to send
     * @param blocking flag, if false use a nonblocking send
     * @param retry number of send retries when socket would block
     */
    void directSend(const string &data, bool blocking = false, ssize_t retry = 10) override
    {
        return directSend(data.c_str(), data.size(), blocking, retry);
    }

    /**
     * @brief synchronously send data to network layer,
     * parameters type conversion method
     *
     * @param data to send
     * @param dataSize size of data to send
     * @param blocking flag, if false use a nonblocking send
     * @param retry number of send retries when socket would block
     */
    void directSend(const char *data, const size_t dataSize, bool blocking = false, ssize_t retry = 10) override
    {
        return directSend(reinterpret_cast<const uint8_t *>(data), dataSize, blocking, retry);
    }

    /**
     * @brief synchronously send data to network layer,
     * calls (low-level) directSendBase to actually send data
     *
     * @param data to send
     * @param dataSize size of data to send
     * @param blocking flag, if false use a nonblocking send
     * @param retry number of send retries when socket would block
     */
    void directSend(const uint8_t *data, const size_t dataSize, bool blocking = false, ssize_t retry = 10) override
    {
        directSendBase(data, dataSize, blocking, retry);
    }

    /**
     * @brief Get Sender Buffer Content Size
     *
     * @return size_t size of buffer content
     */
    size_t getSenderBufferContentSize(void) const noexcept override { return m_sender.getBufferContentSize(); }

    /**
     * @brief Get Receiver Buffer Content Size
     *
     * @return size_t size of buffer content
     */
    size_t getReceiverBufferContentSize(void) const noexcept override { return m_receiver.getBufferContentSize(); }

    /**
     * @brief flush sender buffer
     *
     */
    void clearSenderBuffer(void) noexcept override { m_sender.clearBuffer(); }

    /**
     * @brief flush receiver buffer
     *
     */
    void clearReceiverBuffer(void) noexcept override { m_receiver.clearBuffer(); }

    /**
     * @brief Get Fd
     *
     * @return fd_t socket ascociated to this client
     */
    fd_t getFd(void) const noexcept override { return m_fd; }

    /**
     * @brief register client automatic-destruction
     *
     */
    void destroy(void) noexcept override { selfDestroy(); }

    /**
     * @brief is any new message available in network buffer?
     *
     * @return true message is available
     * @return false no message is available
     */
    bool isNewMessageToRead(void) const noexcept override { return m_messageToReadFlag; }

    /**
     * @brief if message is available in network buffer,
     * read network socket until retrieved all available data or limit is reached
     *
     * @param limit maximum number of read operations
     */
    void readSocket(size_t limit) noexcept override
    {
        while (m_messageToReadFlag && limit-- > 0)
        {
            m_messageToReadFlag = false;
            m_network->readSocket(m_fd);
        }
    }

    /**
     * @brief Get Auto-Read flag
     *
     * @return true : network automatically reads new messages then triggers onDataReceived callback
     * @return false : network just sets MessageToRead flag then triggers onNewMessageToReadBase callback
     */
    bool getAutoReadFlag(void) const noexcept override { return m_autoreadFlag; }

    /**
     * @brief Set Auto Read flag
     * @param autoread true : network automatically reads new messages then triggers onDataReceived callback
     * false : network just sets MessageToRead flag then triggers onNewMessageToReadBase callback
     *
     * @param autoread flag value
     */
    void setAutoRead(const bool autoread) noexcept override { m_autoreadFlag = autoread; }

    /**
     * @brief Set Message Header Size
     *
     * @param messageHeaderSize
     */
    void setMessageHeaderSize(size_t messageHeaderSize) { m_receiver.setHeaderSize(messageHeaderSize); }

    friend Receiver<TcpClient, _ReceiveBufferSize>;
    friend _TNetwork;

    protected:
    /**
     * @brief callback triggered when auto-read flag is false and new message is received
     *
     */
    virtual void onNewMessageToRead(void) noexcept override {}

    /**
     * @brief Get Header Size
     *
     * @param data received
     * @return size_t current Header Size
     */
    virtual size_t getHeaderSize(const uint8_t *data) noexcept override
    {
        (void)data;
        return 0;
    }

    /**
     * @brief Retrieve data size
     *
     * @param data received
     * @param currentHeaderSize current Header Size
     * @return uint64_t data Size
     */
    virtual uint64_t getDataSize(const uint8_t *data, size_t currentHeaderSize) noexcept override
    {
        (void)data;
        (void)currentHeaderSize;
        return 0;
    }

    /**
     * @brief callback triggered after failed connection
     *
     */
    virtual void onConnectFailed(void) noexcept override {}

    /**
     * @brief callback triggered after connection timeout reached 
     *
     */
    virtual void onConnectTimedOut(void) noexcept override {}

    /**
     * @brief callback triggered on lost connection
     *
     */
    virtual void onConnectionLost(void) noexcept override {}

    /**
     * @brief callback triggered after connection success
     *
     */
    virtual void onConnect(void) noexcept override {}

    /**
     * @brief callback triggered after disconnection
     *
     */
    virtual void onDisconnect(void) noexcept override {}

    /**
     * @brief callback triggered after complete data (according to header) received
     *
     * @param data received
     * @param dataSize size of data
     */
    virtual void onDataReceived(const uint8_t *data, size_t dataSize) noexcept override
    {
        (void)data;
        (void)dataSize;
    }

    /**
     * @brief callback triggered after data is received but its size (according to header) would overflow receiver buffer. 
     * Method will be called for each data chunk received until entire payload has been received. Hence, 
     * buffering must be managed by client itself. 
     * Next header will be processed normally.
     *
     * @param data received (probably partial)
     * @param dataSize (probably partial)
     */
    virtual void onOverflowDataReceived(const uint8_t *data, size_t dataSize) noexcept override
    {
        (void)data;
        (void)dataSize;
    }

    /**
     * @brief Get Data To Send (sender buffer)
     *
     * @return tuple<const uint8_t *, size_t> buffer as a tuple data pointer and size
     */
    tuple<const uint8_t *, size_t> getDataToSend(void) noexcept override { return m_sender.getDataToSend(); }

    /**
     * @brief Get connection Timeout
     *
     * @return const microseconds& timeout
     */
    const microseconds &getTimeout(void) const noexcept override { return m_timeout; }

    /**
     * @brief Get Registration Time
     *
     * @return const steady_clock::time_point
     */
    const steady_clock::time_point getRegistrationTime(void) const noexcept override { return m_registrationTimePoint; }

    struct DestroyCallback : public Actor::Callback
    {
        TcpClient &m_actor;
        DestroyCallback(TcpClient &actor) : m_actor(actor) {}
        void onCallback(void) { m_actor.selfDestroy(); }
    };

    /**
     * @brief disconnection process low-level part
     * actually disconnects the socket
     *
     */
    void disconnectBase(void) noexcept override final
    {
        clearSenderBuffer();
        m_connectingFlag = false;
        if (m_fd != FD_DISCONNECTED)
        {
            m_network->disconnect(m_fd);
            m_fd = FD_DISCONNECTED;
            onDisconnect();
        }
        selfDestroy();
    }

    /**
     * @brief low-level, asynchronously send data
     *
     * @param data to send
     * @param dataSize to send
     *
     * @throw NotConnectedException
     * @throw SendBufferFullException
     */
    void sendBase(const uint8_t *data, const size_t dataSize) override final
    {
        if (m_fd == FD_DISCONNECTED)
            throw typename IClient<_TNetwork>::NotConnectedException(getActorId());

        if (!m_sender.addToBuffer(data, dataSize))
            throw typename IClient<_TNetwork>::SendBufferFullException(getActorId());

        if (!m_inSendQueueFlag)
        {
            m_network->addToSendQueue(m_fd);
            m_inSendQueueFlag = true;
        }
    }

    /**
     * @brief low-level directSend method that actually send data (sync)
     *
     * @param data to send
     * @param dataSize to send
     * @param blocking flag, if false use a nonblocking send
     * @param retry number of try to send when socket would block
     *
     * @throw NotConnectedException
     */
    void directSendBase(const uint8_t *data, const size_t dataSize, bool blocking = false,
                        ssize_t retry = 10) override final
    {
        if (m_fd == FD_DISCONNECTED)
            throw typename IClient<_TNetwork>::NotConnectedException(getActorId());

        if (blocking)
            m_network->blockingDirectSend(m_fd, data, dataSize, retry);
        else
            m_network->directSend(m_fd, data, dataSize, retry);
    }

    /**
     * @brief disconnect socket
     * register to destruction at next callback
     * send notification at all actors subscribed
     * request to destroy
     *
     */
    void selfDestroy(void)
    {

        if (m_fd != FD_DISCONNECTED)
            disconnect();
        requestDestroy();
        registerCallback(m_destroyCallback);
        for (const ActorId &id : m_actorToNotifyOnDestroy)
        {
            m_pipe.setDestinationActorId(id);
            m_pipe.push<DestroyNotificationEvent>();
        }
    }

    /**
     * @brief clear part of sender buffer
     *
     * @param dataSize size of data to clear from buffer
     * @return size_t size of remaining data in buffer
     */
    size_t shiftBuffer(size_t dataSize) noexcept override final
    {
        size_t dataSizeStillInBuffer = m_sender.shiftBuffer(dataSize);
        if (dataSizeStillInBuffer == 0)
            m_inSendQueueFlag = false;
        return dataSizeStillInBuffer;
    }

private:
    /**
     * @brief low-level callback triggered by network when auto-read flag is false and a new message is received
     * updates internal state then calls higher-level-callback onNewMessageToRead
     *
     */
    void onNewMessageToReadBase(void) noexcept override final
    {
        m_messageToReadFlag = true;
        onNewMessageToRead();
    }

    /**
     * @brief low-level callback triggered upon connection
     * stores socket bound to connection then calls higher-level onConnect()
     *
     * @param fd connection socket
     */
    void onConnectBase(const fd_t fd) noexcept override final
    {
        m_connectingFlag = false;
        m_fd             = fd;
        onConnect();
    }

    /**
     * @brief low-level callback triggered upon connection timeout
     * updates internal state then calls higher-level onConnectTimedOut()
     *
     */
    void onConnectTimedOutBase(void) noexcept override final
    {
        m_connectingFlag = false;
        m_fd             = FD_DISCONNECTED;
        m_sender.clearBuffer();
        onConnectTimedOut();
    }

    /**
     * @brief low-level callback triggered upon failed connection attempt
     * calls (high-level) onConnectFailed
     *
     */
    void onConnectFailedBase(void) noexcept override final
    {
        m_connectingFlag = false;
        m_fd             = FD_DISCONNECTED;
        m_sender.clearBuffer();
        onConnectFailed();
    }

    /**
     * @brief low-level callback triggered upon lost connection
     * calls (high-level) onConnectionLost
     *
     */
    void onConnectionLostBase(void) noexcept override final
    {
        m_fd = FD_DISCONNECTED;
        m_sender.clearBuffer();
        onConnectionLost();
    }

    /**
     * @brief low-level callback triggered upon disconnection
     * calls (high-level) onDisconnect
     *
     */
    void onDisconnectBase(void) noexcept override final
    {
        m_fd = FD_DISCONNECTED;
        m_sender.clearBuffer();
        onDisconnect();
    }

    /**
     * @brief low-level callback triggered (if [AutoRead flag] is true) upon new data received
     * send data to receiver 
     *
     * @param data received
     * @param dataSize size of data received
     */
    void onDataReceivedBase(const uint8_t *data, size_t dataSize) noexcept override final
    {
        m_receiver.onDataReceived(data, dataSize);
    }

    protected:
    ConnectParam                            m_connectParam;
    ActorReference<_TNetwork>               m_network;
    Sender<_SendBufferSize>                 m_sender;
    Receiver<TcpClient, _ReceiveBufferSize> m_receiver;
    DestroyCallback                         m_destroyCallback;

    unordered_set<ActorId> m_actorToNotifyOnDestroy;

    Event::Pipe m_pipe;

    microseconds             m_timeout;
    steady_clock::time_point m_registrationTimePoint;

    bool         m_connectingFlag     = false;
    bool         m_inSendQueueFlag    = false;
    bool         m_autoreadFlag       = true;
    bool         m_messageToReadFlag  = false;
    mutable bool m_inConnectQueueFlag = false;

    private:
    fd_t                     m_fd            = FD_DISCONNECTED;
    static constexpr int64_t FD_DISCONNECTED = -1;
};
} // namespace tcp

} // namespace connector

} // namespace tredzone
