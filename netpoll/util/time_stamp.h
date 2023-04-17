//
// Created by Alone on 2022-10-11.
//

#pragma once
#include <cstdint>
#include <string>

namespace netpoll {
enum class Time { Seconds, Milliseconds, Microseconds };

class Timestamp
{
public:
   friend auto operator<(Timestamp lhs, Timestamp rhs) -> bool;

   friend auto operator>(Timestamp lhs, Timestamp rhs) -> bool;

   friend auto operator==(Timestamp lhs, Timestamp rhs) -> bool;

   friend auto operator-(Timestamp high, Timestamp low) -> double;

   friend auto operator+(Timestamp timestamp, double seconds) -> Timestamp;

   enum TimePerSecond {
      kMillisecondsPerSecond = 1000,
      kMicroSecondsPerSecond = kMillisecondsPerSecond * 1000,
   };

   Timestamp() : m_microSecondsSinceEpoch(0) {}

   Timestamp(int64_t microSecondsSinceEpoch)
     : m_microSecondsSinceEpoch(microSecondsSinceEpoch)
   {
   }

   void swap(Timestamp& other)
   {
      std::swap(m_microSecondsSinceEpoch, other.m_microSecondsSinceEpoch);
   }

   auto toString(Time timePrecision = Time::Seconds) const -> std::string;

   auto toFormatString(Time timePrecision = Time::Seconds) const -> std::string;

   auto valid() const -> bool { return m_microSecondsSinceEpoch > 0; }

   template <Time precision>
   auto sinceEpoch() const -> int64_t
   {
      if (precision == Time::Milliseconds)
         return m_microSecondsSinceEpoch / kMillisecondsPerSecond;
      if (precision == Time::Seconds)
         return m_microSecondsSinceEpoch / kMicroSecondsPerSecond;
      return m_microSecondsSinceEpoch;
   }

   template <Time precision>
   static auto fromSinceEpoch(int64_t epoch) -> Timestamp
   {
      if (precision == Time::Milliseconds)
         return epoch * kMillisecondsPerSecond;
      if (precision == Time::Seconds) return epoch * kMicroSecondsPerSecond;
      return epoch;
   }

   static auto now() -> Timestamp;

private:
   int64_t m_microSecondsSinceEpoch;
};

inline auto operator<(Timestamp lhs, Timestamp rhs) -> bool
{
   return lhs.m_microSecondsSinceEpoch < rhs.m_microSecondsSinceEpoch;
}

inline auto operator>(Timestamp lhs, Timestamp rhs) -> bool
{
   return lhs.m_microSecondsSinceEpoch > rhs.m_microSecondsSinceEpoch;
}

inline auto operator==(Timestamp lhs, Timestamp rhs) -> bool
{
   return lhs.m_microSecondsSinceEpoch == rhs.m_microSecondsSinceEpoch;
}

inline auto operator-(Timestamp high, Timestamp low) -> double
{
   int64_t diff = high.m_microSecondsSinceEpoch - low.m_microSecondsSinceEpoch;
   return static_cast<double>(diff) / Timestamp::kMicroSecondsPerSecond;
}

inline auto operator+(Timestamp timestamp, double seconds) -> Timestamp
{
   auto delta =
     static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
   return timestamp.m_microSecondsSinceEpoch + delta;
}

}   // namespace netpoll