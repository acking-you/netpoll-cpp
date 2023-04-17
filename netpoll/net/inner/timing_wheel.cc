#include "timing_wheel.h"

#include <elog/logger.h>

using namespace netpoll;
using namespace elog;

TimingWheel::TimingWheel(EventLoop* loop, size_t maxTimeout,
                         float ticksInterval, size_t bucketsNumPerQueue)
  : m_loop(loop),
    m_ticksInterval(ticksInterval),
    m_bucketsNumPerQueue(bucketsNumPerQueue)
{
   assert(maxTimeout > 1);
   assert(ticksInterval > 0);
   assert(m_bucketsNumPerQueue > 1);

   auto maxTickNum = static_cast<size_t>(maxTimeout / ticksInterval);
   auto ticksNum   = bucketsNumPerQueue;
   m_QueueNum      = 1;
   // Find out how many task queue of different accuracy are needed.
   while (maxTickNum > ticksNum)
   {
      ++m_QueueNum;
      ticksNum *= m_bucketsNumPerQueue;
   }
   m_taskQueues.resize(m_QueueNum);
   for (size_t i = 0; i < m_QueueNum; ++i)
   {
      m_taskQueues[i].resize(m_bucketsNumPerQueue);
   }

   auto cb = [this](TimerId _) {
      ++m_ticksCounter;
      size_t t   = m_ticksCounter;
      size_t pow = 1;
      // bucketsNumPerQueue is used as a base for counting. For example, in
      // base 100, suppose there are 4 task queues: Q1, Q2, Q3, and Q4.
      // Q1 is advanced once every revolution.Q2 is advanced once
      // every 100 revolutions. Q3 is 100^2 and 100^4.
      for (size_t i = 0; i < m_QueueNum; ++i)
      {
         if ((t % pow) == 0)
         {
            EntryBucket tmp;
            {
               // use tmp val to make this critical area as short as
               // possible.
               m_taskQueues[i].front().swap(tmp);
               m_taskQueues[i].pop_front();
               m_taskQueues[i].emplace_back();
            }
         }
         pow = pow * m_bucketsNumPerQueue;
      }
   };
   // Mark the lowest priority and rotate every m_ticksInterval.
   m_timerId = m_loop->runEvery(m_ticksInterval, cb, false, true);
}

TimingWheel::~TimingWheel()
{
   m_loop->assertInLoopThread();
   m_loop->cancelTimer(m_timerId);
   for (auto iter = m_taskQueues.rbegin(); iter != m_taskQueues.rend(); ++iter)
   {
      iter->clear();
   }
   Log::trace("TimingWheel destruct!");
}

void TimingWheel::insertEntry(size_t delay, const EntryPtr& entryPtr)
{
   if (delay <= 0) return;
   if (!entryPtr) return;
   if (m_loop->isInLoopThread()) { insertEntryInLoop(delay, entryPtr); }
   else
   {
      m_loop->runInLoop(
        [this, delay, entryPtr]() { insertEntryInLoop(delay, entryPtr); });
   }
}

void TimingWheel::insertEntryInLoop(size_t delay, EntryPtr entryPtr)
{
   m_loop->assertInLoopThread();

   // If delay is not a multiple of the rotation interval, then the number of
   // rotations needs plus one.
   delay = static_cast<size_t>(delay / m_ticksInterval) +
           (delay % static_cast<size_t>(m_ticksInterval) ? 1 : 0);
   size_t t = m_ticksCounter;
   for (size_t i = 0; i < m_QueueNum; ++i)
   {
      // The number of rotations required is less than or equal to the maximum
      // number of rotations of the TimingWheel with the current accuracy.
      if (delay <= m_bucketsNumPerQueue)
      {
         m_taskQueues[i][delay - 1].insert(entryPtr);
         break;
      }
      if (i < (m_QueueNum - 1))
      {
         entryPtr =
           std::make_shared<CallbackEntry>([this, delay, i, t, entryPtr]() {
              if (delay > 0)
              {
                 m_taskQueues[i][(delay + (t % m_bucketsNumPerQueue) - 0) %
                                 m_bucketsNumPerQueue]
                   .insert(entryPtr);
              }
           });
      }
      else
      {
         // delay is too long to put entry at valid position in wheels.
         m_taskQueues[i][m_bucketsNumPerQueue - 1].insert(entryPtr);
      }
      delay = (delay + (t % m_bucketsNumPerQueue) - 1) / m_bucketsNumPerQueue;
      t     = t / m_bucketsNumPerQueue;
   }
}