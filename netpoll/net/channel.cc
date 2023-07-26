#include "channel.h"

#include <netpoll/net/eventloop.h>
#ifdef _WIN32
#include "dependencies/wepoll/wepoll.h"
#define POLLIN   EPOLLIN
#define POLLPRI  EPOLLPRI
#define POLLOUT  EPOLLOUT
#define POLLHUP  EPOLLHUP
#define POLLNVAL 0
#define POLLERR  EPOLLERR
#else
#include <poll.h>
#endif
using namespace netpoll;

const int Channel::kNoneEvent = 0;

const int Channel::kReadEvent  = POLLIN | POLLPRI;
const int Channel::kWriteEvent = POLLOUT;

Channel::Channel(EventLoop *loop, int fd)
  : m_loop(loop),
    m_fd(fd),
    m_events(0),
    m_revents(0),
    m_index(-1),
    m_tied(false)
{
}

void Channel::remove()
{
   assert(m_events == kNoneEvent);
   m_addedToLoop = false;
   m_loop->removeChannel(this);
}

void Channel::update() { m_loop->updateChannel(this); }

void Channel::handleEvent()
{
   if (m_events == kNoneEvent) return;
   if (m_tied)
   {
      std::shared_ptr<void> guard = m_tie.lock();
      if (guard) { handleEventSafely(); }
   }
   else { handleEventSafely(); }
}
void Channel::handleEventSafely()
{
   if (m_eventCallback)
   {
      m_eventCallback();
      return;
   }
   if ((m_revents & POLLHUP) && !(m_revents & POLLIN))
   {
      if (m_closeCallback) m_closeCallback();
   }
   if (m_revents & (POLLNVAL | POLLERR))
   {
      if (m_errorCallback) m_errorCallback();
   }
#ifdef __linux__
   if (m_revents & (POLLIN | POLLPRI | POLLRDHUP))
#else
   if (m_revents & (POLLIN | POLLPRI))
#endif
   {
      if (m_readCallback) m_readCallback();
   }
#ifdef _WIN32
   if ((m_revents & POLLOUT) && !(m_revents & POLLHUP))
#else
   if (m_revents & POLLOUT)
#endif
   {
      if (m_writeCallback) m_writeCallback();
   }
}