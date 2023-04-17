#include <list>

#include "timing_wheel.h"
#ifndef _WIN32
#include <unistd.h>
#endif
#include "../tcp_connection.h"

namespace netpoll {
class Channel;
class Socket;
class TcpConnectionImpl : public TcpConnection,
                          public noncopyable,
                          public std::enable_shared_from_this<TcpConnectionImpl>
{
public:
   friend class TcpServer;
   friend class TcpClient;

   TcpConnectionImpl(EventLoop *loop, int socketfd,
                     const InetAddress &localAddr, const InetAddress &peerAddr);

   static TcpConnectionPtr New(EventLoop *loop, int socketfd,
                               const InetAddress &localAddr,
                               const InetAddress &peerAddr);

   class KickoffEntry
   {
   public:
      explicit KickoffEntry(const std::weak_ptr<TcpConnection> &conn)
        : m_conn(conn)
      {
      }
      void reset() { m_conn.reset(); }
      ~KickoffEntry()
      {
         auto conn = m_conn.lock();
         if (conn) { conn->forceClose(); }
      }

   private:
      std::weak_ptr<TcpConnection> m_conn;
   };

   ~TcpConnectionImpl() override = default;
   void send(StringView const &msg) override;
   void send(const MessageBuffer &buffer) override;
   void send(MessageBuffer &&buffer) override;
   void send(const std::shared_ptr<MessageBuffer> &msgPtr) override;
   void sendFile(StringView const &fileName, size_t offset = 0,
                 size_t length = 0) override;
   void sendStream(
     std::function<std::size_t(char *, std::size_t)> callback) override;

   const InetAddress &localAddr() const override { return m_localAddr; }
   const InetAddress &peerAddr() const override { return m_peerAddr; }

   bool connected() const override { return m_status == ConnStatus::Connected; }
   bool disconnected() const override
   {
      return m_status == ConnStatus::Disconnected;
   }

   MessageBuffer *getRecvBuffer() override { return &m_readBuffer; }

   // set callbacks
   void setHighWaterMarkCallback(const HighWaterMarkCallback &cb,
                                 size_t                       markLen) override
   {
      m_highWaterMarkCallback = cb;
      m_highWaterMarkLen      = markLen;
   }
   void keepAlive() override
   {
      m_idleTimeout = 0;
      auto entry    = m_kickoffEntry.lock();
      if (entry) { entry->reset(); }
   }
   bool       isKeepAlive() override { return m_idleTimeout == 0; }
   void       setTcpNoDelay(bool on) override;
   void       shutdown() override;
   void       forceClose() override;
   EventLoop *getLoop() override { return m_loop; }
   size_t     bytesSent() const override { return m_bytesSent; }
   size_t     bytesReceived() const override { return m_bytesReceived; }

private:
   /// Internal use only.

   std::weak_ptr<KickoffEntry> m_kickoffEntry;
   std::weak_ptr<TimingWheel>  m_timingWheelWeakPtr;
   size_t                      m_idleTimeout{0};
   Timestamp                   m_lastTimingWheelUpdateTime;

   void enableKickingOff(size_t timeout, const TimingWheel::Ptr &timingWheel)
   {
      assert(timingWheel);
      assert(timeout > 0);
      assert(timingWheel->getLoop() == m_loop);
      auto entry           = std::make_shared<KickoffEntry>(shared_from_this());
      m_timingWheelWeakPtr = timingWheel;
      m_kickoffEntry       = entry;
      timingWheel->insertEntry(timeout, entry);
   }
   void extendLife();
#ifndef _WIN32
   void sendFile(int sfd, size_t offset = 0, size_t length = 0);
#else
   void sendFile(const wchar_t *fileName, size_t offset, size_t length);
   void sendFile(FILE *fp, size_t offset = 0, size_t length = 0);
#endif
   void setRecvMsgCallback(const RecvMessageCallback &cb)
   {
      m_recvMsgCallback = cb;
   }
   void setRecvMsgCallback(RecvMessageCallback &&cb)
   {
      m_recvMsgCallback = std::move(cb);
   }
   void setConnectionCallback(const ConnectionCallback &cb)
   {
      m_connectionCallback = cb;
   }
   void setConnectionCallback(ConnectionCallback &&cb)
   {
      m_connectionCallback = std::move(cb);
   }
   void setWriteCompleteCallback(const WriteCompleteCallback &cb)
   {
      m_writeCompleteCallback = cb;
   }
   void setWriteCompleteCallback(WriteCompleteCallback &&cb)
   {
      m_writeCompleteCallback = std::move(cb);
   }
   void setCloseCallback(const CloseCallback &cb) { m_closeCallback = cb; }
   void setCloseCallback(CloseCallback &&cb)
   {
      m_closeCallback = std::move(cb);
   }
   void         connectDestroyed();
   virtual void connectEstablished();

protected:
   struct BufferNode
   {
      // sendFile() specific
#ifndef _WIN32
      int   m_sendFd{-1};
      off_t m_offset{0};
#else
      FILE     *m_sendFp{nullptr};
      long long m_offset{0};
#endif
      ssize_t                                         fileBytesToSend_{0};
      // sendStream() specific
      std::function<std::size_t(char *, std::size_t)> streamCallback_;
#ifndef NDEBUG   // defined by CMake for release build
      std::size_t nDataWritten_{0};
#endif
      // generic
      std::shared_ptr<MessageBuffer> msgBuffer_;

      bool isFile() const
      {
         if (streamCallback_) return true;
#ifndef _WIN32
         if (m_sendFd >= 0) return true;
#else
         if (m_sendFp) return true;
#endif
         return false;
      }
      ~BufferNode()
      {
#ifndef _WIN32
         if (m_sendFd >= 0) ::close(m_sendFd);
#else
         if (m_sendFp) ::fclose(m_sendFp);
#endif
         if (streamCallback_)
            streamCallback_(nullptr, 0);   // cleanup callback internals
      }
   };
   using BufferNodePtr = std::shared_ptr<BufferNode>;
   enum class ConnStatus { Disconnected, Connecting, Connected, Disconnecting };

   void sendFileInLoop(const BufferNodePtr &file);
#ifndef _WIN32
   void    sendInLoop(const void *buffer, size_t length);
   ssize_t writeInLoop(const void *buffer, size_t length);
#else
   void    sendInLoop(const char *buffer, size_t length);
   ssize_t writeInLoop(const char *buffer, size_t length);
#endif
   void handleRead();
   void handleWrite();
   void sendNext();
   void handleClose();
   void handleError();

   EventLoop               *m_loop;
   std::unique_ptr<Channel> m_ioChannelPtr;
   std::unique_ptr<Socket>  m_socketPtr;
   MessageBuffer            m_readBuffer;
   std::list<BufferNodePtr> m_writeBufferList;

   InetAddress           m_localAddr, m_peerAddr;
   ConnStatus            m_status{ConnStatus::Connecting};
   // callbacks
   RecvMessageCallback   m_recvMsgCallback;
   ConnectionCallback    m_connectionCallback;
   CloseCallback         m_closeCallback;
   WriteCompleteCallback m_writeCompleteCallback;
   HighWaterMarkCallback m_highWaterMarkCallback;

   size_t      m_highWaterMarkLen{};
   std::string m_name;

   uint64_t   m_sendNum{0};
   std::mutex m_sendNumMutex;
   uint64_t   getSendNumByGuard()
   {
      std::lock_guard<std::mutex> lockGuard(m_sendNumMutex);
      return m_sendNum;
   }
   uint64_t plusSendNumByGuard()
   {
      std::lock_guard<std::mutex> lockGuard(m_sendNumMutex);
      return ++m_sendNum;
   }
   uint64_t minusSendNumByGuard()
   {
      std::lock_guard<std::mutex> lockGuard(m_sendNumMutex);
      return --m_sendNum;
   }

   size_t m_bytesSent{0};
   size_t m_bytesReceived{0};

   std::unique_ptr<std::vector<char>> m_fileBufferPtr;
};
using TcpConnectionImplPtr = std::shared_ptr<TcpConnectionImpl>;
}   // namespace netpoll
