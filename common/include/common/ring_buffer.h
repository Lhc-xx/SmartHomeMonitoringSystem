#ifndef SMART_HOME_COMMON_RING_BUFFER_H
#define SMART_HOME_COMMON_RING_BUFFER_H

// ============================================================================
// ring_buffer.h —— 线程安全的环形缓冲（角色 C 维护，服务端"拉流 -> 发送"用）
//
// 生产者/消费者模型：拉流线程 push，发送线程 pop。
// 线程安全：所有方法内部用同一把互斥锁保护，配合两个条件变量：
//   _notEmpty —— 有数据了，通知消费者；
//   _notFull  —— 有空位了，通知生产者。
// ============================================================================

#include <chrono>              // std::chrono::milliseconds：给"带超时的 pop"用
#include <condition_variable>  // std::condition_variable
#include <cstddef>             // size_t
#include <mutex>               // std::mutex / std::unique_lock / std::lock_guard
#include <vector>              // std::vector：底层存储

namespace smart_home {
namespace common {

template <typename T>
class RingBuffer {
public:
    // capacity：容量（最多存几个元素），必须 >= 1
    explicit RingBuffer(size_t capacity)
        : _capacity(capacity),
          _buffer(capacity),   // 预先分配固定大小，之后不再扩容
          _head(0),            // 队头下标（下一个要读的位置）
          _tail(0),            // 队尾下标（下一个要写的位置）
          _size(0),            // 当前元素个数
          _closed(false) {}    // 是否已关闭

    // ---- 阻塞入队：满了就等，直到有空位或 close() ----
    bool push(const T &item) {
        std::unique_lock<std::mutex> lock(_mutex);
        // wait 的第二个参数是"谓词"：为 false 才继续等。这里"满且未关闭"时睡觉。
        _notFull.wait(lock, [this] { return _closed || _size < _capacity; });

        if (_closed) {
            return false;      // 已关闭，不再接收
        }

        _buffer[_tail] = item;                 // 写到 tail 位置
        _tail = (_tail + 1) % _capacity;       // tail 前进，到末尾绕回 0
        ++_size;
        _notEmpty.notify_one();                // 叫醒一个等数据的消费者
        return true;
    }

    // ---- 非阻塞入队：满或已关闭立即返回 false，不等待 ----
    bool tryPush(const T &item) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_closed || _size >= _capacity) {
            return false;
        }
        _buffer[_tail] = item;
        _tail = (_tail + 1) % _capacity;
        ++_size;
        _notEmpty.notify_one();
        return true;
    }

    // ---- 阻塞出队：空了就等，直到有数据或 close() ----
    bool pop(T &out) {
        std::unique_lock<std::mutex> lock(_mutex);
        _notEmpty.wait(lock, [this] { return _closed || _size > 0; });

        if (_size == 0) {
            return false;      // close() 触发且已取空
        }

        out = _buffer[_head];                   // 取出队头
        _head = (_head + 1) % _capacity;
        --_size;
        _notFull.notify_one();                  // 腾出空位，叫醒一个生产者
        return true;
    }

    // ---- 带超时出队：最多等 timeoutMs 毫秒。发送线程用它"周期醒来检查停止标志" ----
    bool pop(T &out, int timeoutMs) {
        std::unique_lock<std::mutex> lock(_mutex);
        bool got = _notEmpty.wait_for(lock,
                                      std::chrono::milliseconds(timeoutMs),
                                      [this] { return _closed || _size > 0; });
        if (!got) {
            return false;      // 超时没数据
        }
        if (_size == 0) {
            return false;      // close() 且已空
        }
        out = _buffer[_head];
        _head = (_head + 1) % _capacity;
        --_size;
        _notFull.notify_one();
        return true;
    }

    // ---- 非阻塞出队：空立即返回 false ----
    bool tryPop(T &out) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_size == 0) {
            return false;
        }
        out = _buffer[_head];
        _head = (_head + 1) % _capacity;
        --_size;
        _notFull.notify_one();
        return true;
    }

    // ---- 状态查询（const 方法也要加锁，所以 _mutex 声明为 mutable）----
    size_t size() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _size;
    }

    size_t capacity() const { return _capacity; }   // 容量固定，无需加锁

    bool empty() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _size == 0;
    }

    bool full() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _size >= _capacity;
    }

    bool isClosed() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _closed;
    }

    // ---- 清空（丢弃所有未消费元素），叫醒等待空位的生产者 ----
    void clear() {
        std::lock_guard<std::mutex> lock(_mutex);
        _head = _tail = _size = 0;
        _notFull.notify_all();
    }

    // ---- 关闭：用于停机。叫醒所有阻塞的 push/pop 线程，让它们安全退出 ----
    void close() {
        std::lock_guard<std::mutex> lock(_mutex);
        _closed = true;
        _notEmpty.notify_all();
        _notFull.notify_all();
    }

private:
    size_t _capacity;               // 容量（固定）
    std::vector<T> _buffer;         // 底层数组，大小 = capacity
    size_t _head;                   // 队头下标（读指针）
    size_t _tail;                   // 队尾下标（写指针）
    size_t _size;                   // 当前元素个数
    bool _closed;                   // 是否已关闭
    mutable std::mutex _mutex;      // 互斥锁；mutable 让 const 方法也能加锁
    std::condition_variable _notEmpty;  // "非空"条件：消费者等它
    std::condition_variable _notFull;   // "非满"条件：生产者等它
};

}  // namespace common
}  // namespace smart_home

#endif  // SMART_HOME_COMMON_RING_BUFFER_H