#include "acceptor.h"

#define ENABLE_ELG_LOG
#include <elog/logger.h>
using namespace netpoll;
using namespace elog;

#ifndef O_CLOEXEC
#define O_CLOEXEC O_NOINHERIT
#endif

Acceptor::Acceptor(EventLoop *loop, const InetAddress &addr, bool reUseAddr,
                   bool reUsePort)

  :
#ifndef _WIN32
    // To prevent busy loop when file descriptors run out
    m_idleFd(::open("/dev/null", O_RDONLY | O_CLOEXEC)),
#endif
    m_sock(Socket::createNonblockingSocketOrDie(addr.getSockAddr()->sa_family)),
    m_addr(addr),
    m_loop(loop),
    m_acceptChannel(loop, m_sock.fd())
{
   m_sock.setReuseAddr(reUseAddr);
   m_sock.setReusePort(reUsePort);
   m_sock.bindAddress(m_addr);
   m_acceptChannel.setReadCallback([this] { handleRead(); });
   if (m_addr.toPort() == 0)
   {
      m_addr = InetAddress{Socket::getLocalAddr(m_sock.fd())};
   }
}

Acceptor::~Acceptor()
{
   m_acceptChannel.disableAll();
   m_acceptChannel.remove();
#ifndef _WIN32
   ::close(m_idleFd);
#endif
}

void Acceptor::listen()
{
   m_loop->assertInLoopThread();
   m_sock.listen();
   m_acceptChannel.enableReading();
}

void Acceptor::handleRead()
{
   InetAddress peer;
   int         newsock = m_sock.accept(&peer);
   if (newsock >= 0)
   {
      if (m_newConnectionCallback) { m_newConnectionCallback(newsock, peer); }
      else
      {
#ifndef _WIN32
         ::close(newsock);
#else
         closesocket(newsock);
#endif
      }
   }
   else
   {
      ELG_ERROR("Accpetor::handleRead");
// Read the section named "The special problem of
// accept()ing when you can't" in libev's doc.
// By Marc Lehmann, author of libev.
/// errno is thread safe
#ifndef _WIN32
      if (errno == EMFILE)
      {
         ::close(m_idleFd);
         m_idleFd = m_sock.accept(&peer);
         ::close(m_idleFd);
         m_idleFd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
      }
#endif
   }
}
