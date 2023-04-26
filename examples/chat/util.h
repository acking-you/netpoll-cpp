#pragma once
#include <mutex>

namespace chat {

template <typename T>
struct MutexGuard
{
   T          value;
   std::mutex mutex;
};

}   // namespace chat