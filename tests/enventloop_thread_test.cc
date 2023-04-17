#include <doctest/doctest.h>
#include <netpoll/net/eventloop_thread.h>

#include <iostream>

using namespace netpoll;

TEST_CASE("test EventLoopThread")
{
   netpoll::EventLoopThread thread;
   thread.run();
   auto* loop = thread.getLoop();
   loop->setContext(0);
   loop->runEvery(1, [loop](TimerId id) {
      auto& count    = loop->getMutableContext();
      auto& countRef = any_cast<int&>(count);
      if (countRef == 10)
      {
         loop->cancelTimer(id);
         loop->quit();
      }
      std::cout << "hello EventLoopThread "
                << "count:" << countRef << std::endl;
      ++countRef;
   });
   thread.wait();
}
