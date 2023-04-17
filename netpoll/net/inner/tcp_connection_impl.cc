#include "tcp_connection_impl.h"

#include <netpoll/net/channel.h>
#include <netpoll/util/encode_util.h>
#define ENABLE_ELG_LOG
#include <elog/logger.h>
#include <netpoll/util/make_copy_mv.h>

#include "socket.h"

#ifdef __linux__
#include <sys/sendfile.h>
#endif
#ifdef _WIN32
#include <wincrypt.h>
#include <windows.h>
#include <winsock2.h>
#endif
#include <fcntl.h>
#include <sys/stat.h>

using namespace netpoll;

#ifdef _WIN32
// Winsock does not set errno, and WSAGetLastError() has different values than
// errno socket errors
#undef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#undef EPIPE
#define EPIPE WSAENOTCONN
#undef ECONNRESET
#define ECONNRESET WSAECONNRESET
#endif

namespace util {
inline bool WriteSocketError(const char *op)
{
#ifdef _WIN32
   if (errno != 0 && errno != EWOULDBLOCK)
#else
   if (errno != EWOULDBLOCK)
#endif
   {
      // Fatal error
      //  TODO: any other errors?
      if (errno == EPIPE || errno == ECONNRESET)
      {
#ifdef _WIN32
         ELG_TRACE("WSAENOTCONN or WSAECONNRESET, errno={}", errno);
#else
         ELG_TRACE("EPIPE or ECONNRESET, errno={}", errno);
#endif
         ELG_TRACE("{}: return on connection closed", op);
         return true;
      }
      ELG_ERROR("{}: return on unexpected error:{}", op, errno);
      return true;
   }
   ELG_TRACE("{}: break on socket buffer full (?)", op);
   return false;
}
}   // namespace util

TcpConnectionImpl::TcpConnectionImpl(EventLoop *loop, int socketfd,
                                     const InetAddress &localAddr,
                                     const InetAddress &peerAddr)
  : m_loop(loop),
    m_ioChannelPtr(new Channel(loop, socketfd)),
    m_socketPtr(new Socket(socketfd)),
    m_localAddr(localAddr),
    m_peerAddr(peerAddr)
{
   ELG_TRACE("new connection:{} -> {}", peerAddr.toIpPort(),
             localAddr.toIpPort());
   m_ioChannelPtr->setReadCallback([this] { handleRead(); });
   m_ioChannelPtr->setWriteCallback([this] { handleWrite(); });
   m_ioChannelPtr->setCloseCallback([this] { handleClose(); });
   m_ioChannelPtr->setErrorCallback([this] { handleError(); });
   m_socketPtr->setKeepAlive(true);
   m_name = localAddr.toIpPort() + "--" + peerAddr.toIpPort();
}

TcpConnectionPtr TcpConnectionImpl::New(netpoll::EventLoop *loop, int socketfd,
                                        const netpoll::InetAddress &localAddr,
                                        const netpoll::InetAddress &peerAddr)
{
   return std::make_shared<TcpConnectionImpl>(loop, socketfd, localAddr,
                                              peerAddr);
}

void TcpConnectionImpl::handleRead()
{
   m_loop->assertInLoopThread();
   int ret = 0;

   ssize_t n = m_readBuffer.readFd(m_socketPtr->fd(), &ret);
   if (n == 0)
   {
      // socket closed by peer
      handleClose();
   }
   else if (n < 0)
   {
      if (errno == EPIPE || errno == ECONNRESET)
      {
#ifdef _WIN32
         ELG_TRACE("WSAENOTCONN or WSAECONNRESET, errno={} fd={}", errno,
                   m_socketPtr->fd());
#else
         ELG_TRACE("EPIPE or ECONNRESET, errno={} fd={}", errno,
                   m_socketPtr->fd());
#endif
         return;
      }
#ifdef _WIN32
      if (errno == WSAECONNABORTED)
      {
         ELG_TRACE("WSAECONNABORTED, errno={}", errno);
         handleClose();
         return;
      }
#else
      if (errno == EAGAIN)   // TODO: any other errors?
      {
         ELG_TRACE("EAGAIN, errno={} fd = {}", errno, m_socketPtr->fd());
         return;
      }
#endif
      ELG_ERROR("read socket error");
      handleClose();
      return;
   }
   extendLife();
   if (n > 0)
   {
      m_bytesReceived += n;
      if (m_recvMsgCallback)
      {
         m_recvMsgCallback(shared_from_this(), &m_readBuffer);
      }
   }
}

