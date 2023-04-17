#pragma once
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "noncopyable.h"
#include "string_view.h"
#ifdef _WIN32
using ssize_t = std::intptr_t;
#endif

namespace netpoll {
static constexpr size_t kBufferDefaultLength{2048};
static constexpr char   CRLF[]{"\r\n"};

/**
 * @brief This class represents a memory buffer used for sending and receiving
 * data.
 *
 */
class MessageBuffer
{
public:
   /**
    * @brief Construct a new message buffer instance.
    *
    * @param len The initial size of the buffer.
    */
   explicit MessageBuffer(size_t len = kBufferDefaultLength);

   /**
    * prevent copyable deleted
    */
   MessageBuffer(MessageBuffer const &)            = default;
   MessageBuffer &operator=(MessageBuffer const &) = default;

   /**
    * make it movable
    * @param other
    */
   MessageBuffer(MessageBuffer &&other) noexcept { swap(other); }
   MessageBuffer &operator=(MessageBuffer &&other) noexcept
   {
      swap(other);
      return *this;
   }

   /**
    * @brief New the beginning of the buffer.
    *
    * @return const char*
    */
   const char *peek() const { return begin() + m_head; }

   /**
    * @brief New the end of the buffer where new data can be written.
    *
    * @return const char*
    */
   const char *beginWrite() const { return begin() + m_tail; }
   char       *beginWrite() { return begin() + m_tail; }

   /**
    * @brief New a string value from the buffer.
    * @param size
    * @return
    */
   std::string peekString(size_t size) const
   {
      assert(readableBytes() >= size);
      return {peek(), size};
   }

   /**
    * @brief New a byte value from the buffer.
    *
    * @return uint8_t
    */
   uint8_t peekInt8() const
   {
      assert(readableBytes() >= 1);
      return *(static_cast<const uint8_t *>((void *)peek()));
   }

   /**
    * @brief New a unsigned short value from the buffer.
    *
    * @return uint16_t
    */
   uint16_t peekInt16() const;

   /**
    * @brief New a unsigned int value from the buffer.
    *
    * @return uint32_t
    */
   uint32_t peekInt32() const;

   /**
    * @brief New a unsigned int64 value from the buffer.
    *
    * @return uint64_t
    */
   uint64_t peekInt64() const;

   /**
    * @brief New and remove some bytes from the buffer.
    *
    * @param len
    * @return std::string
    */
   std::string read(size_t len) const;

   /**
    * @brief New all readable bytes.
    *
    * @param len
    * @return std::string
    */
   std::string readAll() const { return read(readableBytes()); }

   /**
    * @brief New the remove a byte value from the buffer.
    *
    * @return uint8_t
    */
   uint8_t readInt8() const;

   /**
    * @brief New and remove a unsigned short value from the buffer.
    *
    * @return uint16_t
    */
   uint16_t readInt16() const;

   /**
    * @brief New and remove a unsigned int value from the buffer.
    *
    * @return uint32_t
    */
   uint32_t readInt32() const;

   /**
    * @brief New and remove a unsigned int64 value from the buffer.
    *
    * @return uint64_t
    */
   uint64_t readInt64() const;

   /**
    * @brief swap the buffer with another.
    *
    * @param buf
    */
   void swap(MessageBuffer &buf) noexcept;

   /**
    * @brief Return the size of the data in the buffer.
    *
    * @return size_t
    */
   size_t readableBytes() const { return m_tail - m_head; }

   /**
    * @brief Return the size of the empty part in the buffer
    *
    * @return size_t
    */
   size_t writableBytes() { return m_buffer.size() - m_tail; }

   /**
    * @brief Append new data to the buffer.
    *
    */
   void pushBack(const MessageBuffer &buf);
   void pushBack(const StringView &buf);

   /**
    * @brief Append a byte value to the end of the buffer.
    *
    * @param b
    */
   void pushBackInt8(const uint8_t b)
   {
      pushBack({static_cast<const char *>((void *)&b), 1});
   }

