#pragma once

// 若cpp版本为17以上则直接使用标准库的string_view，否则使用第三方库的
#if __cplusplus >= 201703L || (_MSC_VER && _MSVC_LANG >= 201703L)
#include <string_view>
namespace netpoll {
using StringView = std::string_view;
}
#else
#include <elog/string_view.h>
namespace netpoll {
using StringView = elog::StringView;
}
#endif
