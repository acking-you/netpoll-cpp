#pragma once
#include <memory>

namespace netpoll {
class EventLoopWrap;
namespace tcp {
class Listener;
using ListenerPtr     = std::shared_ptr<Listener>;
using ListenerWeakPtr = std::weak_ptr<Listener>;
class Dialer;
using DialerPtr      = std::shared_ptr<Dialer>;
using DialerrWeakPtr = std::weak_ptr<Dialer>;
}   // namespace tcp
}   // namespace netpoll