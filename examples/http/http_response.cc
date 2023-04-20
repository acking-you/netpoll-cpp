#include "http_response.h"

#include <fmt/core.h>

std::string http::Response::toString()
{
   std::string ret;
   const char* version{};
   switch (version_)
   {
      case Version::kUnknown: version = "HTTP/1.1"; break;
      case Version::kHttp10: version = "HTTP/1.0"; break;
      case Version::kHttp11: version = "HTTP/1.1"; break;
   }
   ret.append(fmt::format("{} {} {}", version, static_cast<int>(statusCode_),
                          statusMsg_));
   switch (connectionState_)
   {
      case ConnectionState::kClose: ret.append("Connection: close\r\n"); break;
      case ConnectionState::kKeepAlive: break;
   }
   if (hasFile())
   {
      ret.append(fmt::format("Content-Length: {}\r\nConnection:keep-alive\r\n",
                             fileinfo_.filesize));
   }
   if (!body_.empty())
   {
      ret.append(fmt::format("Content-Length: {}\r\nConnection: Keep-Alive\r\n",
                             body_.size()));
   }
   for (auto&& header : headers_)
   {
      ret.append(header.first);
      ret.append(": ");
      ret.append(header.second);
      ret.append("\r\n");
   }
   ret.append("\r\n");
   // If you need to transfer the file
   if (hasFile()) { return ret; }
   ret.append(body_);
   return ret;
}