   /**
    * @brief Append a unsigned short value to the end of the buffer.
    *
    * @param s
    */
   void pushBackInt16(uint16_t s);

   /**
    * @brief Append a unsigned int value to the end of the buffer.
    *
    * @param i
    */
   void pushBackInt32(uint32_t i);

   /**
    * @brief Appaend a unsigned int64 value to the end of the buffer.
    *
    * @param l
    */
   void pushBackInt64(uint64_t l);

   /**
    * @brief Put new data to the beginning of the buffer.
    *        If the length of the data is less than k Buffer Offset then it will
    * be pushed into the reserved area
    *
    * @param buf
    * @param len
    */
   void pushFront(StringView const &buf);

   /**
    * @brief Put a byte value to the beginning of the buffer.
    *
    * @param b
    */
   void pushFrontInt8(const uint8_t b)
   {
      pushFront({static_cast<const char *>((void *)&b), 1});
   }

   /**
    * @brief Put a unsigned short value to the beginning of the buffer.
    *
    * @param s
    */
   void pushFrontInt16(uint16_t s);

   /**
    * @brief Put a unsigned int value to the beginning of the buffer.
    *
    * @param i
    */
   void pushFrontInt32(uint32_t i);

   /**
    * @brief Put a unsigned int64 value to the beginning of the buffer.
    *
    * @param l
    */
   void pushFrontInt64(uint64_t l);

   /**
    * @brief Remove all data in the buffer.
    *
    */
   void retrieveAll() const;

   /**
    * @brief Remove some bytes in the buffer.
    *
    * @param len
    */
   void retrieve(size_t len) const;

   /**
    * @brief Read data from a file descriptor and put it into the buffer.˝
    *
    * @param fd The file descriptor. It is usually a socket.
    * @param retErrno The error code when reading.
    * @return ssize_t The number of bytes read from the file descriptor. -1 is
    * returned when an error occurs.
    */
   ssize_t readFd(int fd, int *retErrno);

   /**
    * @brief Remove the data before a certain position from the buffer.
    *
    * @param end The position.
    */
   void retrieveUntil(const char *end) const
   {
      assert(peek() <= end);
      assert(end <= beginWrite());
      retrieve(end - peek());
   }

   /**
    * @brief Find the position of the buffer where the CRLF is found.
    *
    * @return const char*
    */
   const char *findCRLF() const
   {
      const char *crlf = std::search(peek(), beginWrite(), CRLF, CRLF + 2);
      return crlf == beginWrite() ? nullptr : crlf;
   }

   /**
    * @brief Make sure the buffer has enough spaces to write data.
    *
    * @param len
    */
   void ensureWritableBytes(size_t len);

   /**
    * @brief Move the write pointer forward when the new data has been written
    * to the buffer.
    *
    * @param len
    */
   void hasWritten(size_t len)
   {
      assert(len <= writableBytes());
      m_tail += len;
   }

   /**
    * @brief Move the write pointer backward to remove data in the end of the
    * buffer.
    *
    * @param offset
    */
   void unwrite(size_t offset)
   {
      assert(readableBytes() >= offset);
      m_tail -= offset;
   }

   /**
    * @brief Access a byte in the buffer.
    *
    * @param offset
    * @return const char&
    */
   const char &operator[](size_t offset) const
   {
      assert(readableBytes() >= offset);
      return peek()[offset];
   }
   char &operator[](size_t offset)
   {
      assert(readableBytes() >= offset);
      return begin()[m_head + offset];
   }

private:
   const char *begin() const { return &m_buffer[0]; }
   char       *begin() { return &m_buffer[0]; }

   mutable size_t            m_head{};
   mutable size_t            m_tail{};
   // Used to keep the length of the buffer stable over a suitable range, rather
   // than growing indefinitely
   size_t                    m_initCap{};
   mutable std::vector<char> m_buffer;
};

}   // namespace netpoll