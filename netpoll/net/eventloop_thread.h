#pragma once

#include <netpoll/net/eventloop.h>
#include <netpoll/util/noncopyable.h>
#include <netpoll/util/string_view.h>

#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

namespace netpoll {
/**
 * @brief This class represents an event loop thread. it lock-free
 *
 */
class EventLoopThread : noncopyable
{
public:
   explicit EventLoopThread(StringView const &threadName = "EventLoopThread");
   ~EventLoopThread();

   /**
    * @brief Wait for the event loop to exit.
    * @note This method blocks the current thread until the event loop exits.
    */
   void wait();

   /**
    * @brief New the pointer of the event loop of the thread.
    *
    * @return EventLoop*
    */
   EventLoop *getLoop() const { return m_loop.load(std::memory_order_acquire); }

   /**
    * @brief Run the event loop of the thread. This method doesn't block the
    * current thread.
    *
    */
   void run();

private:
   void threadWorker();

   std::atomic<EventLoop *>  m_loop;
   std::string               m_loopThreadName;
   std::promise<EventLoop *> m_promiseForLoopPointer;
   std::promise<void>        m_promiseForRun;
   std::promise<void>        m_promiseForLoop;
   std::once_flag            m_once;
   std::thread               m_thread;
};
}   // namespace netpoll
