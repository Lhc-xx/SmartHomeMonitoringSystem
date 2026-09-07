#ifndef RECORDER_H
#define RECORDER_H

// recorder.h —— 录像文件写入器（角色 A）
//
// 负责录像文件的生命周期：创建/写入/关闭。文件内容是连续的媒体包帧
// （用 protocol::MediaPacketSerializer::encode 编码的带长度前缀帧），
// 后续由角色 C 替换为 TS 切片等可回放格式。
//
// 只依赖标准库与 common 协议，不依赖 MySQL / log4cpp / epoll，便于单测。

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace smart_home {

// 递归创建目录（mkdir -p），目录已存在或创建成功均返回 true。
bool ensureDirectoryExists(const std::string &path);

// Recorder：录像文件写入器。open/write/close 均线程安全。
class Recorder {
public:
    Recorder();
    ~Recorder();

    Recorder(const Recorder &) = delete;
    Recorder &operator=(const Recorder &) = delete;

    // 创建并打开录像文件（二进制、截断覆盖）。失败返回 false。
    bool open(const std::string &filePath);

    // 写入一段已序列化的媒体帧。仅 open 后有效，失败返回 false。
    bool write(const std::vector<uint8_t> &data);

    // 关闭并 flush 文件。幂等：已关闭时再次调用无副作用。
    void close();

    bool isOpen() const;
    std::size_t bytesWritten() const;
    const std::string &filePath() const;

private:
    std::string _filePath;
    std::size_t _bytesWritten;
    int _fd;              // 文件描述符（-1 表示未打开）
    mutable std::mutex _mutex;
};

} // namespace smart_home

#endif // RECORDER_H
