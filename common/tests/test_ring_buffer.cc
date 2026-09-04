//环形缓冲（含多线程生产者/消费者）

#include <atomic> //std::atomic
#include <chrono>
#include <iostream>
#include <thread> //std::thread

#include "common/ring_buffer.h"

using smart_home::common::RingBuffer;

static int g_failures = 0;
static void expect(bool cond,const char *msg) {
    if (cond) {
        std::cout << " [ok] " << msg << std::endl;
    } else {
        std::cout << " [FAIL] " << msg << std::endl;
        ++g_failures;
    }
}

int main() {
    std::cout << "==== RingBuffer test begin ====" << std::endl;

    // 1) 单线程基本读写 + 满/空判断
    {
        RingBuffer<int> buf(4);
        expect(buf.push(1) && buf.push(2) && buf.push(3) && buf.push(4),
                "push 4 items into capacity 4");
        expect(buf.size() == 4,"size == 4");
        expect(buf.full(),"full() == true");
        expect(!buf.tryPush(99),"tryPush fail when full");

        int v = 0;
        expect(buf.pop(v) && v ==1,"pop 1st = 1 (FIFO)");
        expect(buf.pop(v) && v ==2,"pop 1st = 2(FIFO)");
        expect(buf.pop(v) && v ==3,"pop 1st = 3 (FIFO)");
        expect(buf.pop(v) && v ==4,"pop 1st = 4 (FIFO)");
        expect(buf.empty(),"empty() == true after drain");
        expect(!buf.tryPop(v),"tryPop fails when empty");
    }

    // 2) 带超时 pop：空缓冲应超时返回 false
    {
        RingBuffer<int> buf(2);
        int v = 0;
        auto start = std::chrono::steady_clock::now();
        bool got = buf.pop(v, 100);
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        expect(!got, "pop(100ms) on empty returns false");
        expect(ms >= 90 && ms < 1000, "timeout waits ~100ms (not instant, not stuck)");
    }

    // 3) close() 唤醒阻塞中的 pop，且之后 push 返回 false
    {
        RingBuffer<int> buf(4);
        std::atomic<bool> popWoke(false);
        std::thread waiter([&buf, &popWoke]() {
            int v = 0;
            buf.pop(v);           // 空缓冲，会一直阻塞
            popWoke = true;       // 走到这里说明被 close() 唤醒
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        buf.close();
        waiter.join();
        expect(popWoke.load(), "close() wakes blocked pop");
        expect(!buf.push(123), "push after close returns false");
    }

    // 4) 多线程生产者/消费者（核心：线程安全 + FIFO 顺序）
    {
        const int kTotal = 1000;
        RingBuffer<int> buf(8);           // 容量故意设小，制造频繁阻塞
        std::atomic<int> received(0);
        std::atomic<bool> orderOk(true);

        std::thread producer([&buf]() {
            for (int i = 0; i < kTotal; ++i) {
                buf.push(i);              // 满时会阻塞
            }
        });
        std::thread consumer([&buf, &received, &orderOk]() {
            for (int i = 0; i < kTotal; ++i) {
                int v = 0;
                buf.pop(v);               // 空时会阻塞
                if (v != i) orderOk = false;
                received++;
            }
        });

        producer.join();
        consumer.join();
        expect(received.load() == kTotal, "consumer receives all 1000 items");
        expect(orderOk.load(), "1000 items strictly FIFO");
    }

    std::cout << "=== RingBuffer test end ===" << std::endl;
    if (g_failures == 0) {
        std::cout << "ring_buffer test passed." << std::endl;
        return 0;
    }
    std::cout << "ring_buffer test FAILED: " << g_failures << " items." << std::endl;
    return 1;
}