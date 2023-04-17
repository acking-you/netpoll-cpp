#include "resolver_impl.h"

#include <elog/logger.h>
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <strings.h>   // memset
#include <sys/socket.h>
#endif
using namespace netpoll;
using namespace elog;

auto Resolver::New(size_t timeout) -> std::shared_ptr<Resolver>
{
   return std::make_shared<ResolverImpl>(timeout);
}

void netpoll::ResolverImpl::resolve(const netpoll::StringView         &hostname,
                                    const netpoll::Resolver::callback &cb)
{
   {
      std::lock_guard<std::mutex> guard(globalMutex());

      auto iter = globalCache().find(hostname.data());
      if (iter != globalCache().end())
      {
         auto &cachedAddr = iter->second;
         if (m_timeout == 0 ||
             (cachedAddr.second + static_cast<double>(m_timeout)) >
               Timestamp::now())
         {
            cb(cachedAddr.first);
            return;
         }
      }
   }

   std::string name{hostname.data(), hostname.size()};
   concurrentTaskQueue().runTask([self = shared_from_this(), cb, name]() {
      {
         std::lock_guard<std::mutex> guard(self->globalMutex());
         auto                        iter = self->globalCache().find(name);
         if (iter != self->globalCache().end())
         {
            auto &cachedAddr = iter->second;
            if (self->m_timeout == 0 ||
                cachedAddr.second + static_cast<double>(self->m_timeout) >
                  Timestamp::now())
            {
               cb(cachedAddr.first);
               return;
            }
         }
      }
      struct addrinfo hints, *res = nullptr;
      memset(&hints, 0, sizeof(hints));
      hints.ai_family   = PF_UNSPEC;
      hints.ai_socktype = SOCK_STREAM;
      hints.ai_flags    = AI_PASSIVE;
      auto error        = getaddrinfo(name.c_str(), nullptr, &hints, &res);
      if (error != 0 || res == nullptr)
      {
         Log::error("InetAddress::resolve");
         if (res != nullptr) { freeaddrinfo(res); }
         cb(InetAddress{});
         return;
      }
      InetAddress inet;
      if (res->ai_family == AF_INET)
      {
         struct sockaddr_in addr;
         memset(&addr, 0, sizeof addr);
         addr = *reinterpret_cast<struct sockaddr_in *>(res->ai_addr);
         inet = InetAddress(addr);
      }
      else if (res->ai_family == AF_INET6)
      {
         struct sockaddr_in6 addr;
         memset(&addr, 0, sizeof addr);
         addr = *reinterpret_cast<struct sockaddr_in6 *>(res->ai_addr);
         inet = InetAddress(addr);
      }
      freeaddrinfo(res);
      cb(inet);
      {
         std::lock_guard<std::mutex> guard(self->globalMutex());

         auto &addrItem  = self->globalCache()[name];
         addrItem.first  = inet;
         addrItem.second = Timestamp::now();
      }
      return;
   });
}

void ResolverImpl::syncResolve(const StringView &hostname, InetAddress &addr)
{
   std::promise<void> promise;

   auto fu = promise.get_future();
   resolve(hostname, [&](InetAddress const &v) {
      addr = v;
      promise.set_value();
   });
   fu.get();
}
