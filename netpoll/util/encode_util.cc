#include "encode_util.h"
#ifdef _WIN32
#include <Windows.h>

#include <algorithm>
#else   // _WIN32
#if __cplusplus < 201103L || __cplusplus >= 201703L
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#else   // __cplusplus
#include <codecvt>
#include <locale>
#endif   // __cplusplus
#endif   // _WIN32

using namespace netpoll;

std::string utils::toUtf8(const std::wstring &wstr)
{
   if (wstr.empty()) return {};

   std::string strTo;
#ifdef _WIN32
   int nSizeNeeded = ::WideCharToMultiByte(
     CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
   strTo.resize(nSizeNeeded, 0);
   ::WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0],
                         nSizeNeeded, NULL, NULL);
#else    // _WIN32
#if __cplusplus < 201103L || __cplusplus >= 201703L
   // Note: Introduced in c++11 and deprecated with c++17.
   // Revert to C99 code since there no replacement yet
   strTo.resize(3 * wstr.length(), 0);
   auto nLen = std::wcstombs(&strTo[0], wstr.c_str(), strTo.length());
   strTo.resize(nLen);
#else    // c++11 to c++14
   std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> utf8conv;
   strTo = utf8conv.to_bytes(wstr);
#endif   // __cplusplus
#endif   // _WIN32
   return strTo;
}
std::wstring utils::fromUtf8(const std::string &str)
{
   if (str.empty()) return {};
   std::wstring wstrTo;
#ifdef _WIN32
   int nSizeNeeded =
     ::MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
   wstrTo.resize(nSizeNeeded, 0);
   ::MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0],
                         nSizeNeeded);
#else    // _WIN32
#if __cplusplus < 201103L || __cplusplus >= 201703L
   // Note: Introduced in c++11 and deprecated with c++17.
   // Revert to C99 code since there no replacement yet
   wstrTo.resize(str.length(), 0);
   auto nLen = std::mbstowcs(&wstrTo[0], str.c_str(), wstrTo.length());
   wstrTo.resize(nLen);
#else    // c++11 to c++14
   std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> utf8conv;
   try
   {
      wstrTo = utf8conv.from_bytes(str);
   }
   catch (...)   // Should never fail if str valid UTF-8
   {
   }
#endif   // __cplusplus
#endif   // _WIN32
   return wstrTo;
}

std::wstring utils::toWidePath(const std::string &strUtf8Path)
{
   auto wstrPath{fromUtf8(strUtf8Path)};
#ifdef _WIN32
   // Not needed: normalize path (just replaces '/' with '\')
   std::replace(wstrPath.begin(), wstrPath.end(), L'/', L'\\');
#endif   // _WIN32
   return wstrPath;
}

std::string utils::fromWidePath(const std::wstring &wstrPath)
{
#ifdef _WIN32
   auto srcPath{wstrPath};
   // Not needed: to portable path (just replaces '\' with '/')
   std::replace(srcPath.begin(), srcPath.end(), L'\\', L'/');
#else    // _WIN32
   auto &srcPath{wstrPath};
#endif   // _WIN32
   return toUtf8(srcPath);
}