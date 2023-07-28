#include <doctest/doctest.h>
#include <elog/logger.h>
#include <netpoll/util/time_stamp.h>

using namespace netpoll;
using namespace elog;

auto test_toString()
{
   auto us  = Timestamp::now().toFormatString(Time::Microseconds);
   auto ms  = Timestamp::now().toFormatString(Time::Milliseconds);
   auto s   = Timestamp::now().toFormatString();
   auto str = Timestamp::now().toString(Time::Seconds);
   Log::info(R"(Time=> s:"{}",ms:"{}",us:"{}")", s, ms, us);
   Log::info("Timestamp=> {}", str);
}

auto test_operator()
{
   auto tm1 = Timestamp::fromSinceEpoch<Time::Seconds>(time(NULL));
   auto tm2 = Timestamp::now();
   auto t   = tm2 - tm1;
   auto tt  = tm1 + t;
   Log::info("Duration=> {}", t);
   Log::info("Timestamp=> after operator plus:{},",
             tt.sinceEpoch<Time::Seconds>());
}

TEST_CASE("test Timestamp")
{
   GlobalConfig::Get().setFormatter(formatter::customFromString("%v"));
   test_toString();
   test_operator();
}
