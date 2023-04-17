#include <doctest/doctest.h>
#define ENABLE_ELG_LOG
#include <elog/logger.h>
#include <netpoll/net/eventloop_threadpool.h>
#include <netpoll/util/defer_call.h>
using namespace netpoll;
using namespace elog;

TEST_CASE("test EventLoopThreadPool")
{
   EventLoopThreadPool threadPool(8);
   GlobalConfig::Get().setFormatter(formatter::customFromString("%v"));
   threadPool.start();
   SUBCASE("getNextLoop()")
   {
      auto deferWait = makeDeferCall([&]() {
         threadPool.wait();
         ELG_INFO("--------------end of getNextLoop() test--------------");
      });
      for (int i = 0; i < 8; i++)
      {
         auto* loop = threadPool.getNextLoop();
         loop->setContext(0);
         loop->runEvery(1, [loop, i](auto timerId) {
            auto& ctx   = loop->getMutableContext();
            auto& count = any_cast<int&>(ctx);
            if (count == 2)
            {
               loop->cancelTimer(timerId);
               loop->quit();
            }
            ELG_INFO("getNextLoop() timerId:{},thread:{},count:{}", timerId, i,
                     count);
            ++count;
         });
      }
   }
   SUBCASE("getLoops()")
   {
      auto deferWait = makeDeferCall([&]() {
         threadPool.wait();
         ELG_INFO("--------------end of getLoops() test--------------");
      });
      auto loops     = threadPool.getLoops();
      for (auto& loop : loops)
      {
         loop->setContext(0);
         loop->runEvery(1, [loop](auto timerId) {
            auto& ctx   = loop->getMutableContext();
            auto& count = any_cast<int&>(ctx);
            if (count == 2)
            {
               loop->cancelTimer(timerId);
               loop->quit();
            }
            ELG_INFO("getLoops() timerId:{},count:{}", timerId, count);
            ++count;
         });
      }
   }
   SUBCASE("size()")
   {
      auto deferWait = makeDeferCall([&]() {
         threadPool.wait();
         ELG_INFO("--------------end of size() test--------------");
      });
      CHECK_EQ(threadPool.size(), 8);
      for (auto&& loop : threadPool.getLoops()) { loop->quit(); }
   }
   SUBCASE("getLoop()")
   {
      auto  deferWait = makeDeferCall([&]() {
         threadPool.wait();
         ELG_INFO("--------------end of getLoop() test--------------");
      });
      auto* loop      = threadPool.getLoop(0);
      loop->setContext(0);
      loop->runEvery(1, [&](auto timerId) {
         auto& ctx   = loop->getMutableContext();
         auto& count = any_cast<int&>(ctx);
         if (count == 2)
         {
            loop->quit();
            for (auto&& item : threadPool.getLoops())
            {
               if (item) item->quit();
            }
            loop->cancelTimer(timerId);
         }
         ELG_INFO("getLoop() timerId:{},count:{}", timerId, count);
         ++count;
      });
   }
}
