#pragma once
#include <mutex>

#include "noncopyable.h"

namespace netpoll {

template <typename T>
class Mutex : private noncopyable
{
public:
   NETPOLL_MAKE_MOVEABLE(Mutex)
   explicit Mutex(T&& data) : m_data(std::move(data)) {}
   Mutex() = default;
   class Guard : private noncopyable
   {
   public:
      NETPOLL_MAKE_MOVEABLE(Guard)
      explicit Guard(std::mutex& mtx, T& data)
        : m_mtx_ref(mtx), m_data_ref(data)
      {
         m_mtx_ref.lock();
      }
      ~Guard()
      {
         if (!m_mtx_ref.try_lock()) { m_mtx_ref.unlock(); }
      }

      T& ref_data_mut() { return m_data_ref; }

      const T& ref_data() { return m_data_ref; }

   private:
      std::mutex& m_mtx_ref;
      T&          m_data_ref;
   };

   Guard into_guard() { return Guard{m_mtx, m_data}; }

private:
   mutable std::mutex m_mtx;
   T                  m_data;
};
}   // namespace netpoll