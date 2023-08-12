#include "eventloop_wrap.h"

#include <netpoll/util/task_per_thread_queue.h>

#include "tcp_dialer.h"
#include "tcp_listener.h"

using namespace netpoll;

EventLoop* EventLoopWrap::getEventLoop()
{
   thread_local EventLoop t_instance;
   return &t_instance;
}

std::vector<EventLoop*>& EventLoopWrap::allEventLoop()
{
   static std::vector<EventLoop*> loops;
   return loops;
}

void netpoll::QuitAllEventLoop()
{
   for (auto&& loop : EventLoopWrap::allEventLoop()) { loop->quit(); }
}

void EventLoopWrap::serve(tcp::Listener& listener)
{
   if (m_quit)
   {
      allEventLoop().push_back(getEventLoop());
      for (size_t i = 0; i < m_pool->size(); ++i)
         allEventLoop().push_back(m_pool->getLoop(i));
   }
   listener.m_server->setIoLoopThreadPool(m_pool);
   listener.m_server->setLoop(getEventLoop());
   listener.m_server->start();
   listener.m_server->getLoop()->loop();
}

void EventLoopWrap::serve(tcp::Dialer& dialer)
{
   if (m_quit)
   {
      for (size_t i = 0; i < m_pool->size(); ++i)
         allEventLoop().push_back(m_pool->getLoop(i));
   }
   dialer.setLoop(m_pool->getNextLoop());
   dialer.connect();
   m_pool->start();
   m_pool->wait();
}

void EventLoopWrap::serve(const std::vector<tcp::Dialer>& dialers)
{
   if (m_quit)
   {
      for (size_t i = 0; i < m_pool->size(); ++i)
         allEventLoop().push_back(m_pool->getLoop(i));
   }
   for (auto&& dialer : dialers)
   {
      dialer.setLoop(m_pool->getNextLoop());
      dialer.connect();
   }
   m_pool->start();
   m_pool->wait();
}

std::future<void> EventLoopWrap::runAsDaemon(const std::function<void()>& func)
{
   return util::TaskPerThreadQueue::Get().runTask(func);
}

std::future<void> EventLoopWrap::runAsDaemon(std::function<void()>&& func)
{
   return util::TaskPerThreadQueue::Get().runTask(func);
}
