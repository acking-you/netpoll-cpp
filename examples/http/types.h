#pragma once

namespace http {

enum class Version { kUnknown, kHttp10, kHttp11 };
enum class Method { kInvalid, kGET, kPOST, kHEAD, kPUT, kDELETE };
enum class StatusCode {
   kUnknown,
   k200Ok               = 200,
   k301MovedPermanently = 301,
   k400BadRequest       = 400,
   k404NotFound         = 404,
};
enum class ConnectionState { kClose, kKeepAlive };

}   // namespace http