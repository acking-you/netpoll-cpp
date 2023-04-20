#include <elog/logger.h>
#include <netpoll/core.h>
#include <netpoll/util/encode_util.h>

#include "http_context.h"
#include "http_response.h"
#include "request_handler.h"

namespace http {

class Server
{
public:
   NETPOLL_TCP_CONNECTION(conn)
   {
      if (conn->connected()) { conn->setContext(Context{}); }
      if (conn->disconnected()) { elog::Log::info("disconnected"); }
   }

   NETPOLL_TCP_MESSAGE(conn, buffer)
   {
      auto& ctx = netpoll::any_cast<Context&>(conn->getMutableContext());
      if (!ctx.parseRequest(buffer))
      {
         conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
         conn->shutdown();
      }
      if (ctx.parseOk())
      {
         onRequest(conn, ctx.request());
         ctx.reset();
      }
   }

private:
   static void onRequest(netpoll::TcpConnectionPtr const& conn, Request& req)
   {
      auto&    connection = req.headers()["Connection"];
      Response response;
      if (connection == "close" ||
          (req.version() == Version::kHttp10 && connection != "keep-alive"))
      {
         response.connectionState() = ConnectionState::kClose;
      }

      RequestHandler(req, response);
      conn->send(response.toString());

      if (response.hasFile())
      {
         conn->sendFile(response.fileinfo().filename, 0,
                        response.fileinfo().filesize);
      }
      // If you need to disconnect
      //      if (response.connectionState() == ConnectionState::kClose)
      //      {
      //         conn->shutdown();
      //      }
   }
};
}   // namespace http

int main()
{
   elog::GlobalConfig::Get()
     .setLevel(elog::kTrace)
     .setFormatter(elog::formatter::colorfulFormatter);
   auto loop     = netpoll::NewEventLoop();
   auto listener = netpoll::tcp::Listener::New({1314});

   listener->bind<http::Server>();
   listener->enableKickoffIdle(60);
   loop.serve(listener);
}