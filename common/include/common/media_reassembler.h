#ifndef SMART_HOME_COMMON_MEDIA_REASSEMBLER_H
#define SMART_HOME_COMMON_MEDIA_REASSEMBLER_H

// ============================================================================
// media_reassembler.h —— 媒体包重组器（角色 C 维护，Qt 客户端使用）
//
// 用法（在 Qt 的网络收包回调里）：
//   1. 每次 recv 到一批字节，调 feed(data, len)；
//   2. 反复调 nextPacket(pkt) 取出完整包，直到返回 false；
//   3. 等下一批字节，回到第 1 步。
//
// 与角色 A 的接口：A 把 socket 收到的字节原样交给 feed()，其余不用管。
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <vector>

#include "protocol/media_packet.h"   // MediaPacket（重组出来的目标类型）

namespace smart_home {
namespace common {

class MediaReassembler {
public:
    // 预留缓冲大小，减少初期扩容次数
    explicit MediaReassembler(size_t initialCapacity = 64 * 1024);

    // 喂入一批从网络收到的字节（只拷贝，不解析）
    void feed(const uint8_t *data, size_t len);

    // 尝试切出一个完整媒体包；成功写 out 返回 true，数据不够返回 false
    bool nextPacket(protocol::MediaPacket &out);

    // 当前还攒着多少字节（调试/监控用）
    size_t bufferedBytes() const { return _buffer.size() - _offset; }

    // 丢弃所有未切出的字节
    void reset();

    // 单帧最大字节数（口径与序列化器一致）
    static size_t maxFrameSize();

private:
    void compact();                     // 清理已消费字节，防止缓冲无限增长

    std::vector<uint8_t> _buffer;       // 累积的原始字节
    size_t _offset;                     // 已消费到的位置
};

}  // namespace common
}  // namespace smart_home

#endif