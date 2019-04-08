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
 * @brief communication class to communicate through a network.
 *
 * @tparam _TNetwork the network actor that manages the connection and communication to the network (low level)
 * @tparam _SendBufferSize
 * @tparam _ReceiveBufferSize
 */
template <class _TNetwork, size_t _SendBufferSize, size_t _ReceiveBufferSize>
class TcpClient : public Actor, public IClient<_TNetwork>
{
    using typename IClient<_TNetwork>::ConnectParam;

    public:
    /**
     * @brief event to ask the destruction of a client
     *
     */
    class DestroyRequestEvent : public Actor::Event
    {
    };

    /**
     * @brief event to notify of an incoming destruction of the client
     *
     */
    class DestroyNotificationEvent : public Actor::Event
    {
    };

    /**
     * @brief event to be added to the list of actor notified of the destruction
     *
     */
    class RegisterNotificationEvent : public Actor::Event
    {
    };

    /**
     * @brief Construct a new Tcp Client
     * if no _TNetwork exist on this core, instanciate it
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
     * @brief callback called when the client is asked to destroy itself
     *
     * @param e the destroy request event
     */
    void onEvent(const DestroyRequestEvent &e)
    {
        (void)e;
        selfDestroy();
    }

    /**
     * @brief Set the socket for this client
     * used when this client is spawned by server
     *
     * @param fd the socket
     */
    void setFd(fd_t fd)
    {
        m_fd = fd;
        m_network->bindClientHandler(this, fd);
    }

    /**
     * @brief subscribe to destruction notification
     * used when this client is spawned by server
     *
     * @param id the actor id to send the destroy notification to
     */
    void registerDestroyNotification(const ActorId &id) { m_actorToNotifyOnDestroy.insert(id); }

    /**
     * @brief callback called when the client is asked to register a subscriber to destroy notification
     *
     * @param e the register request event containing the actor id of the subscriber
     */
    void onEvent(const RegisterNotificationEvent &e) { registerDestroyNotification(e.getSourceActorId()); }

    /**
     * @brief callback called when the actor is requested to destroy itself
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
     * @param ipAddress the ip to connect to
     * @param addressFamily ip address type AF_INET(IPv4) - AF_INET6(IPv6)
     * @param port the port to connect to
     * @param timeoutUSec the delay in microSecond before timeout the connection process
     * @param messageHeaderSize the minimum size of the header (0 means no header)
     * @param ipAddressSource the adress of the interface to use to connect from ( *.*.*.* for any)
     */
    void registerConnect(const string &ipAddress, int64_t addressFamily, uint64_t port, uint64_t timeoutUSec,
                         size_t messageHeaderSize, const string &ipAddressSource = string()) override
    {
        setConnectParam(ipAddress, addressFamily, port, timeoutUSec, messageHeaderSize, ipAddressSource);
        registerConnect();
    }

    /**
     * @brief Set the Connection Parameters object
     *
     * @param ipAddress the ip to connect to
     * @param addressFamily ip address type AF_INET(IPv4) - AF_INET6(IPv6)
     * @param port the port to connect to
     * @param timeoutUSec the delay in microSecond before timeout the connection process
     * @param messageHeaderSize the minimum size of the header (0 means no header)
     * @param ipAddressSource the adress of the interface to use to connect from ( *.*.*.* for any)
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
     * @brief Get the Connect Parameters object
     *
     * @return const ConnectParam&
     */
    const ConnectParam &getConnectParam(void) const noexcept override
    {
        m_inConnectQueueFlag = false;
        return m_connectParam;
    }

    /**
     * @brief register this client to be connected
     * the connection will use the parameters set using setConnectParam
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
     * @brief hight level method closing the connection.
     *
     */
    virtual void disconnect(void) noexcept override
    {
        // Calls disconnectBase to effectively disconnect
        disconnectBase();
    }

    /**
     * @brief add data to the send buffer and register (async) this buffer to be sent by network
     * wrapper method for type conversion of parameters
     *
     * @param data the data to send
     */
    void send(const uint8_t data) override { return send(&data, 1); }

    /**
     * @brief add data to the send buffer and register (async) this buffer to be sent by network
     * wrapper method for type conversion of parameters
     *
     * @param data the data to send
     */
    void send(const string &data) override { return send(data.c_str(), data.size()); }

    /**
     * @brief add data to the send buffer and register (async) this buffer to be sent by network
     * wrapper method for type conversion of parameters
     *
     * @param data the data to send
     * @param dataSize the size of the data to send
     */
    void send(const char *data, const size_t dataSize) override
    {
        return send(reinterpret_cast<const uint8_t *>(data), dataSize);
    }

