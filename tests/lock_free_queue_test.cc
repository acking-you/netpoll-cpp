/**
 * it is proved that the lockfree queue performance under the MPSC model will be
 * better
 */

#include <doctest/doctest.h>
#include <fmt/format.h>
#include <netpoll/util/lockfree_queue.h>

#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "inner/timer.h"

const int kProducerNum = 10;
const int n            = 100;

std::mutex                      mtx;
std::vector<std::string>        mtx_que;
netpoll::MpscQueue<std::string> lock_freeQue;

void run_mutex_producer()
{
   for (int i = 0; i < n; ++i)
   {
      std::lock_guard<std::mutex> lock(mtx);
      mtx_que.push_back(fmt::format("mutex:{}", i));
   }
}

void run_lockfree_producer()
{
   for (int i = 0; i < n; ++i)
   {
      lock_freeQue.enqueue(fmt::format("free:{}", i));
   }
}

struct thread_helper
{
   thread_helper(int threadNum, const std::function<void()>& task)
   {
      for (int i = 0; i < threadNum; i++) { m_threads.emplace_back(task); }
   }
   ~thread_helper()
   {
      for (auto&& t : m_threads)
      {
         if (t.joinable()) { t.join(); }
      }
   }

private:
   std::vector<std::thread> m_threads;
};

void push_mutex()
{
   thread_helper producers(kProducerNum, &run_mutex_producer);
}

void push_lockfree()
{
   thread_helper producers(kProducerNum, &run_lockfree_producer);
}

TEST_CASE("bench thread-safe queue push")
{
   {
      Timer tm;
      push_mutex();
   }
   {
      Timer tm;
      push_lockfree();
   }
}

TEST_CASE("bench thread-safe queue push&get")
{
   const int sum   = kProducerNum * n;
   int       count = 0;

   std::string p;
   {
      Timer tm;
      push_mutex();
      for (int i = 0; i < sum; i++)
      {
         if (mtx_que.empty()) break;
         ++count;
         p = mtx_que.back();
         mtx_que.pop_back();
      }
   }
   CHECK_EQ(count, sum);
   count = 0;
   {
      Timer tm;
      push_lockfree();
      for (int i = 0; i < sum; i++)
      {
         if (lock_freeQue.empty()) break;
         ++count;
         lock_freeQue.dequeue(p);
      }
   }
   CHECK_EQ(count, sum);
   std::cout << p;
}