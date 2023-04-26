#pragma once
#define EJSON_TAG_WITH_METHOD
#include <ejson/base64.h>
#include <ejson/parser.h>

#include <string>

namespace chat {
enum ParseState { kGetLength, kGetBody, kParseOk };
enum Code {
   kRequestInit,
   kRequestAllList,
   kRequestChat,
   kResponseInit,
   kResponseAllList,
   kResponseChat,
   kBadResponse,
};

struct CommonData
{
   int         code;
   std::string text;
   OPTION_EJSON(code, -1)
   static CommonData Bad(ejson::string_view const &sv)
   {
      return CommonData{kBadResponse, {sv.data(), sv.length()}};
   }
   void        encodeText() { text = ejson::base64_encode(text); }
   std::string decodeText() const { return ejson::base64_decode(text); }
};

struct ChatMsg
{
   int         sender;
   int         receiver;
   std::string text;
   OPTION_EJSON(sender, -1)
   OPTION_EJSON(receiver, -1)
};

struct ConnectionList
{
   std::vector<int> value;
};

AUTO_GEN_NON_INTRUSIVE(CommonData, code, text)
AUTO_GEN_NON_INTRUSIVE(ChatMsg, sender, receiver, text)
AUTO_GEN_NON_INTRUSIVE(ConnectionList, value)

}   // namespace chat