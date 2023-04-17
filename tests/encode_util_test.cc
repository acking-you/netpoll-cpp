#include <doctest/doctest.h>
#include <netpoll/util/encode_util.h>

#include <string>

using namespace netpoll;
TEST_SUITE_BEGIN("encode_util");
TEST_CASE("test fromUtf8&toUtf8")
{
   const char*    u8chars = u8"你好世界";
   const wchar_t* wchars  = L"你好世界";
   SUBCASE("toUtf8") { CHECK_EQ(u8chars, netpoll::utils::toUtf8(wchars)); }
   SUBCASE("fromUtf8") { CHECK_EQ(wchars, netpoll::utils::fromUtf8(u8chars)); }
}

TEST_CASE("test fromNativePath&toNativePath")
{
   const char*    windowsNetPath       = u8"C:/学习资料";
   const wchar_t* windowsNativePath    = L"C:\\学习资料";
   const char*    linuxNetPath         = u8"home/学习资料";
   const wchar_t* linuxNativeWcharPath = L"home/学习资料";
#ifdef _WIN32
   SUBCASE("fromNativePath")
   {
      CHECK_EQ(windowsNetPath,
               netpoll::utils::fromNativePath(windowsNativePath));
   }
   // windows中文环境本地编码默认为UCS2
   SUBCASE("toNativePath")
   {
      CHECK_EQ(windowsNativePath, netpoll::utils::toNativePath(windowsNetPath));
   }
#else
   SUBCASE("fromNativePath")
   {
      CHECK_EQ(linuxNetPath,
               netpoll::utils::fromNativePath(linuxNativeWcharPath));
   }
   // 因为Linux本地默认编码就是utf8，除非谁脑子抽了用wchar
   SUBCASE("toNativePath")
   {
      CHECK_EQ(utils::toUtf8(linuxNativeWcharPath),
               netpoll::utils::toNativePath(linuxNetPath));
   }
#endif
}
TEST_SUITE_END;