void TcpConnectionImpl::extendLife()
{
   if (m_idleTimeout > 0)
   {
      auto now = Timestamp::now();
      if (now < (m_lastTimingWheelUpdateTime + 1.0)) return;
      m_lastTimingWheelUpdateTime = now;
      auto entry                  = m_kickoffEntry.lock();
      if (entry)
      {
         auto ptr = m_timingWheelWeakPtr.lock();
         if (ptr) ptr->insertEntry(m_idleTimeout, entry);
      }
   }
}

void TcpConnectionImpl::sendNext()
{
   assert(!m_writeBufferList.empty());
   // next is not a file
   if (!m_writeBufferList.front()->isFile())
   {
      // There is data to be sent in the buffer.
      auto n =
        writeInLoop(m_writeBufferList.front()->msgBuffer_->peek(),
                    m_writeBufferList.front()->msgBuffer_->readableBytes());
      if (n >= 0) { m_writeBufferList.front()->msgBuffer_->retrieve(n); }
      else
      {
         if (util::WriteSocketError("send data in sendNext")) { return; }
      }
   }
   else
   {
      // next is a file
      sendFileInLoop(m_writeBufferList.front());
   }
}

// Maintain a send buffer and listen for write events when the socket's write
// buffer is full
void TcpConnectionImpl::handleWrite()
{
   m_loop->assertInLoopThread();
   extendLife();
   if (!m_ioChannelPtr->isWriting())
   {
      ELG_ERROR("no writing but write callback called");
      return;
   }
   assert(!m_writeBufferList.empty());
   auto writeBuffer = m_writeBufferList.front();
   // Case 1, is a file
   if (writeBuffer->isFile())
   {
      // finished sending
      if (writeBuffer->fileBytesToSend_ <= 0)
      {
         m_writeBufferList.pop_front();
         // All write buffer data has been processed. You need to stop focusing
         // on write events.
         if (m_writeBufferList.empty())
         {
            // stop writing
            m_ioChannelPtr->disableWriting();
            if (m_writeCompleteCallback)
               m_writeCompleteCallback(shared_from_this());
            if (m_status == ConnStatus::Disconnecting)
            {
               m_socketPtr->closeWrite();
            }
         }
         else
         {
            // send next
            sendNext();
         }
      }
      else { sendFileInLoop(writeBuffer); }
      return;
   }

   // Case 2, not a file
   if (writeBuffer->msgBuffer_->readableBytes() <= 0)
   {
      // finished sending
      m_writeBufferList.pop_front();
      if (m_writeBufferList.empty())
      {
         // stop writing
         m_ioChannelPtr->disableWriting();
         if (m_writeCompleteCallback)
            m_writeCompleteCallback(shared_from_this());
         if (m_status == ConnStatus::Disconnecting)
         {
            m_socketPtr->closeWrite();
         }
      }
      else
      {
         // send next
         sendNext();
      }
   }
   else
   {
      // continue sending
      auto n = writeInLoop(writeBuffer->msgBuffer_->peek(),
                           writeBuffer->msgBuffer_->readableBytes());
      if (n >= 0) { writeBuffer->msgBuffer_->retrieve(n); }
      else
      {
         if (util::WriteSocketError("send data in writeBuffer")) { return; }
      }
   }
}

void TcpConnectionImpl::connectEstablished()
{
   auto self = shared_from_this();
   m_loop->runInLoop([self]() {
      ELG_TRACE("connectEstablished");
      assert(self->m_status == ConnStatus::Connecting);
      self->m_ioChannelPtr->tie(self);
      self->m_ioChannelPtr->enableReading();
      self->m_status = ConnStatus::Connected;
      if (self->m_connectionCallback) self->m_connectionCallback(self);
   });
}

