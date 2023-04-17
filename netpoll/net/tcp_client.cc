#include "tcp_client.h"

#include <netpoll/wrap/eventloop_wrap.h>
#define ENABLE_ELG_LOG
#include <elog/logger.h>
#include <netpoll/net/channel.h>
#include <netpoll/net/eventloop.h>
#include <netpoll/wrap/signal_task.h>

#include <algorithm>
#include <csignal>
#include <cstdio>   // snprintf
#include <functional>

#include "inner/connector.h"
#include "inner/socket.h"
#include "inner/tcp_connection_impl.h"

using namespace netpoll;
using namespace elog;

namespace netpoll {
static void defaultConnectionCallback(const TcpConnectionPtr &conn)
{
   ELG_TRACE("{} -> {} is {}", conn->localAddr().toIpPort(),
             conn->peerAddr().toIpPort(), (conn->connected() ? "UP" : "DOWN"));
   // do not call conn->forceClose(), because some users want to register
   // message callback only.
}
static void defaultMessageCallback(const TcpConnectionPtr &,
                                   const MessageBuffer *buf)
{
   buf->retrieveAll();
}
}   // namespace netpoll

TcpClient::TcpClient(const netpoll::InetAddress &serverAddr,
                     const netpoll::StringView  &nameArg)
  : m_connector(std::make_shared<Connector>()),
    m_name(nameArg.data(), nameArg.size()),
    m_retry(false),
    m_connect(false)
{
#ifndef _WIN32
   IgnoreSigPipe::Register();
#endif
   m_connector->setServerAddress(serverAddr);
   m_connector->setRetry(false);
   m_connector->setNewConnectionCallback(
     [this](int sockfd) { newConnection(sockfd); });
}

void TcpClient::setLoop(netpoll::EventLoop *loop)
{
   m_loop = loop;
   m_connector->setLoop(loop);
}

void TcpClient::setConnectionErrorCallback(
  const netpoll::ConnectionErrorCallback &cb)
{
   m_connectionErrorCallback = cb;
   m_connector->setErrorCallback(m_connectionErrorCallback);
}

TcpClient::TcpClient(EventLoop *loop, const InetAddress &serverAddr,
                     const StringView &nameArg)
  : m_loop(loop),
    m_connector(std::make_shared<Connector>(loop, serverAddr, false)),
    m_name(nameArg.data(), nameArg.size()),
    m_connectionCallback(defaultConnectionCallback),
    m_messageCallback(defaultMessageCallback),
    m_retry(false),
    m_connect(false)
{
#ifndef _WIN32
   IgnoreSigPipe::Register();
#endif
   m_connector->setNewConnectionCallback(
     [this](int sockfd) { newConnection(sockfd); });
   ELG_TRACE("TcpClient::TcpClient[{}] - connector ", m_name);
}

TcpClient::~TcpClient()
{
   assert(m_loop != nullptr);
   ELG_TRACE("TcpClient::~TcpClient[{}] - connector ", m_name);
   // Check whether the EventLoop is quit
   if (!EventLoopWrap::allEventLoop().empty())
   {
      for (auto *loop : EventLoopWrap::allEventLoop())
      {
         if (m_loop == loop) { return; }
      }
   }

   TcpConnectionImplPtr conn;
   {
      std::lock_guard<std::mutex> lock(m_mutex);
      conn = std::dynamic_pointer_cast<TcpConnectionImpl>(m_connection);
      if (conn)
      {
         assert(m_loop == conn->getLoop());
         // TODO: not 100% safe, if we are in different thread
         auto loop = m_loop;
         m_loop->runInLoop([conn, loop]() {
            conn->setCloseCallback([loop](const TcpConnectionPtr &connPtr) {
               loop->queueInLoop([connPtr]() {
                  dynamic_cast<TcpConnectionImpl *>(connPtr.get())
                    ->connectDestroyed();
               });
            });
         });
         conn->forceClose();
      }
      else { m_connector->stop(); }
   }
}

void TcpClient::connect()
{
   assert(m_loop != nullptr);
   if (m_connect)
   {
      ELG_WARN("It's already connected");
      return;
   }
   ELG_TRACE("TcpClient::connect[{}] - connecting to {}", m_name,
             m_connector->serverAddress().toIpPort());
   m_connect = true;
   m_connector->start();
}

void TcpClient::disconnect()
{
   m_connect = false;

   {
      std::lock_guard<std::mutex> lock(m_mutex);
      if (m_connection) { m_connection->shutdown(); }
   }
}

void TcpClient::stop()
{
   m_connect = false;
   m_connector->stop();
}

void TcpClient::newConnection(int sockfd)
{
   assert(m_loop != nullptr);
   m_loop->assertInLoopThread();
   InetAddress                        peerAddr(Socket::getPeerAddr(sockfd));
   InetAddress                        localAddr(Socket::getLocalAddr(sockfd));
   std::shared_ptr<TcpConnectionImpl> conn;
   conn =
     std::make_shared<TcpConnectionImpl>(m_loop, sockfd, localAddr, peerAddr);
   conn->setConnectionCallback(m_connectionCallback);
   conn->setRecvMsgCallback(m_messageCallback);
   conn->setWriteCompleteCallback(m_writeCompleteCallback);

   // Reconnection after disconnection
   std::weak_ptr<TcpClient> weakSelf(shared_from_this());
   auto closeCb = std::function<void(const TcpConnectionPtr &)>(
     [weakSelf](const TcpConnectionPtr &c) {
        // If the TcpClient still exists, perform the customized close(support
        // reconnection).
        if (auto self = weakSelf.lock()) { self->removeConnection(c); }
        // Else the TcpClient instance has already been destroyed
        else
        {
           ELG_TRACE("TcpClient::removeConnection was skipped because "
                     "TcpClient instanced already freed");
           c->getLoop()->queueInLoop(
             [conn = std::dynamic_pointer_cast<TcpConnectionImpl>(c)] {
                conn->connectDestroyed();
             });
        }
     });
   conn->setCloseCallback(std::move(closeCb));

   {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_connection = conn;
   }
   conn->connectEstablished();
}

// The tcp connection is reconnected after it is disconnected
void TcpClient::removeConnection(const TcpConnectionPtr &conn)
{
   assert(m_loop != nullptr);
   m_loop->assertInLoopThread();
   assert(m_loop == conn->getLoop());

   {
      std::lock_guard<std::mutex> lock(m_mutex);
      assert(m_connection == conn);
      m_connection.reset();
   }

   m_loop->queueInLoop([conn = std::dynamic_pointer_cast<TcpConnectionImpl>(
                          conn)] { conn->connectDestroyed(); });

   // Start reconnection
   if (m_retry && m_connect)
   {
      ELG_TRACE("TcpClient::connect[{}] - Reconnecting to {}", m_name,
                m_connector->serverAddress().toIpPort());
      m_connector->restart();
   }
}
