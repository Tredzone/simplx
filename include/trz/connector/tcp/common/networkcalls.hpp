/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file networkcalls.hpp
 * @brief tcp system functions wrapper
 * (minimize system headers dependencies && mock allowance)
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <arpa/inet.h>
#include <fcntl.h>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

namespace tredzone
{
namespace connector
{
namespace tcp
{

class NetworkCalls
{
    public:
    using Epoll_event = struct epoll_event;
    using Sockaddr_in = sockaddr_in;
    using Sockaddr    = sockaddr;
    using Sockaddr_un = sockaddr_un;

    const static int Sock_stream      = SOCK_STREAM;
    const static int Inaddr_any       = INADDR_ANY;
    const static int Inet6_addrstrlen = INET6_ADDRSTRLEN;
    const static int Sol_Socket       = SOL_SOCKET;
    const static int So_reuseaddr     = SO_REUSEADDR;
    const static int So_reuseport     = SO_REUSEPORT;
    const static int Msg_dontwait     = MSG_DONTWAIT;

    static int connect(int sockfd, const sockaddr_in *addr, socklen_t addrlen) noexcept
    {
        return ::connect(sockfd, (struct sockaddr *)addr, addrlen);
    }

    static int bind(int sockfd, const Sockaddr_in *addr, socklen_t addrlen) noexcept
    {
        return ::bind(sockfd, (Sockaddr *)addr, addrlen);
    }

    static int bind(int sockfd, const sockaddr *addr, socklen_t addrlen) noexcept
    {
        return ::bind(sockfd, addr, addrlen);
    }

    static int shutdown(int sockfd, int how) noexcept { return ::shutdown(sockfd, how); }

    static int epoll_create1(int flags) noexcept { return ::epoll_create1(flags); }

    static int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) noexcept
    {
        return ::epoll_ctl(epfd, op, fd, event);
    }

    static int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) noexcept
    {
        return ::epoll_wait(epfd, events, maxevents, timeout);
    }

    static int read(int fd, void *buf, size_t count) noexcept
    {
        ssize_t ret = ::read(fd, buf, count);

        return ret;
    }

    static int recv(int fd, void *buf, size_t len, int flags) noexcept
    {
        return ::recv(fd, buf, len, flags);
    }

    static int write(int fd, const void *buf, size_t count) noexcept
    {
        return ::write(fd, buf, count);
    }

    static int send(int fd, const void *buf, size_t len, int flags) noexcept
    {
        return ::send(fd, buf, len, flags);
    }

    static int listen(int sockfd, int backlog) noexcept { return ::listen(sockfd, backlog); }

    static int close(int fd) noexcept { return ::close(fd); }

    static int socket(int domain, int type, int protocol) noexcept { return ::socket(domain, type, protocol); }

    static int inet_pton(int af, const char *src, void *dst) noexcept { return ::inet_pton(af, src, dst); }

    static in_addr_t inet_addr(const char *cp) noexcept { return ::inet_addr(cp); }

    static bool setSocketNonBlocking(int socket) noexcept
    {
        int flags = ::fcntl(socket, F_GETFL, 0);
        if (-1 == flags)
            return false;
        flags |= O_NONBLOCK;
        return ::fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
    }

    static bool setSocketBlocking(int socket) noexcept
    {
        int flags = ::fcntl(socket, F_GETFL, 0);
        if (-1 == flags)
            return false;
        flags &= !O_NONBLOCK;
        return ::fcntl(socket, F_SETFL, flags ) == 0;
    }

    static int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) noexcept
    {
        return ::setsockopt(sockfd, level, optname, optval, optlen);
    }

    // get sockaddr, IPv4 or IPv6:
    static void *get_in_addr(struct sockaddr *sa)
    {
        if (sa->sa_family == AF_INET)
        {
            return &(((struct sockaddr_in *)sa)->sin_addr);
        }

        return &(((struct sockaddr_in6 *)sa)->sin6_addr);
    }

    static int accept4(int sockfd, char *clientAdress, int flags)
    {
        struct sockaddr_storage remoteaddr; // client address
        socklen_t               addrlen = sizeof remoteaddr;

        int result = ::accept4(sockfd, (struct sockaddr *)&remoteaddr, &addrlen, flags);

        ::inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr *)&remoteaddr), clientAdress, INET6_ADDRSTRLEN);

        return result;
    }

    static uint16_t wrapped_htons(uint16_t hostshort) noexcept { return htons(hostshort); }

    // (minimize system headers dependencies)
    enum
    {
        Epollout = EPOLLOUT,
        Epollin  = EPOLLIN,
        Epollerr = EPOLLERR,
        Epollhup = EPOLLHUP,
        Epollet  = EPOLLET
    };

    enum
    {
        Epoll_ctl_add = EPOLL_CTL_ADD,
        Epoll_ctl_mod = EPOLL_CTL_MOD,
        Epoll_ctl_del = EPOLL_CTL_DEL
    };

    enum
    {
        Einprogress   = EINPROGRESS,
        Ewouldblock   = EWOULDBLOCK,
        Eagain        = EAGAIN,
        Sock_nonblock = SOCK_NONBLOCK,
    };

    enum
    {
        Shut_rd   = SHUT_RD,
        Shut_wr   = SHUT_WR,
        Shut_rdwr = SHUT_RDWR
    };

    enum
    {
        Af_inet  = AF_INET,
        Af_inet6 = AF_INET6,
    };
};

} // namespace tcp

} // namespace connector

} // namespace tredzone
