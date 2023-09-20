#ifndef EVENT_LOOP_THREAD_POLL_H
#define EVENT_LOOP_THREAD_POLL_H

#include <functional>
#include <string>
#include <vector>
#include <memory>

#include "noncopyable.h"

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) { numThreads_ = numThreads; }

    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    // 如果工作在多线程中，mainLoop 会默认以轮询的方式分配Channel给subLoop
    EventLoop *getNextLoop();

    std::vector<EventLoop *> getAllLoops();

    bool started() const { return started_; }
    const std::string name() const { return name_; }

private:
    EventLoop *baseLoop_;   // 主线程（主Reactor）对应的EventLoop
    std::string name_;
    bool started_;          // 是否已经开启线程池
    int numThreads_;
    int next_;              // 轮询的下标
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop *> loops_;
};

#endif