#pragma once
#include <string>

namespace http {
class Response;
class Request;
}   // namespace http

void RequestHandler(const http::Request &, http::Response &);

void ResponseFilesLink(http::Response &);

void ResponseFile(std::string const &, http::Response &);