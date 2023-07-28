#include <doctest/doctest.h>
#include <netpoll/net/eventloop_thread.h>

#include <iostream>

using namespace netpoll;

TEST_SUITE_BEGIN("test EventLoopThread");

TEST_CASE("get mut context and plus one by every 1s")
{
   netpoll::EventLoopThread thread;
   thread.run();
   //   Must be notice that the pointer is not thread-safe,and when quit() is
   //   called,it will become a dangling pointer!!!!
   auto* loop = thread.getLoop();
   loop->setContext(0);
   loop->runEvery(1, [loop](TimerId id) {
      auto&& count    = loop->getContextRefMut();
      auto&  countRef = any_cast<int&>(count);
      if (countRef == 5)
      {
         loop->cancelTimer(id);
         REQUIRE_EQ(any_cast<int&>(loop->getContextRefMut()), 5);
         loop->quit();
      }
      std::cout << "count:" << countRef << std::endl;
      ++countRef;
   });
   thread.wait();
}

TEST_SUITE_END;