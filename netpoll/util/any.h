#pragma once

// use std::any if cpp >= 17
#if __cplusplus >= 201703L
#include <any>
namespace netpoll {
using Any = std::any;
using std::any_cast;
}   // namespace netpoll
#else
#include <elog/any.h>
namespace netpoll {
using Any = elog::Any_t;
using elog::any_cast;
}   // namespace netpoll
#endif
