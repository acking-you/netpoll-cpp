#include <doctest/doctest.h>
#define ENABLE_ELG_LOG
#include <elog/logger.h>

#include "netpoll/net/eventloop.h"

using namespace netpoll;

int s_num = 0;

void testRunEveryAndCancel()
{
   elog::GlobalConfig::Get().setFormatter(elog::formatter::colorfulFormatter);
   EventLoop loop;
   loop.setContext(0);
   loop.runEvery(1, [l = &loop](TimerId id1) mutable {
      auto& p   = l->getMutableContext();
      auto& num = netpoll::any_cast<int&>(p);
      if (num == s_num) { ELG_INFO("timerId:{},num == s_num", id1); }
      ELG_INFO("timerId:{} ,num = {},s_num = {}", id1, num, s_num);
      num++;
      s_num++;

      if (num == 10)
      {
         l->cancelTimer(id1);
         l->runEvery(1, [l](TimerId id2) {
            auto& p   = l->getMutableContext();
            auto& num = netpoll::any_cast<int&>(p);
            if (num == s_num) { ELG_INFO("timerId:{},num == s_num", id2); }
            ELG_INFO("timerId:{} ,num = {},s_num = {}", id2, num, s_num);
            s_num++;
            num++;
            if (num == 20) { l->quit(); }
         });
      }
   });
   loop.loop();
}

TEST_CASE("test timer_queue") { testRunEveryAndCancel(); }
