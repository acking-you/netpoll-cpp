#include "Socket.h"
#define ENABLE_ELG_LOG
#include <elog/logger.h>
#include <sys/types.h>

#include <cassert>
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <netinet/tcp.h>
#include <sys/socket.h>
#endif

using namespace netpoll;
using namespace elog;

int Socket::createNonblockingSocketOrDie(int family)
{
#ifdef __linux__
   int sock =
     ::socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
#else
   int sock = static_cast<int>(::socket(family, SOCK_STREAM, IPPROTO_TCP));
   setNonBlockAndCloseOnExec(sock);
#endif
   if (sock < 0)
   {
      ELG_ERROR("sockets::createNonblockingOrDie");
      exit(1);
   }
   ELG_TRACE("sock={}", sock);
   return sock;
}
int Socket::getSocketError(int sockfd)
{
   int  optval;
   auto optlen = static_cast<socklen_t>(sizeof optval);
#ifdef _WIN32
   if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char *)&optval, &optlen) < 0)
#else
   if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
#endif
   {
      return errno;
   }
   else { return optval; }
}

int Socket::connect(int sockfd, const InetAddress &addr)
{
   if (addr.isIpV6())
      return ::connect(sockfd, addr.getSockAddr(),
                       static_cast<socklen_t>(sizeof(struct sockaddr_in6)));
   else
      return ::connect(sockfd, addr.getSockAddr(),
                       static_cast<socklen_t>(sizeof(struct sockaddr_in)));
}

void Socket::setNonBlockAndCloseOnExec(int sockfd)
{
#ifdef _WIN32
   // TODO how to set FD_CLOEXEC on windows? is it necessary?
   u_long arg = 1;
   auto   ret = ioctlsocket(sockfd, (long)FIONBIO, &arg);
   if (ret) { Log::error(loc::current(), "ioctlsocket error"); }
#else
   // non-block
   int flags = ::fcntl(sockfd, F_GETFL, 0);
   flags |= O_NONBLOCK;
   int ret = ::fcntl(sockfd, F_SETFL, flags);
   if (ret) { Log::error("Socket::fcntl set nonblock"); }
   // close-on-exec
   flags = ::fcntl(sockfd, F_GETFD, 0);
   flags |= FD_CLOEXEC;
   ret = ::fcntl(sockfd, F_SETFD, flags);
   if (ret) { Log::error("Socket::fcntl set close-on-exec"); }
#endif
}

bool Socket::isSelfConnect(int sockfd)
{
   struct sockaddr_in6 localaddr = getLocalAddr(sockfd);
   struct sockaddr_in6 peeraddr  = getPeerAddr(sockfd);
   if (localaddr.sin6_family == AF_INET)
   {
      const struct sockaddr_in *laddr4 =
        reinterpret_cast<struct sockaddr_in *>(&localaddr);
      const struct sockaddr_in *raddr4 =
        reinterpret_cast<struct sockaddr_in *>(&peeraddr);
      return laddr4->sin_port == raddr4->sin_port &&
             laddr4->sin_addr.s_addr == raddr4->sin_addr.s_addr;
   }
   else if (localaddr.sin6_family == AF_INET6)
   {
      return localaddr.sin6_port == peeraddr.sin6_port &&
             memcmp(&localaddr.sin6_addr, &peeraddr.sin6_addr,
                    sizeof localaddr.sin6_addr) == 0;
   }
   else { return false; }
}

void Socket::bindAddress(const InetAddress &localaddr)
{
   assert(m_sockFd > 0);
   int ret;
   if (localaddr.isIpV6())
      ret = ::bind(m_sockFd, localaddr.getSockAddr(), sizeof(sockaddr_in6));
   else ret = ::bind(m_sockFd, localaddr.getSockAddr(), sizeof(sockaddr_in));

   if (ret == 0) return;
   else
   {
      Log::error(loc::current(), "Bind address failed at {}",
                 localaddr.toIpPort());
      exit(1);
   }
}
void Socket::listen()
{
   assert(m_sockFd > 0);
   int ret = ::listen(m_sockFd, SOMAXCONN);
   if (ret < 0)
   {
      ELG_ERROR("listen failed");
      exit(1);
   }
}
int Socket::accept(InetAddress *peeraddr)
{
   struct sockaddr_in6 addr6;
   memset(&addr6, 0, sizeof(addr6));
   socklen_t size = sizeof(addr6);
#ifdef __linux__
   int connfd = ::accept4(m_sockFd, (struct sockaddr *)&addr6, &size,
                          SOCK_NONBLOCK | SOCK_CLOEXEC);
#else
   int connfd =
     static_cast<int>(::accept(m_sockFd, (struct sockaddr *)&addr6, &size));
   setNonBlockAndCloseOnExec(connfd);
#endif
   if (connfd >= 0) { peeraddr->setSockAddrInet6(addr6); }
   return connfd;
}

