#include "thread_pool.h"
#include <cstddef>
#include <mutex>


namespace smart_home{
    // 构造函数
    ThreadPool::ThreadPool(size_t thread_num, size_t capacity)
    : _capacity(capacity)
    , _stopFlag(false)
    {
        for(size_t i = 0; i < thread_num; ++i){
            _threads.emplace_back(&ThreadPool::loop, this);
        }
    }

    ThreadPool::~ThreadPool(){
        stop();
    }

    void ThreadPool::loop(){
        while(true){
            Task task;
            {
                // 1.上锁
                std::unique_lock<std::mutex> lock(_mtx);
                // 2.条件判断 谓词为真 放行
                // 3.停止或任务队列不为空时 取出任务
                _cv.wait(lock, [this](){
                    return _stopFlag || !_taskQueue.empty();
                });
                if(_stopFlag && _taskQueue.empty()){
                    return;
                }
                // 4.从队列取出任务
                task = std::move(_taskQueue.front());
                _taskQueue.pop();
            }  
            // 5.锁外  执行任务
            task(); 
        }
    }

    // 把任务塞进队列
    bool ThreadPool::addTask(const Task &task){
        {
            // 1.上锁
            std::unique_lock<std::mutex> lock(_mtx);
            // 2.退出条件
            if(_stopFlag || _taskQueue.size() == _capacity){
                return false;
            }
            // 3.放入队列
            _taskQueue.push(task);
        } // 4.解锁
        // 5.唤醒
        _cv.notify_one();
        return true;
    }

    void ThreadPool::stop(){
        {
            // 1.加锁
            std::lock_guard<std::mutex> lock(_mtx);
            if(_stopFlag) {
                return;
            }
            _stopFlag = true;
        }
        // 2.唤醒所有等待线程
        _cv.notify_all();
        // 3.遍历_threads  等待每个线程干完退出
        for(auto& th: _threads){
            if(th.joinable()){ // true 表示可以被join
                th.join();
            }
        }

        // 4.清空线程对象列表（此时它们都已结束）
        _threads.clear();
    }
}