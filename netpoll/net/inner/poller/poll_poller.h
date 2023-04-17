#pragma once

#include <vector>

#include "../poller.h"

#if defined __unix__ || defined __HAIKU__
#include <poll.h>
#endif

namespace netpoll {
class PollPoller : public Poller
{
public:
   PollPoller(EventLoop* loop);
   ~PollPoller() override;

   void poll(int timeoutMs, ChannelList* activeChannels) override;
   void updateChannel(Channel* channel) override;
   void removeChannel(Channel* channel) override;

private:
   void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;

#if defined __unix__ || defined __HAIKU__
   std::vector<struct pollfd> pollfds_;
   std::map<int, Channel*>    channels_;
#endif
};

}   // namespace netpoll
