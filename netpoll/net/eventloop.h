#pragma once
#include <netpoll/net/inner/timer.h>
#include <netpoll/util/any.h>
#include <netpoll/util/lockfree_queue.h>
#include <netpoll/util/noncopyable.h>
#include <netpoll/util/time_stamp.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace netpoll {
class Poller;
class TimerQueue;
class Channel;
using ChannelList = std::vector<Channel *>;
using Functor     = std::function<void()>;
enum { InvalidTimerId = 0 };

/**
 * @brief As the name implies, this class represents an event loop that runs in
 * a perticular thread. The event loop can handle network I/O events and timers
 * in asynchronous mode.
 * @note An event loop object always belongs to a separate thread, and there is
 * one event loop object at most in a thread. We can call an event loop object
 * the event loop of the thread it belongs to, or call that thread the thread of
 * the event loop.
 */
class EventLoop : noncopyable
{
public:
   friend class TimingWheel;
   EventLoop();
   ~EventLoop();

   /**
    * @brief Run the event loop. This method will be blocked until the event
    * loop exits.
    *
    */
   void loop();

   /**
    * @brief Let the event loop quit.
    *
    */
   void quit();

   /**
    * @brief Assertion that the current thread is the thread to which the event
    * loop belongs. If the assertion fails, the program aborts.
    */
   void assertInLoopThread() const
   {
      if (!isInLoopThread()) { abortNotInLoopThread(); }
   };
#ifdef __linux__
   /**
    * @brief Make the timer queue works after calling the fork() function.
    *
    */
   void resetTimerQueue();
#endif
   /**
    * @brief Make the event loop works after calling the fork() function.
    *
    */
   void resetAfterFork();

   /**
    * @brief Return true if the current thread is the thread to which the event
    * loop belongs.
    *
    * @return true
    * @return false
    */
   bool isInLoopThread() const { return m_tid == std::this_thread::get_id(); };

   /**
    * @brief New the event loop of the current thread. Return nullptr if there
    * is no event loop in the current thread.
    *
    * @return EventLoop*
    */
   static EventLoop *getEventLoopOfCurrentThread();

   /**
    * @brief Run the function f in the thread of the event loop.
    *
    * @param f
    * @note If the current thread is the thread of the event loop, the function
    * f is executed directly before the method exiting.
    */
   template <typename Functor>
   inline void runInLoop(Functor &&f)
   {
      if (isInLoopThread()) { f(); }
      else { queueInLoop(std::forward<Functor>(f)); }
   }

   /**
    * @brief Run the function f in the thread of the event loop.
    *
    * @param f
    * @note The difference between this method and the runInLoop() method is
    * that the function f is executed after the method exiting no matter if the
    * current thread is the thread of the event loop.
    */
   void queueInLoop(const Functor &f);
   void queueInLoop(Functor &&f);

   /**
    * @brief Run a function at a time point.
    *
    * @param time The time to run the function.
    * @param cb The function to run.
    * @param highest Want to get the highest priority.
    * @param lowest Want to get the lowest priority.
    * @return TimerId The ID of the timer.
    */
   TimerId runAt(const Timestamp &time, const TimerCallback &cb,
                 bool highest = false, bool lowest = false);
   TimerId runAt(const Timestamp &time, TimerCallback &&cb,
                 bool highest = false, bool lowest = false);

   /**
    * @brief Run a function after a period of time.
    *
    * @param delay Represent the period of time in seconds.
    * @param cb The function to run.
    * @param highest Want to get the highest priority.
    * @param lowest Want to get the lowest priority.
    * @return TimerId The ID of the timer.
    */
   TimerId runAfter(double delay, const TimerCallback &cb, bool highest = false,
                    bool lowest = false);
   TimerId runAfter(double delay, TimerCallback &&cb, bool highest = false,
                    bool lowest = false);

   /**
    * @brief Run a function after a period of time.
    * @note Users could use chrono literals to represent a time duration
    * For example:
    * @code
      runAfter(5s, task);
      runAfter(10min, task);
      @endcode
    */
   TimerId runAfter(const std::chrono::duration<double> &delay,
                    const TimerCallback &cb, bool highest = false,
                    bool lowest = false)
   {
      return runAfter(delay.count(), cb, highest, lowest);
   }
   TimerId runAfter(const std::chrono::duration<double> &delay,
                    TimerCallback &&cb, bool highest = false,
                    bool lowest = false)
   {
      return runAfter(delay.count(), std::move(cb), highest, lowest);
   }

