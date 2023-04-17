#pragma once
#include <functional>
#include <future>
#include <thread>
#include <vector>

#include "make_copy_mv.h"

namespace netpoll {
namespace util {
class TaskPerThreadQueue
{
public:
   static TaskPerThreadQueue &Get()
   {
      static TaskPerThreadQueue instance;
      return instance;
   }
   ~TaskPerThreadQueue()
   {
      for (auto &&thread : m_threads)
      {
         if (thread.joinable()) thread.join();
      }
   }

   std::future<void> runTask(std::function<void()> &&task)
   {
      std::packaged_task<void()> packagedTask{std::move(task)};

      auto fu   = packagedTask.get_future();
      auto copy = makeCopyMove(std::move(packagedTask));
      m_threads.emplace_back([copy]() mutable {
         if (copy.value.valid()) { copy.value(); }
      });
      return fu;
   }

   std::future<void> runTask(std::function<void()> const &task)
   {
      std::packaged_task<void()> packagedTask{task};

      auto fu   = packagedTask.get_future();
      auto copy = makeCopyMove(std::move(packagedTask));
      m_threads.emplace_back([copy]() mutable {
         if (copy.value.valid()) { copy.value(); }
      });
      return fu;
   }

private:
   std::vector<std::thread> m_threads;
};
}   // namespace util
}   // namespace netpoll