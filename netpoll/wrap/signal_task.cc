#include "signal_task.h"
#define ENABLE_ELG_LOG
#include <elog/logger.h>

#include <csignal>
#include <cstdlib>
#include <mutex>

#ifndef _WIN32
netpoll::IgnoreSigPipe::IgnoreSigPipe()
{
   ::signal(SIGPIPE, SIG_IGN);
   ELG_INFO("Ignore SIGPIPE");
}

netpoll::IgnoreSigPipe &netpoll::IgnoreSigPipe::Register()
{
   static IgnoreSigPipe instance;
   return instance;
}
#endif

void netpoll::SignalTask::Register(std::vector<int> const &signals)
{
   static std::once_flag flag;
   std::call_once(flag, [&]() {
      for (auto &&sig : signals) { signal(sig, &SignalTask::HandleTask); }
   });
}

std::vector<std::function<void()>> &netpoll::SignalTask::instance()
{
   static std::vector<std::function<void()>> tasks;
   return tasks;
}

void netpoll::SignalTask::HandleTask(int x)
{
   ELG_ERROR("signal received:{}", x);
   for (auto &&t : instance()) { t(); }
   // Ensure that the log refresh is complete
   elog::WaitForDone();
   abort();
}
