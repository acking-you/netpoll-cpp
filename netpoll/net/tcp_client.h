#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <thread>

#include "eventloop.h"
#include "inet_address.h"
#include "tcp_connection.h"
namespace netpoll {
class Connector;
using ConnectorPtr = std::shared_ptr<Connector>;
class TcpClient;
using TcpClientPtr = std::shared_ptr<TcpClient>;
namespace tcp {
class Dialer;
}
/**
 * @brief This class represents a TCP client.
 *
 */
class TcpClient : noncopyable, public std::enable_shared_from_this<TcpClient>
{
private:
   friend class tcp::Dialer;

   /**
    * @param serverAddr The address of the server.
    * @param nameArg The name of the client.
    * @note only be used for tcp::Dialer
    */
   explicit TcpClient(const InetAddress &serverAddr,
                      const StringView  &nameArg = "tcp_client");
   // set before call connect()
   void setLoop(EventLoop *loop);

public:
   /**
    * @brief Construct a new TCP client instance.
    *
    * @param loop The event loop in which the client runs.
    * @param serverAddr The address of the server.
    * @param nameArg The name of the client.
    * @note Please use shared_ptr to manage memory.
    */
   TcpClient(EventLoop *loop, const InetAddress &serverAddr,
             const StringView &nameArg = "tcp_client");

   static auto New(EventLoop *loop, const InetAddress &serverAddr,
                   const StringView &nameArg) -> TcpClientPtr
   {
      return std::make_shared<TcpClient>(loop, serverAddr, nameArg);
   }

   ~TcpClient();

   /**
    * @brief Connect to the server.
    *
    */
   void connect();

   /**
    * @brief Disconnect from the server.
    *
    */
   void disconnect();

   /**
    * @brief Stop connecting to the server.
    *
    */
   void stop();

   /**
    * @brief New the TCP connection to the server.
    *
    * @return TcpConnectionPtr
    */
   TcpConnectionPtr connection() const
   {
      std::lock_guard<std::mutex> lock(m_mutex);
      return m_connection;
   }

   /**
    * @brief New the event loop.
    *
    * @return EventLoop*
    */
   EventLoop *getLoop() const { return m_loop; }

   /**
    * @brief Check whether the client re-connect to the server.
    *
    * @return true
    * @return false
    */
   bool retry() const { return m_retry; }

   /**
    * @brief Enable retry after connection is disconnected
    *
    */
   void enableRetry() { m_retry = true; }

   /**
    * @brief New the name of the client.
    *
    * @return const std::string&
    */
   const std::string &name() const { return m_name; }

   /**
    * @brief Set the connection callback.
    *
    * @param cb The callback is called when the connection to the server is
    * established or closed.
    */
   void setConnectionCallback(const ConnectionCallback &cb)
   {
      m_connectionCallback = cb;
   }
   void setConnectionCallback(ConnectionCallback &&cb)
   {
      m_connectionCallback = std::move(cb);
   }

   /**
    * @brief Set the connection error callback.
    *
    * @param cb The callback is called when an error occurs during connecting
    * to the server.
    */
   void setConnectionErrorCallback(const ConnectionErrorCallback &cb);

   /**
    * @brief Set the message callback.
    *
    * @param cb The callback is called when some data is received from the
    * server.
    */
   void setMessageCallback(const RecvMessageCallback &cb)
   {
      m_messageCallback = cb;
   }

   void setMessageCallback(RecvMessageCallback &&cb)
   {
      m_messageCallback = std::move(cb);
   }
   /// Set write complete callback.
   /// Not thread safe.

   /**
    * @brief Set the write complete callback.
    *
    * @param cb The callback is called when data to send is written to the
    * socket.
    */
   void setWriteCompleteCallback(const WriteCompleteCallback &cb)
   {
      m_writeCompleteCallback = cb;
   }

   void setWriteCompleteCallback(WriteCompleteCallback &&cb)
   {
      m_writeCompleteCallback = std::move(cb);
   }

private:
   /// Not thread safe, but in loop
   void newConnection(int sockfd);
   /// Not thread safe, but in loop
   void removeConnection(const TcpConnectionPtr &conn);

   EventLoop              *m_loop{};
   ConnectorPtr            m_connector;   // avoid revealing Connector
   const std::string       m_name;
   // callbacks
   ConnectionCallback      m_connectionCallback;
   ConnectionErrorCallback m_connectionErrorCallback;
   RecvMessageCallback     m_messageCallback;
   WriteCompleteCallback   m_writeCompleteCallback;
   // flags
   std::atomic_bool        m_retry{};     // atomic
   std::atomic_bool        m_connect{};   // atomic
   // always in loop thread
   mutable std::mutex      m_mutex;
   TcpConnectionPtr        m_connection;   // @GuardedBy mutex_
};
}   // namespace netpoll
