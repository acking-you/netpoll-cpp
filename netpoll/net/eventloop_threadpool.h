#pragma once

#include <netpoll/net/eventloop_thread.h>

#include <memory>
#include <vector>

namespace netpoll {
/**
 * @brief This class represents a pool of EventLoopThread objects
 *
 */
class EventLoopThreadPool : noncopyable
{
public:
   EventLoopThreadPool() = delete;

   /**
    * @brief Construct a new event loop thread pool instance.
    *
    * @param threadNum The number of threads
    * @param name The name of the EventLoopThreadPool object.
    */
   explicit EventLoopThreadPool(size_t            threadNum,
                                StringView const &name = "EventLoopThreadPool");

   /**
    * @brief Run all event loops in the pool.
    * @note This function doesn't block the current thread.
    */
   void start();

   /**
    * @brief Wait for all event loops in the pool to quit.
    *
    * @note This function blocks the current thread.
    */
   void wait();

   /**
    * @brief Return the number of the event loop.
    *
    * @return size_t
    */
   size_t size() { return m_loopThreadList.size(); }

   /**
    * @brief New the next event loop in the pool.
    *
    * @return EventLoop*
    */
   EventLoop *getNextLoop();

   /**
    * @brief New the event loop in the `id` position in the pool.
    *
    * @param id The id of the first event loop is zero. If the id >= the number
    * of event loops, nullptr is returned.
    * @return EventLoop*
    */
   EventLoop *getLoop(size_t id);

   /**
    * @brief New all event loops in the pool.
    *
    * @return std::vector<EventLoop *>
    */
   std::vector<EventLoop *> getLoops() const;

private:
   std::vector<std::unique_ptr<EventLoopThread>> m_loopThreadList;
   size_t                                        m_loopIndex;
};
}   // namespace netpoll
