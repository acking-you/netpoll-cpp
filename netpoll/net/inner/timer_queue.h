#pragma once

#include <netpoll/util/noncopyable.h>

#include <atomic>
#include <memory>
#include <queue>
#include <unordered_set>

#include "timer.h"
namespace netpoll {
// class Timer;
class EventLoop;
class Channel;

using TimerPtr = std::unique_ptr<Timer>;

struct TimerPtrComparer
{
   bool operator()(const TimerPtr &x, const TimerPtr &y) const
   {
      return *x > *y;
   }
};
using TimerPriorityQueue =
  std::priority_queue<TimerPtr, std::vector<TimerPtr>, TimerPtrComparer>;
using ActiveTimerSet = std::unordered_set<uint64_t>;

class TimerQueue : noncopyable
{
public:
   explicit TimerQueue(EventLoop *loop);
   ~TimerQueue();
   TimerId addTimer(const TimerCallback &cb, const TimePoint &when,
                    const TimeInterval &interval, bool h, bool l);
   TimerId addTimer(TimerCallback &&cb, const TimePoint &when,
                    const TimeInterval &interval, bool h, bool l);
   void    addTimerInLoop(TimerPtr &&timer);
   void    cancelTimer(TimerId id);
#ifdef __linux__
   void reset();
#else
   int64_t getTimeout() const;
   void    processTimers();
#endif
protected:
   std::vector<TimerPtr> getExpired(const TimePoint &now);
   bool                  insert(TimerPtr &&timePtr);
   void reset(const std::vector<TimerPtr> &expired, const TimePoint &now);

   EventLoop *m_loop;
   TimerPtr   m_highestTask;
   TimerPtr   m_lowestTask;
#ifdef __linux__
   int                      m_timerFd;
   std::unique_ptr<Channel> m_timerFdChannelPtr;
   void                     handleRead();
#endif
   TimerPriorityQueue m_timers;
   bool               m_callingExpiredTimers;

private:
   // The timer ID is currently valid
   ActiveTimerSet m_timerIdSet;
};
}   // namespace netpoll
