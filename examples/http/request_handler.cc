#include "request_handler.h"

#include <fmt/core.h>

#include "http_request.h"
#include "http_response.h"
#include "inner/filesystem.hpp"

using namespace ghc::filesystem;

void RequestHandler(const http::Request &req, http::Response &rsp)
{
   auto const &filename = req.query();
   if (filename.empty())
   {
      ResponseFilesLink(rsp);
      return;
   }
   ResponseFile(filename, rsp);
}

std::vector<std::string> getAllFilenamesByBaseDir(std::string const &targetDir)
{
   std::vector<std::string> ret;
   for (const auto &entry : directory_iterator(targetDir))
   {
      if (!is_directory(entry))
      {
         ret.push_back(entry.path().filename().string());
      }
   }
   return ret;
}

void ResponseFilesLink(http::Response &rsp)
{
   std::string links;
   for (auto &&filename : getAllFilenamesByBaseDir(BASE_FILES_PATH))
   {
      links.append(
        fmt::format(R"(<li><a href="/?{}">{}</a></li>)", filename, filename));
   }
   rsp.body() = fmt::format(
     R"(<html><meta http-equiv="Content-Type" content="text/html;charset=utf-8"/><ul>{}</ul></html>)",
     links);
   rsp.responseOk();
}

void ResponseFile(const std::string &filename, http::Response &rsp)
{
   auto filePath = path(BASE_FILES_PATH);
   filePath /= filename;
   if (!exists(filePath))
   {
      rsp.body() = R"(
<html>
<meta http-equiv="Content-Type" content="text/html;charset=utf-8"/>
<head>
<title>HTTP 400 错误：请求无效</title>
</head>
<body>
<h1>HTTP 400 错误：请求无效</h1>
<p>很抱歉，您的请求无效。请检查您的请求是否正确。</p>
</body>
</html>)";
      rsp.responseBad();
      return;
   }
   rsp.fileinfo() = http::fileinfo{
     file_size(filePath),
     filePath.string(),
   };
   rsp.setContentType("application/octet-stream");
   rsp.headers()["Content-Disposition"] =
     fmt::format(R"(attachment; filename="{}")", filename);
   rsp.responseOk();
}
