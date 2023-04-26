#include <ejson/base64.h>
#define ENABLE_ELG_LOG
#include <elog/logger.h>
#include <netpoll/core.h>

#include "context.h"
using namespace chat;

void Context::processCodec(const netpoll::TcpConnectionPtr &conn,
                           const netpoll::MessageBuffer    *buffer)
{
   for (;;)
   {
      switch (state_)
      {
         case kGetLength: {
            if (buffer->readableBytes() < 8) { return; }
            length_ = buffer->readInt64();
            state_  = kGetBody;
            break;
         }
         case kGetBody: {
            if (buffer->readableBytes() < expectedLength())
            {
               text_.append(buffer->readAll());
               return;
            }
            else
            {
               auto t = buffer->read(expectedLength());
               ELG_TRACE("recv:{}", t);
               text_.append(t);
               state_ = kParseOk;
            }
            break;
         }
         case kParseOk: {
            return;
         }
      }
   }
}

void Context::Send(const netpoll::TcpConnectionPtr &conn, CommonData const &msg)
try
{
   auto buffer  = netpoll::MessageBuffer();
   auto jsonMsg = ejson::Parser::ToJSON(msg);
   ELG_TRACE("json msg:{}", jsonMsg);
   buffer.pushFrontInt64(jsonMsg.size());
   buffer.pushBack(jsonMsg);
   conn->send(buffer);
}
catch (std::exception const &e)
{
   ELG_WARN("exception:{}", e.what());
}