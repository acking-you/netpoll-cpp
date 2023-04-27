#include "eventloop.h"

#define ENABLE_ELG_LOG
#include <elog/logger.h>
#include <netpoll/util/defer_call.h>

#include <cassert>
#include <thread>

#include "channel.h"
#include "inner/poller.h"
#include "inner/timer_queue.h"
#ifdef _WIN32
#include <windows.h>
using ssize_t = long long;
#else
#include <poll.h>
#endif
#ifdef __linux__
#include <sys/eventfd.h>
#endif
#ifndef _WIN32
#include <unistd.h>
#endif
#include <fcntl.h>

#include <algorithm>
#include <csignal>
using namespace elog;
using namespace netpoll;

#ifdef __linux__
int createEventfd()
{
   int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
   if (evtfd < 0) { ELG_FATAL("Failed in eventfd"); }

   return evtfd;
}
const int kPollTimeMs = 10000;
#endif
thread_local EventLoop *t_loopInThisThread = nullptr;

EventLoop::EventLoop()
  : m_looping(false),
    m_tid(std::this_thread::get_id()),
    m_quit(false),
    m_poller(Poller::New(this)),
    m_currentActiveChannel(nullptr),
    m_eventHandling(false),
    m_timerQueue(new TimerQueue(this)),
#ifdef __linux__
    m_wakeupFd(createEventfd()),
    m_wakeupChannelPtr(new Channel(this, m_wakeupFd)),
#endif
    m_threadLocalLoopPtr(&t_loopInThisThread)
{
   if (t_loopInThisThread)
   {
      ELG_ERROR("There is already an EventLoop in this thread");
      abort();
   }
   t_loopInThisThread = this;
#ifdef __linux__
   m_wakeupChannelPtr->setReadCallback([this] { wakeupRead(); });
   m_wakeupChannelPtr->enableReading();
#elif !defined _WIN32
   auto r = pipe(m_wakeupFd);
   (void)r;
   assert(!r);
   fcntl(m_wakeupFd[0], F_SETFL, O_NONBLOCK | O_CLOEXEC);
   fcntl(m_wakeupFd[1], F_SETFL, O_NONBLOCK | O_CLOEXEC);
   m_wakeupChannelPtr =
     std::unique_ptr<Channel>(new Channel(this, m_wakeupFd[0]));
   m_wakeupChannelPtr->setReadCallback([this] { wakeupRead(); });
   m_wakeupChannelPtr->enableReading();
#else
   m_poller->setEventCallback([](uint64_t event) { assert(event == 1); });
#endif
}
#ifdef __linux__
void EventLoop::resetTimerQueue()
{
   assertInLoopThread();
   assert(!m_looping.load(std::memory_order_acquire));
   m_timerQueue->reset();
}
#endif
void EventLoop::resetAfterFork() { m_poller->resetAfterFork(); }
EventLoop::~EventLoop()
{
#ifdef _WIN32
   DWORD delay = 1; /* 1 msec */
#else
   struct timespec delay = {0, 1000000}; /* 1 msec */
#endif

   quit();

   // Spin waiting for the loop to exit because
   // this may take some time to complete. We
   // assume the loop thread will *always* exit.
   // If this cannot be guaranteed then one option
   // might be to abort waiting and
   // assert(!looping_) after some delay;
   while (m_looping.load(std::memory_order_acquire))
   {
#ifdef _WIN32
      Sleep(delay);
#else
      nanosleep(&delay, nullptr);
#endif
   }

   t_loopInThisThread = nullptr;
#ifdef __linux__
   close(m_wakeupFd);
#elif defined _WIN32
#else
   close(wakeupFd_[0]);
   close(wakeupFd_[1]);
#endif
}
EventLoop *EventLoop::getEventLoopOfCurrentThread()
{
   return t_loopInThisThread;
}

void EventLoop::updateChannel(Channel *channel)
{
   assert(channel->ownerLoop() == this);
   assertInLoopThread();
   m_poller->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
   assert(channel->ownerLoop() == this);
   assertInLoopThread();
   m_poller->removeChannel(channel);
}

void EventLoop::quit()
{
   if (m_quit.load(std::memory_order_acquire)) { return; }
   m_quit.store(true, std::memory_order_release);

   if (!isInLoopThread()) { wakeup(); }
}

void EventLoop::loop()
{
   assert(!m_looping);
   assertInLoopThread();
   m_looping.store(true, std::memory_order_release);
   m_quit.store(false, std::memory_order_release);

   std::exception_ptr loopException;
   try
   {   // Scope where the loop flag is set
      auto loopFlagCleaner = makeDeferCall(
        [this]() { m_looping.store(false, std::memory_order_release); });
      while (!m_quit.load(std::memory_order_acquire))
      {
         m_activeChannels.clear();
#ifdef __linux__
         m_poller->poll(kPollTimeMs, &m_activeChannels);
#else
         m_poller->poll(static_cast<int>(m_timerQueue->getTimeout()),
                        &m_activeChannels);
         m_timerQueue->processTimers();
#endif
         // TODO sort channel by priority?
         m_eventHandling = true;
         for (auto &activeChannel : m_activeChannels)
         {
            m_currentActiveChannel = activeChannel;
            m_currentActiveChannel->handleEvent();
         }
         m_currentActiveChannel = nullptr;
         m_eventHandling        = false;
         doRunInLoopFuncs();
      }
      // loopFlagCleaner clears the loop flag here
   }
   catch (...)
   {
      ELG_WARN("Exception thrown from event loop, rethrowing after "
               "running functions on quit");
      loopException = std::current_exception();
   }

   // Run the quit functions even if exceptions were thrown
   // TODO: if more exceptions are thrown in the quit functions, some are left
   // un-run. Can this be made exception safe?
   Functor f;
   while (m_funcOnQuit.dequeue(f)) { f(); }

   // Throw the exception from the end
   if (loopException)
   {
      ELG_WARN("Rethrowing exception from event loop");
      std::rethrow_exception(loopException);
   }
}

