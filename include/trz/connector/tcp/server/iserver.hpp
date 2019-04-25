/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file iserver.hpp
 * @brief tcp server interface
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
 * @brief tcp server interface
 * 
 * @tparam _TNetwork network managing all server/clients for this core 
 */
template <class _TNetwork> class IServer
{
public:
    /**
     * @brief Listen parameters
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
     * @brief exception trown when server is asked to listen but was was already listening
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
     * @brief stop all open communications for this server
     */
    virtual void stopAllServerProcess(void) = 0;
    
    /**
     * @brief register this service to start listening
     * 
     * @param addressFamily ip address type AF_INET(IPv4) - AF_INET6(IPv6) 
     * @param addressType listenning adress ( *.*.*.* for any)
     * @param port to bind listenning socket to (0 to any)
     * 
     * @throw AlreadyListeningException
     * 
     */
    virtual void registerListen(const int64_t addressFamily, const int64_t addressType, const int64_t port) = 0;

    /**
     * @brief stop listening
     * 
     */
    virtual void stopListening(void) noexcept = 0;

    protected:
    friend _TNetwork;

    /**
     * @brief callback triggered when server process actor (handling a communication) sent a notification of its destruction
     * 
     * @param serverProcess the actor id of the server process actor
     */
    virtual void onServerProcessDestroy(const Actor::ActorId& serverProcess) noexcept = 0;

    /**
     * @brief callback triggered after a new client connected and connection has been established & handled by an actor
     * 
     * @param fd socket of new communication (used to later close connection)
     * @param clientIp ip of client that just connect (used to filter)
     * @param serverProcessActorId id of actor handling communication
     */
    virtual void onNewConnection(fd_t fd, const char *clientIp, const Actor::ActorId &serverProcessActorId) noexcept = 0;

    /**
     * @brief callback triggered after failed listen
     * 
     */
    virtual void onListenFailed(void) noexcept = 0;

    /**
     * @brief callback triggered after successful listen
     * 
     */
    virtual void onListenSucceed(void) noexcept = 0;

    /**
     * @brief callback triggered after failed accept (on incomming connection request)
     * 
     */
    virtual void onAcceptFailed(void) noexcept = 0;

    /**
     * @brief callback triggered after server stoped listening
     * 
     */
    virtual void onListenStopped(void) noexcept = 0;

    /**
     * @brief Get Listen parameters
     * 
     * @return const ListenParam& the params
     */
    virtual const ListenParam &getListenParam(void) const noexcept = 0;

private:

    /**
     * @brief low-level callback triggered upon new client has been accepted
     * 
     * @param fd new socket of accepted connection (later used to close connection)
     * @param clientIp ip of client that just connected
     * 
     */
    virtual void onNewConnectionBase(fd_t fd, const char *clientIp) noexcept = 0;

    /**
     * @brief low-level callback triggered upon failed listen attempt
     * 
     */
    virtual void onListenFailedBase(void) noexcept = 0;

    /**
     * @brief low-level callback triggered after successful listen
     * 
     * @param fd the new listening socket
     */
    virtual void onListenSucceedBase(fd_t fd) noexcept = 0;

    /**
     * @brief low-level callback triggered upon listen stopped
     * 
     */
    virtual void onListenStoppedBase(void) noexcept = 0;
};

} // namespace tcp

} // namespace connector

} // namespace tredzone