// test_media_reassembler.cc —— 第四步：重组器（半包/粘包/截断/乱码自愈）

#include <cstdint>
#include <iostream>
#include <vector>

#include "common/media_reassembler.h"
#include "protocol/media_packet.h"

using smart_home::common::MediaReassembler;
using smart_home::protocol::MediaPacket;
using smart_home::protocol::MediaPacketSerializer;
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

// 造一个测试包：负载长度随 id 变化
static MediaPacket makePacket(int id) {
    MediaPacket p;
    p.streamIndex = 0;
    p.pts = id;
    p.dts = id;
    p.duration = 40;
    p.flags = (id % 30 == 0) ? kMediaFlagKeyFrame : 0;
    p.data.resize(1 + (id % 7));
    for (size_t i = 0; i < p.data.size(); ++i) {
        p.data[i] = static_cast<uint8_t>((id + i) & 0xFF);
    }
    return p;
}

// 把若干包编码成一段连续字节流
static std::vector<uint8_t> buildStream(int count) {
    std::vector<uint8_t> s;
    for (int i = 0; i < count; ++i) MediaPacketSerializer::encode(makePacket(i), s);
    return s;
}

static bool equals(const MediaPacket &a, const MediaPacket &b) {
    return a.streamIndex == b.streamIndex && a.pts == b.pts && a.dts == b.dts &&
           a.duration == b.duration && a.flags == b.flags && a.data == b.data;
}

int main() {
    std::cout << "=== MediaReassembler test begin ===" << std::endl;

    // 1) 一次喂一整帧
    {
        std::vector<uint8_t> s = buildStream(1);
        MediaReassembler r;
        r.feed(s.data(), s.size());
        MediaPacket got;
        expect(r.nextPacket(got) && equals(got, makePacket(0)), "whole frame reassembled");
        expect(!r.nextPacket(got), "no more packet after drain");
    }

    // 2) 逐字节喂入（最极端的半包）
    {
        std::vector<uint8_t> s = buildStream(1);
        MediaReassembler r;
        MediaPacket got;
        bool ok = false;
        for (size_t i = 0; i < s.size(); ++i) {
            r.feed(s.data() + i, 1);
            if (r.nextPacket(got)) { ok = true; break; }
        }
        expect(ok && equals(got, makePacket(0)), "byte-by-byte (half packet) reassembled");
    }

    // 3) 两帧黏在一起（粘包）
    {
        std::vector<uint8_t> a, b, glue;
        MediaPacketSerializer::encode(makePacket(0), a);
        MediaPacketSerializer::encode(makePacket(1), b);
        glue.insert(glue.end(), a.begin(), a.end());
        glue.insert(glue.end(), b.begin(), b.end());

        MediaReassembler r;
        r.feed(glue.data(), glue.size());
        MediaPacket p1, p2;
        expect(r.nextPacket(p1) && equals(p1, makePacket(0)), "sticky: 1st frame");
        expect(r.nextPacket(p2) && equals(p2, makePacket(1)), "sticky: 2nd frame");
    }

    // 4) 从负载中间断开
    {
        MediaPacket big;
        big.data.assign(20, 0xAB);
        std::vector<uint8_t> s;
        MediaPacketSerializer::encode(big, s);

        size_t cut = 38 + 10;   // 切在负载中间
        MediaReassembler r;
        r.feed(s.data(), cut);
        MediaPacket tmp;
        expect(!r.nextPacket(tmp), "half frame -> returns false");
        r.feed(s.data() + cut, s.size() - cut);
        expect(r.nextPacket(tmp) && equals(tmp, big), "split in payload reassembled");
    }

    // 5) 前面有乱码（自愈重新对齐，验证魔数校验生效）
    {
        std::vector<uint8_t> garbage = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02};
        std::vector<uint8_t> frame;
        MediaPacketSerializer::encode(makePacket(5), frame);
        std::vector<uint8_t> s;
        s.insert(s.end(), garbage.begin(), garbage.end());
        s.insert(s.end(), frame.begin(), frame.end());

        MediaReassembler r;
        r.feed(s.data(), s.size());
        MediaPacket got;
        expect(r.nextPacket(got) && equals(got, makePacket(5)), "garbage prefix self-heals");
    }

    // 6) 随机分块喂入大流量（综合压力 + 顺序校验）
    {
        const int kTotal = 500;
        std::vector<uint8_t> s = buildStream(kTotal);
        MediaReassembler r;
        int received = 0;
        bool orderOk = true;
        size_t pos = 0, chunk = 1;
        while (pos < s.size()) {
            size_t n = (chunk < s.size() - pos) ? chunk : (s.size() - pos);
            r.feed(s.data() + pos, n);
            pos += n;
            MediaPacket p;
            while (r.nextPacket(p)) {
                if (p.pts != received) orderOk = false;
                ++received;
            }
            chunk = (chunk * 3 + 4) % 31 + 1;
        }
        expect(received == kTotal, "random chunking: all 500 frames received");
        expect(orderOk, "random chunking: order preserved");
    }

    std::cout << "=== MediaReassembler test end ===" << std::endl;
    if (g_failures == 0) {
        std::cout << "media_reassembler test passed." << std::endl;
        return 0;
    }
    std::cout << "media_reassembler test FAILED: " << g_failures << " items." << std::endl;
    return 1;
}