void EventLoop::abortNotInLoopThread()
{
   ELG_ERROR(
     "It is forbidden to run loop on threads other than event-loop thread");
   abort();
}

void EventLoop::queueInLoop(const Functor &cb)
{
   m_funcs.enqueue(cb);
   if (!isInLoopThread() || !m_looping.load(std::memory_order_acquire))
   {
      wakeup();
   }
}

void EventLoop::queueInLoop(Functor &&cb)
{
   m_funcs.enqueue(std::move(cb));
   if (!isInLoopThread() || !m_looping.load(std::memory_order_acquire))
   {
      wakeup();
   }
}

TimerId EventLoop::runAt(const Timestamp &time, const TimerCallback &cb, bool h,
                         bool l)
{
   // Since the time point is a steady_clock, it cannot be directly converted
   // through the timestamp, and it is necessary to indirectly construct a
   // steady_clock's time plus interval
   auto microSeconds = time.sinceEpoch<Time::Microseconds>() -
                       Timestamp::now().sinceEpoch<Time::Microseconds>();
   std::chrono::steady_clock::time_point tp =
     std::chrono::steady_clock::now() + std::chrono::microseconds(microSeconds);
   return m_timerQueue->addTimer(cb, tp, std::chrono::microseconds(0), h, l);
}

TimerId EventLoop::runAt(const Timestamp &time, TimerCallback &&cb, bool h,
                         bool l)
{
   auto microSeconds = time.sinceEpoch<Time::Microseconds>() -
                       Timestamp::now().sinceEpoch<Time::Microseconds>();
   std::chrono::steady_clock::time_point tp =
     std::chrono::steady_clock::now() + std::chrono::microseconds(microSeconds);
   return m_timerQueue->addTimer(std::move(cb), tp,
                                 std::chrono::microseconds(0), h, l);
}

TimerId EventLoop::runAfter(double delay, const TimerCallback &cb, bool h,
                            bool l)
{
   return runAt(Timestamp::now() + delay, cb, h, l);
}

TimerId EventLoop::runAfter(double delay, TimerCallback &&cb, bool h, bool l)
{
   return runAt(Timestamp::now() + delay, std::move(cb), h, l);
}

TimerId EventLoop::runEvery(double interval, const TimerCallback &cb, bool h,
                            bool l)
{
   std::chrono::microseconds dur(static_cast<std::chrono::microseconds::rep>(
     interval * Timestamp::kMicroSecondsPerSecond));

   auto tp = std::chrono::steady_clock::now() + dur;
   return m_timerQueue->addTimer(cb, tp, dur, h, l);
}

TimerId EventLoop::runEvery(double interval, TimerCallback &&cb, bool h, bool l)
{
   std::chrono::microseconds dur(static_cast<std::chrono::microseconds::rep>(
     interval * Timestamp::kMicroSecondsPerSecond));

   auto tp = std::chrono::steady_clock::now() + dur;
   return m_timerQueue->addTimer(std::move(cb), tp, dur, h, l);
}

void EventLoop::cancelTimer(TimerId id)
{
   if (isRunning() && m_timerQueue) m_timerQueue->cancelTimer(id);
}

void EventLoop::doRunInLoopFuncs()
{
   m_callingFuncs = true;
   {
      // Assure the flag is cleared even if func throws
      auto callingFlagCleaner =
        makeDeferCall([this]() { m_callingFuncs = false; });
      // the destructor for the Func may itself insert a new entry into the
      // queue
      // TODO: The following is exception-unsafe. If one  of the funcs throws,
      // the remaining ones will not get run. The simplest fix is to catch any
      // exceptions and rethrow them later, but somehow that seems fishy...
      while (!m_funcs.empty())
      {
         Functor func;
         while (m_funcs.dequeue(func)) { func(); }
      }
   }
}

void EventLoop::wakeup()
{
   // TODO check ret?
   uint64_t tmp = 1;
#ifdef __linux__
   int ret = write(m_wakeupFd, &tmp, sizeof(tmp));
   (void)ret;
#elif defined _WIN32
   m_poller->postEvent(1);
#else
   int ret = write(m_wakeupFd[1], &tmp, sizeof(tmp));
   (void)ret;
#endif
}

#if defined(__linux__) || !defined(_WIN32)
void EventLoop::wakeupRead() const
{
   ssize_t  ret = 0;
   uint64_t tmp;
#ifdef __linux__
   ret = read(m_wakeupFd, &tmp, sizeof(tmp));
#else
   ret = read(m_wakeupFd[0], &tmp, sizeof(tmp));
#endif
   if (ret < 0) ELG_ERROR("wakeup read error");
}
#endif

void EventLoop::moveToCurrentThread()
{
   if (isRunning())
   {
      ELG_ERROR("EventLoop cannot be moved when running");
      exit(-1);
   }
   if (isInLoopThread())
   {
      ELG_WARN("This EventLoop is already in the current thread");
      return;
   }
   if (t_loopInThisThread)
   {
      ELG_ERROR("There is already an EventLoop in this thread, you cannot "
                "move another in");
      exit(-1);
   }
   *m_threadLocalLoopPtr = nullptr;
   t_loopInThisThread    = this;
   m_threadLocalLoopPtr  = &t_loopInThisThread;
   m_tid                 = std::this_thread::get_id();
}

void EventLoop::runOnQuit(Functor &&cb) { m_funcOnQuit.enqueue(std::move(cb)); }

void EventLoop::runOnQuit(const Functor &cb) { m_funcOnQuit.enqueue(cb); }
