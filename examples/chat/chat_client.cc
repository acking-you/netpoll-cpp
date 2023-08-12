#define ENABLE_ELG_LOG
#include <elog/logger.h>
#include <netpoll/core.h>
#include <netpoll/util/concurrent_task_queue.h>

#include "context.h"
using namespace std::chrono_literals;

namespace chat {
struct client
{
   NETPOLL_TCP_CONNECTION(conn)
   {
      if (conn->connected())
      {
         ELG_TRACE("connect successful!");
         conn->setContext(chat::Context{-1});
         // send init request
         Context::Send(conn, CommonData{kRequestInit, "init request"});
      }
      else { ELG_INFO("disconnect"); }
   }

   NETPOLL_TCP_MESSAGE(conn, buffer)
   {
      auto& ctx = netpoll::any_cast<Context&>(conn->getMutableContext());
      ctx.processCodec(conn, buffer);
      if (ctx.parseOk()) { onResponse(conn, ctx.getText()); }
   }

   static void sendRequest(netpoll::TcpConnectionPtr const& conn)
   {
      ELG_TRACE("getting all valid connection");
      // send get all connection list
      Context::Send(conn, CommonData{kRequestAllList, "request all list"});
   }

   // async only
   void chatByIdAsync(const netpoll::TcpConnectionPtr& conn,
                      std::vector<int> const&          idList)
   {
      std::cout << "your id is " << id_ << "\n";
      std::cout << "please choose id which you want to chat:\n";
      for (auto&& id : idList) { std::cout << id << " "; }
      std::cout << std::endl;

   flag:
      std::string str;
      std::cin >> str;
      size_t pos{};
      int    x = std::stoi(str, &pos);
      if (pos != str.size())
      {
         std::cout << "input error!" << std::endl;
         str.clear();
         goto flag;
      }
      auto ret = std::find_if(idList.begin(), idList.end(),
                              [x](int p) { return p == x; });
      if (ret == idList.end())
      {
         std::cout << "your choose invalid" << std::endl;
         str.clear();
         goto flag;
      }
      std::cout << "please input your msg:" << std::endl;
      std::string text;
      // absorb '\n'
      getchar();
      std::getline(std::cin, text);
      // send to msg
      Context::Send(conn, CommonData{kRequestChat,
                                     ejson::base64_encode(ejson::Parser::ToJSON(
                                       ChatMsg{id_, x, text}))});
      sendRequest(conn);
   }

   void onResponse(netpoll::TcpConnectionPtr const& conn,
                   std::string const&               text)
   try
   {
      CommonData data;
      ejson::Parser::FromJSON(text, data);
      if (data.code == -1)
      {
         ELG_WARN("json parse error");
         return;
      }
      switch (data.code)
      {
         case kResponseInit: {
            ELG_TRACE("init successful");
            size_t idx;
            id_ = std::stoi(data.text, &idx);
            if (idx != data.text.size())
            {
               ELG_WARN("init error in parsing id,reinit start");
               Context::Send(conn, CommonData{kRequestInit, "init request"});
               return;
            }
            sendRequest(conn);
            break;
         }
         case kResponseChat: {
            ELG_TRACE("response chat");
            ChatMsg msg;
            ejson::Parser::FromJSON(data.decodeText(), msg);
            if (msg.sender == -1)
            {
               ELG_WARN("response msg error");
               return;
            }
            ELG_INFO("msg received:{}->{} {}", msg.sender, msg.receiver,
                     msg.text);
            break;
         }
         case kResponseAllList: {
            ELG_TRACE("response all list");
            ConnectionList connectionList;
            ejson::Parser::FromJSON(data.decodeText(), connectionList);
            if (connectionList.value.empty())
            {
               ELG_INFO("Connection empty.Request again in one second.");
               taskQueue_.runTask([conn]() {
                  std::this_thread::sleep_for(1s);
                  sendRequest(conn);
               });
               return;
            }

            taskQueue_.runTask(
              [this, conn, list = std::move(connectionList.value)]() {
                 chatByIdAsync(conn, list);
              });
            break;
         }
         case kBadResponse: {
            ELG_INFO("bad response:{}", data.text);
            break;
         }
      }
   }
   catch (std::exception const& e)
   {
      ELG_WARN("json parsing or serialization error!");
   }

   int                          id_{-1};
   netpoll::ConcurrentTaskQueue taskQueue_{1, "async task"};
};
}   // namespace chat

int main()
{
   auto loop   = netpoll::NewEventLoop(1);
   auto dialer = netpoll::tcp::Dialer::New({"127.0.0.1", 8080});
#if __cplusplus >= 201703L
   dialer.bind<chat::client>();
#else
   netpoll::tcp::Dialer::Register<chat::client>();
   dialer.bindOnMessage<chat::client>();
   dialer.bindOnConnection<chat::client>();
#endif
   loop.serve(dialer);
}