void TcpConnectionImpl::handleClose()
{
   ELG_TRACE("connection closed, fd={}", m_socketPtr->fd());
   m_loop->assertInLoopThread();
   m_status = ConnStatus::Disconnected;
   m_ioChannelPtr->disableAll();
   auto self = shared_from_this();
   if (m_connectionCallback) m_connectionCallback(self);
   if (m_closeCallback)
   {
      ELG_TRACE("to call close callback");
      m_closeCallback(self);
   }
}

void TcpConnectionImpl::handleError()
{
   int err = m_socketPtr->getSocketError();
   if (err == 0) return;
   if (err == EPIPE ||
#ifndef _WIN32
       err == EBADMSG ||   // ??? 104=EBADMSG
#endif
       err == ECONNRESET)
   {
      ELG_TRACE("[{}] - SO_ERROR:{} - ", m_name, err);
   }
   else { ELG_TRACE("[{}] - SO_ERROR:{} - ", m_name, err); }
}

void TcpConnectionImpl::setTcpNoDelay(bool on)
{
   m_socketPtr->setTcpNoDelay(on);
}

void TcpConnectionImpl::connectDestroyed()
{
   m_loop->assertInLoopThread();
   if (m_status == ConnStatus::Connected)
   {
      m_status = ConnStatus::Disconnected;
      m_ioChannelPtr->disableAll();

      m_connectionCallback(shared_from_this());
   }
   m_ioChannelPtr->remove();
}

void TcpConnectionImpl::shutdown()
{
   auto self = shared_from_this();
   m_loop->runInLoop([self]() {
      if (self->m_status == ConnStatus::Connected)
      {
         self->m_status = ConnStatus::Disconnecting;
         if (!self->m_ioChannelPtr->isWriting())
         {
            self->m_socketPtr->closeWrite();
         }
      }
   });
}

void TcpConnectionImpl::forceClose()
{
   auto self = shared_from_this();
   m_loop->runInLoop([self]() {
      if (self->m_status == ConnStatus::Connected ||
          self->m_status == ConnStatus::Disconnecting)
      {
         self->m_status = ConnStatus::Disconnecting;
         self->handleClose();
      }
   });
}

#ifndef _WIN32
void TcpConnectionImpl::sendInLoop(const void *buffer, size_t length)
#else
void    TcpConnectionImpl::sendInLoop(const char *buffer, size_t length)
#endif
{
   m_loop->assertInLoopThread();
   if (m_status != ConnStatus::Connected)
   {
      ELG_WARN("Connection is not connected,give up sending");
      return;
   }
   extendLife();
   size_t  remainLen = length;
   ssize_t sendLen   = 0;
   // Case 1
   if (!m_ioChannelPtr->isWriting() && m_writeBufferList.empty())
   {
      // send directly
      sendLen = writeInLoop(buffer, length);
      if (sendLen < 0)
      {
         // error
         if (util::WriteSocketError("sendInLoop")) { return; }
         sendLen = 0;
      }
      remainLen -= sendLen;
   }
   // Case 2.If the write buffer is full
   if (remainLen > 0 && m_status == ConnStatus::Connected)
   {
      // If the writable buffer is empty or only one file needs to be sent
      if (m_writeBufferList.empty() || m_writeBufferList.back()->isFile())
      {
         BufferNodePtr node = std::make_shared<BufferNode>();
         node->msgBuffer_   = std::make_shared<MessageBuffer>();
         m_writeBufferList.push_back(std::move(node));
      }
      m_writeBufferList.back()->msgBuffer_->pushBack(
        {static_cast<const char *>(buffer) + sendLen, remainLen});
      // Start listening for writable status
      if (!m_ioChannelPtr->isWriting()) m_ioChannelPtr->enableWriting();
      // If there is too much data in the writable buffer
      if (m_highWaterMarkCallback &&
          m_writeBufferList.back()->msgBuffer_->readableBytes() >
            m_highWaterMarkLen)
      {
         m_highWaterMarkCallback(
           shared_from_this(),
           m_writeBufferList.back()->msgBuffer_->readableBytes());
      }
   }
}

