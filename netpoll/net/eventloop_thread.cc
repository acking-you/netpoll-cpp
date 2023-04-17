#include "eventloop_thread.h"

#include <elog/logger.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif

using namespace netpoll;
using namespace elog;

EventLoopThread::EventLoopThread(StringView const& threadName)
  : m_loop(nullptr),
    m_loopThreadName(threadName.data(), threadName.size()),
    m_thread([this]() { threadWorker(); })
{
   auto f = m_promiseForLoopPointer.get_future();
   m_loop = f.get();
}

EventLoopThread::~EventLoopThread()
{
   run();
   EventLoop* loop = m_loop.load(std::memory_order_acquire);
   if (loop) { loop->quit(); }
   if (m_thread.joinable()) { m_thread.join(); }
}

void EventLoopThread::wait() { m_thread.join(); }

void EventLoopThread::threadWorker()
{
#ifdef __linux__
   ::prctl(PR_SET_NAME, m_loopThreadName.c_str());
#endif
   EventLoop loop;
   loop.queueInLoop([this]() { m_promiseForLoop.set_value(); });
   // Promise get pointer finish
   m_promiseForLoopPointer.set_value(&loop);
   auto f = m_promiseForRun.get_future();
   // Block until the EventLoopThread::run() is called
   f.get();
   loop.loop();

   m_loop.store(nullptr, std::memory_order_release);
}

void EventLoopThread::run()
{
   // Only first call valid
   std::call_once(m_once, [this]() {
      auto f = m_promiseForLoop.get_future();
      m_promiseForRun.set_value();
      // Make sure the event loop loops before returning.
      (void)f.get();
   });
}
