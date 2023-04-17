#pragma once

#include <atomic>
#include <cassert>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../eventloop.h"

#define TIMING_BUCKET_NUM_PER_WHEEL 100
#define TIMING_TICK_INTERVAL        1.0

namespace netpoll {
using EntryPtr = std::shared_ptr<void>;

using EntryBucket = std::unordered_set<EntryPtr>;
using BucketQueue = std::deque<EntryBucket>;

/**
 * @brief This class implements a timer strategy with high performance and low
 * accuracy. This is usually used internally.
 *
 */
class TimingWheel
{
public:
   using Ptr = std::shared_ptr<TimingWheel>;
   class CallbackEntry
   {
   public:
      explicit CallbackEntry(std::function<void()> cb) : m_cb(std::move(cb)) {}
      ~CallbackEntry() { m_cb(); }

   private:
      std::function<void()> m_cb;
   };

   /**
    * @brief Construct a new timing wheel instance.
    *
    * @param loop The event loop in which the timing wheel runs.
    * @param maxTimeout The maximum timeout of the task queue.
    * @param ticksInterval The internal timer tick interval.  It affects the
    * accuracy of the timing wheel.
    * @param bucketsNumPerQueue The number of buckets per queue.
    * @note The max delay of the timing wheel is about
    * ticksInterval*(bucketsNumPerWheel^wheelsNum) seconds.
    * @example Four task queue with 200 buckets per queue means the timing wheel
    * can work with a timeout up to 200^4 seconds, about 50 years;
    */
   TimingWheel(EventLoop *loop, size_t maxTimeout,
               float  ticksInterval      = TIMING_TICK_INTERVAL,
               size_t bucketsNumPerQueue = TIMING_BUCKET_NUM_PER_WHEEL);

   // If this method is called in a timer task, the precision of one rotation
   // may be lost.
   void insertEntry(size_t delay, const EntryPtr &entryPtr);

   void insertEntryInLoop(size_t delay, EntryPtr entryPtr);

   EventLoop *getLoop() { return m_loop; }

   ~TimingWheel();

private:
   // TaskQueue of varying precision
   std::vector<BucketQueue> m_taskQueues;

   // The number of times the TimingWheel turns
   size_t m_ticksCounter{0};

   // The ID of the task and the loop where the task is located
   TimerId    m_timerId;
   EventLoop *m_loop;

   float  m_ticksInterval;
   size_t m_QueueNum;
   size_t m_bucketsNumPerQueue;
};
}   // namespace netpoll
