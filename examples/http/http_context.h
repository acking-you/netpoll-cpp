#include <netpoll/core.h>

#include "http_request.h"

namespace http {
class Context
{
public:
   enum RequestParseState {
      kExpectRequestLine,
      kExpectHeaders,
      kExpectBody,
      kGotAll,
   };

   // Returns false if protocol resolution fails
   bool parseRequest(const netpoll::MessageBuffer* buffer);

   bool parseOk() { return state_ == kGotAll; }

   Request& request() { return request_; }

   void reset()
   {
      state_   = kExpectRequestLine;
      request_ = Request{};
   }

private:
   bool processRequestLine(netpoll::StringView const& text);

   RequestParseState state_{kExpectRequestLine};
   Request           request_;
};
}   // namespace http
