// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#pragma once

#include <netpoll/util/string_view.h>
#include <netpoll/util/time_stamp.h>

#ifdef _WIN32
#include <ws2tcpip.h>
using sa_family_t = unsigned short;
using in_addr_t   = uint32_t;
using uint16_t    = unsigned short;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#include <mutex>
#include <string>
#include <unordered_map>
namespace netpoll {
/**
 * @brief Wrapper of sockaddr_in. This is an POD interface class.
 *
 */
class InetAddress
{
public:
   /**
    * @brief Constructs an endpoint with given port number. Mostly used in
    * TcpServer listening.
    *
    * @param port
    * @param loopbackOnly
    * @param ipv6
    */
   InetAddress(uint16_t port = 0, bool loopbackOnly = false, bool ipv6 = false);

   // copyable
   InetAddress(const InetAddress &)            = default;
   InetAddress &operator=(const InetAddress &) = default;

   /**
    * @brief Constructs an endpoint with given ip and port.
    *
    * @param ip A IPv4 or IPv6 address.
    * @param port
    * @param ipv6
    */
   InetAddress(const StringView &ip, uint16_t port, bool ipv6 = false);

   /**
    * @brief Constructs an endpoint with given struct `sockaddr_in`. Mostly
    * used when accepting new connections
    *
    * @param addr
    */
   explicit InetAddress(const struct sockaddr_in &addr)
     : m_addr(addr), m_isUnspecified(false)
   {
   }

   /**
    * @brief Constructs an IPv6 endpoint with given struct `sockaddr_in6`.
    * Mostly used when accepting new connections
    *
    * @param addr
    */
   explicit InetAddress(const struct sockaddr_in6 &addr)
     : m_addr6(addr), m_isIpv6(true), m_isUnspecified(false)
   {
   }

   /**
    * @brief Return the sin_family of the endpoint.
    *
    * @return sa_family_t
    */
   sa_family_t family() const { return m_addr.sin_family; }

   /**
    * @brief Return the IP string of the endpoint.
    *
    * @return std::string
    */
   std::string toIp() const;

   /**
    * @brief Return the IP and port string of the endpoint.
    *
    * @return std::string
    */
   std::string toIpPort() const;

   /**
    * @brief Return the port number of the endpoint.
    *
    * @return uint16_t
    */
   uint16_t toPort() const;

   /**
    * @brief Check if the endpoint is IPv4 or IPv6.
    *
    * @return true
    * @return false
    */
   bool isIpV6() const { return m_isIpv6; }

   /**
    * @brief Return true if the endpoint is an intranet endpoint.
    *
    * @return true
    * @return false
    */
   bool isIntranetIp() const;

   /**
    * @brief Return true if the endpoint is a loopback endpoint.
    *
    * @return true
    * @return false
    */
   bool isLoopbackIp() const;

   /**
    * @brief New the pointer to the sockaddr struct.
    *
    * @return const struct sockaddr*
    */
   const struct sockaddr *getSockAddr() const
   {
      return static_cast<const struct sockaddr *>((void *)(&m_addr6));
   }

   /**
    * @brief Set the sockaddr_in6 struct in the endpoint.
    *
    * @param addr6
    */
   void setSockAddrInet6(const struct sockaddr_in6 &addr6)
   {
      m_addr6         = addr6;
      m_isIpv6        = (m_addr6.sin6_family == AF_INET6);
      m_isUnspecified = false;
   }

   /**
    * @brief Return the integer value of the IP(v4) in net endian byte order.
    *
    * @return uint32_t
    */
   uint32_t ipNetEndian() const;

   /**
    * @brief Return the pointer to the integer value of the IP(v6) in net
    * endian byte order.
    *
    * @return const uint32_t*
    */
   const uint32_t *ip6NetEndian() const;

   /**
    * @brief Return the port number in net endian byte order.
    *
    * @return uint16_t
    */
   uint16_t portNetEndian() const { return m_addr.sin_port; }

   /**
    * @brief Set the port number in net endian byte order.
    *
    * @param port
    */
   void setPortNetEndian(uint16_t port) { m_addr.sin_port = port; }

   /**
    * @brief Return true if the address is not initalized.
    */
   inline bool isUnspecified() const { return m_isUnspecified; }

private:
   union
   {
      struct sockaddr_in  m_addr;
      struct sockaddr_in6 m_addr6;
   };
   bool m_isIpv6{false};
   bool m_isUnspecified{true};
};

}   // namespace netpoll
