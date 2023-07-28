#define ENABLE_ELG_LOG
#include <elog/logger.h>
#include <netpoll/core.h>

using namespace elog;

std::string input()
{
   std::cout << "please enter you message:";
   std::string ret;
   std::cin >> ret;
   return ret;
}

struct client
{
   NETPOLL_TCP_CONNECTION(conn)
   {
      if (conn->connected()) { conn->send(input()); }
   }
   NETPOLL_TCP_MESSAGE(conn, buffer)
   {
      ELG_INFO("msg received:{}", buffer->readAll());
      conn->send(input());
   }
};

int main()
{
   elog::GlobalConfig::Get().setLevel(elog::kTrace);
   auto loop   = netpoll::NewEventLoop(1);
   auto dialer = netpoll::tcp::Dialer::New({"127.0.0.1", 6666});
#if __cplusplus >= 201703L || (_MSC_VER && _MSVC_LANG >= 201703L)
   dialer.bind<client>();
#else
   netpoll::tcp::Dialer::Register<client>();
   dialer.bindOnConnection<client>().bindOnMessage<client>();
#endif
   loop.serve(dialer);
}