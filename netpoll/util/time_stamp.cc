//
// Created by Alone on 2022-10-11.
//
#include "time_stamp.h"

#include "funcs.h"
#ifndef _WIN32
#include <sys/time.h>
#else
#include <time.h>
#include <WinSock2.h>
#endif

using namespace netpoll;

#ifdef _WIN32
int gettimeofday(timeval *tp, void *tzp)
{
   time_t     clock;
   struct tm  tm;
   SYSTEMTIME wtm;

   GetLocalTime(&wtm);
   tm.tm_year  = wtm.wYear - 1900;
   tm.tm_mon   = wtm.wMonth - 1;
   tm.tm_mday  = wtm.wDay;
   tm.tm_hour  = wtm.wHour;
   tm.tm_min   = wtm.wMinute;
   tm.tm_sec   = wtm.wSecond;
   tm.tm_isdst = -1;
   clock       = mktime(&tm);
   tp->tv_sec  = static_cast<long>(clock);
   tp->tv_usec = wtm.wMilliseconds * 1000;

   return (0);
}
#endif

auto Timestamp::toString(Time timePrecision) const -> std::string
{
   int     k            = kMicroSecondsPerSecond / kMillisecondsPerSecond;
   int64_t milliSeconds = m_microSecondsSinceEpoch / k;
   int64_t seconds      = milliSeconds / k;
   if (timePrecision == Time::Microseconds)
      return std::to_string(m_microSecondsSinceEpoch);
   if (timePrecision == Time::Milliseconds) return std::to_string(milliSeconds);
   return std::to_string(seconds);
}
auto Timestamp::toFormatString(Time timePrecision) const -> std::string
{
   char buf[64] = {0};
   auto seconds =
     static_cast<time_t>(m_microSecondsSinceEpoch / kMicroSecondsPerSecond);
   struct tm tm_time
   {
   };

#ifndef _WIN32
   localtime_r(&seconds, &tm_time);
#else
   localtime_s(&tm_time, &seconds);
#endif

   if (timePrecision == Time::Microseconds)
   {
      int microseconds =
        static_cast<int>(m_microSecondsSinceEpoch % kMicroSecondsPerSecond);
      snprintf(buf, sizeof(buf), "%4d-%02d-%02d %02d:%02d:%02d.%06d",
               tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
               tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec, microseconds);
   }
   else if (timePrecision == Time::Milliseconds)
   {
      int k            = kMicroSecondsPerSecond / kMillisecondsPerSecond;
      int milliseconds = static_cast<int>((m_microSecondsSinceEpoch / k) %
                                          kMillisecondsPerSecond);
      snprintf(buf, sizeof(buf), "%4d-%02d-%02d %02d:%02d:%02d.%03d",
               tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
               tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec, milliseconds);
   }
   else
   {
      snprintf(buf, sizeof(buf), "%4d-%02d-%02d %02d:%02d:%02d",
               tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
               tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
   }
   return buf;
}

auto Timestamp::now() -> Timestamp
{
   struct timeval tv;
   gettimeofday(&tv, nullptr);
   int64_t seconds = tv.tv_sec;
   return {seconds * kMicroSecondsPerSecond + tv.tv_usec};
}
