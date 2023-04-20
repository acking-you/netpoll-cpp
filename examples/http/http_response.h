#pragma once
#include <netpoll/util/message_buffer.h>
#include <netpoll/util/string_view.h>

#include <string>
#include <unordered_map>

#include "types.h"

namespace http {
struct fileinfo
{
   unsigned long long filesize{};
   std::string        filename;
};
class Response
{
public:
   auto version() -> Version& { return version_; }

   auto statusCode() -> StatusCode& { return statusCode_; }

   auto connectionState() -> ConnectionState& { return connectionState_; }

   void responseBad()
   {
      statusCode_      = StatusCode::k400BadRequest;
      connectionState_ = ConnectionState::kClose;
   }

   void responseOk()
   {
      statusCode_      = StatusCode::k200Ok;
      connectionState_ = ConnectionState::kClose;
   }

   auto statusMsg() -> std::string& { return statusMsg_; }

   auto body() -> std::string& { return body_; }

   auto headers() -> std::unordered_map<std::string, std::string>&
   {
      return headers_;
   }

   auto fileinfo() -> fileinfo& { return fileinfo_; }
   bool hasFile() const { return fileinfo_.filesize != 0; }

   void setContentType(netpoll::StringView const& type)
   {
      headers_["Content-Type"] = {type.data(), type.size()};
   }

   std::string toString();

private:
   Version         version_{Version::kHttp11};
   StatusCode      statusCode_{StatusCode::kUnknown};
   ConnectionState connectionState_{ConnectionState::kKeepAlive};
   std::string     statusMsg_;
   struct fileinfo fileinfo_;
   std::string     body_;
   std::unordered_map<std::string, std::string> headers_;
};

}   // namespace http