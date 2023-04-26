#include "message_buffer.h"

#include <cstring>

#include "funcs.h"
#ifndef _WIN32
#include <netinet/in.h>
#include <sys/uio.h>
#else
#include <winsock2.h>

#include "../../dependencies/windows_support.h"
#endif
#include <cassert>
#include <cerrno>

using namespace netpoll;
namespace netpoll {
static constexpr size_t kBufferOffset{8};
}

MessageBuffer::MessageBuffer(size_t len)
  : m_head(kBufferOffset),
    m_tail(m_head),
    m_initCap(len),
    m_buffer(len + m_head)
{
}

void MessageBuffer::ensureWritableBytes(size_t len)
{
   if (writableBytes() >= len) return;
   // move readable bytes
   if (m_head + writableBytes() >= (len + kBufferOffset))
   {
      std::copy(begin() + m_head, begin() + m_tail, begin() + kBufferOffset);
      m_tail = kBufferOffset + (m_tail - m_head);
      m_head = kBufferOffset;
      return;
   }
   // create new buffer
   size_t newLen;
   if ((m_buffer.size() * 2) > (kBufferOffset + readableBytes() + len))
      newLen = m_buffer.size() * 2;
   else newLen = kBufferOffset + readableBytes() + len;
   // Avoid the inefficiency of using resize
   MessageBuffer newbuffer(newLen);
   newbuffer.pushBack(*this);
   swap(newbuffer);
}

void MessageBuffer::swap(MessageBuffer &buf) noexcept
{
   m_buffer.swap(buf.m_buffer);
   std::swap(m_head, buf.m_head);
   std::swap(m_tail, buf.m_tail);
   std::swap(m_initCap, buf.m_initCap);
}

void MessageBuffer::pushBack(const MessageBuffer &buf)
{
   ensureWritableBytes(buf.readableBytes());
   memcpy(&m_buffer[m_tail], buf.peek(), buf.readableBytes());
   m_tail += buf.readableBytes();
}

void MessageBuffer::pushBack(const StringView &buf)
{
   ensureWritableBytes(buf.size());
   memcpy(&m_buffer[m_tail], buf.data(), buf.size());
   m_tail += buf.size();
}

void MessageBuffer::pushBackInt16(uint16_t s)
{
   uint16_t ss = htons(s);
   pushBack({static_cast<const char *>((void *)&ss), 2});
}

void MessageBuffer::pushBackInt32(uint32_t i)
{
   uint32_t ii = htonl(i);
   pushBack({static_cast<const char *>((void *)&ii), 4});
}

void MessageBuffer::pushBackInt64(uint64_t l)
{
   uint64_t ll = hton64(l);
   pushBack({static_cast<const char *>((void *)&ll), 8});
}

void MessageBuffer::pushFrontInt16(uint16_t s)
{
   uint16_t ss = htons(s);
   pushFront({static_cast<const char *>((void *)&ss), 2});
}

void MessageBuffer::pushFrontInt32(uint32_t i)
{
   uint32_t ii = htonl(i);
   pushFront({static_cast<const char *>((void *)&ii), 4});
}

void MessageBuffer::pushFrontInt64(uint64_t l)
{
   uint64_t ll = hton64(l);
   pushFront({static_cast<const char *>((void *)&ll), 8});
}

uint16_t MessageBuffer::peekInt16() const
{
   assert(readableBytes() >= 2);
   uint16_t rs;
   ::memcpy(&rs, peek(), sizeof rs);
   return ntohs(rs);
}

uint32_t MessageBuffer::peekInt32() const
{
   assert(readableBytes() >= 4);
   uint32_t rl;
   ::memcpy(&rl, peek(), sizeof rl);
   return ntohl(rl);
}

uint64_t MessageBuffer::peekInt64() const
{
   assert(readableBytes() >= 8);
   uint64_t rll;
   ::memcpy(&rll, peek(), sizeof rll);
   return ntoh64(rll);
}

void MessageBuffer::retrieve(size_t len) const
{
   if (len >= readableBytes())
   {
      retrieveAll();
      return;
   }
   m_head += len;
}

void MessageBuffer::retrieveAll() const
{
   if (m_buffer.size() > (m_initCap * 2)) { m_buffer.resize(m_initCap); }
   m_tail = m_head = kBufferOffset;
}

ssize_t MessageBuffer::readFd(int fd, int *retErrno)
{
   char         extBuffer[8192];
   struct iovec vec[2];
   size_t       writable = writableBytes();
   vec[0].iov_base       = begin() + m_tail;
   vec[0].iov_len        = static_cast<int>(writable);
   vec[1].iov_base       = extBuffer;
   vec[1].iov_len        = sizeof(extBuffer);
   const int iovcnt      = (writable < sizeof extBuffer) ? 2 : 1;
   ssize_t   n           = ::readv(fd, vec, iovcnt);
   if (n < 0) { *retErrno = errno; }
   else if (static_cast<size_t>(n) <= writable) { m_tail += n; }
   else
   {
      m_tail = m_buffer.size();
      pushBack({extBuffer, n - writable});
   }
   return n;
}

std::string MessageBuffer::read(size_t len) const
{
   if (len > readableBytes()) len = readableBytes();
   std::string ret(peek(), len);
   retrieve(len);
   return ret;
}

uint8_t MessageBuffer::readInt8() const
{
   uint8_t ret = peekInt8();
   retrieve(1);
   return ret;
}

uint16_t MessageBuffer::readInt16() const
{
   uint16_t ret = peekInt16();
   retrieve(2);
   return ret;
}

uint32_t MessageBuffer::readInt32() const
{
   uint32_t ret = peekInt32();
   retrieve(4);
   return ret;
}

uint64_t MessageBuffer::readInt64() const
{
   uint64_t ret = peekInt64();
   retrieve(8);
   return ret;
}

void MessageBuffer::pushFront(StringView const &buf)
{
   // If the reserved space is sufficient
   if (m_head >= buf.size())
   {
      memcpy(begin() + m_head - buf.size(), buf.data(), buf.size());
      m_head -= buf.size();
      return;
   }
   // If the writable length is sufficient, move the written content back and
   // then push front
   if (buf.size() <= writableBytes())
   {
      std::copy(begin() + m_head, begin() + m_tail,
                begin() + m_head + buf.size());
      memcpy(begin() + m_head, buf.data(), buf.size());
      m_tail += buf.size();
      return;
   }
   // If the space is insufficient, expand the capacity. The expansion method is
   // the same as that of push back
   size_t newLen;
   if (buf.size() + readableBytes() < m_initCap) newLen = m_initCap;
   else newLen = buf.size() + readableBytes();
   MessageBuffer newBuf(newLen);
   newBuf.pushBack(buf);
   newBuf.pushBack(*this);
   swap(newBuf);
}
