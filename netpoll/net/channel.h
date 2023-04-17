#pragma once

#include <netpoll/util/noncopyable.h>

#include <cassert>
#include <functional>
#include <memory>
namespace netpoll {
class EventLoop;
class Acceptor;
/**
 * @brief This class is used to implement reactor pattern. A Channel object
 * manages a socket fd. Users use a Channel object to receive write or read
 * events on the socket it manages.
 *
 */
class Channel : noncopyable
{
private:
   friend class Acceptor;
   // To support  EventLoop decoupling,for internal encapsulation only.
   void setLoop(EventLoop *loop) { m_loop = loop; }

public:
   using EventCallback = std::function<void()>;
   /**
    * @brief Construct a new Channel instance.
    *
    * @param loop The event loop in which the channel works.
    * @param fd The socket fd.
    */
   Channel(EventLoop *loop, int fd);

   /**
    * @brief Set the read callback.
    *
    * @param cb The callback is called when read event occurs on the socket.
    * @note One should call the enableReading() method to ensure that the
    * callback would be called when some data is received on the socket.
    */
   void setReadCallback(const EventCallback &cb) { m_readCallback = cb; };
   void setReadCallback(EventCallback &&cb) { m_readCallback = std::move(cb); }

   /**
    * @brief Set the write callback.
    *
    * @param cb The callback is called when write event occurs on the socket.
    * @note One should call the enableWriting() method to ensure that the
    * callback would be called when the socket can be written.
    */
   void setWriteCallback(const EventCallback &cb) { m_writeCallback = cb; };
   void setWriteCallback(EventCallback &&cb)
   {
      m_writeCallback = std::move(cb);
   }

   /**
    * @brief Set the close callback.
    *
    * @param cb The callback is called when the socket is closed.
    */
   void setCloseCallback(const EventCallback &cb) { m_closeCallback = cb; }
   void setCloseCallback(EventCallback &&cb)
   {
      m_closeCallback = std::move(cb);
   }

   /**
    * @brief Set the error callback.
    *
    * @param cb The callback is called when an error occurs on the socket.
    */
   void setErrorCallback(const EventCallback &cb) { m_errorCallback = cb; }
   void setErrorCallback(EventCallback &&cb)
   {
      m_errorCallback = std::move(cb);
   }

   /**
    * @brief Set the event callback.
    *
    * @param cb The callback is called when any event occurs on the socket.
    * @note If the event callback is set to the channel, any other callback
    * wouldn't be called again.
    */
   void setEventCallback(const EventCallback &cb) { m_eventCallback = cb; }
   void setEventCallback(EventCallback &&cb)
   {
      m_eventCallback = std::move(cb);
   }

   /**
    * @brief Return the fd of the socket.
    *
    * @return int
    */
   int fd() const { return m_fd; }

   /**
    * @brief Return the events enabled on the socket.
    *
    * @return int
    */
   int events() const { return m_events; }

   /**
    * @brief Return the events that occurred on the socket.
    *
    * @return int
    */
   int revents() const { return m_revents; }

   /**
    * @brief Check whether there is no event enabled on the socket.
    *
    * @return true
    * @return false
    */
   bool isNoneEvent() const { return m_events == kNoneEvent; };

   /**
    * @brief Disable all events on the socket.
    *
    */
   void disableAll()
   {
      m_events = kNoneEvent;
      update();
   }

   /**
    * @brief Remove the socket from the poller in the event loop.
    *
    */
   void remove();

   /**
    * @brief Return the event loop.
    *
    * @return EventLoop*
    */
   EventLoop *ownerLoop() { return m_loop; };

   /**
    * @brief Enable the read event on the socket.
    *
    */
   void enableReading()
   {
      m_events |= kReadEvent;
      update();
   }

   /**
    * @brief Disable the read event on the socket.
    *
    */
   void disableReading()
   {
      m_events &= ~kReadEvent;
      update();
   }

   /**
    * @brief Enable the write event on the socket.
    *
    */
   void enableWriting()
   {
      m_events |= kWriteEvent;
      update();
   }

   /**
    * @brief Disable the write event on the socket.
    *
    */
   void disableWriting()
   {
      m_events &= ~kWriteEvent;
      update();
   }

   /**
    * @brief Check whether the write event is enabled on the socket.
    *
    * @return true
    * @return false
    */
   bool isWriting() const { return m_events & kWriteEvent; }

   /**
    * @brief Check whether the read event is enabled on the socket.
    *
    * @return true
    * @return false
    */
   bool isReading() const { return m_events & kReadEvent; }

   /**
    * @brief Set and update the events enabled.
    *
    * @param events
    */
   void updateEvents(int events)
   {
      m_events = events;
      update();
   }

   /**
    * @brief This method is used to ensure that the callback owner is valid
    * when a callback is called.
    *
    * @param obj The callback owner. Usually, the owner is also the owner of
    * the channel.
    * @note The 'obj' is kept in a weak_ptr object, so this method does not
    * cause a circular reference problem.
    */
   void tie(const std::shared_ptr<void> &obj)
   {
      m_tie  = obj;
      m_tied = true;
   }

   static const int kNoneEvent;
   static const int kReadEvent;
   static const int kWriteEvent;

private:
   friend class EventLoop;
   friend class EpollPoller;
   friend class KQueue;
   friend class PollPoller;

   void update();
   void handleEvent();
   void handleEventSafely();
   int  setRevents(int revents)
   {
      m_revents = revents;
      return revents;
   };
   int  index() const { return m_index; };
   void setIndex(int index) { m_index = index; };

   EventLoop          *m_loop;
   const int           m_fd;
   int                 m_events;
   int                 m_revents;
   int                 m_index;
   bool                m_addedToLoop{false};
   EventCallback       m_readCallback;
   EventCallback       m_writeCallback;
   EventCallback       m_errorCallback;
   EventCallback       m_closeCallback;
   EventCallback       m_eventCallback;
   std::weak_ptr<void> m_tie;
   bool                m_tied;
};
}   // namespace netpoll
