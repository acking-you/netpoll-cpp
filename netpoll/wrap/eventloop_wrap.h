#pragma once
#include <netpoll/net/eventloop_threadpool.h>

#include <future>
#include <thread>
#include <vector>

#include "types.h"

namespace netpoll {
class TcpClient;
namespace tcp {
class Listener;
class Dialer;
}   // namespace tcp

// only call once
void QuitAllEventLoop();

class EventLoopWrap
{
private:
   explicit EventLoopWrap(size_t            threadNum,
                          StringView const &name = "EventLoop")
     : m_pool(std::make_shared<EventLoopThreadPool>(threadNum, name))
   {
   }

public:
   EventLoopWrap(EventLoopWrap const &)            = default;
   EventLoopWrap &operator=(EventLoopWrap const &) = default;

   static EventLoopWrap New(size_t                     threadNum,
                            const netpoll::StringView &name = "EventLoop")
   {
      return EventLoopWrap{threadNum, name};
   }

   void serve(tcp::ListenerPtr &listener);

   void serve(tcp::DialerPtr &dialer);

   void serve(const std::vector<tcp::DialerPtr> &dialers);

   template <typename T>
   std::future<void> serveAsDaemon(T &src)
   {
      return runAsDaemon([self = *this, src]() mutable { self.serve(src); });
   }

   void enableQuit() { m_quit = true; }

   friend void netpoll::QuitAllEventLoop();

private:
   friend class netpoll::TcpClient;
   static EventLoop                *getEventLoop();
   static std::vector<EventLoop *> &allEventLoop();
   static std::future<void> runAsDaemon(std::function<void()> const &func);
   static std::future<void> runAsDaemon(std::function<void()> &&func);

   bool                                 m_quit{};
   std::shared_ptr<EventLoopThreadPool> m_pool;
};

inline EventLoopWrap NewEventLoop(size_t                     threadNum = 2,
                                  const netpoll::StringView &name = "eventloop")
{
   return EventLoopWrap::New(threadNum, name);
}

}   // namespace netpoll