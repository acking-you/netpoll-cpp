#pragma once

#include <netpoll/util/noncopyable.h>
#include <netpoll/util/string_view.h>

#include <functional>
#include <memory>

#include "inet_address.h"

namespace netpoll {
class Resolver
{
public:
   using callback = std::function<void(InetAddress const&)>;

   static auto New(size_t timeout) -> std::shared_ptr<Resolver>;

   virtual void resolve(StringView const& hostname, callback const& cb)    = 0;
   virtual void syncResolve(StringView const& hostname, InetAddress& addr) = 0;
   virtual ~Resolver() = default;
};
}   // namespace netpoll
