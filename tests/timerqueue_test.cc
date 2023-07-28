#include <doctest/doctest.h>
#define ENABLE_ELG_LOG
#include <elog/logger.h>

#include "netpoll/net/eventloop.h"

using namespace netpoll;

void testRunEveryAndCancel()
{
   elog::GlobalConfig::Get().setFormatter(
     elog::formatter::customFromString("%c%l%C:%v"));
   EventLoop loop;
   loop.setContext(0);
   loop.runEvery(1, [&loop](TimerId id) {
      auto&& count    = loop.getContextRefMut();
      auto&  countRef = any_cast<int&>(count);
      if (countRef == 5)
      {
         loop.cancelTimer(id);
         REQUIRE_EQ(any_cast<int&>(loop.getContextRefMut()), 5);
         loop.quit();
      }
      ELG_INFO("count{}", countRef);
      ++countRef;
   });
   loop.loop();
}

TEST_SUITE("test TimerQueue"){
    TEST_CASE("runEvery&cancel") { testRunEveryAndCancel(); }
}
