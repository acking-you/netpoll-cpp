#include <doctest/doctest.h>
#include <netpoll/net/resolver.h>

#include <future>
#include <iostream>

using namespace netpoll;

TEST_SUITE_BEGIN("test Resolver");

TEST_CASE("Resolver::syncResolve")
{
   auto        res = Resolver::New(0);
   InetAddress addr;
   res->syncResolve("github.com", addr);
   std::cout << "url:github.com ip:" << addr.toIpPort();
}

TEST_CASE("Resolver::resolve")
{
   auto               res = Resolver::New(0);
   InetAddress        addr;
   std::promise<void> pro;
   res->resolve("acking-you.github.io", [&](InetAddress const& tmp) {
      pro.set_value();
      addr = tmp;
   });
   pro.get_future().get();
   std::cout << "url:acking-you.github.io ip:" << addr.toIpPort();
}
TEST_SUITE_END;
