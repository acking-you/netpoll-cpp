#pragma once
#include <netpoll/util/noncopyable.h>

#include <memory>
#include <set>
#include <string>

#include "callbacks.h"
#include "eventloop_threadpool.h"
#include "inet_address.h"
#include "inner/timing_wheel.h"
#include "tcp_connection.h"

namespace netpoll {
class Acceptor;
namespace tcp {
class Listener;
}
/**
 * @brief This class represents a TCP server.
 *
 */
class TcpServer : noncopyable
{
private:
   // To support  EventLoop decoupling,for internal encapsulation only
   void setLoop(EventLoop *loop);

public:
   /**
    * @brief Construct a new TCP server instance.
    *
    * @param loop The event loop in which the acceptor of the server is
    * handled.
    * @param address The address of the server.
    * @param name The name of the server.
    * @param reUseAddr The SO_REUSEADDR option.
    * @param reUsePort The SO_REUSEPORT option.
    */
   TcpServer(EventLoop *loop, const InetAddress &address,
             const StringView &name, bool reUseAddr = true,
             bool reUsePort = true);
   ~TcpServer();

   /**
    * @brief Start the server.
    *
    */
   void start();

   /**
    * @brief Stop the server.
    *
    */
   void stop();

   /**
    * @brief Set the number of event loops in which the I/O of connections to
    * the server is handled.
    *
    * @param num
    */
   void setIoLoopNum(size_t num)
   {
      assert(!m_started);
      m_loopPoolPtr = std::make_shared<EventLoopThreadPool>(num);
      m_loopPoolPtr->start();
   }

   /**
    * @brief Set the event loops pool in which the I/O of connections to
    * the server is handled.
    *
    * @param pool
    */
   void setIoLoopThreadPool(const std::shared_ptr<EventLoopThreadPool> &pool)
   {
      assert(pool->size() > 0);
      assert(!m_started);
      m_loopPoolPtr = pool;
      m_loopPoolPtr->start();
   }

   /**
    * @brief Set the message callback.
    *
    * @param cb The callback is called when some data is received on a
    * connection to the server.
    */
   void setRecvMessageCallback(const RecvMessageCallback &cb)
   {
      m_recvMessageCallback = cb;
   }
   void setRecvMessageCallback(RecvMessageCallback &&cb)
   {
      m_recvMessageCallback = std::move(cb);
   }

   /**
    * @brief Set the connection callback.
    *
    * @param cb The callback is called when a connection is established or
    * closed.
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
    * @brief Set the write complete callback.
    *
    * @param cb The callback is called when data to send is written to the
    * socket of a connection.
    */
   void setWriteCompleteCallback(const WriteCompleteCallback &cb)
   {
      m_writeCompleteCallback = cb;
   }
   void setWriteCompleteCallback(WriteCompleteCallback &&cb)
   {
      m_writeCompleteCallback = std::move(cb);
   }

   /**
    * @brief New the name of the server.
    *
    * @return const std::string&
    */
   const std::string &name() const { return m_serverName; }

   /**
    * @brief New the IP and port string of the server.
    *
    * @return const std::string
    */
   std::string ipPort() const;

   /**
    * @brief New the address of the server.
    *
    * @return const trantor::InetAddress&
    */
   const InetAddress &address() const;

   /**
    * @brief New the event loop of the server.
    *
    * @return EventLoop*
    */
   EventLoop *getLoop() const { return m_loop; }

   /**
    * @brief New the I/O event loops of the server.
    *
    * @return std::vector<EventLoop *>
    */
   std::vector<EventLoop *> getIoLoops() const
   {
      return m_loopPoolPtr->getLoops();
   }

   /**
    * @brief An idle connection is a connection that has no read or write, kick
    * off it after timeout seconds.
    *
    * @param timeout
    */
   void kickoffIdleConnections(size_t timeout)
   {
      m_loop->runInLoop([this, timeout]() {
         assert(!m_started);
         m_idleTimeout = timeout;
      });
   }

private:
   friend class netpoll::tcp::Listener;
   void handleCloseInLoop(const TcpConnectionPtr &connectionPtr);
   void newConnection(int fd, const InetAddress &peer);
   void connectionClosed(const TcpConnectionPtr &connectionPtr);

   EventLoop                 *m_loop;
   std::unique_ptr<Acceptor>  m_acceptorPtr;
   std::string                m_serverName;
   std::set<TcpConnectionPtr> m_connSet;

   RecvMessageCallback   m_recvMessageCallback;
   ConnectionCallback    m_connectionCallback;
   WriteCompleteCallback m_writeCompleteCallback;

   size_t                                              m_idleTimeout{0};
   std::map<EventLoop *, std::shared_ptr<TimingWheel>> m_timingWheelMap;
   std::shared_ptr<EventLoopThreadPool>                m_loopPoolPtr;
   bool                                                m_started{false};
};

}   // namespace netpoll
