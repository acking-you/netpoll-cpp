#pragma once

#include <netpoll/util/noncopyable.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>

namespace netpoll {
using TimerId       = uint64_t;
using TimerCallback = std::function<void(TimerId)>;
using TimePoint     = std::chrono::steady_clock::time_point;
using TimeInterval  = std::chrono::microseconds;

class Timer : public noncopyable
{
public:
   Timer(const TimerCallback &cb, const TimePoint &when,
         const TimeInterval &interval, bool highest = false,
         bool lowest = false);
   Timer(TimerCallback &&cb, const TimePoint &when,
         const TimeInterval &interval, bool highest = false,
         bool lowest = false);
   void             run() const;
   void             restart(const TimePoint &now);
   bool             operator<(const Timer &t) const;
   bool             operator>(const Timer &t) const;
   const TimePoint &when() const { return m_when; }
   bool             isRepeat() const { return m_repeat; }
   bool             isHighest() const { return m_highest; }
   bool             isLowest() const { return m_lowest; }
   TimerId          id() const { return m_id; }

private:
   TimerCallback      m_callback;
   TimePoint          m_when;
   const TimeInterval m_interval;
   const bool         m_repeat;
   const bool         m_highest;
   const bool         m_lowest;
   const TimerId      m_id;

   // Used to create a globally unique ID
   static std::atomic<TimerId> timersCreated_;
};

}   // namespace netpoll
