#include "timer_queue.h"

#include <netpoll/net/channel.h>
#include <netpoll/net/eventloop.h>
#include <netpoll/util/move_wrapper.h>
#ifdef __linux__
#define ENABLE_ELG_LOG
#include <elog/logger.h>
#include <sys/timerfd.h>
#endif
#include <string.h>

#include <iostream>
#ifndef _WIN32
#include <unistd.h>
#endif

using namespace netpoll;
using namespace std::chrono_literals;
#ifdef __linux__
static int createTimerfd()
{
   int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
   if (timerfd < 0) { ELG_ERROR("create timerfd failed!"); }
   return timerfd;
}

static struct timespec howMuchTimeFromNow(const TimePoint &when)
{
   auto microSeconds = std::chrono::duration_cast<std::chrono::microseconds>(
                         when - std::chrono::steady_clock::now())
                         .count();
   if (microSeconds < 100) { microSeconds = 100; }
   struct timespec ts;
   ts.tv_sec  = static_cast<time_t>(microSeconds / 1000000);
   ts.tv_nsec = static_cast<long>((microSeconds % 1000000) * 1000);
   return ts;
}

static void resetTimerfd(int timerfd, const TimePoint &expiration)
{
   // wake up loop by timerfd_settime()
   struct itimerspec newValue;
   struct itimerspec oldValue;
   memset(&newValue, 0, sizeof(newValue));
   memset(&oldValue, 0, sizeof(oldValue));
   newValue.it_value = howMuchTimeFromNow(expiration);
   int ret           = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
   if (ret) { ELG_ERROR("timerfd_settime()"); }
}

static void readTimerfd(int timerfd)
{
   uint64_t howmany;
   ssize_t  n = ::read(timerfd, &howmany, sizeof howmany);
   if (n != sizeof howmany)
   {
      ELG_ERROR("TimerQueue::handleRead() reads {} bytes instead of 8", n);
   }
}

void TimerQueue::handleRead()
{
   m_loop->assertInLoopThread();
   const auto now = std::chrono::steady_clock::now();
   readTimerfd(m_timerFd);

   std::vector<TimerPtr> expired = getExpired(now);

   m_callingExpiredTimers = true;
   // Invoke the highest priority task
   if (m_highestTask &&
       m_timerIdSet.find(m_highestTask->id()) != m_timerIdSet.end())
   {
      m_highestTask->run();
   }
   // Invoke the normal task
   for (auto const &timerPtr : expired)
   {
      if (m_timerIdSet.find(timerPtr->id()) != m_timerIdSet.end())
      {
         timerPtr->run();
      }
   }
   // Invoke the lowest priority task
   if (m_lowestTask &&
       m_timerIdSet.find(m_lowestTask->id()) != m_timerIdSet.end())
   {
      m_lowestTask->run();
   }
   m_callingExpiredTimers = false;

   // Adds the highest and lowest priority tasks to the reset process
   if (m_highestTask) { expired.push_back(std::move(m_highestTask)); }
   if (m_lowestTask) { expired.push_back(std::move(m_lowestTask)); }
   reset(expired, now);
}
#else
static int64_t howMuchTimeFromNow(const TimePoint &when)
{
   auto microSeconds = std::chrono::duration_cast<std::chrono::microseconds>(
                         when - std::chrono::steady_clock::now())
                         .count();
   if (microSeconds < 1000) { microSeconds = 1000; }
   return microSeconds / 1000;
}
void TimerQueue::processTimers()
{
   m_loop->assertInLoopThread();
   const auto now = std::chrono::steady_clock::now();

   std::vector<TimerPtr> expired = getExpired(now);

   m_callingExpiredTimers = true;
   // Invoke the highest priority task
   if (m_highestTask &&
       m_timerIdSet.find(m_highestTask->id()) != m_timerIdSet.end())
   {
      m_highestTask->run();
   }
   // Invoke the normal task
   for (auto const &timerPtr : expired)
   {
      if (m_timerIdSet.find(timerPtr->id()) != m_timerIdSet.end())
      {
         timerPtr->run();
      }
   }
   // Invoke the lowest priority task
   if (m_lowestTask &&
       m_timerIdSet.find(m_lowestTask->id()) != m_timerIdSet.end())
   {
      m_lowestTask->run();
   }
   m_callingExpiredTimers = false;

   // Adds the highest and lowest priority tasks to the reset process
   if (m_highestTask) { expired.push_back(std::move(m_highestTask)); }
   if (m_lowestTask) { expired.push_back(std::move(m_lowestTask)); }
   reset(expired, now);
}
#endif
///////////////////////////////////////
TimerQueue::TimerQueue(EventLoop *loop)
  : m_loop(loop),
#ifdef __linux__
    m_timerFd(createTimerfd()),
    m_timerFdChannelPtr(new Channel(loop, m_timerFd)),
