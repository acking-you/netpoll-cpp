#include "http_context.h"

using StrView = netpoll::StringView;

bool http::Context::processRequestLine(const netpoll::StringView &text)
{
   StrView sv   = text;
   // set method
   size_t  next = sv.find(' ');
   if (next == std::string::npos) return false;
   if (auto method = Request::MethodMapping({sv.begin(), next});
       method != Method::kInvalid)
   {
      request_.method() = method;
   }
   else return false;

   // set path&query
   if (sv.size() < next + 1) return false;
   sv   = StrView{sv.begin() + next + 1, sv.size() - next - 1};
   next = sv.find(' ');
   if (next == std::string::npos) return false;
   auto path     = StrView{sv.begin(), next};
   auto tmp_next = path.find('?');
   if (tmp_next != std::string::npos)
   {
      request_.query() =
        StrView{path.begin() + tmp_next + 1, path.size() - tmp_next - 1};
      path = StrView{path.begin(), tmp_next};
   }
   request_.path() = path;

   // set version
   if (sv.size() < next + 1) return false;
   sv = StrView{sv.begin() + next + 1, sv.size() - next - 1};
   if (StrView{sv.begin(), sv.size() - 1} != "HTTP/1.") return false;
   switch (sv.back())
   {
      case '0': request_.version() = Version::kHttp10; return true;
      case '1': request_.version() = Version::kHttp11; return true;
   }
   return false;
}

bool http::Context::parseRequest(const netpoll::MessageBuffer *buffer)
{
   for (;;)
   {
      const char *crlf;
      switch (state_)
      {
         case kExpectRequestLine: {
            crlf = buffer->findCRLF();
            if (crlf == nullptr)
            {
               // Incomplete package
               return true;
            }
            if (!processRequestLine(
                  {buffer->peek(), static_cast<size_t>(crlf - buffer->peek())}))
            {
               // parse request line error
               return false;
            }
            // parse ok
            buffer->retrieveUntil(crlf + 2);
            state_ = kExpectHeaders;
            break;
         }

         case kExpectHeaders: {
            crlf = buffer->findCRLF();
            if (crlf == nullptr)
            {
               // Incomplete package
               return true;
            }
            // header parsing finished
            if (buffer->readableBytes() == 2)
            {
               // FIXME This is the last CRLF that indicates  the request header
               // parsing is finished, so we  need switch state.I don't switch
               // to body parsing state,it depends on some header field like
               // Content-Length.
               buffer->retrieve(2);
               state_ = kGotAll;
               return true;
            }

            // parsing header
            auto *sep = std::find(buffer->peek(), crlf, ':');
            if (sep == crlf)
            {
               // parsing error
               return false;
            }
            request_.addHeader(
              {buffer->peek(), static_cast<size_t>(sep - buffer->peek())},
              {sep + 1, static_cast<size_t>(crlf - (sep + 1))});
            buffer->retrieveUntil(crlf + 2);
            break;
         }
         case kExpectBody: {
            // TODO The length of the body is confirmed by either
            // `Content-Length` or `Transfer-Encoding: chunked`.

            break;
         }
         case kGotAll: {
            return true;
         }
      }
   }
}
