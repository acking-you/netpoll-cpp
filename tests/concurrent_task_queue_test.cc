#include <doctest/doctest.h>
#include <netpoll/util/concurrent_task_queue.h>

std::mutex                   s_mtx;
std::string                  s_text;
netpoll::ConcurrentTaskQueue s_queue(2, "test append");

void append(const char* text)
{
   std::lock_guard<std::mutex> lock(s_mtx);
   s_text.append(text);
}

bool check_value()
{
   return s_text == "abc" || s_text == "acb" || s_text == "bac" ||
          s_text == "bca" || s_text == "cab" || s_text == "cba";
}

TEST_CASE("test ConcurrentTaskQueue")
{
   std::thread th1([&]() { s_queue.syncTask([]() { append("a"); }); });
   std::thread th2([&]() { s_queue.syncTask([]() { append("b"); }); });
   std::thread th3([&]() { s_queue.syncTask([]() { append("c"); }); });
   th1.join();
   th2.join();
   th3.join();

   CHECK(check_value());
}
