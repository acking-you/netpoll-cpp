#pragma once
#include <utility>

namespace netpoll {
template <typename F>
struct DefferCall
{
   explicit DefferCall(F &&f) : f_(std::forward<F>(f)) {}
   ~DefferCall() { f_(); }
   F f_;
};

template <typename F>
DefferCall<F> makeDeferCall(F &&f)
{
   return DefferCall<F>(std::forward<F>(f));
};
}   // namespace netpoll
