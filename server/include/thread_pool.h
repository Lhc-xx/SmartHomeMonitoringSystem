#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <cstddef>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>


namespace smart_home{
    using Task = std::function<void()>; // 存放任务

    class ThreadPool{
    public:
        ThreadPool(size_t thread_num, size_t capacity);
        ~ThreadPool();
        bool addTask(const Task &task);
        void stop();

    private:
        void loop();

    private:
        std::vector<std::thread> _threads; // 存放线程列表
        std::queue<Task> _taskQueue; // 存放任务
        std::mutex _mtx; // 互斥锁
        std::condition_variable _cv; // 条件变量
        size_t _capacity; // 队列容量
        bool _stopFlag; // 停止标志
    };
}
#endif //  THREADPOOL_H