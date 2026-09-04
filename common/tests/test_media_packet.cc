// test_media_packet.cc —— 第二步：验证 MediaPacket 结构体的字段和辅助函数

#include <iostream>
#include "protocol/media_packet.h"   // 我们要测的头文件

using smart_home::protocol::MediaPacket;
using smart_home::protocol::kMediaFlagKeyFrame;

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
    std::cout << "=== MediaPacket test begin ===" << std::endl;

    // ---- 1) 默认构造：所有字段应初始化为 0 ----
    MediaPacket p;
    expect(p.streamIndex == 0, "default streamIndex is 0");
    expect(p.pts == 0 && p.dts == 0, "default pts/dts are 0");
    expect(p.duration == 0, "default duration is 0");
    expect(p.flags == 0, "default flags is 0");
    expect(p.empty(), "default packet has empty data");

    // ---- 2) 赋值：字段能正确设置 ----
    p.streamIndex = 0;
    p.pts = 12345;
    p.dts = 12344;
    p.duration = 40;
    p.flags = kMediaFlagKeyFrame;      // 标记为关键帧
    p.data = {0x00, 0x00, 0x00, 0x01}; // 模拟一小段 H.264 数据（4 字节）

    expect(p.pts == 12345, "pts is set correctly");
    expect(p.dts == 12344, "dts is set correctly");
    expect(p.duration == 40, "duration is set correctly");
    expect(p.data.size() == 4, "data size is 4");
    expect(!p.empty(), "packet is not empty after data assigned");

    // ---- 3) isKeyFrame() 标志位判断 ----
    expect(p.isKeyFrame(), "isKeyFrame() returns true when flag set");

    p.flags = 0;                        // 清掉关键帧标志
    expect(!p.isKeyFrame(), "isKeyFrame() returns false when flag cleared");

    // ---- 4) clear() 复位 ----
    p.clear();
    expect(p.empty() && p.streamIndex == 0 && p.pts == 0,
           "clear() resets all fields");

    // ---- 汇总 ----
    std::cout << "=== MediaPacket test end ===" << std::endl;
    if (g_failures == 0) {
        std::cout << "media_packet test passed." << std::endl;
        return 0;
    }
    std::cout << "media_packet test FAILED: " << g_failures << " items." << std::endl;
    return 1;
}