#pragma once

#include <netpoll/net/tcp_server.h>

#include <csignal>

#include "signal_task.h"
#include "trait.h"
#include "types.h"

namespace netpoll {

namespace tcp {
class Listener : private TcpServer,
                 public std::enable_shared_from_this<Listener>
{
private:
   explicit Listener(const InetAddress &address,
                     const StringView  &name = "tcp_listener",
                     bool reUseAddr = true, bool reUsePort = true)
     : TcpServer(nullptr, address, name, reUseAddr, reUsePort)
   {
   }

   void setLoop(EventLoop *loop) { TcpServer::setLoop(loop); }

public:
   inline static ListenerPtr New(const InetAddress &address,
                                 const StringView  &name = "tcp_listener",
                                 bool reUseAddr = true, bool reUsePort = true)
   {
      return std::shared_ptr<Listener>{
        new Listener{address, name, reUseAddr, reUsePort}};
   }

   /**
    * @brief An idle connection is a connection that has no read or write, kick
    * off it after timeout seconds.
    *
    * @param timeout
    */
   void enableKickoffIdle(size_t timeout)
   {
      TcpServer::kickoffIdleConnections(timeout);
   }

   template <
     typename T, typename... Args,
     typename std::enable_if<trait::has_msg<T>() && trait::has_conn<T>() &&
                               trait::has_write_complete<T>(),
                             bool>::type = true>
   void bind(Args &&...args)
   {
      auto data = std::make_shared<T>(std::forward<Args>(args)...);
      TcpServer::setRecvMessageCallback(
        [data](TcpConnectionPtr const &conn, const MessageBuffer *buffer) {
           data->onMessage(conn, buffer);
        });
      TcpServer::setConnectionCallback(
        [data](TcpConnectionPtr const &conn) { data->onConnection(conn); });
      TcpServer::setWriteCompleteCallback(
        [data](TcpConnectionPtr const &conn) { data->onWriteComplete(conn); });
      m_bind = data;
   }

   template <
     typename T, typename... Args,
     typename std::enable_if<trait::has_msg<T>() && trait::has_conn<T>() &&
                               !trait::has_write_complete<T>(),
                             bool>::type = true>
   void bind(Args &&...args)
   {
      auto data = std::make_shared<T>(std::forward<Args>(args)...);
      TcpServer::setRecvMessageCallback(
        [data](TcpConnectionPtr const &conn, const MessageBuffer *buffer) {
           data->onMessage(conn, buffer);
        });
      TcpServer::setConnectionCallback(
        [data](TcpConnectionPtr const &conn) { data->onConnection(conn); });
      m_bind = data;
   }

   template <
     typename T, typename... Args,
     typename std::enable_if<trait::has_msg<T>() && !trait::has_conn<T>() &&
                               trait::has_write_complete<T>(),
                             bool>::type = true>
   void bind(Args &&...args)
   {
      auto data = std::make_shared<T>(std::forward<Args>(args)...);
      TcpServer::setRecvMessageCallback(
        [data](TcpConnectionPtr const &conn, const MessageBuffer *buffer) {
           data->onMessage(conn, buffer);
        });
      TcpServer::setWriteCompleteCallback(
        [data](TcpConnectionPtr const &conn) { data->onWriteComplete(conn); });
      m_bind = data;
   }

   template <
     typename T, typename... Args,
     typename std::enable_if<!trait::has_msg<T>() && trait::has_conn<T>() &&
                               trait::has_write_complete<T>(),
                             bool>::type = true>
   void bind(Args &&...args)
   {
      auto data = std::make_shared<T>(std::forward<Args>(args)...);
      TcpServer::setConnectionCallback(
        [data](TcpConnectionPtr const &conn, const MessageBuffer *buffer) {
           data->onConnection(conn, buffer);
        });
      TcpServer::setWriteCompleteCallback(
        [data](TcpConnectionPtr const &conn) { data->onWriteComplete(conn); });
      m_bind = data;
   }

   template <
     typename T, typename... Args,
     typename std::enable_if<trait::has_msg<T>() && !trait::has_conn<T>() &&
                               !trait::has_write_complete<T>(),
                             bool>::type = true>
   void bind(Args &&...args)
   {
      auto data = std::make_shared<T>(std::forward<Args>(args)...);
      TcpServer::setRecvMessageCallback(
        [data](TcpConnectionPtr const &conn, const MessageBuffer *buffer) {
           data->onMessage(conn, buffer);
        });
      m_bind = data;
   }

   template <
     typename T, typename... Args,
     typename std::enable_if<trait::has_conn<T>() && !trait::has_msg<T>() &&
                               !trait::has_write_complete<T>(),
                             bool>::type = true>
   void bind(Args &&...args)
   {
      auto data = std::make_shared<T>(std::forward<Args>(args)...);
      TcpServer::setConnectionCallback(
        [data](TcpConnectionPtr const &conn, const MessageBuffer *buffer) {
           data->onConnection(conn, buffer);
        });
      m_bind = data;
   }

   template <
     typename T, typename... Args,
     typename std::enable_if<trait::has_write_complete<T>() &&
                               !trait::has_msg<T>() && !trait::has_conn<T>(),
                             bool>::type = true>
   void bind(Args &&...args)
   {
      auto data = std::make_shared<T>(std::forward<Args>(args)...);
      TcpServer::setWriteCompleteCallback(
        [data](TcpConnectionPtr const &conn, const MessageBuffer *buffer) {
           data->onWriteComplete(conn, buffer);
        });
      m_bind = data;
   }

private:
   friend class netpoll::EventLoopWrap;

   std::shared_ptr<void> m_bind;
};

}   // namespace tcp
}   // namespace netpoll