void TcpConnectionImpl::send(const StringView &msg)
{
   // SendNum ensures the order in which the task are executed
   if (m_loop->isInLoopThread())
   {
      if (getSendNumByGuard() == 0) { sendInLoop(msg.data(), msg.size()); }
      else
      {
         plusSendNumByGuard();
         auto buffer = makeCopyMove(std::string{msg.data(), msg.size()});
         auto self   = shared_from_this();
         m_loop->queueInLoop([buffer, self]() {
            self->sendInLoop(buffer.value.data(), buffer.value.length());
            self->minusSendNumByGuard();
         });
      }
   }
   else
   {
      auto buffer = makeCopyMove(std::string{msg.data(), msg.size()});
      auto self   = shared_from_this();
      plusSendNumByGuard();
      m_loop->queueInLoop([self, buffer]() {
         self->sendInLoop(buffer.value.data(), buffer.value.length());
         self->minusSendNumByGuard();
      });
   }
}

// The order of data sending should be same as the order of calls of send()
void TcpConnectionImpl::send(const std::shared_ptr<MessageBuffer> &msgPtr)
{
   if (m_loop->isInLoopThread())
   {
      std::lock_guard<std::mutex> guard(m_sendNumMutex);
      if (m_sendNum == 0)
      {
         sendInLoop(msgPtr->peek(), msgPtr->readableBytes());
      }
      else
      {
         ++m_sendNum;
         auto self = shared_from_this();
         m_loop->queueInLoop([self, msgPtr]() {
            self->sendInLoop(msgPtr->peek(), msgPtr->readableBytes());
            std::lock_guard<std::mutex> guard1(self->m_sendNumMutex);
            --self->m_sendNum;
         });
      }
   }
   else
   {
      auto                        self = shared_from_this();
      std::lock_guard<std::mutex> guard(m_sendNumMutex);
      ++m_sendNum;
      m_loop->queueInLoop([self, msgPtr]() {
         self->sendInLoop(msgPtr->peek(), msgPtr->readableBytes());
         std::lock_guard<std::mutex> guard1(self->m_sendNumMutex);
         --self->m_sendNum;
      });
   }
}

void TcpConnectionImpl::send(const MessageBuffer &buffer)
{
   if (m_loop->isInLoopThread())
   {
      std::lock_guard<std::mutex> guard(m_sendNumMutex);
      if (m_sendNum == 0) { sendInLoop(buffer.peek(), buffer.readableBytes()); }
      else
      {
         ++m_sendNum;
         auto self = shared_from_this();
         m_loop->queueInLoop([self, buffer]() {
            self->sendInLoop(buffer.peek(), buffer.readableBytes());
            std::lock_guard<std::mutex> guard1(self->m_sendNumMutex);
            --self->m_sendNum;
         });
      }
   }
   else
   {
      auto                        self = shared_from_this();
      std::lock_guard<std::mutex> guard(m_sendNumMutex);
      ++m_sendNum;
      m_loop->queueInLoop([self, buffer]() {
         self->sendInLoop(buffer.peek(), buffer.readableBytes());
         std::lock_guard<std::mutex> guard1(self->m_sendNumMutex);
         --self->m_sendNum;
      });
   }
}

void TcpConnectionImpl::send(MessageBuffer &&buffer)
{
   if (m_loop->isInLoopThread())
   {
      if (getSendNumByGuard() == 0)
      {
         sendInLoop(buffer.peek(), buffer.readableBytes());
      }
      else
      {
         auto self = shared_from_this();
         plusSendNumByGuard();
         m_loop->queueInLoop([self, buffer = std::move(buffer)]() {
            self->sendInLoop(buffer.peek(), buffer.readableBytes());
            self->minusSendNumByGuard();
         });
      }
   }
   else
   {
      auto self = shared_from_this();
      plusSendNumByGuard();
      m_loop->queueInLoop([self, buffer = std::move(buffer)]() {
         self->sendInLoop(buffer.peek(), buffer.readableBytes());
         self->minusSendNumByGuard();
      });
   }
}