    /**
     * @brief add data to the send buffer and register (async) this buffer to be sent by network
     * calls sendBase to effectively send the data
     *
     * @param data the data to send
     * @param dataSize the size of the data to send
     */
    void send(const uint8_t *data, const size_t dataSize) override { sendBase(data, dataSize); }

    /**
     * @brief directly send data (sync) through the network
     * wrapper method for type conversion of parameters
     *
     * @param data the data to send
     * @param blocking if true use a nonblocking send
     * @param retry number of try to send when the socket would block
     */
    void directSend(const uint8_t data, bool blocking = false, ssize_t retry = 10) override
    {
        return directSend(&data, 1, blocking, retry);
    }

    /**
     * @brief directly send data (sync) through the network
     * wrapper method for type conversion of parameters
     *
     * @param data the data to send
     * @param blocking if true use a nonblocking send
     * @param retry number of try to send when the socket would block
     */
    void directSend(const string &data, bool blocking = false, ssize_t retry = 10) override
    {
        return directSend(data.c_str(), data.size(), blocking, retry);
    }

    /**
     * @brief directly send data (sync) through the network
     * wrapper method for type conversion of parameters
     *
     * @param data the data to send
     * @param dataSize the size of the data to send
     * @param blocking if true use a nonblocking send
     * @param retry number of try to send when the socket would block
     */
    void directSend(const char *data, const size_t dataSize, bool blocking = false, ssize_t retry = 10) override
    {
        return directSend(reinterpret_cast<const uint8_t *>(data), dataSize, blocking, retry);
    }

    /**
     * @brief directly send data (sync) through the network
     * calls directSendBase to effectively send the data
     *
     * @param data the data to send
     * @param dataSize the size of the data to send
     * @param blocking if true use a nonblocking send
     * @param retry number of try to send when the socket would block
     */
    void directSend(const uint8_t *data, const size_t dataSize, bool blocking = false, ssize_t retry = 10) override
    {
        directSendBase(data, dataSize, blocking, retry);
    }

    /**
     * @brief Get the Sender Buffer Content Size
     *
     * @return size_t size of the buffer content
     */
    size_t getSenderBufferContentSize(void) const noexcept override { return m_sender.getBufferContentSize(); }

    /**
     * @brief Get the Receiver Buffer Content Size
     *
     * @return size_t size of the buffer content
     */
    size_t getReceiverBufferContentSize(void) const noexcept override { return m_receiver.getBufferContentSize(); }

    /**
     * @brief empty the sender buffer
     *
     */
    void clearSenderBuffer(void) noexcept override { m_sender.clearBuffer(); }

    /**
     * @brief empty the receiver buffer
     *
     */
    void clearReceiverBuffer(void) noexcept override { m_receiver.clearBuffer(); }

    /**
     * @brief Get the Fd object
     *
     * @return fd_t the socket ascociated to this client
     */
    fd_t getFd(void) const noexcept override { return m_fd; }

    /**
     * @brief register object to self destroy
     *
     */
    void destroy(void) noexcept override { selfDestroy(); }

    /**
     * @brief check if a new message is available in the buffer
     *
     * @return true message is available
     * @return false no message is available
     */
    bool isNewMessageToRead(void) const noexcept override { return m_messageToReadFlag; }

    /**
     * @brief if a message is available,
     * read the socket through the network until no message are available or limit is reached
     *
     * @param limit maximum number of read to process
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
     * @brief Get the Auto Read flag
     *
     * @return true network automaticaly read new message then callback onDataReceived
     * @return false network just set the flag MessageToRead then run the callback onNewMessageToReadBase
     */
    bool getAutoReadFlag(void) const noexcept override { return m_autoreadFlag; }

    /**
     * @brief Set the Auto Read flag
     * true: network automaticaly read new message then callback onDataReceived
     * false: network just set the flag MessageToRead then run the callback onNewMessageToReadBase
     *
     * @param autoread flag value
     */
    void setAutoRead(const bool autoread) noexcept override { m_autoreadFlag = autoread; }

    /**
     * @brief Set the Message Header Size
     *
     * @param messageHeaderSize
     */
    void setMessageHeaderSize(size_t messageHeaderSize) { m_receiver.setHeaderSize(messageHeaderSize); }

    friend Receiver<TcpClient, _ReceiveBufferSize>;
    friend _TNetwork;

    protected:
    /**
     * @brief callback called when auto read flag is false and a new message is received
     *
     */
    virtual void onNewMessageToRead(void) noexcept override {}

    /**
     * @brief Get the Header Size
     *
     * @param data the data received
     * @return size_t the current Header Size
     */
    virtual size_t getHeaderSize(const uint8_t *data) noexcept override
    {
        (void)data;
        return 0;
    }

