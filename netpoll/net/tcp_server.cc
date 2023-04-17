#include "tcp_server.h"

#include <netpoll/wrap/signal_task.h>
#define ENABLE_ELG_LOG
#include <elog/logger.h>

#include <vector>

#include "inner/acceptor.h"
#include "inner/tcp_connection_impl.h"
using namespace netpoll;
using namespace elog;

void TcpServer::setLoop(netpoll::EventLoop *loop)
{
   m_loop = loop;
   m_acceptorPtr->setLoop(loop);
}

TcpServer::TcpServer(EventLoop *loop, const InetAddress &address,
                     const StringView &name, bool reUseAddr, bool reUsePort)
  : m_loop(loop),
    m_acceptorPtr(new Acceptor(loop, address, reUseAddr, reUsePort)),
    m_serverName(name.data(), name.size()),
    m_recvMessageCallback([](const TcpConnectionPtr &,
                             const MessageBuffer *buffer) {
       ELG_ERROR("unhandled recv message [{} bytes]", buffer->readableBytes());
       buffer->retrieveAll();
    })
{
#ifndef _WIN32
   IgnoreSigPipe::Register();
#endif
   m_acceptorPtr->setNewConnectionCallback(
     [&](int fd, InetAddress const &addr) { newConnection(fd, addr); });
}

TcpServer::~TcpServer()
{
   ELG_TRACE("TcpServer::~TcpServer [{}] destructing", m_serverName);
   stop();
}

void TcpServer::newConnection(int sockfd, const InetAddress &peer)
{
   ELG_TRACE("new connection:fd={} address={}", sockfd, peer.toIpPort());
   m_loop->assertInLoopThread();
   EventLoop *ioLoop{};
   if (m_loopPoolPtr && m_loopPoolPtr->size() > 0)
   {
      ioLoop = m_loopPoolPtr->getNextLoop();
   }
   else { ioLoop = m_loop; }
   auto connPtr = std::make_shared<TcpConnectionImpl>(
     ioLoop, sockfd, InetAddress(Socket::getLocalAddr(sockfd)), peer);

   if (m_idleTimeout > 0)
   {
      assert(m_timingWheelMap[ioLoop]);
      connPtr->enableKickingOff(m_idleTimeout, m_timingWheelMap[ioLoop]);
   }
   connPtr->setRecvMsgCallback(m_recvMessageCallback);
   if (m_connectionCallback)
      connPtr->setConnectionCallback(
        [this](const TcpConnectionPtr &connectionPtr) {
           m_connectionCallback(connectionPtr);
        });
   if (m_writeCompleteCallback)
      connPtr->setWriteCompleteCallback(
        [this](const TcpConnectionPtr &connectionPtr) {
           m_writeCompleteCallback(connectionPtr);
        });
   connPtr->setCloseCallback(
     [this](TcpConnectionPtr const &conn) { connectionClosed(conn); });
   m_connSet.insert(connPtr);
   connPtr->connectEstablished();
}

void TcpServer::start()
{
   m_loop->runInLoop([this]() {
      assert(!m_started);
      m_started = true;
      // Initializes the TimingWheel per loop
      if (m_idleTimeout > 0)
      {
         m_timingWheelMap[m_loop] = std::make_shared<TimingWheel>(
           m_loop, m_idleTimeout, 1.0F,
           m_idleTimeout < 500 ? m_idleTimeout + 1 : 100);
         if (m_loopPoolPtr)
         {
            auto loopNum = m_loopPoolPtr->size();
            while (loopNum > 0)
            {
               auto poolLoop              = m_loopPoolPtr->getNextLoop();
               m_timingWheelMap[poolLoop] = std::make_shared<TimingWheel>(
                 poolLoop, m_idleTimeout, 1.0F,
                 m_idleTimeout < 500 ? m_idleTimeout + 1 : 100);
               --loopNum;
            }
         }
      }
      ELG_TRACE("map size={}", m_timingWheelMap.size());
      m_acceptorPtr->listen();
   });
}

void TcpServer::stop()
{
   if (m_loop->isInLoopThread())
   {
      m_acceptorPtr.reset();
      // copy the connSet_ to a vector, use the vector to close the
      // connections to avoid the iterator invalidation.
      std::vector<TcpConnectionPtr> connPtrs;
      connPtrs.reserve(m_connSet.size());
      for (auto &conn : m_connSet) { connPtrs.push_back(conn); }
      for (const auto &connection : connPtrs) { connection->forceClose(); }
   }
   else
   {
      std::promise<void> pro;
      auto               f = pro.get_future();
      m_loop->queueInLoop([this, &pro]() {
         m_acceptorPtr.reset();
         std::vector<TcpConnectionPtr> connPtrs;
         connPtrs.reserve(m_connSet.size());
         for (auto &conn : m_connSet) { connPtrs.push_back(conn); }
         for (const auto &connection : connPtrs) { connection->forceClose(); }
         pro.set_value();
      });
      f.get();
   }
   m_loopPoolPtr.reset();
   for (auto &iter : m_timingWheelMap)
   {
      std::promise<void> pro;
      auto               f = pro.get_future();
      iter.second->getLoop()->runInLoop([&iter, &pro]() mutable {
         iter.second.reset();
         pro.set_value();
      });
      f.get();
   }
}

void TcpServer::handleCloseInLoop(const TcpConnectionPtr &connectionPtr)
{
   size_t n = m_connSet.erase(connectionPtr);
   (void)n;
   assert(n == 1);
   auto connLoop = connectionPtr->getLoop();

   // NOTE: always queue this operation in connLoop, because this connection
   // may be in loop_'s current active channels, waiting to be processed.
   // If `connectDestroyed()` is called here, we will be using an wild pointer
   // later.
   connLoop->queueInLoop([connectionPtr]() {
      dynamic_cast<TcpConnectionImpl *>(connectionPtr.get())
        ->connectDestroyed();
   });
}

void TcpServer::connectionClosed(const TcpConnectionPtr &connectionPtr)
{
   ELG_TRACE("connectionClosed");
   if (m_loop->isInLoopThread()) { handleCloseInLoop(connectionPtr); }
   else
   {
      m_loop->queueInLoop(
        [this, connectionPtr]() { handleCloseInLoop(connectionPtr); });
   }
}

std::string TcpServer::ipPort() const
{
   return m_acceptorPtr->addr().toIpPort();
}

const InetAddress &TcpServer::address() const { return m_acceptorPtr->addr(); }