#pragma once

#include <netpoll/net/eventloop.h>
#include <netpoll/util/noncopyable.h>

#include "../poller.h"

#if defined __linux__ || defined _WIN32
#include <map>
#include <memory>
using EventList = std::vector<struct epoll_event>;
#endif
namespace netpoll {
class Channel;

class EpollPoller : public Poller
{
public:
   explicit EpollPoller(EventLoop *loop);
   ~EpollPoller() override;
   void poll(int timeoutMs, ChannelList *activeChannels) override;
   void updateChannel(Channel *channel) override;
   void removeChannel(Channel *channel) override;
#ifdef _WIN32
   void postEvent(uint64_t event) override;
   void setEventCallback(const EventCallback &cb) override
   {
      m_eventCallback = cb;
   }
#endif

private:
   void update(int operation, Channel *channel);
   void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;

#if defined __linux__ || defined _WIN32
   static const int kInitEventListSize = 16;
#ifdef _WIN32
   void         *m_epollFd;
   EventCallback m_eventCallback{[](uint64_t event) {}};
#else
   int m_epollFd;
#endif
   EventList m_events;
#endif

#ifndef NDEBUG
   using ChannelMap = std::map<int, Channel *>;
   ChannelMap m_channels;
#endif
};
}   // namespace netpoll
