#pragma once
#include <netpoll/util/any.h>
#include <netpoll/util/message_buffer.h>
#include <netpoll/util/noncopyable.h>
#include <netpoll/util/string_view.h>

#include <functional>
#include <memory>
#include <string>

#include "callbacks.h"
#include "eventloop.h"
#include "inet_address.h"

namespace netpoll {

/**
 * @brief This class represents a TCP connection.
 *
 */
class TcpConnection
{
public:
   TcpConnection()          = default;
   virtual ~TcpConnection() = default;
   ;

   /**
    * @brief Send some data to the peer.
    *
    * @param msg
    * @param len
    */
   virtual void send(StringView const &msg)                        = 0;
   virtual void send(const MessageBuffer &buffer)                  = 0;
   virtual void send(MessageBuffer &&buffer)                       = 0;
   virtual void send(const std::shared_ptr<MessageBuffer> &msgPtr) = 0;

   /**
    * @brief Send a file to the peer.
    *
    * @param fileName in UTF-8
    * @param offset
    * @param length
    */
   virtual void sendFile(StringView const &fileName, size_t offset = 0,
                         size_t length = 0) = 0;
   /**
    * @brief Send a stream to the peer.
    *
    * @param callback function to retrieve the stream data (stream ends when a
    * zero size is returned) the callback will be called with nullptr when the
    * send is finished/interrupted, so that it cleans up any internal data (ex:
    * close file).
    * @warning The buffer size should be >= 10 to allow http chunked-encoding
    * data stream
    */
   // (buffer, buffer size) -> size
   // of data put in buffer
   virtual void sendStream(
     std::function<std::size_t(char *, std::size_t)> callback) = 0;

   /**
    * @brief New the local address of the connection.
    *
    * @return const InetAddress&
    */
   virtual const InetAddress &localAddr() const = 0;

   /**
    * @brief New the remote address of the connection.
    *
    * @return const InetAddress&
    */
   virtual const InetAddress &peerAddr() const = 0;

   /**
    * @brief Return true if the connection is established.
    *
    * @return true
    * @return false
    */
   virtual bool connected() const = 0;

   /**
    * @brief Return false if the connection is established.
    *
    * @return true
    * @return false
    */
   virtual bool disconnected() const = 0;

   /**
    * @brief New the buffer in which the received data stored.
    *
    * @return MsgBuffer*
    */
   virtual MessageBuffer *getRecvBuffer() = 0;

   /**
    * @brief Set the high water mark callback
    *
    * @param cb The callback is called when the data in sending buffer is
    * larger than the water mark.
    * @param markLen The water mark in bytes.
    */
   virtual void setHighWaterMarkCallback(const HighWaterMarkCallback &cb,
                                         size_t markLen) = 0;

   /**
    * @brief Set the TCP_NODELAY option to the socket.
    *
    * @param on
    */
   virtual void setTcpNoDelay(bool on) = 0;

   /**
    * @brief Shutdown the connection.
    * @note This method only closes the writing direction.
    */
   virtual void shutdown() = 0;

   /**
    * @brief Close the connection forcefully.
    *
    */
   virtual void forceClose() = 0;

   /**
    * @brief New the event loop in which the connection I/O is handled.
    *
    * @return EventLoop*
    */
   virtual EventLoop *getLoop() = 0;

   /**
    * @brief Set the custom data on the connection.
    *
    * @param context
    */
   void setContext(const Any &context) { m_context = context; }
   void setContext(Any &&context) { m_context = std::move(context); }

   /**
    * @brief New mutable context
    *
    * @return Any
    */
   Any &getMutableContext() { return m_context; }

   /**
    * @brief New unmutable context
    *
    * @return Any
    */
   Any const &getContext() const { return m_context; }

   /**
    * @brief Return true if the custom data is set by user.
    *
    * @return true
    * @return false
    */
   bool hasContext() const
   {
#if __cplusplus >= 201703L
      return m_context.has_value();
#else
      return m_context.empty();
#endif
   }

   /**
    * @brief Clear the custom data.
    *
    */
   void clearContext()
   {
#if __cplusplus >= 201703L
      m_context.reset();
#else
      m_context.clear();
#endif
   }

   /**
    * @brief Call this method to avoid being kicked off by TcpServer, refer to
    * the kickoffIdleConnections method in the TcpServer class.
    *
    */
   virtual void keepAlive() = 0;

   /**
    * @brief Return true if the keepAlive() method is called.
    *
    * @return true
    * @return false
    */
   virtual bool isKeepAlive() = 0;

   /**
    * @brief Return the number of bytes sent
    *
    * @return size_t
    */
   virtual size_t bytesSent() const = 0;

   /**
    * @brief Return the number of bytes received.
    *
    * @return size_t
    */
   virtual size_t bytesReceived() const = 0;

private:
   Any m_context;
};

}   // namespace netpoll