void TcpConnectionImpl::sendFile(StringView const &fileName, size_t offset,
                                 size_t length)
{
#ifdef _WIN32
   sendFile(utils::toNativePath({fileName.data(), fileName.size()}).c_str(),
            offset, length);
#else    // _WIN32
   assert(fileName.data()[fileName.size()] == '\0');
   int fd = open(fileName.data(), O_RDONLY);

   if (fd < 0)
   {
      ELG_ERROR("{} open error", fileName.data());
      return;
   }

   if (length == 0)
   {
      struct stat filestat
      {
      };
      if (stat(fileName.data(), &filestat) < 0)
      {
         ELG_ERROR("{} stat error", fileName.data());
         close(fd);
         return;
      }
      length = filestat.st_size;
   }

   sendFile(fd, offset, length);
#endif   // _WIN32
}

#ifdef _WIN32
void TcpConnectionImpl::sendFile(const wchar_t *fileName, size_t offset,
                                 size_t length)
{
   FILE *fp;
#ifndef _MSC_VER
   fp = _wfopen(fileName, L"rb");
#else    // _MSC_VER
   if (_wfopen_s(&fp, fileName, L"rb") != 0) fp = nullptr;
#endif   // _MSC_VER
   if (fp == nullptr)
   {
      ELG_ERROR("{} open error", utils::toUtf8(fileName));
      return;
   }

   if (length == 0)
   {
      struct _stati64 filestat;
      if (_wstati64(fileName, &filestat) < 0)
      {
         ELG_ERROR("{} stat error", utils::toUtf8(fileName));
         fclose(fp);
         return;
      }
      length = filestat.st_size;
   }

   sendFile(fp, offset, length);
}
#endif

#ifndef _WIN32
void TcpConnectionImpl::sendFile(int sfd, size_t offset, size_t length)
#else
   void TcpConnectionImpl::sendFile(FILE *fp, size_t offset, size_t length)
#endif
{
   assert(length > 0);
#ifndef _WIN32
   assert(sfd >= 0);
   BufferNodePtr node = std::make_shared<BufferNode>();
   node->m_sendFd     = sfd;
#else
   assert(fp);
   BufferNodePtr node = std::make_shared<BufferNode>();
   node->m_sendFp     = fp;
#endif
   node->m_offset         = static_cast<off_t>(offset);
   node->fileBytesToSend_ = length;
   if (m_loop->isInLoopThread())
   {
      if (getSendNumByGuard() == 0)
      {
         m_writeBufferList.push_back(node);
         if (m_writeBufferList.size() == 1)
         {
            sendFileInLoop(m_writeBufferList.front());
            return;
         }
      }
      else
      {
         auto self = shared_from_this();
         plusSendNumByGuard();
         m_loop->queueInLoop([self, node]() {
            self->m_writeBufferList.push_back(node);
            if (self->m_writeBufferList.size() == 1)
            {
               self->sendFileInLoop(self->m_writeBufferList.front());
            }
            self->minusSendNumByGuard();
         });
      }
   }
   else
   {
      auto self = shared_from_this();
      plusSendNumByGuard();
      m_loop->queueInLoop([self, node]() {
         ELG_TRACE("Push sendfile to list");
         self->m_writeBufferList.push_back(node);
         if (self->m_writeBufferList.size() == 1)
         {
            self->sendFileInLoop(self->m_writeBufferList.front());
         }
         self->minusSendNumByGuard();
      });
   }
}

