// 帧队列（满时丢最老）

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include "common/frame_queue.h"

using smart_home::common::FrameQueue;

static int g_failures = 0;
static void expect(bool cond, const char *msg) {
    if (cond) {
        std::cout << "  [ok]   " << msg << std::endl;
    } else {
        std::cout << "  [FAIL] " << msg << std::endl;
        ++g_failures;
    }
}

int main() {
    std::cout << "=== FrameQueue test begin ===" << std::endl;

    // 1) 基本 FIFO
    {
        FrameQueue<int> q(4);
        q.push(1); q.push(2); q.push(3);
        expect(q.size() == 3, "size == 3 after 3 pushes");

        int v = 0;
        expect(q.pop(v) && v == 1, "pop 1st = 1 (FIFO)");
        expect(q.pop(v) && v == 2, "pop 2nd = 2 (FIFO)");
        expect(q.pop(v) && v == 3, "pop 3rd = 3 (FIFO)");
        expect(q.empty(), "empty after drain");
    }

    // 2) 满时丢弃最老帧（核心行为，区别于 RingBuffer 的阻塞）
    {
        FrameQueue<int> q(3);
        q.push(1); q.push(2); q.push(3);   // 满了 [1,2,3]
        expect(q.size() == 3, "full at capacity 3");

        q.push(4);                          // 满了 -> 丢最老的 1，变 [2,3,4]
        expect(q.size() == 3, "size still 3 after push when full");

        int v = 0;
        expect(q.pop(v) && v == 2, "oldest popped is 2 (1 was dropped)");
        expect(q.pop(v) && v == 3, "then 3");
        expect(q.pop(v) && v == 4, "newest 4 kept");
        expect(q.empty(), "empty after drain");
    }

    // 3) 空队列超时 pop
    {
        FrameQueue<int> q(2);
        int v = 0;
        auto start = std::chrono::steady_clock::now();
        bool got = q.pop(v, 100);
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        expect(!got, "pop(100ms) on empty returns false");
        expect(ms >= 90 && ms < 1000, "timeout waits ~100ms");
    }

    // 4) close() 唤醒阻塞中的 pop
    {
        FrameQueue<int> q(4);
        std::atomic<bool> woke(false);
        std::thread waiter([&q, &woke]() {
            int v = 0;
            q.pop(v);
            woke = true;
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        q.close();
        waiter.join();
        expect(woke.load(), "close() wakes blocked pop");
        expect(!q.push(123), "push after close returns false");
    }

    // 5) 多线程：生产者快、消费者慢，必然大量丢帧
    {
        const int kTotal = 1000;
        FrameQueue<int> q(8);
        std::atomic<bool> gotLast(false);
        std::atomic<bool> orderOk(true);
        std::atomic<bool> overCap(false);

        std::thread producer([&q, &overCap]() {
            for (int i = 0; i < kTotal; ++i) {
                q.push(i);
                if (q.size() > 8) overCap = true;
            }
        });
        std::thread consumer([&q, &gotLast, &orderOk]() {
            int prev = -1;
            while (!gotLast.load()) {
                int v = 0;
                if (!q.pop(v, 5000)) break;   // 5s 超时兜底
                if (v <= prev) orderOk = false;
                prev = v;
                if (v == kTotal - 1) gotLast = true;
            }
        });

        producer.join();
        consumer.join();
        expect(!overCap.load(), "size never exceeds capacity");
        expect(gotLast.load(), "consumer eventually gets newest frame (999)");
        expect(orderOk.load(), "popped sequence strictly increasing (only oldest dropped)");
    }

    std::cout << "=== FrameQueue test end ===" << std::endl;
    if (g_failures == 0) {
        std::cout << "frame_queue test passed." << std::endl;
        return 0;
    }
    std::cout << "frame_queue test FAILED: " << g_failures << " items." << std::endl;
    return 1;
}