   /**
    * @brief Repeatedly run a function every period of time.
    *
    * @param interval The duration in seconds.
    * @param cb The function to run.
    * @param highest Want to get the highest priority.
    * @param lowest Want to get the lowest priority.
    * @return TimerId The ID of the timer.
    */
   TimerId runEvery(double interval, const TimerCallback &cb,
                    bool highest = false, bool lowest = false);
   TimerId runEvery(double interval, TimerCallback &&cb, bool highest = false,
                    bool lowest = false);

   /**
    * @brief Repeatedly run a function every period of time.
    * Users could use chrono literals to represent a time duration
    * For example:
    * @code
      runEvery(5s, task);
      runEvery(10min, task);
      runEvery(0.1h, task);
      @endcode
    */
   TimerId runEvery(const std::chrono::duration<double> &interval,
                    const TimerCallback &cb, bool highest = false,
                    bool lowest = false)
   {
      return runEvery(interval.count(), cb, highest, lowest);
   }
   TimerId runEvery(const std::chrono::duration<double> &interval,
                    TimerCallback &&cb, bool highest = false,
                    bool lowest = false)
   {
      return runEvery(interval.count(), std::move(cb), highest, lowest);
   }

   /**
    * @brief cancel the timer identified by the given ID.
    *
    * @param id The ID of the timer.
    */
   void cancelTimer(TimerId id);

   /**
    * @brief Move the EventLoop to the current thread, this method must be
    * called before the loop is running.
    *
    */
   void moveToCurrentThread();

   /**
    * @brief Update channel status. This method is usually used internally.
    *
    * @param chl
    */
   void updateChannel(Channel *chl);

   /**
    * @brief Remove a channel from the event loop. This method is usually used
    * internally.
    *
    * @param chl
    */
   void removeChannel(Channel *chl);

   /**
    * @brief Return the index of the event loop.
    *
    * @return size_t
    */
   size_t index() const { return m_index; }

   /**
    * @brief Set the index of the event loop.
    *
    * @param index
    */
   void setIndex(size_t index) { m_index = index; }

   /**
    * @brief Return true if the event loop is running.
    *
    * @return true
    * @return false
    */
   bool isRunning()
   {
      return m_looping.load(std::memory_order_acquire) &&
             (!m_quit.load(std::memory_order_acquire));
   }

   /**
    * @brief Check if the event loop is calling a function.
    *
    * @return true
    * @return false
    */
   bool isCallingFunctions() const { return m_callingFuncs; }

   /**
    * @brief Run functions when the event loop quits
    *
    * @param cb the function to run
    * @note the function runs on the thread that quits the EventLoop
    */
   void runOnQuit(Functor &&cb);
   void runOnQuit(const Functor &cb);

   /**
    * @brief New a context that can access any type
    *
    * @note You must be clear that the order in which it is executed is secure
    */
   const Any &getContext() { return m_context; }
   Any       &getMutableContext() { return m_context; }
   /**
    * @brief Set a context
    * @param ctx the context to set
    * @note You must be clear that the order in which it is executed is secure
    */
   void       setContext(Any const &ctx) { m_context = ctx; }
   void       setContext(Any &&ctx) { m_context = std::move(ctx); }

private:
   static void abortNotInLoopThread();
   void        wakeup();
   bool        isEventHandling() const { return m_eventHandling; }

#if defined(__linux__) || !defined(_WIN32)
   void wakeupRead() const;
#endif
   void doRunInLoopFuncs();

   std::atomic<bool> m_looping;
   std::atomic<bool> m_quit;

   // For internal use only
   bool m_eventHandling;
   bool m_callingFuncs{false};

   std::thread::id         m_tid;
   std::unique_ptr<Poller> m_poller;

   ChannelList m_activeChannels;
   Channel    *m_currentActiveChannel;

   MpscQueue<Functor>          m_funcs;
   std::unique_ptr<TimerQueue> m_timerQueue;
   MpscQueue<Functor>          m_funcOnQuit;
#ifdef __linux__
   int                      m_wakeupFd;
   std::unique_ptr<Channel> m_wakeupChannelPtr;
#elif defined _WIN32
#else
   int                      m_wakeupFd[2];
   std::unique_ptr<Channel> m_wakeupChannelPtr;
#endif

#ifdef _WIN32
   size_t m_index{size_t(-1)};
#else
   size_t m_index{std::numeric_limits<size_t>::max()};
#endif
   /**
    * m_context not thread-safe,You must be clear that the order in which it is
    * executed is secure
    */
   Any         m_context;
   EventLoop **m_threadLocalLoopPtr;
};

}   // namespace netpoll