    /**
     * @brief Get the Data Size using the header
     *
     * @param data the data received
     * @param currentHeaderSize the current Header Size
     * @return uint64_t the data Size
     */
    virtual uint64_t getDataSize(const uint8_t *data, size_t currentHeaderSize) noexcept override
    {
        (void)data;
        (void)currentHeaderSize;
        return 0;
    }

    /**
     * @brief callback called after a connection failed
     *
     */
    virtual void onConnectFailed(void) noexcept override {}

    /**
     * @brief callback called after the connection atempt reached timeout
     *
     */
    virtual void onConnectTimedOut(void) noexcept override {}

    /**
     * @brief callback called when the connection has been lost
     *
     */
    virtual void onConnectionLost(void) noexcept override {}

    /**
     * @brief callback called after a successful connection
     *
     */
    virtual void onConnect(void) noexcept override {}

    /**
     * @brief callback called after a disconnection
     *
     */
    virtual void onDisconnect(void) noexcept override {}

    /**
     * @brief callback called after a completed data (according to header) is received
     *
     * @param data the data received
     * @param dataSize the size of the data
     */
    virtual void onDataReceived(const uint8_t *data, size_t dataSize) noexcept override
    {
        (void)data;
        (void)dataSize;
    }

    /**
     * @brief callback called after a data is received but its size (according to header) is bigger than the receiver
     * buffer this method will be called for each data received until the all "to big data" has been received so the
     * client has to manage the buffering. then the next header is read and if the next data fit the buffer size
     * onDataReceived is used
     *
     * @param data the (probably partial) data received
     * @param dataSize the (probably partial) data size
     */
    virtual void onOverflowDataReceived(const uint8_t *data, size_t dataSize) noexcept override
    {
        (void)data;
        (void)dataSize;
    }

    /**
     * @brief Get the Data To Send (sender buffer)
     *
     * @return tuple<const uint8_t *, size_t> the buffer as a tuple of data pointer and the size of the data
     */
    tuple<const uint8_t *, size_t> getDataToSend(void) noexcept override { return m_sender.getDataToSend(); }

    /**
     * @brief Get the connection Timeout
     *
     * @return const microseconds& the timeout
     */
    const microseconds &getTimeout(void) const noexcept override { return m_timeout; }

    /**
     * @brief Get the Registration Time
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
     * @brief technical part of the disconnection process.
     * effectivelly disconnect must be called by disconnect
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
     * @brief technical send method that effectively send the data (async)
     *
     * @param data the data to send
     * @param dataSize the size of the data to send
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
     * @brief technical directSend method that effectively send the data (sync)
     *
     * @param data the data to send
     * @param dataSize the size of the data to send
     * @param blocking if true use a nonblocking send
     * @param retry number of try to send when the socket would block
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
     * @brief disconnect the socket
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
     * @brief clear a part of sender buffer
     *
     * @param dataSize the size of the data to clear from buffer
     * @return size_t the size of the remaining data in the buffer
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
     * @brief technical callback called by network when auto read flag is false and a new message is received
     * define flags then call the higher level callback onNewMessageToRead
     *
     */
    void onNewMessageToReadBase(void) noexcept override final
    {
        m_messageToReadFlag = true;
        onNewMessageToRead();
    }

    /**
     * @brief technical callback called just after a connection.
     * define the socket bound to the connection
     * then call the higher level callback onConnect
     *
     * @param fd socket of connection
     */
    void onConnectBase(const fd_t fd) noexcept override final
    {
        m_connectingFlag = false;
        m_fd             = fd;
        onConnect();
    }

    /**
     * @brief technical callback called just after connection atempt timed out
     * define flags then call the higher level callback onConnectTimedOut
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
     * @brief technical callback called just after connection atempt failed
     * define flags then call the higher level callback onConnectFailed
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
     * @brief technical callback called just after connection has been lost
     * define flags then call the higher level callback onConnectionLost
     *
     */
    void onConnectionLostBase(void) noexcept override final
    {
        m_fd = FD_DISCONNECTED;
        m_sender.clearBuffer();
        onConnectionLost();
    }

    /**
     * @brief technical callback called just after the disconnection
     * define flags then call the higher level callback onDisconnect
     *
     */
    void onDisconnectBase(void) noexcept override final
    {
        m_fd = FD_DISCONNECTED;
        m_sender.clearBuffer();
        onDisconnect();
    }

    /**
     * @brief technical callback called (if [AutoRead flag] is true) just after new data have been received
     * send the data to the receiver
     *
     * @param data the data received
     * @param dataSize the size of the data received
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
