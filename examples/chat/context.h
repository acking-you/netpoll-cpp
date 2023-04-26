#pragma once
#include <netpoll/net/callbacks.h>

#include "type.h"

namespace chat {
class Context
{
public:
   explicit Context(int id) : id_(id) {}

   int getId() const { return id_; }

   void processCodec(const netpoll::TcpConnectionPtr &conn,
                     const netpoll::MessageBuffer    *buffer);

   bool parseOk() { return state_ == kParseOk; }

   std::string getText()
   {
      auto text = std::move(text_);
      reset();
      return text;
   }

   static void Send(const netpoll::TcpConnectionPtr &conn,
                    CommonData const                &msg);

private:
   uint64_t expectedLength() { return length_ - text_.size(); }

   void reset()
   {
      state_ = kGetLength;
      text_.clear();
   }

   const int   id_;
   ParseState  state_{kGetLength};
   uint64_t    length_{};
   std::string text_{};
};
}   // namespace chat