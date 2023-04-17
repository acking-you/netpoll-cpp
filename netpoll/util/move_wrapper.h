#pragma once
#include <utility>

namespace netpoll {
// adapted from folly
template <class T>
class MoveWrapper
{
public:
   /** If value can be default-constructed, why not?
     Then we don't have to move it in */
   MoveWrapper() = default;
   explicit MoveWrapper(T&& value) : value_(std::move(value)) {}
   MoveWrapper& operator=(MoveWrapper const&) = delete;
   MoveWrapper& operator=(MoveWrapper&&)      = delete;
   // copy is move
   MoveWrapper(const MoveWrapper& other) : value_(std::move(other.value_)) {}
   // move is also move
   MoveWrapper(MoveWrapper&& other) noexcept : value_(std::move(other.value_))
   {
   }

   // move the value out (sugar for std::move(*moveWrapper))
   T&& move() { return std::move(value_); }

   const T& operator*() const { return value_; }
   T&       operator*() { return value_; }
   const T* operator->() const { return &value_; }
   T*       operator->() { return &value_; }

private:
   mutable T value_;
};

template <class T, class T0 = std::remove_reference_t<T>>
MoveWrapper<T0> makeMoveWrapper(T&& t)
{
   return MoveWrapper<T0>(std::forward<T0>(t));
}

template <class T>
T&& moveConstRef(T const& value)
{
   return std::move(const_cast<T&>(value));
}

}   // namespace netpoll