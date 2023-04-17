## netpoll-cpp

这是一个兼顾性能和易用性的 `C++ NIO` 网络库,采用与 `muduo` 网络库类似的 `one loop per thread` 模型进行封装.

这是我目前已经完成的核心特性:

* 垮全平台 `windows/linux/macos` 各个平台分别用该平台最高性能的多路复用实现(IOCP实现的epoll/epoll/kqueue).
* 性能极高,暂时测试了[asio](https://www.boost.org/doc/libs/1_82_0/doc/html/boost_asio.html)(
  C++)/[netty](https://netty.io/wiki/index.html)(
  java)/[netpoll](https://www.cloudwego.io/zh/docs/netpoll/)(go语言)
  在作为echo服务器的表现上延迟达到最低.
* 支持一键文件发送,如果所在操作系统支持 `sendfile` 调用,则会调用该零拷贝调用而不是调用C标准库.
* 易用性超高,比如想要写一个 `echo` 服务器,只需要下面的代码:
  ```cpp
  struct server 
  {
    NETPOLL_TCP_MESSAGE(conn, buffer){conn->send(buffer->readAll());}
  };
  int main()
  {
    auto loop = netpoll::NewEventLoop();
    auto listener = netpoll::tcp::Listener::New({6666});
    listener->bind<server>();
    loop.serve(listener);
  }
  ```
* 两个层级的定时器,
  底层使用优先队列,支持最高和最低优先级的任务调度,上层使用时间轮,这是一个优化了内存的支持高延时的高性能时间轮(
  比如定时 `100^8s`,可能只需要`100*8byte`)
  具体可以查看博客:[详细介绍](https://acking-you.github.io/posts/%E5%AE%9E%E7%8E%B0%E9%AB%98%E6%80%A7%E8%83%BD%E6%97%B6%E9%97%B4%E8%BD%AE%E7%94%A8%E4%BA%8E%E8%B8%A2%E5%87%BA%E7%A9%BA%E9%97%B2%E8%BF%9E%E6%8E%A5/).

* 高性能的MPSC队列,正好和 `one loop per thread` 模型相贴近.[lockfree_queue](./netpoll/util/lockfree_queue.h)
* 支持作为守护线程的启动方式(serveAsDaemon),它将返回一个 `future` 用于同步,这在客户端编程中可能很有用.
* 支持优雅的捕获相关信号做优雅退出处理,比如下列代码:
  ```cpp
  netpoll::SignalTask::Register({SIGINT,SIGTERM});
  netpoll::SignalTask::Add([](){
  //do something	
  });
  ```
* 日志库使用 [elog4cpp](https://github.com/ACking-you/elog4cpp) 性能和易用性都极高.
* 依赖轻量(使用[CPM](https://github.com/iauns/cpm)管理依赖),支持通过cmake命令一键导入使用.

将来会支持的特性:

* 增加UDP的封装.
* 增加unix domain socket 的封装.
* 引入OpenSSL支持权限认证.

后续打算基于这个网络库封装一个http框架,类似于go语言的 [gin](https://github.com/gin-gonic/gin)

### 一键导入使用

使用如下cmake代码进行使用:
(如果网络问题,可以尝试换仓库链接:https://gitee.com/acking-you/netpoll-cpp.git)

```cmake
include(FetchContent)
FetchContent_Declare(
        netpoll-cpp
        GIT_REPOSITORY https://github.com/ACking-you/netpoll-cpp.git
        GIT_TAG origin/master
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(netpoll-cpp)

target_link_libraries(target_name netpoll)
```

可以试着跑以下echo服务demo:

```cpp
#include <netpoll/core.h>

struct server {
	NETPOLL_TCP_MESSAGE(conn, buffer)
	{
		conn->send(buffer->readAll());
	}
};
int main()
{
	auto loop = netpoll::NewEventLoop();
	auto listener = netpoll::tcp::Listener::New({6666});
	listener->bind<server>();
	loop.serve(listener);
}
```

具体文档暂时搁置,需要等我对整个网络库进行全面的梳理和测试再进行详细文档编写.