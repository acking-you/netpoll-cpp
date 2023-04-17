#pragma once

#include <netpoll/net/eventloop.h>
#include <netpoll/net/inet_address.h>

#include <atomic>
#include <memory>

namespace netpoll {
// A class used to replace the connect operation
class Connector : public noncopyable,
                  public std::enable_shared_from_this<Connector>
{
public:
   using NewConnectionCallback   = std::function<void(int sockfd)>;
   using ConnectionErrorCallback = std::function<void()>;

   Connector() = default;
   Connector(EventLoop *loop, const InetAddress &addr, bool retry = true);
   Connector(EventLoop *loop, InetAddress &&addr, bool retry = true);
   ~Connector();

   void setNewConnectionCallback(const NewConnectionCallback &cb)
   {
      m_newConnectionCallback = cb;
   }

   void setNewConnectionCallback(NewConnectionCallback &&cb)
   {
      m_newConnectionCallback = std::move(cb);
   }

   void setErrorCallback(const ConnectionErrorCallback &cb)
   {
      m_errorCallback = cb;
   }

   void setErrorCallback(ConnectionErrorCallback &&cb)
   {
      m_errorCallback = std::move(cb);
   }

   // set before call start()
   void setServerAddress(const InetAddress &addr) { m_serverAddr = addr; }
   void setLoop(EventLoop *loop) { m_loop = loop; }
   // If the connection fails, use this flag to determine whether to retry
   void setRetry(bool retry) { m_retry = retry; }

   const InetAddress &serverAddress() const { return m_serverAddr; }

   void start();

   void restart();

   void stop();

private:
   enum class Status { Disconnected, Connecting, Connected };
   static constexpr int kMaxRetryDelayMs  = 30 * 1000;
   static constexpr int kInitRetryDelayMs = 500;

   void startInLoop();
   void connect();
   void connecting(int sockfd);
   int  removeAndResetChannel();
   void handleWrite();
   void handleError();
   void retry(int sockfd);

   std::shared_ptr<Channel> m_channelPtr;
   EventLoop               *m_loop{};
   InetAddress              m_serverAddr;
   std::atomic_bool         m_started{false};
   std::atomic<Status>      m_status{Status::Disconnected};
   int                      m_retryInterval{kInitRetryDelayMs};
   int                      m_maxRetryInterval{kMaxRetryDelayMs};
   bool                     m_retry{};
   bool                     m_socketHanded{false};
   int                      m_fd{-1};
   NewConnectionCallback    m_newConnectionCallback;
   ConnectionErrorCallback  m_errorCallback;
};

}   // namespace netpoll