#endif
    m_timers(),
    m_callingExpiredTimers(false)
{
#ifdef __linux__
   m_timerFdChannelPtr->setReadCallback([this] { handleRead(); });
   // we are always reading the timerfd, we disarm it with timerfd_settime.
   m_timerFdChannelPtr->enableReading();
#endif
}
#ifdef __linux__
void TimerQueue::reset()
{
   m_loop->runInLoop([this]() {
      m_timerFdChannelPtr->disableAll();
      m_timerFdChannelPtr->remove();
      close(m_timerFd);
      m_timerFd           = createTimerfd();
      m_timerFdChannelPtr = std::make_unique<Channel>(m_loop, m_timerFd);
      m_timerFdChannelPtr->setReadCallback([this] { handleRead(); });
      // we are always reading the timerfd, we disarm it with timerfd_settime.
      m_timerFdChannelPtr->enableReading();
      if (!m_timers.empty())
      {
         const auto nextExpire = m_timers.top()->when();
         resetTimerfd(m_timerFd, nextExpire);
      }
   });
}
#endif
TimerQueue::~TimerQueue()
{
#ifdef __linux__
   auto fd = m_timerFd;
   m_loop->runInLoop([chlPtr = makeMoveWrapper(m_timerFdChannelPtr), fd]() {
      (*chlPtr)->disableAll();
      (*chlPtr)->remove();
      ::close(fd);
   });
#endif
}

TimerId TimerQueue::addTimer(const TimerCallback &cb, const TimePoint &when,
                             const TimeInterval &interval, bool h, bool l)
{
   assert(h == false || l == false);
   auto *timerPtr = new Timer(cb, when, interval, h, l);

   m_loop->runInLoop(
     [this, timerPtr]() { addTimerInLoop(std::unique_ptr<Timer>(timerPtr)); });
   return timerPtr->id();
}

TimerId TimerQueue::addTimer(TimerCallback &&cb, const TimePoint &when,
                             const TimeInterval &interval, bool h, bool l)
{
   assert(h == false || l == false);
   auto *timerPtr = new Timer(std::move(cb), when, interval, h, l);

   m_loop->runInLoop(
     [this, timerPtr]() { addTimerInLoop(std::unique_ptr<Timer>(timerPtr)); });
   return timerPtr->id();
}

void TimerQueue::addTimerInLoop(TimerPtr &&timer)
{
   m_loop->assertInLoopThread();
#ifdef __linux__
   auto when = timer->when();
#endif
   // the earliest timer changed
   m_timerIdSet.insert(timer->id());
#ifdef __linux__
   if (insert(std::move(timer))) { resetTimerfd(m_timerFd, when); }
#endif
}

void TimerQueue::cancelTimer(TimerId id)
{
   m_loop->runInLoop([this, id]() { m_timerIdSet.erase(id); });
}

bool TimerQueue::insert(TimerPtr &&timerPtr)
{
   m_loop->assertInLoopThread();
   bool earliestChanged = false;
   if (m_timers.empty() || *timerPtr < *m_timers.top())
   {
      earliestChanged = true;
   }
   m_timers.push(std::move(timerPtr));
   return earliestChanged;
}
#ifndef __linux__
int64_t TimerQueue::getTimeout() const
{
   m_loop->assertInLoopThread();
   if (m_timers.empty()) { return 10000; }
   else { return howMuchTimeFromNow(m_timers.top()->when()); }
}
#endif

std::vector<TimerPtr> TimerQueue::getExpired(const TimePoint &now)
{
   std::vector<TimerPtr> expired;
   while (!m_timers.empty())
   {
      if (m_timers.top()->when() < now)
      {
         // The tag determines whether the highest priority task is required. If
         // there is already a task of the highest priority to be executed, the
         // tag is invalid.
         if (m_timers.top()->isHighest() && m_highestTask)
         {
            m_highestTask = moveConstRef(m_timers.top());
         }
         else if (m_timers.top()->isLowest() && m_lowestTask)
         {
            m_lowestTask = moveConstRef(m_timers.top());
         }
         else { expired.emplace_back(moveConstRef(m_timers.top())); }
         m_timers.pop();
      }
      else break;
   }
   return expired;
}

void TimerQueue::reset(const std::vector<TimerPtr> &expired,
                       const TimePoint             &now)
{
   m_loop->assertInLoopThread();
   for (auto &&timerPtr : expired)
   {
      auto iter = m_timerIdSet.find(timerPtr->id());
      if (iter != m_timerIdSet.end())
      {
         if (timerPtr->isRepeat())
         {
            timerPtr->restart(now);
            insert(moveConstRef(timerPtr));
         }
         else { m_timerIdSet.erase(iter); }
      }
   }
#ifdef __linux__
   if (!m_timers.empty())
   {
      const auto nextExpire = m_timers.top()->when();
      resetTimerfd(m_timerFd, nextExpire);
   }
#endif
}
