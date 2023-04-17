#pragma once

#include <utility>

#include "move_wrapper.h"

namespace netpoll {
namespace detail {
template <typename T>
struct CopyMoveImpl
{
   explicit CopyMoveImpl(T v) : value(std::move(v)) {}
   CopyMoveImpl(CopyMoveImpl const& src) : value(moveConstRef(src.value)) {}
   CopyMoveImpl& operator=(CopyMoveImpl const& other)
   {
      value = moveConstRef(other.value);
      return *this;
   }
   T value;
};
}   // namespace detail

template <typename T>
auto makeCopyMove(T value) -> detail::CopyMoveImpl<T>
{
   return detail::CopyMoveImpl<T>{std::move(value)};
}

}   // namespace netpoll