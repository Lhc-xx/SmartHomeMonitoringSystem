#ifndef SERVER_MEDIA_MEDIA_SOURCE_H
#define SERVER_MEDIA_MEDIA_SOURCE_H

// ============================================================================
// media_source.h —— 媒体源抽象接口（角色 C 维护）
//
// 统一的拉流接口。FFmpegMediaSource（真摄像头）和 MockMediaSource（假源）
// 都实现它，上层（StreamSession）只面向这个接口，不关心具体是哪个源。
//
// 抽象类要点：
//   1. 含纯虚函数（= 0），不能直接实例化，只能被继承；
//   2. 必须有虚析构函数：否则"用基类指针 delete 派生类对象"时，
//      派生类的析构不会被调用，导致资源泄漏。
// ============================================================================

#include <string>                 // std::string

#include "protocol/media_packet.h"   // MediaPacket（读出来的媒体包）

namespace smart_home {
namespace media {

class MediaSource {
public:
    virtual ~MediaSource() {}     // 虚析构：保证派生类资源能被正确释放

    // 打开媒体源。url 例如 "rtsp://..."（真源）或 "mock://..."（假源）。
    // 成功返回 true。
    virtual bool open(const std::string &url) = 0;

    // 读一个媒体包。读到返回 true 并写入 out；
    // 返回 false 表示：未打开 / 流结束 / 出错。
    virtual bool readPacket(protocol::MediaPacket &out) = 0;

    // 关闭媒体源，释放资源。
    virtual void close() = 0;

    // 断流重连（对应分工计划里的"断流重连"）。成功返回 true。
    virtual bool reconnect() = 0;
};

}  // namespace media
}  // namespace smart_home

#endif  // SERVER_MEDIA_MEDIA_SOURCE_H