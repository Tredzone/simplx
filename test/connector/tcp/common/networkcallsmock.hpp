/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file networkcallsmock.hpp
 * @brief mock for system network calls
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <arpa/inet.h>
#include <fcntl.h>
#include <functional>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h> /* See NOTES */
#include <unistd.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

class NetworkCallsMock
{
  public:
    static int                                        connectReturnValue;
    static int                                        errnoValue;
    static int                                        epollFdValue;
    static int                                        epollEvent;
    static int                                        epollWaitCount;
    static int                                        closeReturnValue;
    static int                                        listenReturnValue;
    static int                                        acceptedSocket;
    static std::function<void(int, void *, int &ret)> mockRead;
    static std::function<void(int, const void *, size_t, int &ret)> mockWrite;

    const static int Sock_stream      = 0;
    const static int Inaddr_any       = 0;
    const static int Inet6_addrstrlen = 46;
    const static int Sol_Socket       = 0;
    const static int So_reuseaddr     = 0;
    const static int So_reuseport     = 0;

    static int connect(int sockfd, const sockaddr_in *addr,
                       socklen_t addrlen) noexcept
    {
        errno = errnoValue;
        return connectReturnValue;
    }

    static int epoll_create1(int flags) noexcept
    {
        return 0; // fd
    }

    static int epoll_ctl(int epfd, int op, int fd,
                         struct epoll_event *event) noexcept
    {
        return 0;
    }

    static int epoll_wait(int epfd, struct epoll_event *events, int maxevents,
                          int timeout) noexcept
    {
        events[0].data.fd = epollFdValue;
        events[0].events  = epollEvent;

        return epollWaitCount;
    }

    static int read(int fd, void *buf, size_t count) noexcept
    {
        int ret = 0;
        mockRead(fd, buf, ret);
        return ret;
    }

    static int write(int fd, const void *buf, size_t count) noexcept
    {
        int ret = 0;
        mockWrite(fd, buf, count, ret);
        return ret;
    }

    static int shutdown(int sockfd, int how) noexcept { return 0; }

    static int close(int fd) noexcept { return closeReturnValue; }

    static int socket(int domain, int type, int protocol) noexcept
    {
        return epollFdValue; // fd
    }

    static int inet_pton(int af, const char *src, void *dst) noexcept
    {
        return 0;
    }

    static bool setSocketNonBlocking(int socket) noexcept { return true; }

    static struct sockaddr_in setSocketAddress(int adressFamily, const char *ip,
                                               int port) noexcept
    {
        struct sockaddr_in serv_addr;
        return serv_addr;
    }

    static int bind(int sockfd, sockaddr_in *addr, socklen_t addrlen) noexcept
    {
        return 0;
    }

    static int listen(int sockfd, int backlog) noexcept
    {
        return listenReturnValue;
    }

    static int setsockopt(int sockfd, int level, int optname,
                          const void *optval, socklen_t optlen) noexcept
    {
        return 0;
    }

    static int accept4(int sockfd, char *addr, int flags)
    {
        memcpy(addr,"127.0.0.1",9);   
        return acceptedSocket;
    }

    static uint16_t htons(uint16_t hostshort) noexcept { return 0; }

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

    using Epoll_event = struct epoll_event;
    using Sockaddr_in = sockaddr_in;
};
#pragma GCC diagnostic pop
