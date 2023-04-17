#include "connector.h"
#define ENABLE_ELG_LOG
#include <elog/logger.h>
#include <netpoll/net/channel.h>

#include <memory>

#include "socket.h"

using namespace netpoll;
using namespace elog;

Connector::Connector(EventLoop *loop, const InetAddress &addr, bool retry)
  : m_loop(loop), m_serverAddr(addr), m_retry(retry)
{
}
Connector::Connector(EventLoop *loop, InetAddress &&addr, bool retry)
  : m_loop(loop), m_serverAddr(addr), m_retry(retry)
{
}

Connector::~Connector()
{
   if (!m_socketHanded && m_fd != -1)
   {
#ifndef _WIN32
      ::close(m_fd);
#else
      closesocket(m_fd);
#endif
   }
}

void Connector::start()
{
   assert(m_loop != nullptr);
   m_started = true;
   m_loop->runInLoop([this]() { startInLoop(); });
}

void Connector::stop()
{
   m_started = false;
   m_status  = Status::Disconnected;
   if (m_loop->isInLoopThread()) { removeAndResetChannel(); }
   else
   {
      m_loop->queueInLoop(
        [thisPtr = shared_from_this()]() { thisPtr->removeAndResetChannel(); });
   }
}

void Connector::restart()
{
   stop();
   start();
}

void Connector::startInLoop()
{
   assert(m_loop != nullptr);
   m_loop->assertInLoopThread();
   assert(m_status == Status::Disconnected);
   if (m_started) { connect(); }
   else { ELG_TRACE("do not connect"); }
}
void Connector::connect()
{
   m_socketHanded = false;
   m_fd           = Socket::createNonblockingSocketOrDie(m_serverAddr.family());
   errno          = 0;
   int ret        = Socket::connect(m_fd, m_serverAddr);
   int savedErrno = (ret == 0) ? 0 : errno;
   switch (savedErrno)
   {
      case 0:
      case EINPROGRESS:
      case EINTR:
      case EISCONN:
         ELG_TRACE("connecting");
         connecting(m_fd);
         break;

      case EAGAIN:
      case EADDRINUSE:
      case EADDRNOTAVAIL:
      case ECONNREFUSED:
      case ENETUNREACH:
         if (m_retry) { retry(m_fd); }
         break;

      case EACCES:
      case EPERM:
      case EAFNOSUPPORT:
      case EALREADY:
      case EBADF:
      case EFAULT:
      case ENOTSOCK:
         ELG_ERROR("connect error in Connector::startInLoop");
         m_socketHanded = true;
#ifndef _WIN32
         ::close(m_fd);
#else
         closesocket(m_fd);
#endif
         if (m_errorCallback) m_errorCallback();
         break;

      default:
         ELG_ERROR("Unexpected error in Connector::startInLoop ");
         m_socketHanded = true;
#ifndef _WIN32
         ::close(m_fd);
#else
         closesocket(m_fd);
#endif
         if (m_errorCallback) m_errorCallback();
         break;
   }
}

void Connector::connecting(int sockfd)
{
   assert(m_loop != nullptr);
   m_status = Status::Connecting;
   assert(!m_channelPtr);
   m_channelPtr = std::make_shared<Channel>(m_loop, sockfd);
   m_channelPtr->setWriteCallback(
     [self = shared_from_this()] { self->handleWrite(); });
   m_channelPtr->setErrorCallback(
     [self = shared_from_this()] { self->handleError(); });
   m_channelPtr->setCloseCallback(
     [self = shared_from_this()] { self->handleError(); });
   ELG_TRACE("connecting sockfd:{}", sockfd);
   m_channelPtr->enableWriting();
}

int Connector::removeAndResetChannel()
{
   if (!m_channelPtr) { return -1; }
   m_channelPtr->disableAll();
   m_channelPtr->remove();
   int sockfd = m_channelPtr->fd();
   // Can't reset m_channel here, because we are inside Channel::handleEvent.
   // Since the EventLoop holds the original pointer to the channel,
   // to ensure the security of callback, we can keep a reference count.
   m_loop->queueInLoop([channelPtr = m_channelPtr]() {});
   m_channelPtr.reset();
   return sockfd;
}

void Connector::handleWrite()
{
   m_socketHanded = true;
   if (m_status == Status::Connecting)
   {
      int sockfd = removeAndResetChannel();
      int err    = Socket::getSocketError(sockfd);
      if (err)
      {
         ELG_ERROR("Connector::handleWrite - SO_ERROR ");
         if (m_retry) { retry(sockfd); }
         else
         {
            m_socketHanded = true;
#ifndef _WIN32
            ::close(sockfd);
#else
            closesocket(sockfd);
#endif
         }
         if (m_errorCallback) { m_errorCallback(); }
      }
      else if (Socket::isSelfConnect(sockfd))
      {
         ELG_WARN("Connector::handleWrite - Self connect");
         if (m_retry) { retry(sockfd); }
         else
         {
            m_socketHanded = true;
#ifndef _WIN32
            ::close(sockfd);
#else
            closesocket(sockfd);
#endif
         }
         if (m_errorCallback) { m_errorCallback(); }
      }
      else
      {
         m_status = Status::Connected;
         if (m_started) { m_newConnectionCallback(sockfd); }
         else
         {
            m_socketHanded = true;
#ifndef _WIN32
            ::close(sockfd);
#else
            closesocket(sockfd);
#endif
         }
      }
   }
   else
   {
      // has been stopped
      assert(m_status == Status::Disconnected);
   }
}

void Connector::handleError()
{
   m_socketHanded = true;
   if (m_status == Status::Connecting)
   {
      m_status   = Status::Disconnected;
      int sockfd = removeAndResetChannel();
      int err    = Socket::getSocketError(sockfd);
      ELG_ERROR("SO_ERROR = {}", err);
      if (m_retry) { retry(sockfd); }
      else
      {
#ifndef _WIN32
         ::close(sockfd);
#else
         closesocket(sockfd);
#endif
      }
      if (m_errorCallback) { m_errorCallback(); }
   }
}

void Connector::retry(int sockfd)
{
   assert(m_retry);
#ifndef _WIN32
   ::close(sockfd);
#else
   closesocket(sockfd);
#endif
   m_status = Status::Disconnected;
   if (m_started)
   {
      ELG_INFO("Connector::retry - Retry connecting to {} in {} milliseconds. ",
               m_serverAddr.toIpPort(), m_retryInterval);
      m_loop->runAfter(
        m_retryInterval / 1000.0,
        [self = shared_from_this()](TimerId) { self->startInLoop(); });
      m_retryInterval = m_retryInterval * 2;
      if (m_retryInterval > m_maxRetryInterval)
         m_retryInterval = m_maxRetryInterval;
   }
   else { ELG_TRACE("do not connect"); }
}
