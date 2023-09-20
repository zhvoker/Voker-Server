#ifndef EVENT_LOOP_THREAD_POLL_H
#define EVENT_LOOP_THREAD_POLL_H

#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>

#include "noncopyable.h"
#include "Thread.h"

class EventLoop;

class EventLoopThread : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                    const std::string &name = std::string());
    ~EventLoopThread();

    EventLoop *startLoop();

private:
    void threadFunc();

    EventLoop *loop_;               // subloop
    bool exiting_;
    Thread thread_;
    std::mutex mutex_;              // 互斥锁, 配合条件变量使用
    std::condition_variable cond_;  // 条件变量
    ThreadInitCallback callback_;   // 线程初始化完成后要执行的函数
};

#endif