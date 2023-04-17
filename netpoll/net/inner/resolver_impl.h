#pragma once
#include <netpoll/util/concurrent_task_queue.h>
#include <netpoll/util/noncopyable.h>
#include <netpoll/util/time_stamp.h>

#include <unordered_map>
#include <vector>

#include "../resolver.h"

namespace netpoll {
class ResolverImpl : public Resolver,
                     public noncopyable,
                     public std::enable_shared_from_this<ResolverImpl>
{
public:
   explicit ResolverImpl(size_t timeout) : m_timeout(timeout) {}

   ~ResolverImpl() override = default;

   void resolve(const StringView& hostname, const callback& cb) override;

   void syncResolve(const StringView& hostname, InetAddress& addr) override;

private:
   static auto globalCache()
     -> std::unordered_map<std::string, std::pair<InetAddress, Timestamp>>&
   {
      static std::unordered_map<std::string, std::pair<InetAddress, Timestamp>>
        dnsCache_;
      return dnsCache_;
   }

   static auto globalMutex() -> std::mutex&
   {
      static std::mutex mutex_;
      return mutex_;
   }

   static auto concurrentTaskQueue() -> ConcurrentTaskQueue&
   {
      static ConcurrentTaskQueue queue(1, "Dns Queue");
      return queue;
   }

   size_t m_timeout;
};
}   // namespace netpoll
