#pragma once
#include <netpoll/util/string_view.h>

#include <string>
#include <unordered_map>

#include "types.h"

namespace http {

class Request
{
public:
   static Method MethodMapping(netpoll::StringView const& method)
   {
#define METHOD_CODE(m)                                                         \
   if (method == #m) return Method::k##m;
      METHOD_CODE(GET)
      METHOD_CODE(POST)
      METHOD_CODE(HEAD)
      METHOD_CODE(PUT)
      METHOD_CODE(DELETE)
#undef METHOD_CODE
      return Method::kInvalid;
   }

   Request() : method_(Method::kInvalid), version_(Version::kUnknown) {}

   void addHeader(netpoll::StringView const& key,
                  netpoll::StringView const& value)
   {
      // Remove Spaces at both ends
      int i = 0;
      while (i < value.size() && value[i] == ' ') ++i;
      if (i == value.size())
      {
         headers_[{key.data(), key.size()}] = "";
         return;
      }

      int k = static_cast<int>(value.size()) - 1;
      while (k >= 0 && value[k] == ' ') --k;
      if (k == -1)
      {
         headers_[{key.data(), key.size()}] = "";
         return;
      }

      headers_[{key.data(), key.size()}] = {value.data() + i,
                                            value.data() + k + 1};
   }

   auto headers() -> std::unordered_map<std::string, std::string>&
   {
      return headers_;
   }

   auto method() -> Method& { return method_; }
   auto method() const -> const Method& { return method_; }

   auto version() -> Version& { return version_; }
   auto version() const -> const Version& { return version_; }

   auto body() -> std::string& { return body_; }
   auto body() const -> const std::string& { return body_; }

   auto path() -> std::string& { return path_; }
   auto path() const -> const std::string& { return path_; }

   auto query() -> std::string& { return query_; }
   auto query() const -> const std::string& { return query_; }

private:
   Method                                       method_;
   Version                                      version_;
   std::string                                  path_;
   std::string                                  query_;
   std::string                                  body_;
   std::unordered_map<std::string, std::string> headers_;
};
}   // namespace http