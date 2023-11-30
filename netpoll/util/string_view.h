#pragma once

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
