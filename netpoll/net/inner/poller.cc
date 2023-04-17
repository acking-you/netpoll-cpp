#include "poller.h"
#ifdef __linux__
#include "poller/epoll_poller.h"
#elif defined _WIN32
#include "dependencies/wepoll/wepoll.h"
#include "poller/epoll_poller.h"
#elif defined __FreeBSD__ || defined __OpenBSD__ || defined __APPLE__
#include "poller/kqueue.h"
#else
#include "poller/poll_poller.h"
#endif
using namespace netpoll;
Poller *Poller::New(EventLoop *loop)
{
#if defined __linux__ || defined _WIN32
   return new EpollPoller(loop);
#elif defined __FreeBSD__ || defined __OpenBSD__ || defined __APPLE__
   return new KQueue(loop);
#else
   return new PollPoller(loop);
#endif
}
