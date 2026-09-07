#ifndef SMART_HOME_COMMON_FRAME_QUEUE_H
#define SMART_HOME_COMMON_FRAME_QUEUE_H

// ============================================================================
// frame_queue.h —— 解码帧队列（角色 C 维护，Qt 客户端"解码 -> 显示"用）
//
// 和 RingBuffer 的区别（重点）：
//   - RingBuffer：满了【阻塞等待】（可靠性优先，丢包会花屏）；
//   - FrameQueue：满了【丢弃最老的一帧】（实时性优先，预览要最新画面）。
//
// 典型用法：解码线程 push(帧)，显示线程 pop(帧, 超时) 取出渲染。
// 实际项目中 T 通常是 std::shared_ptr<AVFrame>（避免深拷贝昂贵的帧）。
// ============================================================================

#include <chrono>              // std::chrono::milliseconds
#include <condition_variable>  // std::condition_variable
#include <cstddef>             // size_t
#include <deque>               // std::deque：队头丢老帧、队尾插新帧
#include <mutex>               // std::mutex / std::unique_lock / std::lock_guard

namespace smart_home {
namespace common {

template <typename T>
class FrameQueue {
public:
    // capacity：队列容量（最多缓存多少帧）
    explicit FrameQueue(size_t capacity)
        : _capacity(capacity), _closed(false) {}

    // ---- 入队：满了就丢弃最老的一帧（队头），再插入新帧（队尾）----
    bool push(const T &frame) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_closed) {
            return false;      // 已关闭，不再接收
        }
        if (_queue.size() >= _capacity) {
            _queue.pop_front();   // 丢弃最老的一帧，腾出空位
        }
        _queue.push_back(frame);  // 新帧入队尾
        _notEmpty.notify_one();   // 叫醒等待帧的显示线程
        return true;
    }

    // ---- 阻塞出队（取最老的一帧）----
    bool pop(T &out) {
        std::unique_lock<std::mutex> lock(_mutex);
        _notEmpty.wait(lock, [this] { return _closed || !_queue.empty(); });
        if (_queue.empty()) {
            return false;      // close() 且已空
        }
        out = _queue.front();
        _queue.pop_front();
        return true;
    }

    // ---- 带超时出队：显示线程常用它"定时刷新"，超时没帧就渲染上一帧 ----
    bool pop(T &out, int timeoutMs) {
        std::unique_lock<std::mutex> lock(_mutex);
        bool got = _notEmpty.wait_for(lock,
                                      std::chrono::milliseconds(timeoutMs),
                                      [this] { return _closed || !_queue.empty(); });
        if (!got) {
            return false;      // 超时没帧
        }
        if (_queue.empty()) {
            return false;      // close() 且已空
        }
        out = _queue.front();
        _queue.pop_front();
        return true;
    }

    // ---- 非阻塞出队 ----
    bool tryPop(T &out) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_queue.empty()) {
            return false;
        }
        out = _queue.front();
        _queue.pop_front();
        return true;
    }

    // ---- 状态查询 ----
    size_t size() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _queue.size();
    }

    size_t capacity() const { return _capacity; }

    bool empty() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _queue.empty();
    }

    bool isClosed() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _closed;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(_mutex);
        _queue.clear();
    }

    // ---- 关闭：叫醒所有阻塞在 pop 上的线程 ----
    void close() {
        std::lock_guard<std::mutex> lock(_mutex);
        _closed = true;
        _notEmpty.notify_all();
    }

private:
    std::deque<T> _queue;           // 底层双端队列
    size_t _capacity;               // 容量上限
    bool _closed;                   // 是否已关闭
    mutable std::mutex _mutex;      // 互斥锁
    std::condition_variable _notEmpty;  // "非空"条件：显示线程等它
};

}  // namespace common
}  // namespace smart_home

#endif  // SMART_HOME_COMMON_FRAME_QUEUE_H