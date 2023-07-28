#pragma once

namespace netpoll {
class noncopyable
{
public:
   noncopyable(const noncopyable&)    = delete;
   void operator=(const noncopyable&) = delete;

protected:
   noncopyable()  = default;
   ~noncopyable() = default;
};
#define NETPOLL_MAKE_MOVEABLE(classname) \
   classname(classname&&) = default; \
   classname& operator=(classname&&) = default;
}   // namespace netpoll