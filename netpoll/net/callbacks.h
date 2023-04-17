#pragma once

#include <functional>
#include <memory>

namespace netpoll {

// the data has been read to (buf, len)
class TcpConnection;
class MessageBuffer;
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
// tcp server and connection callback
using RecvMessageCallback =
  std::function<void(const TcpConnectionPtr &, const MessageBuffer *)>;
using ConnectionErrorCallback = std::function<void()>;
using ConnectionCallback      = std::function<void(const TcpConnectionPtr &)>;
using CloseCallback           = std::function<void(const TcpConnectionPtr &)>;
using WriteCompleteCallback   = std::function<void(const TcpConnectionPtr &)>;
using HighWaterMarkCallback =
  std::function<void(const TcpConnectionPtr &, const size_t)>;

}   // namespace netpoll
