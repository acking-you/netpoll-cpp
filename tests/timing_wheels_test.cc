#include <doctest/doctest.h>
#include <elog/logger.h>
#include <netpoll/net/inner/timing_wheel.h>

using namespace netpoll;
struct Call
{
   Call() { elog::Log::info("start call "); }
   ~Call() { elog::Log::info("end call "); }
};

TEST_CASE("test timing_wheels")
{
   EventLoop loop;
   elog::GlobalConfig::Get().setLevel(elog::kTrace);
   TimingWheel wheel(&loop, 1000, 1);
   elog::Log::info("everything run");
   loop.runAfter(
     23, [&](TimerId) { wheel.insertEntry(101, std::make_shared<Call>()); });
   loop.loop();
}