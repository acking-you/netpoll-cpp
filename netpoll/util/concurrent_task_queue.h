#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "task_queue.h"

namespace netpoll {
class ConcurrentTaskQueue : public TaskQueue
{
public:
   ConcurrentTaskQueue(size_t threadNum, const StringView &name);

   ~ConcurrentTaskQueue() override;

   void runTask(const callback &) override;

   void runTask(callback &&) override;

   StringView getName() const override { return m_name; }
   size_t     getTaskCount()
   {
      std::lock_guard<std::mutex> lock(m_mtx);
      return m_que.size();
   }

   void stop();

private:
   void queueFunc(size_t num);

   std::atomic_bool         m_stop;
   std::mutex               m_mtx;
   std::condition_variable  m_cv;
   std::queue<callback>     m_que;
   std::vector<std::thread> m_threads;
   std::string              m_name;
};
}   // namespace netpoll
