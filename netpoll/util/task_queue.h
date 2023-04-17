#pragma once
#include <functional>
#include <future>

#include "noncopyable.h"
#include "string_view.h"

namespace netpoll {
class TaskQueue : noncopyable
{
public:
   using callback                        = std::function<void()>;
   // copy a task to run
   virtual void runTask(callback const&) = 0;
   // move a task to run
   virtual void runTask(callback&&)      = 0;

   [[nodiscard]] virtual StringView getName() const { return ""; }

   void syncTask(callback const& task)
   {
      std::promise<void> promise;

      auto fu = promise.get_future();
      runTask([&]() {
         task();
         promise.set_value();
      });
      fu.get();
   }

   virtual ~TaskQueue() = default;
};
}   // namespace netpoll