void Socket::closeWrite()
{
#ifndef _WIN32
   if (::shutdown(m_sockFd, SHUT_WR) < 0)
#else
   if (::shutdown(m_sockFd, SD_SEND) < 0)
#endif
   {
      ELG_ERROR("sockets::shutdownWrite");
   }
}

int Socket::read(char *buffer, uint64_t len)
{
#ifndef _WIN32
   return ::read(m_sockFd, buffer, len);
#else
   return recv(m_sockFd, buffer, static_cast<int>(len), 0);
#endif
}

struct sockaddr_in6 Socket::getLocalAddr(int sockfd)
{
   struct sockaddr_in6 localaddr;
   memset(&localaddr, 0, sizeof(localaddr));
   socklen_t addrlen = static_cast<socklen_t>(sizeof localaddr);
   if (::getsockname(sockfd,
                     static_cast<struct sockaddr *>((void *)(&localaddr)),
                     &addrlen) < 0)
   {
      ELG_ERROR("sockets::getLocalAddr");
   }
   return localaddr;
}

struct sockaddr_in6 Socket::getPeerAddr(int sockfd)
{
   struct sockaddr_in6 peeraddr;
   memset(&peeraddr, 0, sizeof(peeraddr));
   socklen_t addrlen = static_cast<socklen_t>(sizeof peeraddr);
   if (::getpeername(sockfd,
                     static_cast<struct sockaddr *>((void *)(&peeraddr)),
                     &addrlen) < 0)
   {
      ELG_ERROR("sockets::getPeerAddr");
   }
   return peeraddr;
}

void Socket::setTcpNoDelay(bool on)
{
#ifdef _WIN32
   char optval = on ? 1 : 0;
#else
   int optval = on ? 1 : 0;
#endif
   ::setsockopt(m_sockFd, IPPROTO_TCP, TCP_NODELAY, &optval,
                static_cast<socklen_t>(sizeof optval));
   // TODO CHECK
}

void Socket::setReuseAddr(bool on)
{
#ifdef _WIN32
   char optval = on ? 1 : 0;
#else
   int optval = on ? 1 : 0;
#endif
   ::setsockopt(m_sockFd, SOL_SOCKET, SO_REUSEADDR, &optval,
                static_cast<socklen_t>(sizeof optval));
   // TODO CHECK
}

void Socket::setReusePort(bool on)
{
#ifdef _WIN32
#ifdef SO_REUSEADDR
   char optval = on ? 1 : 0;
   int  ret    = ::setsockopt(m_sockFd, SOL_SOCKET, SO_REUSEADDR, &optval,
                              static_cast<socklen_t>(sizeof optval));
   if (ret < 0 && on) { ELG_ERROR("SO_REUSEADDR failed."); }
#else
   if (on) { ELG_ERROR("SO_REUSEADDR is not supported."); }
#endif
#else
#ifdef SO_REUSEPORT
   int optval = on ? 1 : 0;
   int ret    = ::setsockopt(m_sockFd, SOL_SOCKET, SO_REUSEPORT, &optval,
                             static_cast<socklen_t>(sizeof optval));
   if (ret < 0 && on) { ELG_ERROR("SO_REUSEPORT failed."); }
#else
   if (on) { ELG_ERROR("SO_REUSEPORT is not supported."); }
#endif
#endif
}

void Socket::setKeepAlive(bool on)
{
#ifdef _WIN32
   char optval = on ? 1 : 0;
#else
   int optval = on ? 1 : 0;
#endif
   ::setsockopt(m_sockFd, SOL_SOCKET, SO_KEEPALIVE, &optval,
                static_cast<socklen_t>(sizeof optval));
   // TODO CHECK
}

int Socket::getSocketError()
{
#ifdef _WIN32
   char optval;
#else
   int optval;
#endif
   auto optlen = static_cast<socklen_t>(sizeof optval);

   if (::getsockopt(m_sockFd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
   {
#ifdef _WIN32
      return ::WSAGetLastError();
#else
      return errno;
#endif
   }
   else { return optval; }
}

Socket::~Socket()
{
   ELG_TRACE("Socket deconstructed:{}", m_sockFd);
   if (m_sockFd >= 0)
#ifndef _WIN32
      close(m_sockFd);
#else
      closesocket(m_sockFd);
#endif
}
