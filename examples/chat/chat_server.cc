#define ENABLE_ELG_LOG
#include <elog/logger.h>
#include <netpoll/core.h>

#include <atomic>
#include <unordered_map>

#include "context.h"
#include "type.h"
#include "util.h"

namespace chat {
struct server
{
   using ConnectionMapType =
     std::unordered_map<int, std::weak_ptr<netpoll::TcpConnection>>;

   NETPOLL_TCP_CONNECTION(conn)
   {
      if (conn->connected())
      {
         int id = gid_++;
         conn->setContext(Context{id});
         std::lock_guard<std::mutex> lockGuard(connMap_.mutex);
         connMap_.value[id] = conn;
      }
      else
      {
         auto& ctx = netpoll::any_cast<Context&>(conn->getMutableContext());
         std::lock_guard<std::mutex> lockGuard(connMap_.mutex);
         connMap_.value.erase(ctx.getId());
      }
   }

   NETPOLL_TCP_MESSAGE(conn, buffer)
   {
   flag:
      auto& ctx = netpoll::any_cast<Context&>(conn->getMutableContext());
      ctx.processCodec(conn, buffer);
      if (ctx.parseOk())
      {
         onRequest(conn, ctx.getText());
         if (buffer->readableBytes() > 0) { goto flag; }
      }
   }

   netpoll::TcpConnectionPtr getConnById(int id)
   {
      netpoll::TcpConnectionPtr conn;
      {
         std::lock_guard<std::mutex> lockGuard(connMap_.mutex);
         conn = connMap_.value[id].lock();
      }
      return conn;
   }

   void onRequest(netpoll::TcpConnectionPtr const& conn, const std::string& msg)
   try
   {
      CommonData data;
      ELG_TRACE("msg:{}", msg);
      ejson::Parser::FromJSON(msg, data);
      if (data.code == -1)
      {
         ELG_WARN("json parse error");
         return;
      }

      auto& ctx = netpoll::any_cast<Context&>(conn->getMutableContext());
      switch (data.code)
      {
         case kRequestInit: {
            ELG_INFO("uid:{} request init", ctx.getId());
            Context::Send(conn, {kResponseInit, std::to_string(ctx.getId())});
            break;
         }
         case kRequestChat: {
            ELG_INFO("uid:{} request chat", ctx.getId());
            ChatMsg requestChat;
            ejson::Parser::FromJSON(data.decodeText(), requestChat);
            if (requestChat.sender == -1 || requestChat.receiver == -1)
            {
               ELG_WARN("request error");
               return;
            }
            if (auto receiver = getConnById(requestChat.receiver); receiver)
            {
               Context::Send(receiver,
                             CommonData{kResponseChat,
                                        ejson::base64_encode(
                                          ejson::Parser::ToJSON(requestChat))});
            }
            else
            {
               Context::Send(receiver,
                             CommonData{kBadResponse, "receiver is not exist"});
            }
            break;
         }
         case kRequestAllList: {
            ELG_INFO("uid:{} request all connection list", ctx.getId());
            ConnectionList response;
            {
               std::lock_guard<std::mutex> lockGuard(connMap_.mutex);
               for (auto&& [k, _] : connMap_.value)
               {
                  if (k == ctx.getId()) { continue; }
                  response.value.push_back(k);
               }
            }
            Context::Send(conn, CommonData{kResponseAllList,
                                           ejson::base64_encode(
                                             ejson::Parser::ToJSON(response))});
            break;
         }
      }
   }
   catch (const std::exception& e)
   {
      ELG_WARN("json parsing or serialization error:{}", e.what());
      return;
   }

   std::atomic_int               gid_{0};
   MutexGuard<ConnectionMapType> connMap_;
};
}   // namespace chat

int main()
{
   auto loop     = netpoll::NewEventLoop();
   auto listener = netpoll::tcp::Listener::New({8080});
   listener->bind<chat::server>();
   loop.serve(listener);
}