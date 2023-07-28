#include <elog/logger.h>
#include <netpoll/core.h>

using namespace netpoll;

struct server
{
	NETPOLL_TCP_MESSAGE(conn, buffer)
	{
		conn->send(buffer->readAll());
	}
};

int main()
{
	elog::GlobalConfig::Get().setLevel(elog::kTrace).setFormatter(elog::formatter::colorfulFormatter);
	auto loop = NewEventLoop();
	auto listener = tcp::Listener::New({ 6666 });
	//   listener->enableKickoffIdle(60);
	listener.bind<server>();
	loop.serve(listener);
}