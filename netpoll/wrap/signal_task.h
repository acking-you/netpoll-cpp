#pragma once
#include <functional>
#include <vector>

namespace netpoll {
#ifndef _WIN32
class IgnoreSigPipe
{
public:
   IgnoreSigPipe();
   static IgnoreSigPipe& Register();
};
#endif

class SignalTask
{
public:
   // not thread safe
   static void Add(const std::function<void()>& task)
   {
      instance().push_back(task);
   }

   static void Register(std::vector<int> const& signals);

   static std::vector<std::function<void()>>& instance();

private:
   static void HandleTask(int x);
};
}   // namespace netpoll