void TcpConnectionImpl::sendStream(
  std::function<std::size_t(char *, std::size_t)> callback)
{
   BufferNodePtr node     = std::make_shared<BufferNode>();
   // not used, the offset should be handled by the callback
   node->m_offset         = 0;
   node->fileBytesToSend_ = 1;   // force to > 0 until stream sent
   node->streamCallback_  = std::move(callback);
   if (m_loop->isInLoopThread())
   {
      if (getSendNumByGuard() == 0)
      {
         m_writeBufferList.push_back(node);
         if (m_writeBufferList.size() == 1)
         {
            sendFileInLoop(m_writeBufferList.front());
            return;
         }
      }
      else
      {
         auto self = shared_from_this();
         plusSendNumByGuard();
         m_loop->queueInLoop([self, node]() {
            self->m_writeBufferList.push_back(node);
            if (self->m_writeBufferList.size() == 1)
            {
               self->sendFileInLoop(self->m_writeBufferList.front());
            }
            self->minusSendNumByGuard();
         });
      }
   }
   else
   {
      auto self = shared_from_this();
      plusSendNumByGuard();
      m_loop->queueInLoop([self, node]() {
         ELG_TRACE("Push sendstream to list");
         self->m_writeBufferList.push_back(node);

         if (self->m_writeBufferList.size() == 1)
         {
            self->sendFileInLoop(self->m_writeBufferList.front());
         }
         self->minusSendNumByGuard();
      });
   }
}

