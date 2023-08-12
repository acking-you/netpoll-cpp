#pragma once

namespace netpoll {
class noncopyable
{
public:
   noncopyable(const noncopyable&)            = delete;
   noncopyable& operator=(const noncopyable&) = delete;
   noncopyable(noncopyable&&)                 = default;
   noncopyable& operator=(noncopyable&&)      = default;

protected:
   noncopyable()  = default;
   ~noncopyable() = default;
};
#define NETPOLL_MAKE_MOVEABLE(classname)                                       \
   classname(classname&&)            = default;                                \
   classname& operator=(classname&&) = default;
}   // namespace netpoll