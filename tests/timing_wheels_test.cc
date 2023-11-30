#include <doctest/doctest.h>
#include <elog/logger.h>
#include <netpoll/net/inner/timing_wheel.h>

using namespace netpoll;
struct Call
{
   Call(EventLoop& l, int* c) : loop(l), count(c)
   {
      elog::Log::info("start call should be get \"end call\" after 5 seconds");
      *c = 1;
   }
   ~Call()
   {
      elog::Log::info("end call ,quit loop");
      loop.quit();
      *count = 2;
   }
   int*       count{};
   EventLoop& loop;
};

TEST_CASE("test timing_wheels")
{
   elog::GlobalConfig::Get().setFormatter(elog::formatter::customFromString("%v"));
   EventLoop loop;
   elog::GlobalConfig::Get().setLevel(elog::kTrace);
   TimingWheel wheel(&loop, 10, 1);
   int         v    = 0;
   auto        call = std::make_shared<Call>(loop, &v);
   wheel.insertEntry(5, std::move(call));
   loop.loop();
   REQUIRE_EQ(v, 2);
}