#include <elog/logger.h>
#include <netpoll/util/concurrent_task_queue.h>
using namespace elog;

netpoll::ConcurrentTaskQueue::ConcurrentTaskQueue(size_t            threadNum,
                                                  const StringView& name)
  : m_stop(false), m_name(name.data(), name.size())
{
   for (auto i = 0; i < threadNum; i++)
   {
      m_threads.emplace_back([this, i] { queueFunc(i); });
   }
}

void netpoll::ConcurrentTaskQueue::queueFunc(size_t num)
{
   char tmpName[32];
   snprintf(tmpName, sizeof(tmpName), "%s%zu", m_name.c_str(), num);

   while (!m_stop)
   {
      callback cb;
      {
         std::unique_lock<std::mutex> lock(m_mtx);
         while (!m_stop && m_que.empty()) { m_cv.wait(lock); }
         if (!m_que.empty())
         {
            Log::trace("thread:{} got a new task", tmpName);
            cb = std::move(m_que.front());
            m_que.pop();
         }
         else continue;
      }
      cb();
   }
}

void netpoll::ConcurrentTaskQueue::stop()
{
   if (!m_stop)
   {
      m_stop = true;
      m_cv.notify_all();
      for (auto& th : m_threads) { th.join(); }
   }
}

netpoll::ConcurrentTaskQueue::~ConcurrentTaskQueue() { stop(); }

void netpoll::ConcurrentTaskQueue::runTask(
  const netpoll::TaskQueue::callback& cb)
{
   std::lock_guard<std::mutex> lock(m_mtx);
   m_que.push(cb);
   m_cv.notify_one();
}

void netpoll::ConcurrentTaskQueue::runTask(netpoll::TaskQueue::callback&& cb)
{
   std::lock_guard<std::mutex> lock(m_mtx);
   m_que.push(std::move(cb));
   m_cv.notify_one();
}
