#pragma once

#include <netpoll/net/channel.h>
#include <netpoll/net/eventloop.h>
#include <netpoll/net/inet_address.h>
#include <netpoll/util/noncopyable.h>

#include <functional>

#include "socket.h"

namespace netpoll {

/**
 * A class used to replace the accept operation
 */
using NewConnectionCallback = std::function<void(int fd, const InetAddress &)>;
class TcpServer;
class Acceptor : noncopyable
{
private:
   friend class TcpServer;
   // To support  EventLoop decoupling,for internal encapsulation only
   void setLoop(EventLoop *loop)
   {
      m_loop = loop;
      m_acceptChannel.setLoop(loop);
   }

public:
   Acceptor(EventLoop *loop, const InetAddress &addr, bool reUseAddr = true,
            bool reUsePort = true);

   ~Acceptor();

   const InetAddress &addr() const { return m_addr; }

   void setNewConnectionCallback(const NewConnectionCallback &cb)
   {
      m_newConnectionCallback = cb;
   };

   void listen();

protected:
   void handleRead();
#ifndef _WIN32
   int m_idleFd;
#endif
   Socket                m_sock;
   InetAddress           m_addr;
   EventLoop            *m_loop;
   NewConnectionCallback m_newConnectionCallback;
   Channel               m_acceptChannel;
};
}   // namespace netpoll
