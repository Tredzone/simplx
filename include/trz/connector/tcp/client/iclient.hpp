/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file iclient.hpp
 * @brief client interface
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include "simplx.h"
#include <chrono>
#include <cstdint>
#include <exception>
#include <string>
#include <tuple>

namespace tredzone
{
namespace connector
{
namespace tcp
{

// import into namespace
using ::std::exception;
using ::std::ostringstream;
using ::std::size_t;
using ::std::string;
using ::std::tuple;
using ::std::chrono::microseconds;
using ::std::chrono::steady_clock;
using fd_t = int64_t;

template <class _TNetwork> class IClient
{
    public:
    /**
     * @brief connection parameters
     *
     */
    class ConnectParam
    {
        public:
        string   m_ipAddressSource;
        string   m_ipAddress;
        int64_t  m_addressFamily;
        uint64_t m_port;
        uint64_t m_timeoutUSec;
        size_t   m_messageHeaderSize;
    };

    /**
     * @brief this exception is thrown when a client registers itself for connection but is already connected or registered for connection
     *
     */
    class AlreadyConnectedException : public exception
    {
        public:
        AlreadyConnectedException(const Actor::ActorId &actorId, const ConnectParam &currentConnectParam)
            : m_msg(), m_actorId(actorId), m_currentConnectParam(currentConnectParam)
        {
            m_msg << "TcpClient (" << m_actorId << ") already connected ("
                  << " ipAddress:" << m_currentConnectParam.m_ipAddress
                  << " addressFamily:" << m_currentConnectParam.m_addressFamily
                  << " port:" << m_currentConnectParam.m_port << " timeoutUSec:" << m_currentConnectParam.m_timeoutUSec
                  << ")";
        }

        AlreadyConnectedException(const AlreadyConnectedException &other)
            : m_msg(), m_actorId(other.m_actorId), m_currentConnectParam(other.m_currentConnectParam)
        {
            m_msg << "TcpClient (" << m_actorId << ") already connected ("
                  << " ipAddress:" << m_currentConnectParam.m_ipAddress
                  << " addressFamily:" << m_currentConnectParam.m_addressFamily
                  << " port:" << m_currentConnectParam.m_port << " timeoutUSec:" << m_currentConnectParam.m_timeoutUSec
                  << ")";
        }

        virtual const char *what() const throw() { return m_msg.str().c_str(); }

        private:
        ostringstream        m_msg;
        const Actor::ActorId m_actorId;
        const ConnectParam & m_currentConnectParam;
    };

    /**
     * @brief this exception is thrown when an unconnected client tries to send a message
     *
     */
    class NotConnectedException : public exception
    {
        public:
        NotConnectedException(const Actor::ActorId &actorId):m_actorId(actorId)
        {
            m_msg << "TcpClient (" << m_actorId << "): TcpClient not connected";
        }

        NotConnectedException(const NotConnectedException &other):m_actorId(other.m_actorId)
        {
            m_msg << "TcpClient (" << m_actorId << "): TcpClient not connected";
        }

        virtual const char *what() const throw() { return m_msg.str().c_str(); }

        private:
        ostringstream m_msg;
        const Actor::ActorId &m_actorId;
    };

    /**
     * @brief this exception is thrown when a client tries sending a message larger than the remaining space in the
     * send buffer
     *
     */
    class SendBufferFullException : public exception
    {
        public:
        SendBufferFullException(const Actor::ActorId &actorId):m_actorId(actorId)
        {
            m_msg << "TcpClient (" << m_actorId << "): Fail to bufferize data: buffer full";
        }

        SendBufferFullException(const SendBufferFullException &other):m_actorId(other.m_actorId)
        {
            m_msg << "TcpClient (" << m_actorId << "): Fail to bufferize data: buffer full";
        }

        virtual const char *what() const throw() { return m_msg.str().c_str(); }

        private:
        ostringstream m_msg;
        const Actor::ActorId &m_actorId;
    };

    /**
     * @brief Destructor
     *
     */
    virtual ~IClient(void) noexcept = default;

    /**
     * @brief register this client for later, asynchronous connection
     * client code should previously have called setConnectionParam()
     *
     * @param ipAddress to connect to
     * @param addressFamily type AF_INET(IPv4) - AF_INET6(IPv6)
     * @param port to connect to
     * @param timeoutUSec connection timeout, in microSeconds
     * @param messageHeaderSize minimum header size (0 for no header)
     * @param ipAddressSource adress of source interface to connect from (use "0.0.0.0" for any)
     */
    virtual void registerConnect(const string &ipAddress, int64_t addressFamily, uint64_t port, uint64_t timeoutUSec,
                                 size_t messageHeaderSize, const string &ipAddressSource) = 0;

    /**
     * @brief Set Connection Parameters
     *
     * @param ipAddress to connect to
     * @param addressFamily ip address  AF_INET(IPv4) - AF_INET6(IPv6)
     * @param port to connect to
     * @param timeoutUSec connection timeout, in microSeconds
     * @param messageHeaderSize minimum header size (0 for no header)
     * @param ipAddressSource the adress of the interface to use to connect from ("0.0.0.0" for any)
     *
     * @throw AlreadyConnectedException
     */
    virtual void setConnectParam(const string &ipAddress, int64_t addressFamily, uint64_t port, uint64_t timeoutUSec,
                                 size_t messageHeaderSize, const string &ipAddressSource) = 0;

    /**
     * @brief Get the Connect Parameters object
     *
     * @return const ConnectParam&
     */
    virtual const ConnectParam &getConnectParam(void) const noexcept = 0;

    /**
     * @brief register client for later, asynchronous connection
     * will use parameters previously set by setConnectParam()
     *
     * @throw AlreadyConnectedException
     */
    virtual void registerConnect() = 0;

    /**
     * @brief high-level method closing the connection
     *
     */
    virtual void disconnect(void) noexcept = 0;

    /**
     * @brief add data to send buffer and register buffer to be asynchronously sent by network
     * user code must thereafter call sendBase(...) to actually send data
     *
     * @param data to send
     */
    virtual void send(const uint8_t data) = 0;

    /**
     * @brief add data to send buffer and register buffer to be asynchronously sent by network
     * user code must thereafter call sendBase(...) to actually send data
     *
     * @param data to send
     */
    virtual void send(const string &data) = 0;

    /**
     * @brief add data to send buffer and register buffer to be asynchronously sent by network
     * user code must thereafter call sendBase(...) to actually send data
     *
     * @param data to send
     * @param dataSize to send
     */
    virtual void send(const char *data, const size_t dataSize) = 0;

    /**
     * @brief add data to send buffer and register buffer to be asynchronously sent by network
     * user code must thereafter call sendBase(...) to actually send data
     *
     * @param data to send
     * @param dataSize to send
     */
    virtual void send(const uint8_t *data, const size_t dataSize) = 0;

    /**
     * @brief synchronously send data through network
     * user code must thereafter call directSendBase(...) to actually send data
     *
     * @param data to send
     * @param blocking flag, if false use a nonblocking send 
     * @param retry number of retries when the socket would block
     */
    virtual void directSend(const uint8_t data, const bool blocking = false, ssize_t retry = 10) = 0;

    /**
     * @brief synchronously send data through network
     * user code must thereafter call directSendBase(...) to actually send data
     *
     * @param data to send
     * @param blocking flag, if false use a nonblocking send
     * @param retry number of retries when socket would block
     */
    virtual void directSend(const string &data, const bool blocking = false, ssize_t retry = 10) = 0;

    /**
     * @brief synchronously send data through network
     * user code must thereafter call directSendBase(...) to actually send data
     *
     * @param data to send
     * @param dataSize to send
     * @param blocking flag, if false use a nonblocking send
     * @param retry number of retries when socket would block
     */
    virtual void directSend(const char *data, const size_t dataSize, const bool blocking = false,
                            ssize_t retry = 10) = 0;

    /**
     * @brief synchronously send data through network
     * user code must thereafter call directSendBase(...) to actually send data
     *
     * @param data to send
     * @param dataSize to send
     * @param blocking flag, if false use a nonblocking send
     * @param retry number of retries when socket would block
     */
    virtual void directSend(const uint8_t *data, const size_t dataSize, const bool blocking = false,
                            ssize_t retry = 10) = 0;

    /**
     * @brief Get Sender Buffer Content Size
     *
     * @return size_t size of buffer content
     */
    virtual size_t getSenderBufferContentSize(void) const noexcept = 0;

    /**
     * @brief Get Receiver Buffer Content Size
     *
     * @return size_t size of buffer content
     */
    virtual size_t getReceiverBufferContentSize(void) const noexcept = 0;

    /**
     * @brief clear sender buffer
     *
     */
    virtual void clearSenderBuffer(void) noexcept = 0;

    /**
     * @brief clear receiver buffer
     *
     */
    virtual void clearReceiverBuffer(void) noexcept = 0;

    /**
     * @brief Get socket file descriptor
     *
     * @return fd_t socket ascociated with this client
     */
    virtual fd_t getFd(void) const noexcept = 0;

    /**
     * @brief register this actor for later, asynchronous self-destruction
     *
     */
    virtual void destroy(void) noexcept = 0;

    /**
     * @brief check if new message is available in buffer
     *
     * @return true message(s) available
     * @return false no message available
     */
    virtual bool isNewMessageToRead(void) const noexcept = 0;

    /**
     * @brief read the socket messages until no more are available or limit is reached
     *
     * @param limit maximum number of read operations
     */
    virtual void readSocket(size_t limit = 1) noexcept = 0;

    /**
     * @brief Get Auto-Read flag
     *
     * @return true network will automaticaly read any new messages, then trigger onDataReceived() callback
     * @return false on new message, network will just set MessageToRead flag to true then trigger onNewMessageToReadBase() callback
     */
    virtual bool getAutoReadFlag(void) const noexcept = 0;

    /**
     * @brief Set the Auto Read flag
     * true: true network will automaticaly read any new messages, then trigger onDataReceived() callback
     * false: false on new message, network will just set MessageToRead flag to true then trigger onNewMessageToReadBase() callback
     *
     * @param autoread flag value
     */
    virtual void setAutoRead(const bool autoread) noexcept = 0;

    friend _TNetwork;

protected:
    /**
     * @brief callback triggered when auto-read flag is false and new message is received
     *
     */
    virtual void onNewMessageToRead(void) noexcept = 0;

    /**
     * @brief Get Header Size
     *
     * @param data received
     * @return size_t current header size
     */
    virtual size_t getHeaderSize(const uint8_t *data) noexcept = 0;

    /**
     * @brief Get Data Size using header
     *
     * @param data received
     * @param currentHeaderSize current header size
     * @return uint64_t data Size
     */
    virtual uint64_t getDataSize(const uint8_t *data, size_t currentHeaderSize) noexcept = 0;

    /**
     * @brief callback triggered upon failed connection
     *
     */
    virtual void onConnectFailed(void) noexcept = 0;

    /**
     * @brief callback triggered upon connection timeout
     *
     */
    virtual void onConnectTimedOut(void) noexcept = 0;

    /**
     * @brief callback triggered upon lost connection
     *
     */
    virtual void onConnectionLost(void) noexcept = 0;

    /**
     * @brief callback triggered upon successful connection
     *
     */
    virtual void onConnect(void) noexcept = 0;

    /**
     * @brief callback triggered upon disconnection
     *
     */
    virtual void onDisconnect(void) noexcept = 0;

    /**
     * @brief callback triggered upon complete data reception (according to size specified in header)
     *
     * @param data received
     * @param dataSize size data
     */
    virtual void onDataReceived(const uint8_t *data, size_t dataSize) noexcept = 0;

    /**
     * @brief callback triggered after data is received but its size (according to header) would overflow receiver buffer. 
     * Method will be called for each data chunk received until entire payload has been received. Hence, 
     * buffering must be managed by user code itself
     * Next header will be processed normally
     *
     * @param data (probably partial) data received
     * @param dataSize (probably partial) data size
     */
    virtual void onOverflowDataReceived(const uint8_t *data, size_t dataSize) noexcept = 0;

    /**
     * @brief Get Data To Send (sender buffer)
     *
     * @return tuple<const uint8_t *, size_t> buffer as tuple of data pointer and size
     */
    virtual tuple<const uint8_t *, size_t> getDataToSend(void) noexcept = 0;

    /**
     * @brief Get connection Timeout
     *
     * @return const microseconds& timeout
     */
    virtual const microseconds &getTimeout(void) const noexcept = 0;

    /**
     * @brief Get Registration Time
     *
     * @return const steady_clock::time_point
     */
    virtual const steady_clock::time_point getRegistrationTime(void) const noexcept = 0;

    /**
     * @brief low-level part of disconnection process
     *
     */
    virtual void disconnectBase(void) noexcept = 0;

    /**
     * @brief asynchronously send data (low-level)
     *
     * @param data to send
     * @param dataSize to send
     *
     * @throw NotConnectedException
     * @throw SendBufferFullException                                               // devrait etre overflowException?
     */
    virtual void sendBase(const uint8_t *data, const size_t dataSize) = 0;

    /**
     * @brief synchronously send data (low-level)
     *
     * @param data to send
     * @param dataSize to send
     * @param blocking flag, if false use a nonblocking send
     * @param retry number of retries when socket would block
     *
     * @throw NotConnectedException
     */
    virtual void directSendBase(const uint8_t *data, const size_t dataSize, bool blocking = false,
                                ssize_t retry = 10) = 0;

    /**
     * @brief clear part of sender buffer
     *
     * @param dataSize size of data to clear from buffer
     * @return size_t remaining data size
     */
    virtual size_t shiftBuffer(size_t dataSize) noexcept = 0;

    private:
    /**
     * @brief low-level callback triggered by network when auto-read flag is false and new message is received
     * updates internal state then calls higher-level onNewMessageToRead()
     *
     */
    virtual void onNewMessageToReadBase(void) noexcept = 0;

    /**
     * @brief low-level callback triggered upon connection
     * stores socket bound to connection then calls higher-level onConnect()
     *
     * @param fd socket of connection
     */
    virtual void onConnectBase(const fd_t fd) noexcept = 0;

    /**
     * @brief low-level callback triggered upon connection timeout
     * updates internal state then calls higher-level onConnectTimedOut()
     *
     */
    virtual void onConnectTimedOutBase(void) noexcept = 0;

    /**
     * @brief low-level callback triggered upon failed connection attempt
     * updates internal state then calls higher-level onConnectFailed()
     *
     */
    virtual void onConnectFailedBase(void) noexcept = 0;

    /**
     * @brief low-level callback triggered upon lost connection
     * updates internal state then calls higher-level onConnectionLost()
     *
     */
    virtual void onConnectionLostBase(void) noexcept = 0;

    /**
     * @brief low-level callback triggered upon new data received (if AutoRead flag is true)
     * then sends data to receiver
     *
     * @param data received
     * @param dataSize received
     */
    virtual void onDataReceivedBase(const uint8_t *data, size_t dataSize) noexcept = 0;

    /**
     * @brief low-level callback triggered upon disconnection
     * updates internal state then calls higher-level onDisconnect()
     *
     */
    virtual void onDisconnectBase(void) noexcept = 0;
};
} // namespace tcp

} // namespace connector

} // namespace tredzone