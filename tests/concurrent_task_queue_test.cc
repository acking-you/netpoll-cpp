#include <doctest/doctest.h>
#include <netpoll/util/concurrent_task_queue.h>
#include <netpoll/util/mutex_guard.h>

netpoll::ConcurrentTaskQueue s_queue(2, "test append");

struct appender
{
   void append(const char* text) { text_.append(text); }
   bool check_value() const
   {
      return text_ == "abc" || text_ == "acb" || text_ == "bac" ||
             text_ == "bca" || text_ == "cab" || text_ == "cba";
   }
   std::string text_;
};

TEST_SUITE_BEGIN("test ConcurrentTaskQueue");

TEST_CASE("test ConcurrentTaskQueue")
{
   netpoll::Mutex<appender> app;
   std::thread              th1([&]() {
      s_queue.syncTask([&]() { app.into_guard().ref_data_mut().append("a"); });
   });
   std::thread              th2([&]() {
      s_queue.syncTask([&]() { app.into_guard().ref_data_mut().append("b"); });
   });
   std::thread              th3([&]() {
      s_queue.syncTask([&]() { app.into_guard().ref_data_mut().append("c"); });
   });
   th1.join();
   th2.join();
   th3.join();

   REQUIRE(app.into_guard().ref_data().check_value());
}

struct Adder
{
   void add()
   {
      for (int i = 0; i < 10; i++) count += 10;
   }
   int count = 0;
};

TEST_CASE("test Mutex=>Guard is thread safe")
{
   netpoll::Mutex<Adder> app;
   std::thread           th1([&]() { app.into_guard().ref_data_mut().add(); });
   std::thread           th2([&]() { app.into_guard().ref_data_mut().add(); });
   std::thread           th3([&]() { app.into_guard().ref_data_mut().add(); });
   th1.join();
   th2.join();
   th3.join();
   REQUIRE_EQ(app.into_guard().ref_data().count, 300);
}

TEST_SUITE_END;