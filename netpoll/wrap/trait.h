#pragma once

#define HAS_MEMBER(member)                                                     \
   template <typename T, typename... Args>                                     \
   struct has_member_##member                                                  \
   {                                                                           \
   private:                                                                    \
      template <typename U>                                                    \
      static auto Check(int)                                                   \
        -> decltype(std::declval<U>().member(std::declval<Args>()...),         \
                    std::true_type());                                         \
      template <typename U>                                                    \
      static std::false_type Check(...);                                       \
                                                                               \
   public:                                                                     \
      enum {                                                                   \
         value = std::is_same<decltype(Check<T>(0)), std::true_type>::value    \
      };                                                                       \
   };
/// Call timing: After the connection is established.
#define NETPOLL_TCP_MESSAGE(conn, buffer)                                      \
   void onMessage(const netpoll::TcpConnectionPtr &conn,                       \
                  const netpoll::MessageBuffer    *buffer)

/// Call timing: When connection is connected or disconnected.
#define NETPOLL_TCP_CONNECTION(conn)                                           \
   void onConnection(const netpoll::TcpConnectionPtr &conn)

/// Call timing: When the message of the send queue is finished.
#define NETPOLL_TCP_WRITE_COMPLETE(conn)                                       \
   void onWriteComplete(const netpoll::TcpConnectionPtr &conn)

/// note: Only called when error generate by `connect`.No effect in a server app.
#define NETPOLL_TCP_CONNECTION_ERROR() void onConnectionError()

namespace netpoll {
HAS_MEMBER(onConnection)
HAS_MEMBER(onConnectionError)
HAS_MEMBER(onMessage)
HAS_MEMBER(onWriteComplete)

namespace trait {
template <typename T>
constexpr bool has_conn()
{
   return has_member_onConnection<T, TcpConnectionPtr const &>::value;
}

template <typename T>
constexpr bool has_conn_error()
{
   return has_member_onConnectionError<T>::value;
}

template <typename T>
constexpr bool has_msg()
{
   return has_member_onMessage<T, TcpConnectionPtr const &,
                               const MessageBuffer *>::value;
}

template <typename T>
constexpr bool has_write_complete()
{
   return has_member_onWriteComplete<T, TcpConnectionPtr const &>::value;
}
}   // namespace trait
}   // namespace netpoll