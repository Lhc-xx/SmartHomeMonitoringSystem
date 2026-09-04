// test_media_packet.cc —— 第二步：验证 MediaPacket 结构体的字段和辅助函数

#include <iostream>
#include "protocol/media_packet.h"   // 我们要测的头文件

using smart_home::protocol::MediaPacket;
using smart_home::protocol::kMediaFlagKeyFrame;
using smart_home::protocol::MediaPacketSerializer;

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


        // ---- 5) 序列化/反序列化：结构体 <-> 字节 ----
    {
        // 造一个带已知字段的包（负载 5 字节）
        MediaPacket src;
        src.streamIndex = 0;
        src.pts = 123456;
        src.dts = 123455;
        src.duration = 40;
        src.flags = kMediaFlagKeyFrame;
        src.data = {0x00, 0x00, 0x00, 0x01, 0x65};

        std::vector<uint8_t> buf;
        bool ok = MediaPacketSerializer::encode(src, buf);
        expect(ok, "encode() succeeds");
        expect(buf.size() == 38 + src.data.size(), "frame size = 38 overhead + payload");

        MediaPacket dst;
        size_t used = MediaPacketSerializer::decode(buf.data(), buf.size(), dst);
        expect(used == buf.size(), "decode() consumes the whole frame");

        // 逐字段比对：解码出来的必须和原来一致
        expect(dst.streamIndex == src.streamIndex, "streamIndex round-trips");
        expect(dst.pts == src.pts, "pts round-trips");
        expect(dst.dts == src.dts, "dts round-trips");
        expect(dst.duration == src.duration, "duration round-trips");
        expect(dst.flags == src.flags, "flags round-trips");
        expect(dst.data == src.data, "payload round-trips");

        // 字节级校验：帧长 = 38+5 = 43 = 0x2B，大端序应为 00 00 00 2B
        expect(buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0x00 && buf[3] == 0x2B,
               "frame length is big-endian 00 00 00 2B");
        // 魔数 "MPKT" = 4D 50 4B 54，正好是 M P K T 四个字符
        expect(buf[4] == 'M' && buf[5] == 'P' && buf[6] == 'K' && buf[7] == 'T',
               "magic bytes are M P K T");
    }

    // ---- 6) 异常处理：坏魔数 / 截断帧应安全失败，不能崩溃 ----
    {
        MediaPacket p;
        p.data = {1, 2, 3};   // 负载 3 字节
        std::vector<uint8_t> buf;
        MediaPacketSerializer::encode(p, buf);

        MediaPacket out;
        // 破坏魔数第一个字节 -> decode 应返回 0
        std::vector<uint8_t> bad = buf;
        bad[4] = 0xFF;
        expect(MediaPacketSerializer::decode(bad.data(), bad.size(), out) == 0,
               "bad magic -> decode returns 0");
        // 只给半帧 -> decode 应返回 0
        expect(MediaPacketSerializer::decode(buf.data(), buf.size() / 2, out) == 0,
               "truncated frame -> decode returns 0");
    }

    // ---- 汇总 ----
    std::cout << "=== MediaPacket test end ===" << std::endl;
    if (g_failures == 0) {
        std::cout << "media_packet test passed." << std::endl;
        return 0;
    }
    std::cout << "media_packet test FAILED: " << g_failures << " items." << std::endl;
    return 1;
}