void TcpConnectionImpl::sendFileInLoop(const BufferNodePtr &filePtr)
{
   m_loop->assertInLoopThread();
   assert(filePtr->isFile());
#ifdef __linux__
   // Case 1
   if (!filePtr->streamCallback_)
   {
      ELG_TRACE("send file in loop using linux kernel sendfile()");
      auto bytesSent = sendfile(m_socketPtr->fd(), filePtr->m_sendFd,
                                &filePtr->m_offset, filePtr->fileBytesToSend_);
      if (bytesSent < 0)
      {
         if (errno != EAGAIN)
         {
            ELG_ERROR("TcpConnectionImpl::sendFileInLoop");
            if (m_ioChannelPtr->isWriting()) m_ioChannelPtr->disableWriting();
         }
         return;
      }
      if (bytesSent < filePtr->fileBytesToSend_ && bytesSent == 0)
      {
         ELG_ERROR("TcpConnectionImpl::sendFileInLoop");
         return;
      }
      ELG_TRACE("sendfile() {} bytes sent", bytesSent);
      filePtr->fileBytesToSend_ -= bytesSent;
      if (!m_ioChannelPtr->isWriting()) { m_ioChannelPtr->enableWriting(); }
      return;
   }
#endif
   // Case 2,Send stream
   if (filePtr->streamCallback_)
   {
      ELG_TRACE("send stream in loop");
      if (!m_fileBufferPtr)
      {
         m_fileBufferPtr = std::make_unique<std::vector<char>>();
         m_fileBufferPtr->reserve(16 * 1024);
      }
      while ((filePtr->fileBytesToSend_ > 0) || !m_fileBufferPtr->empty())
      {
         // get next chunk
         if (m_fileBufferPtr->empty())
         {
            ELG_TRACE("send stream in loop: fetch data on buffer empty");
            m_fileBufferPtr->resize(16 * 1024);
            std::size_t nData;
            nData = filePtr->streamCallback_(m_fileBufferPtr->data(),
                                             m_fileBufferPtr->size());
            m_fileBufferPtr->resize(nData);
            if (nData == 0)   // no more data!
            {
               ELG_TRACE("send stream in loop: no more data");
               filePtr->fileBytesToSend_ = 0;
            }
         }
         if (m_fileBufferPtr->empty())
         {
            ELG_TRACE("send stream in loop: break on buffer empty");
            break;
         }
         auto nToWrite = m_fileBufferPtr->size();
         auto nWritten = writeInLoop(m_fileBufferPtr->data(), nToWrite);
         if (nWritten >= 0)
         {
#ifndef NDEBUG   // defined for debug
            filePtr->nDataWritten_ += nWritten;
            ELG_TRACE(
              "send stream in loop: bytes written: {}/total bytes written:{}",
              nWritten, filePtr->nDataWritten_);
#endif
            if (static_cast<std::size_t>(nWritten) < nToWrite)
            {
               // Partial write - return and wait for next call to continue
               m_fileBufferPtr->erase(m_fileBufferPtr->begin(),
                                      m_fileBufferPtr->begin() + nWritten);
               if (!m_ioChannelPtr->isWriting())
                  m_ioChannelPtr->enableWriting();
               ELG_TRACE("send stream in loop: return on partial write "
                         "(socket buffer full?)");
               return;
            }
            m_fileBufferPtr->resize(0);
            continue;
         }
         // nWritten < 0
         if (util::WriteSocketError("send stream in loop")) { return; }
         break;
      }
      if (!m_ioChannelPtr->isWriting()) m_ioChannelPtr->enableWriting();
      ELG_TRACE("send stream in loop: return on loop exit");
      return;
   }

   // Case 3,send file,enabled if there is no stream or no sendfile call
   ELG_TRACE("send file in loop");
   if (!m_fileBufferPtr)
   {
      m_fileBufferPtr = std::make_unique<std::vector<char>>(16 * 1024);
   }
#ifndef _WIN32
   lseek(filePtr->m_sendFd, filePtr->m_offset, SEEK_SET);
   while (filePtr->fileBytesToSend_ > 0)
   {
      auto n = read(filePtr->m_sendFd, &(*m_fileBufferPtr)[0],
                    std::min(static_cast<ssize_t>(m_fileBufferPtr->size()),
                             filePtr->fileBytesToSend_));
#else
   _fseeki64(filePtr->m_sendFp, filePtr->m_offset, SEEK_SET);
   while (filePtr->fileBytesToSend_ > 0)
   {
      //        LOG_TRACE << "send file in loop: fetch more remaining data";
      auto bytes = static_cast<decltype(m_fileBufferPtr->size())>(
        filePtr->fileBytesToSend_);
      auto n = fread(
        &(*m_fileBufferPtr)[0], 1,
        (m_fileBufferPtr->size() < bytes ? m_fileBufferPtr->size() : bytes),
        filePtr->m_sendFp);
#endif
      if (n > 0)
      {
         auto nSend = writeInLoop(&(*m_fileBufferPtr)[0], n);
         if (nSend >= 0)
         {
            filePtr->fileBytesToSend_ -= nSend;
            filePtr->m_offset += static_cast<off_t>(nSend);
            if (static_cast<size_t>(nSend) < static_cast<size_t>(n))
            {
               if (!m_ioChannelPtr->isWriting())
               {
                  m_ioChannelPtr->enableWriting();
               }
               ELG_TRACE("send file in loop: return on partial write (socket "
                         "buffer full?)");
               return;
            }
            else if (nSend == n) { continue; }
         }
         if (nSend < 0)
         {
            if (util::WriteSocketError("send file in loop")) { return; }
            break;
         }
      }
      if (n < 0)
      {
         ELG_ERROR("send file in loop: return on read error");
         if (m_ioChannelPtr->isWriting()) m_ioChannelPtr->disableWriting();
         return;
      }
      if (n == 0)
      {
         ELG_ERROR("send file in loop: return on read 0 (file truncated)");
         return;
      }
   }
   ELG_TRACE("send file in loop: return on loop exit");
   if (!m_ioChannelPtr->isWriting()) { m_ioChannelPtr->enableWriting(); }
}
#ifndef _WIN32
ssize_t TcpConnectionImpl::writeInLoop(const void *buffer, size_t length)
#else
ssize_t TcpConnectionImpl::writeInLoop(const char *buffer, size_t length)
#endif
{
#ifndef _WIN32
   ssize_t nWritten = write(m_socketPtr->fd(), buffer, length);
#else
   int nWritten =
     ::send(m_socketPtr->fd(), buffer, static_cast<int>(length), 0);
   errno = (nWritten < 0) ? ::WSAGetLastError() : 0;
#endif
   if (nWritten > 0) m_bytesSent += nWritten;
   return nWritten;
}
