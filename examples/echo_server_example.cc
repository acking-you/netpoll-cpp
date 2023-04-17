#include <netpoll/core.h>

using namespace netpoll;

struct test_server
{
   NETPOLL_TCP_MESSAGE(conn, buffer) { conn->send(buffer->readAll()); }
};

int main()
{
   auto loop     = NewEventLoop();
   auto listener = tcp::Listener::New({6666});
   listener->bind<test_server>();
   loop.serve(listener);
}