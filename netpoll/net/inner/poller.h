#pragma once
#include <netpoll/net/eventloop.h>
#include <netpoll/util/noncopyable.h>

#include <map>
#include <memory>

namespace netpoll {
class Channel;
#ifdef _WIN32
using EventCallback = std::function<void(uint64_t)>;
#endif
class Poller : noncopyable
{
public:
   explicit Poller(EventLoop *loop) : m_ownerLoop(loop){};
   virtual ~Poller() = default;
   void         assertInLoopThread() { m_ownerLoop->assertInLoopThread(); }
   virtual void poll(int timeoutMs, ChannelList *activeChannels) = 0;
   virtual void updateChannel(Channel *channel)                  = 0;
   virtual void removeChannel(Channel *channel)                  = 0;
#ifdef _WIN32
   virtual void postEvent(uint64_t event)                 = 0;
   virtual void setEventCallback(const EventCallback &cb) = 0;
#endif
   virtual void   resetAfterFork() {}
   static Poller *New(EventLoop *loop);

private:
   EventLoop *m_ownerLoop;
};
}   // namespace netpoll
