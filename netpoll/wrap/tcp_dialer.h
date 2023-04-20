#pragma once
#include <netpoll/net/tcp_client.h>

#include "trait.h"
#include "types.h"

namespace netpoll {

namespace tcp {

class Dialer : noncopyable
{
private:
   void setLoop(EventLoop *loop) { m_client->setLoop(loop); }
   void connect() { m_client->connect(); }

public:
   explicit Dialer(const InetAddress &serverAddr,
                   const StringView  &nameArg = "tcp_dialer")
     : m_client(new TcpClient(serverAddr, nameArg))
   {
   }

   static auto New(const InetAddress &serverAddr,
                   const StringView  &nameArg = "tcp_dialer") -> DialerPtr
   {
      return std::make_shared<Dialer>(serverAddr, nameArg);
   }

   void enableRetry() { m_client->enableRetry(); }

   void onMessage(netpoll::RecvMessageCallback const &cb)
   {
      m_client->setMessageCallback(cb);
   }

   void onConnection(netpoll::ConnectionCallback const &cb)
   {
      m_client->setConnectionCallback(cb);
   }

   void onConnectionError(netpoll::ConnectionErrorCallback const &cb)
   {
      m_client->setConnectionErrorCallback(cb);
   }

   void onWriteComplete(netpoll::WriteCompleteCallback const &cb)
   {
      m_client->setWriteCompleteCallback(cb);
   }

   template <typename T, typename... Args>
   static std::shared_ptr<T> Register(Args &&...args)
   {
      auto instance = std::make_shared<T>(std::forward<Args>(args)...);
      return RegisteredEntity(instance);
   }
#if __cplusplus >= 201703L || (_MSC_VER && _MSVC_LANG >= 201703L)

   template <typename T, typename... Args,
             typename std::enable_if<
               trait::has_msg<T>() || trait::has_conn<T>() ||
                 trait::has_conn_error<T>() || trait::has_write_complete<T>(),
               bool>::type = true>
   void bind(Args &&...args)
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
   }

   // Gets the singleton object
   template <typename T>
   std::shared_ptr<T> instance()
   {
      return std::static_pointer_cast<T>(m_bind);
   }
#else
   template <typename T>
   void bindOnMessage()
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
   }

   template <typename T>
   void bindOnConnection()
   {
      auto &data = RegisteredEntity<T>(nullptr);
      if (data)
      {
         m_client->setConnectionCallback(
           [data](TcpConnectionPtr const &conn) { data->onConnection(conn); });
      }
      else { std::cerr << "Please register the type first!"; }
   }

   template <typename T>
   void bindOnConnectionError()
   {
      auto &data = RegisteredEntity<T>(nullptr);
      if (data)
      {
         m_client->setConnectionErrorCallback(
           [data]() { data->onConnectionError(); });
      }
      else { std::cerr << "Please register the type first!"; }
   }

   template <typename T>
   void bindOnWriteComplete()
   {
      auto &data = RegisteredEntity<T>(nullptr);
      if (data)
      {
         m_client->setWriteCompleteCallback(
           [data]() { data->onWriteComplete(); });
      }
      else { std::cerr << "Please register the type first!"; }
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