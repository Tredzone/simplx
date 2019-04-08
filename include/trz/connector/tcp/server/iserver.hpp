/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file iserver.hpp
 * @brief server interface
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include "simplx.h"
#include "trz/connector/tcp/client/iclient.hpp"

#include <cstdint>
#include <memory>
#include <tuple>
#include <exception>

namespace tredzone
{
namespace connector
{
namespace tcp
{

using fd_t = int64_t;
using ::std::exception;

/**
 * @brief the server interface
 * 
 * @tparam _TNetwork the network that manages all server/client for this core 
 */
template <class _TNetwork> class IServer
{
    public:
    /**
     * @brief Listen param class
     * 
     */
    class ListenParam
    {
        public:
        int64_t m_addressFamily;
        int64_t m_addressType;
        int64_t m_port;
    };


    /**
     * @brief the exception is trown when the server is already listening and ask to register listen
     * 
     */
    class AlreadyListeningException : public exception
    {
        public:
        AlreadyListeningException(const ListenParam &currentListenningParam):m_msg(), m_currentListenningParam(currentListenningParam)
        {
            m_msg << "TcpServer already listening ("
                  << " addressFamily:" << m_currentListenningParam.m_addressFamily
                  << " addressType:" << m_currentListenningParam.m_addressType
                  << " port:" << m_currentListenningParam.m_port << ")";
        }

        AlreadyListeningException(const AlreadyListeningException &other):m_msg(), m_currentListenningParam(other.m_currentListenningParam)
        {
            m_msg << "TcpServer already listening ("
                  << " addressFamily:" << m_currentListenningParam.m_addressFamily
                  << " addressType:" << m_currentListenningParam.m_addressType
                  << " port:" << m_currentListenningParam.m_port << ")";
        }

        virtual const char *what() const throw() { return m_msg.str().c_str(); }

        private:
        ostringstream m_msg;
        const ListenParam& m_currentListenningParam;
    };

    /**
     * @brief destructor
     */
    virtual ~IServer(void) = default;

    /**
     * @brief stop all open communication for this server
     */
    virtual void stopAllServerProcess(void) = 0;
    
    /**
     * @brief register this service to start listening
     * 
     * @param addressFamily ip address type AF_INET(IPv4) - AF_INET6(IPv6) 
     * @param addressType listenning adress ( *.*.*.* for any)
     * @param port to bind the listenning socket (0 to any)
     * 
     * @throw AlreadyListeningException
     * 
     */
    virtual void registerListen(const int64_t addressFamily, const int64_t addressType, const int64_t port) = 0;

    /**
     * @brief stop the listen
     * 
     */
    virtual void stopListening(void) noexcept = 0;

    protected:
    friend _TNetwork;

    /**
     * @brief callback called when server process actor (handling a communication) send a notification of it's destruction
     * 
     * @param serverProcess the actor id of the server process actor
     */
    virtual void onServerProcessDestroy(const Actor::ActorId& serverProcess) noexcept = 0;

    /**
     * @brief callback called after a new client did connect and the connection has been established & handled in an actor
     * 
     * @param fd socket of the new communication(used to close the connection)
     * @param clientIp ip of the client that just connect (used to filter)
     * @param serverProcessActorId id of the actor handling the communication
     */
    virtual void onNewConnection(fd_t fd, const char *clientIp, const Actor::ActorId &serverProcessActorId) noexcept = 0;

    /**
     * @brief callback called after listen failed
     * 
     */
    virtual void onListenFailed(void) noexcept = 0;

    /**
     * @brief callback called after listen succeed
     * 
     */
    virtual void onListenSucceed(void) noexcept = 0;

    /**
     * @brief callback called after accept (on incomming connection request) failed 
     * 
     */
    virtual void onAcceptFailed(void) noexcept = 0;

    /**
     * @brief callback called after the server stoped listening
     * 
     */
    virtual void onListenStopped(void) noexcept = 0;

    /**
     * @brief Get the Listen parameters object
     * 
     * @return const ListenParam& the params
     */
    virtual const ListenParam &getListenParam(void) const noexcept = 0;

    private:
    /**
     * @brief technical callback called just after a new client as been accepted. 
     * must define the actor handling the communication
     * then call the higher level callback onNewConnection
     * 
     * @param fd socket of the new communication(used to close the connection)
     * @param clientIp ip of the client that just connect (used to filter)
     * 
     */
    virtual void onNewConnectionBase(fd_t fd, const char *clientIp) noexcept = 0;

    /**
     * @brief technical callback called after listen failed.
     * can be use to set internal flags/variable or execute technical method
     * then should call the higher level callback onListenFailed
     * 
     */
    virtual void onListenFailedBase(void) noexcept = 0;

    /**
     * @brief technical callback called after listen succeed.
     * can be use to set internal flags/variable or execute technical method
     * then should call the higher level callback onListenSucceed
     * 
     * @param fd the new listening socket
     */
    virtual void onListenSucceedBase(fd_t fd) noexcept = 0;

    /**
     * @brief technical callback called after listen stopped.
     * can be use to set internal flags/variable or execute technical method
     * then should call the higher level callback onListenStopped
     * 
     */
    virtual void onListenStoppedBase(void) noexcept = 0;

};}}
} // namespace tredzone