#include "epoll_poller.h"
#define ENABLE_ELG_LOG
#include <elog/logger.h>
#include <netpoll/net/channel.h>
#ifdef __linux__
#include <poll.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cassert>
#include <iostream>
#elif defined _WIN32
#include <dependencies/wepoll/wepoll.h>
#include <fcntl.h>
#include <winsock2.h>

#include <cassert>
#include <iostream>
#define EPOLL_CLOEXEC _O_NOINHERIT
#endif
using namespace elog;
using namespace netpoll;

#if defined __linux__ || defined _WIN32

#if defined __linux__
static_assert(EPOLLIN == POLLIN, "EPOLLIN != POLLIN");
static_assert(EPOLLPRI == POLLPRI, "EPOLLPRI != POLLPRI");
static_assert(EPOLLOUT == POLLOUT, "EPOLLOUT != POLLOUT");
static_assert(EPOLLRDHUP == POLLRDHUP, "EPOLLRDHUP != POLLRDHUP");
static_assert(EPOLLERR == POLLERR, "EPOLLERR != POLLERR");
static_assert(EPOLLHUP == POLLHUP, "EPOLLHUP != POLLHUP");
#endif

namespace {
const int kNew     = -1;
const int kAdded   = 1;
const int kDeleted = 2;
}   // namespace

EpollPoller::EpollPoller(EventLoop *loop)
  : Poller(loop),
#ifdef _WIN32
    // wepoll does not suppor flags
    m_epollFd(::epoll_create1(0)),
#else
    m_epollFd(::epoll_create1(EPOLL_CLOEXEC)),
#endif
    m_events(kInitEventListSize)
{
}
EpollPoller::~EpollPoller()
{
#ifdef _WIN32
   epoll_close(m_epollFd);
#else
   close(m_epollFd);
#endif
}
#ifdef _WIN32
void EpollPoller::postEvent(uint64_t event)
{
   epoll_post_signal(m_epollFd, event);
}
#endif

void EpollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
   int numEvents  = ::epoll_wait(m_epollFd, &*m_events.begin(),
                                 static_cast<int>(m_events.size()), timeoutMs);
   int savedErrno = errno;
   if (numEvents > 0)
   {
      fillActiveChannels(numEvents, activeChannels);
      if (static_cast<size_t>(numEvents) == m_events.size())
      {
         m_events.resize(m_events.size() * 2);
      }
   }
   else if (numEvents == 0) { ELG_TRACE("nothing happended"); }
   else
   {
      // error happens, log uncommon ones
      if (savedErrno != EINTR)
      {
         errno = savedErrno;
         ELG_ERROR("EPollEpollPoller::poll()");
      }
   }
}

void EpollPoller::fillActiveChannels(int          numEvents,
                                     ChannelList *activeChannels) const
{
   assert(static_cast<size_t>(numEvents) <= m_events.size());
   for (int i = 0; i < numEvents; ++i)
   {
#ifdef _WIN32
      if (m_events[i].events == EPOLLEVENT)
      {
         m_eventCallback(m_events[i].data.u64);
         continue;
      }
#endif
      auto *channel = static_cast<Channel *>(m_events[i].data.ptr);
#ifndef NDEBUG
      int  fd = channel->fd();
      auto it = m_channels.find(fd);
      assert(it != m_channels.end());
      assert(it->second == channel);
#endif
      channel->setRevents(m_events[i].events);
      activeChannels->push_back(channel);
   }
}

void EpollPoller::updateChannel(Channel *channel)
{
   assertInLoopThread();
   assert(channel->fd() >= 0);

   const int index = channel->index();
   // LOG_TRACE << "fd = " << channel->fd()
   //  << " events = " << channel->events() << " index = " << index;
   if (index == kNew || index == kDeleted)
   {
// a new one, add with EPOLL_CTL_ADD
#ifndef NDEBUG
      int fd = channel->fd();
      if (index == kNew)
      {
         assert(m_channels.find(fd) == m_channels.end());
         m_channels[fd] = channel;
      }
      else
      {   // index == kDeleted
         assert(m_channels.find(fd) != m_channels.end());
         assert(m_channels[fd] == channel);
      }
#endif
      channel->setIndex(kAdded);
      update(EPOLL_CTL_ADD, channel);
   }
   else
   {
// update existing one with EPOLL_CTL_MOD/DEL
#ifndef NDEBUG
      int fd = channel->fd();
      (void)fd;
      assert(m_channels.find(fd) != m_channels.end());
      assert(m_channels[fd] == channel);
#endif
      assert(index == kAdded);
      if (channel->isNoneEvent())
      {
         update(EPOLL_CTL_DEL, channel);
         channel->setIndex(kDeleted);
      }
      else { update(EPOLL_CTL_MOD, channel); }
   }
}
void EpollPoller::removeChannel(Channel *channel)
{
   EpollPoller::assertInLoopThread();
#ifndef NDEBUG
   int fd = channel->fd();
   assert(m_channels.find(fd) != m_channels.end());
   assert(m_channels[fd] == channel);
   assert(channel->isNoneEvent());
   size_t n = m_channels.erase(fd);
   (void)n;
   assert(n == 1);
#endif
   int index = channel->index();
   assert(index == kAdded || index == kDeleted);
   if (index == kAdded) { update(EPOLL_CTL_DEL, channel); }
   channel->setIndex(kNew);
}

void EpollPoller::update(int operation, Channel *channel)
{
   struct epoll_event event;
   memset(&event, 0, sizeof(event));
   event.events   = channel->events();
   event.data.ptr = channel;
   int fd         = channel->fd();
   if (::epoll_ctl(m_epollFd, operation, fd, &event) < 0)
   {
      if (operation == EPOLL_CTL_DEL)
      {
         ELG_ERROR("epoll_ctl op = EPOLL_CTL_DEL fd ={:d}", fd);
      }
      else { ELG_ERROR("epoll_ctl op = {:d} ,fd ={:d}", operation, fd); }
   }
}
#else
EpollPoller::EpollPoller(EventLoop *loop) : Poller(loop) { assert(false); }
EpollPoller::~EpollPoller() {}
void EpollPoller::poll(int, ChannelList *) {}
void EpollPoller::updateChannel(Channel *) {}
void EpollPoller::removeChannel(Channel *) {}

#endif
