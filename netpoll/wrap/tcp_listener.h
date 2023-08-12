#pragma once

#include <netpoll/net/tcp_server.h>
#include <netpoll/util/mutex_guard.h>

#include <csignal>

#include "signal_task.h"
#include "trait.h"
#include "types.h"

namespace netpoll {

namespace tcp {
class Listener
{
private:
   explicit Listener(const InetAddress &address,
                     const StringView  &name = "tcp_listener",
                     bool reUseAddr = true, bool reUsePort = true)
     : m_server(new TcpServer(nullptr, address, name, reUseAddr, reUsePort))
   {
   }

   void setLoop(EventLoop *loop) { m_server->setLoop(loop); }

public:
   Listener(Listener &&)            = default;
   Listener &operator=(Listener &&) = default;

   inline static Listener New(const InetAddress &address,
                              const StringView  &name = "tcp_listener",
                              bool reUseAddr = true, bool reUsePort = true)
   {
      return Listener{address, name, reUseAddr, reUsePort};
   }

   /**
    * @brief An idle connection is a connection that has no read or write, kick
    * off it after timeout seconds.
    *
    * @param timeout
    */
   Listener &enableKickoffIdle(size_t timeout)
   {
      m_server->m_idleTimeout = timeout;
      return *this;
   }

   template <
     typename T, typename... Args,
     typename std::enable_if<trait::has_msg<T>() && trait::has_conn<T>() &&
                               trait::has_write_complete<T>(),
                             bool>::type = true>
   Listener &bind(Args &&...args)
   {
      auto data = std::make_shared<T>(std::forward<Args>(args)...);
      m_server->setRecvMessageCallback(
        [data](TcpConnectionPtr const &conn, const MessageBuffer *buffer) {
           data->onMessage(conn, buffer);
        });
      m_server->setConnectionCallback(
        [data](TcpConnectionPtr const &conn) { data->onConnection(conn); });
      m_server->setWriteCompleteCallback(
        [data](TcpConnectionPtr const &conn) { data->onWriteComplete(conn); });
      m_bind = data;
      return *this;
   }

   template <
     typename T, typename... Args,
     typename std::enable_if<trait::has_msg<T>() && trait::has_conn<T>() &&
                               !trait::has_write_complete<T>(),
                             bool>::type = true>
   Listener &bind(Args &&...args)
   {
      auto data = std::make_shared<T>(std::forward<Args>(args)...);
      m_server->setRecvMessageCallback(
        [data](TcpConnectionPtr const &conn, const MessageBuffer *buffer) {
           data->onMessage(conn, buffer);
        });
      m_server->setConnectionCallback(
        [data](TcpConnectionPtr const &conn) { data->onConnection(conn); });
      m_bind = data;
      return *this;
   }

   template <
     typename T, typename... Args,
     typename std::enable_if<trait::has_msg<T>() && !trait::has_conn<T>() &&
                               trait::has_write_complete<T>(),
                             bool>::type = true>
   Listener &bind(Args &&...args)
   {
      auto data = std::make_shared<T>(std::forward<Args>(args)...);
      m_server->setRecvMessageCallback(
        [data](TcpConnectionPtr const &conn, const MessageBuffer *buffer) {
           data->onMessage(conn, buffer);
        });
      m_server->setWriteCompleteCallback(
        [data](TcpConnectionPtr const &conn) { data->onWriteComplete(conn); });
      m_bind = data;
      return *this;
   }

   template <
     typename T, typename... Args,
     typename std::enable_if<!trait::has_msg<T>() && trait::has_conn<T>() &&
                               trait::has_write_complete<T>(),
                             bool>::type = true>
   Listener &bind(Args &&...args)
   {
      auto data = std::make_shared<T>(std::forward<Args>(args)...);
      m_server->setConnectionCallback(
        [data](TcpConnectionPtr const &conn, const MessageBuffer *buffer) {
           data->onConnection(conn, buffer);
        });
      m_server->setWriteCompleteCallback(
        [data](TcpConnectionPtr const &conn) { data->onWriteComplete(conn); });
      m_bind = data;
      return *this;
   }

   template <
     typename T, typename... Args,
     typename std::enable_if<trait::has_msg<T>() && !trait::has_conn<T>() &&
                               !trait::has_write_complete<T>(),
                             bool>::type = true>
   Listener &bind(Args &&...args)
   {
      auto data = std::make_shared<T>(std::forward<Args>(args)...);
      m_server->setRecvMessageCallback(
        [data](TcpConnectionPtr const &conn, const MessageBuffer *buffer) {
           data->onMessage(conn, buffer);
        });
      m_bind = data;
      return *this;
   }

   template <
     typename T, typename... Args,
     typename std::enable_if<trait::has_conn<T>() && !trait::has_msg<T>() &&
                               !trait::has_write_complete<T>(),
                             bool>::type = true>
   Listener &bind(Args &&...args)
   {
      auto data = std::make_shared<T>(std::forward<Args>(args)...);
      m_server->setConnectionCallback(
        [data](TcpConnectionPtr const &conn) { data->onConnection(conn); });
      m_bind = data;
      return *this;
   }

   template <
     typename T, typename... Args,
     typename std::enable_if<trait::has_write_complete<T>() &&
                               !trait::has_msg<T>() && !trait::has_conn<T>(),
                             bool>::type = true>
   Listener &bind(Args &&...args)
   {
      auto data = std::make_shared<T>(std::forward<Args>(args)...);
      m_server->setWriteCompleteCallback(
        [data](TcpConnectionPtr const &conn) { data->onWriteComplete(conn); });
      m_bind = data;
      return *this;
   }

   // Gets the singleton object
   template <typename T>
   std::shared_ptr<T> instance()
   {
      return std::static_pointer_cast<T>(m_bind);
   }

private:
   friend class netpoll::EventLoopWrap;

   std::unique_ptr<TcpServer> m_server;
   std::shared_ptr<void>      m_bind;
};

}   // namespace tcp
}   // namespace netpoll