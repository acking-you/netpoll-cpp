#include <doctest/doctest.h>
#include <netpoll/util/message_buffer.h>
using namespace netpoll;

TEST_CASE("test MessageBuffer")
{
   MessageBuffer messageBuffer;
   SUBCASE("test pushBack & copy|move construct")
   {
      messageBuffer.pushBack("abc");
      CHECK_EQ(messageBuffer.readableBytes(), 3);
      CHECK_EQ(StringView{messageBuffer.peek(), 3}, "abc");
      MessageBuffer buf = std::move(messageBuffer);
      CHECK_EQ(messageBuffer.readableBytes(), 0);   // NOLINT
      CHECK_EQ(buf.readableBytes(), 3);
      CHECK_EQ(StringView{buf.peek(), 3}, "abc");
      MessageBuffer bufCopy = buf;
      bufCopy.pushBack(buf);
      CHECK_EQ(buf.readableBytes(), 3);
      CHECK_EQ(StringView{buf.peek(), 3}, "abc");
      CHECK_EQ(bufCopy.readableBytes(), 6);
      CHECK_EQ(StringView{bufCopy.peek(), 6}, "abcabc");
   }
   SUBCASE("test pushFront pushFrontIntX | peekIntX | readXXX")
   {
      messageBuffer.pushFrontInt64(1024);
      CHECK_EQ(messageBuffer.peekInt64(), 1024);
      messageBuffer.pushFront("abc");
      CHECK_EQ(messageBuffer.peekString(3), "abc");
      CHECK_EQ(messageBuffer.read(3), "abc");
      CHECK_EQ(messageBuffer.readInt64(), 1024);
   }
}
