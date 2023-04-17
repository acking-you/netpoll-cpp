#include "eventloop_threadpool.h"

using namespace netpoll;

EventLoopThreadPool::EventLoopThreadPool(size_t            threadNum,
                                         const StringView &name)
  : m_loopIndex(0)
{
   for (size_t i = 0; i < threadNum; ++i)
   {
      m_loopThreadList.emplace_back(std::make_unique<EventLoopThread>(name));
   }
}
void EventLoopThreadPool::start()
{
   for (auto &i : m_loopThreadList) { i->run(); }
}

void EventLoopThreadPool::wait()
{
   for (auto &i : m_loopThreadList) { i->wait(); }
}

EventLoop *EventLoopThreadPool::getNextLoop()
{
   if (!m_loopThreadList.empty())
   {
      EventLoop *loop = m_loopThreadList[m_loopIndex]->getLoop();
      ++m_loopIndex;
      if (m_loopIndex >= m_loopThreadList.size()) m_loopIndex = 0;
      return loop;
   }
   return nullptr;
}

EventLoop *EventLoopThreadPool::getLoop(size_t id)
{
   if (id < m_loopThreadList.size()) return m_loopThreadList[id]->getLoop();
   return nullptr;
}

std::vector<EventLoop *> EventLoopThreadPool::getLoops() const
{
   std::vector<EventLoop *> ret;
   for (auto &loopThread : m_loopThreadList)
   {
      ret.push_back(loopThread->getLoop());
   }
   return ret;
}