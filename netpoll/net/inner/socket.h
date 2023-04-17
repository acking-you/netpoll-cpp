#pragma once

#include <netpoll/net/inet_address.h>
#include <netpoll/util/noncopyable.h>

#include <string>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <fcntl.h>

namespace netpoll {
class Socket : noncopyable
{
public:
   static int createNonblockingSocketOrDie(int family);

   static int getSocketError(int sockfd);

   static int connect(int sockfd, const InetAddress &addr);

   static bool isSelfConnect(int sockfd);
   // taken from muduo
   static void setNonBlockAndCloseOnExec(int sockfd);

   explicit Socket(int sockfd) : m_sockFd(sockfd) {}
   ~Socket();
   /// abort if address in use
   void                       bindAddress(const InetAddress &localaddr);
   /// abort if address in use
   void                       listen();
   int                        accept(InetAddress *peeraddr);
   void                       closeWrite();
   int                        read(char *buffer, uint64_t len);
   int                        fd() const { return m_sockFd; }
   static struct sockaddr_in6 getLocalAddr(int sockfd);
   static struct sockaddr_in6 getPeerAddr(int sockfd);

   ///
   /// Enable/disable TCP_NODELAY (disable/enable Nagle's algorithm).
   ///
   void setTcpNoDelay(bool on);

   ///
   /// Enable/disable SO_REUSEADDR
   ///
   void setReuseAddr(bool on);

   ///
   /// Enable/disable SO_REUSEPORT
   ///
   void setReusePort(bool on);

   ///
   /// Enable/disable SO_KEEPALIVE
   ///
   void setKeepAlive(bool on);
   int  getSocketError();

protected:
   int m_sockFd;
};

}   // namespace netpoll