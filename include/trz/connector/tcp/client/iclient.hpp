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
     * @brief connection param class
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
     * @brief the exception is thrown when the client is already connected or registered for connection and ask to
     * register connect
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
     * @brief the exception is thrown when the client is not connected and try to send a message
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
     * @brief the exception is thrown when the client tries to send a message bigger than the remaining space in the
     * sending buffer
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
     * @brief Destroy the IClient object
     *
     */
    virtual ~IClient(void) noexcept = default;

    /**
     * @brief register this client to be connected
     * should call setConnectionParam
     * then registerConnect() which registers current Client with the newly set parameters
     *
     * @param ipAddress the ip to connect to
     * @param addressFamily ip address type AF_INET(IPv4) - AF_INET6(IPv6)
     * @param port the port to connect to
     * @param timeoutUSec the delay in microSecond before timeout the connection process
     * @param messageHeaderSize the minimum size of the header (0 means no header)
     * @param ipAddressSource the adress of the interface to use to connect from ( *.*.*.* for any)
     */
    virtual void registerConnect(const string &ipAddress, int64_t addressFamily, uint64_t port, uint64_t timeoutUSec,
                                 size_t messageHeaderSize, const string &ipAddressSource) = 0;

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
    virtual void setConnectParam(const string &ipAddress, int64_t addressFamily, uint64_t port, uint64_t timeoutUSec,
                                 size_t messageHeaderSize, const string &ipAddressSource) = 0;

    /**
     * @brief Get the Connect Parameters object
     *
     * @return const ConnectParam&
     */
    virtual const ConnectParam &getConnectParam(void) const noexcept = 0;

    /**
     * @brief register this client to be connected
     * the connection will use the parameters set using setConnectParam
     *
     * @throw AlreadyConnectedException
     */
    virtual void registerConnect() = 0;

    /**
     * @brief hight level method closing the connection must call disconnect base to effectively disconnect
     *
     */
    virtual void disconnect(void) noexcept = 0;

    /**
     * @brief add data to the send buffer and register (async) this buffer to be sent by network
     * must call sendBase after user actions and after type conversion of data to effectively send the data
     *
     * @param data the data to send
     */
    virtual void send(const uint8_t data) = 0;

    /**
     * @brief add data to the send buffer and register (async) this buffer to be sent by network
     * must call sendBase after user actions and after type conversion of data to effectively send the data
     *
     * @param data the data to send
     */
    virtual void send(const string &data) = 0;

    /**
     * @brief add data to the send buffer and register (async) this buffer to be sent by network
     * must call sendBase after user actions and after type conversion of data to effectively send the data
     *
     * @param data the data to send
     * @param dataSize the size of the data to send
     */
    virtual void send(const char *data, const size_t dataSize) = 0;

    /**
     * @brief add data to the send buffer and register (async) this buffer to be sent by network
     * must call sendBase after user actions and after type conversion of data to effectively send the data
     *
     * @param data the data to send
     * @param dataSize the size of the data to send
     */
    virtual void send(const uint8_t *data, const size_t dataSize) = 0;

    /**
     * @brief directly send data (sync) through the network
     * must call directSendBase after user actions and after type conversion of data to effectively send the data
     *
     * @param data the data to send
     * @param blocking if true use a nonblocking send
     * @param retry number of try to send when the socket would block
     */
    virtual void directSend(const uint8_t data, const bool blocking = false, ssize_t retry = 10) = 0;

    /**
     * @brief directly send data (sync) through the network
     * must call directSendBase after user actions and after type conversion of data to effectively send the data
     *
     * @param data the data to send
     * @param blocking if true use a nonblocking send
     * @param retry number of try to send when the socket would block
     */
    virtual void directSend(const string &data, const bool blocking = false, ssize_t retry = 10) = 0;

    /**
     * @brief directly send data (sync) through the network
     * must call directSendBase after user actions and after type conversion of data to effectively send the data
     *
     * @param data the data to send
     * @param dataSize the size of the data to send
     * @param blocking if true use a nonblocking send
     * @param retry number of try to send when the socket would block
     */
    virtual void directSend(const char *data, const size_t dataSize, const bool blocking = false,
                            ssize_t retry = 10) = 0;

    /**
     * @brief directly send data (sync) through the network
     * must call directSendBase after user actions and after type conversion of data to effectively send the data
     *
     * @param data the data to send
     * @param dataSize the size of the data to send
     * @param blocking if true use a nonblocking send
     * @param retry number of try to send when the socket would block
     */
    virtual void directSend(const uint8_t *data, const size_t dataSize, const bool blocking = false,
                            ssize_t retry = 10) = 0;

    /**
     * @brief Get the Sender Buffer Content Size
     *
     * @return size_t size of the buffer content
     */
    virtual size_t getSenderBufferContentSize(void) const noexcept = 0;

    /**
     * @brief Get the Receiver Buffer Content Size
     *
     * @return size_t size of the buffer content
     */
    virtual size_t getReceiverBufferContentSize(void) const noexcept = 0;

    /**
     * @brief empty the sender buffer
     *
     */
    virtual void clearSenderBuffer(void) noexcept = 0;

    /**
     * @brief empty the receiver buffer
     *
     */
    virtual void clearReceiverBuffer(void) noexcept = 0;

    /**
     * @brief Get the Fd object
     *
     * @return fd_t the socket ascociated to this client
     */
    virtual fd_t getFd(void) const noexcept = 0;

    /**
     * @brief register object to self destroy
     *
     */
    virtual void destroy(void) noexcept = 0;

    /**
     * @brief check if a new message is available in the buffer
     *
     * @return true message is available
     * @return false no message is available
     */
    virtual bool isNewMessageToRead(void) const noexcept = 0;

    /**
     * @brief if a message is available,
     * read the socket through the network until no message are available or limit is reached
     *
     * @param limit maximum number of read to process
     */
    virtual void readSocket(size_t limit = 1) noexcept = 0;

    /**
     * @brief Get the Auto Read flag
     *
     * @return true network automaticaly read new message then callback onDataReceived
     * @return false network just set the flag MessageToRead then run the callback onNewMessageToReadBase
     */
    virtual bool getAutoReadFlag(void) const noexcept = 0;

    /**
     * @brief Set the Auto Read flag
     * true: network automaticaly read new message then callback onDataReceived
     * false: network just set the flag MessageToRead then run the callback onNewMessageToReadBase
     *
     * @param autoread flag value
     */
    virtual void setAutoRead(const bool autoread) noexcept = 0;

    friend _TNetwork;

    protected:
    /**
     * @brief callback calledwhen auto read flag is false and a new message is received
     *
     */
    virtual void onNewMessageToRead(void) noexcept = 0;

    /**
     * @brief Get the Header Size
     *
     * @param data the data received
     * @return size_t the current Header Size
     */
    virtual size_t getHeaderSize(const uint8_t *data) noexcept = 0;

    /**
     * @brief Get the Data Size using the header
     *
     * @param data the data received
     * @param currentHeaderSize the current Header Size
     * @return uint64_t the data Size
     */
    virtual uint64_t getDataSize(const uint8_t *data, size_t currentHeaderSize) noexcept = 0;

    /**
     * @brief callback called after a connection failed
     *
     */
    virtual void onConnectFailed(void) noexcept = 0;

    /**
     * @brief callback called after the connection atempt reached timeout
     *
     */
    virtual void onConnectTimedOut(void) noexcept = 0;

    /**
     * @brief callback called when the connection has been lost
     *
     */
    virtual void onConnectionLost(void) noexcept = 0;

    /**
     * @brief callback called after a successful connection
     *
     */
    virtual void onConnect(void) noexcept = 0;

    /**
     * @brief callback called after a disconnection
     *
     */
    virtual void onDisconnect(void) noexcept = 0;

    /**
     * @brief callback called after a completed data (according to header) is received
     *
     * @param data the data received
     * @param dataSize the size of the data
     */
    virtual void onDataReceived(const uint8_t *data, size_t dataSize) noexcept = 0;

    /**
     * @brief callback called after a data is received but its size (according to header) is bigger than the receiver
     * buffer this method will be called for each data received until the all "to big data" has been received so the
     * client has to manage the buffering. then the next header is read and if the next data fit the buffer size
     * onDataReceived is used
     *
     * @param data the (probably partial) data received
     * @param dataSize the (probably partial) data size
     */
    virtual void onOverflowDataReceived(const uint8_t *data, size_t dataSize) noexcept = 0;

    /**
     * @brief Get the Data To Send (sender buffer)
     *
     * @return tuple<const uint8_t *, size_t> the buffer as a tuple of data pointer and the size of the data
     */
    virtual tuple<const uint8_t *, size_t> getDataToSend(void) noexcept = 0;

    /**
     * @brief Get the connection Timeout
     *
     * @return const microseconds& the timeout
     */
    virtual const microseconds &getTimeout(void) const noexcept = 0;

    /**
     * @brief Get the Registration Time
     *
     * @return const steady_clock::time_point
     */
    virtual const steady_clock::time_point getRegistrationTime(void) const noexcept = 0;

    /**
     * @brief technical part of the disconnection process.
     * effectivelly disconnect must be called by disconnect
     *
     */
    virtual void disconnectBase(void) noexcept = 0;

    /**
     * @brief technical send method that effectively send the data (async)
     *
     * @param data the data to send
     * @param dataSize the size of the data to send
     *
     * @throw NotConnectedException
     * @throw SendBufferFullException
     */
    virtual void sendBase(const uint8_t *data, const size_t dataSize) = 0;

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
    virtual void directSendBase(const uint8_t *data, const size_t dataSize, bool blocking = false,
                                ssize_t retry = 10) = 0;

    /**
     * @brief clear a part of sender buffer
     *
     * @param dataSize the size of the data to clear from buffer
     * @return size_t the size of the remaining data in the buffer
     */
    virtual size_t shiftBuffer(size_t dataSize) noexcept = 0;

    private:
    /**
     * @brief technical callback calledby network when auto read flag is false and a new message is received
     * define flags then call the higher level callback onNewMessageToRead
     *
     */
    virtual void onNewMessageToReadBase(void) noexcept = 0;

    /**
     * @brief technical callback called just after a connection.
     * define the socket bound to the connection
     * then call the higher level callback onConnect
     *
     * @param fd socket of connection
     */
    virtual void onConnectBase(const fd_t fd) noexcept = 0;

    /**
     * @brief technical callback called just after connection atempt timed out
     * define flags then call the higher level callback onConnectTimedOut
     *
     */
    virtual void onConnectTimedOutBase(void) noexcept = 0;

    /**
     * @brief technical callback called just after connection atempt failed
     * define flags then call the higher level callback onConnectFailed
     *
     */
    virtual void onConnectFailedBase(void) noexcept = 0;

    /**
     * @brief technical callback called just after connection has been lost
     * define flags then call the higher level callback onConnectionLost
     *
     */
    virtual void onConnectionLostBase(void) noexcept = 0;

    /**
     * @brief technical callback called (if [AutoRead flag] is true) just after new data have been received
     * send the data to the receiver
     *
     * @param data the data received
     * @param dataSize the size of the data received
     */
    virtual void onDataReceivedBase(const uint8_t *data, size_t dataSize) noexcept = 0;

    /**
     * @brief technical callback called just after the disconnection
     * define flags then call the higher level callback onDisconnect
     *
     */
    virtual void onDisconnectBase(void) noexcept = 0;
};
} // namespace tcp
} // namespace connector
} // namespace tredzone