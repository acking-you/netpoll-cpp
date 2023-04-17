#include "timer.h"

#include <netpoll/net/eventloop.h>

namespace netpoll {
std::atomic<TimerId> Timer::timersCreated_ = ATOMIC_VAR_INIT(InvalidTimerId);
Timer::Timer(const TimerCallback &cb, const TimePoint &when,
             const TimeInterval &interval, bool highest, bool lowest)
  : m_callback(cb),
    m_when(when),
    m_interval(interval),
    m_repeat(interval.count() > 0),
    m_highest(highest),
    m_lowest(lowest),
    m_id(++timersCreated_)
{
}
Timer::Timer(TimerCallback &&cb, const TimePoint &when,
             const TimeInterval &interval, bool highest, bool lowest)
  : m_callback(std::move(cb)),
    m_when(when),
    m_interval(interval),
    m_repeat(interval.count() > 0),
    m_highest(highest),
    m_lowest(lowest),
    m_id(++timersCreated_)
{
}
void Timer::run() const { m_callback(m_id); }
void Timer::restart(const TimePoint &now)
{
   if (m_repeat) { m_when = now + m_interval; }
   else m_when = std::chrono::steady_clock::now();
}
bool Timer::operator<(const Timer &t) const { return m_when < t.m_when; }
bool Timer::operator>(const Timer &t) const { return m_when > t.m_when; }
}   // namespace netpoll
