#pragma once
#include <netpoll/net/tcp_client.h>

#include "trait.h"
#include "types.h"

namespace netpoll {

namespace tcp {

class Dialer : noncopyable
{
private:
   void setLoop(EventLoop *loop) const { m_client->setLoop(loop); }
   void connect() const { m_client->connect(); }

public:
   explicit Dialer(const InetAddress &serverAddr,
                   const StringView  &nameArg = "tcp_dialer")
     : m_client(new TcpClient(serverAddr, nameArg))
   {
   }

   static auto New(const InetAddress &serverAddr,
                   const StringView  &nameArg = "tcp_dialer") -> Dialer
   {
      return Dialer{serverAddr, nameArg};
   }

   Dialer &enableRetry()
   {
      m_client->enableRetry();
      return *this;
   }

   Dialer &onMessage(netpoll::RecvMessageCallback const &cb)
   {
      m_client->setMessageCallback(cb);
      return *this;
   }

   Dialer &onConnection(netpoll::ConnectionCallback const &cb)
   {
      m_client->setConnectionCallback(cb);
      return *this;
   }

   Dialer &onConnectionError(netpoll::ConnectionErrorCallback const &cb)
   {
      m_client->setConnectionErrorCallback(cb);
      return *this;
   }

   Dialer &onWriteComplete(netpoll::WriteCompleteCallback const &cb)
   {
      m_client->setWriteCompleteCallback(cb);
      return *this;
   }

#if __cplusplus < 201703L || (_MSC_VER && _MSVC_LANG < 201703L)
   template <typename T, typename... Args>
   static std::shared_ptr<T> Register(Args &&...args)
   {
      auto instance = std::make_shared<T>(std::forward<Args>(args)...);
      return RegisteredEntity(instance);
   }
#endif

#if __cplusplus >= 201703L || (_MSC_VER && _MSVC_LANG >= 201703L)

   template <typename T, typename... Args,
             typename std::enable_if<
               trait::has_msg<T>() || trait::has_conn<T>() ||
                 trait::has_conn_error<T>() || trait::has_write_complete<T>(),
               bool>::type = true>
   Dialer &bind(Args &&...args)
   {
      auto data = std::make_shared<T>(std::forward<Args>(args)...);
      if constexpr (trait::has_msg<T>())
      {
         m_client->setMessageCallback(
           [data](TcpConnectionPtr const &conn, const MessageBuffer *buffer) {
              data->onMessage(conn, buffer);
           });
      }
      if constexpr (trait::has_conn<T>())
      {
         m_client->setConnectionCallback(
           [data](TcpConnectionPtr const &conn) { data->onConnection(conn); });
      }
      if constexpr (trait::has_conn_error<T>())
      {
         m_client->setConnectionErrorCallback(
           [data]() { data->onConnectionError(); });
      }
      if constexpr (trait::has_write_complete<T>())
      {
         m_client->setWriteCompleteCallback(
           [data](TcpConnectionPtr const &conn) {
              data->onWriteComplete(conn);
           });
      }
      m_bind = data;
      return *this;
   }

   // Gets the singleton object
   template <typename T>
   std::shared_ptr<T> instance()
   {
      return std::static_pointer_cast<T>(m_bind);
   }
#else
   template <typename T>
   Dialer& bindOnMessage()
   {
      auto &data = RegisteredEntity<T>(nullptr);
      if (data)
      {
         m_client->setMessageCallback(
           [data](TcpConnectionPtr const &conn, const MessageBuffer *buffer) {
              data->onMessage(conn, buffer);
           });
      }
      else { std::cerr << "Please register the type first!"; }
      return *this;
   }

   template <typename T>
   Dialer& bindOnConnection()
   {
      auto &data = RegisteredEntity<T>(nullptr);
      if (data)
      {
         m_client->setConnectionCallback(
           [data](TcpConnectionPtr const &conn) { data->onConnection(conn); });
      }
      else { std::cerr << "Please register the type first!"; }
      return *this;
   }

   template <typename T>
   Dialer& bindOnConnectionError()
   {
      auto &data = RegisteredEntity<T>(nullptr);
      if (data)
      {
         m_client->setConnectionErrorCallback(
           [data]() { data->onConnectionError(); });
      }
      else { std::cerr << "Please register the type first!"; }
      return *this;
   }

   template <typename T>
   Dialer& bindOnWriteComplete()
   {
      auto &data = RegisteredEntity<T>(nullptr);
      if (data)
      {
         m_client->setWriteCompleteCallback(
           [data]() { data->onWriteComplete(); });
      }
      else { std::cerr << "Please register the type first!"; }
      return *this;
   }
#endif

private:
#if __cplusplus >= 201703L || (_MSC_VER && _MSVC_LANG >= 201703L)
   std::shared_ptr<void> m_bind;
#else
   template <typename T>
   static std::shared_ptr<T> &RegisteredEntity(std::shared_ptr<T> const &entity)
   {
      static std::shared_ptr<T> instance = entity;
      return instance;
   }
#endif
   std::shared_ptr<TcpClient> m_client;
   friend class netpoll::EventLoopWrap;
};
}   // namespace tcp
}   // namespace netpoll