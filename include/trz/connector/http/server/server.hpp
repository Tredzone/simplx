/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file server.hpp
 * @brief http server
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include "simplx.h"
#include "trz/connector/tcpconnector.hpp"

#include <unistd.h>

namespace tredzone
{
namespace connector
{
namespace http
{
using std::string;

/**
 * @brief server HTTP
 * /!\ a rootPath folder must be set or a www folder must exist next to the binary
 * otherwise the server can not start (symlink doesen't work)
 *
 * @tparam _TNetwork network actor managing the connection and communication to network (low level)
 * @tparam _TServerProcess class than server spawns to communicate with externals client connecting to this server
 */
template <class _Network, class _HttpServerProcess> class HttpServer : public TcpServer<_Network, _HttpServerProcess>
{
    using parent = TcpServer<_Network, _HttpServerProcess>;

    public:
    using fd_t = int64_t;

    /**
     * @brief server parameters class
     *
     */
    class Parameters
    {
        public:
        Parameters(const string &interface, const int port, const string rootPath = "")
            : m_interface(interface), m_port(port), m_rootPath(rootPath)
        {
        }

        const string m_interface;
        const int    m_port;
        const string m_rootPath;
    };

    /**
     * @brief Instantiates a new Http Server
     *
     * @param param construction parameters
     */
    HttpServer(const Parameters &param)
        : m_interface(param.m_interface), m_port(param.m_port),
          m_rootPath(canonicalize_file_name(param.m_rootPath.c_str()))
    {
        string m_path;
        // check the rootpath
        if (!m_rootPath)
        {
            // if none provided take [current_dir]/www
            char *       current_dir_name = get_current_dir_name();
            const string tmp(current_dir_name);
            m_path = tmp + "/www";
            free(current_dir_name);
            m_rootPath = canonicalize_file_name(m_path.c_str());
        }
        if (!m_rootPath)
        {
            throw std::runtime_error(string("invalid http root folder path : [ ") + m_path +
                                     string(" ], fail to run\ncreate the missing folder or set a valid root_path"));
        }
    }

    /**
     * @brief Destroys Http Server object
     *
     */
    virtual ~HttpServer() { free(m_rootPath); }

    protected:
    /**
     * @brief registers service to start listening
     *
     */
    virtual void listen()
    {
        const int64_t addressFamily = AF_INET;
        const int64_t addressType   = (m_interface == "" ? INADDR_ANY : inet_addr(m_interface.c_str()));
        parent::registerListen(addressFamily, addressType, m_port);
    }

    /**
     * @brief callback called after new client connected and connection has been established & handled by an
     * actor
     *
     * @param fd socket of new communication (also used to close said connection)
     * @param clientIp ip of said connected client. Potentially used for post-connection filtering/processing
     * @param serverProcessActorId id of actor handling said communication
     */
    virtual void onNewConnection(const fd_t /*fd*/, const char * /*clientIp*/,
                                 const Actor::ActorId & /*serverProcessActorId*/) noexcept override
    {
    }

private:
    
    /**
     * @brief internal callback triggered just after a new client as been accepted.
     * (overridden to pass rootPath to serverProcess)
     *
     * @param fd socket of new communication (used to close this connection)
     * @param clientIp ip of client that just connected (also used for filtering)
     *
     */
    void onNewConnectionBase(const fd_t fd, const char *clientIp) noexcept override
    {
        typename _HttpServerProcess::Parameters param(m_rootPath);

        const Actor::ActorId &clientActorId = parent::template newUnreferencedActor<_HttpServerProcess>(param);
        parent::m_serverProcesses.insert(clientActorId);

        {
            Actor::ActorReference<_HttpServerProcess> ref =
                parent::template referenceLocalActor<_HttpServerProcess>(clientActorId);
            ref->registerDestroyNotification(parent::getActorId());
            ref->setFd(fd);
        }
        onNewConnection(fd, clientIp, clientActorId);
    }

    const string  m_interface;
    const int64_t m_port;
    char *        m_rootPath;
};
} // namespace http
} // namespace connector
} // namespace tredzone