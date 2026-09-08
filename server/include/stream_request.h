#ifndef SMART_HOME_STREAM_REQUEST_H
#define SMART_HOME_STREAM_REQUEST_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace smart_home {

/*
 * 流媒体/录像控制请求体解析器。
 *
 * Reactor 收到 TLV 后先经过这里做“字段边界”和“大端序”校验，再创建流会话
 * 或录像任务。解析器不依赖 socket、线程或 FFmpeg，因此既能在连接层复用，
 * 也能用纯单元测试覆盖恶意截断包和尾随字节，避免把坏请求误当成 Mock 流。
 */

/* STREAM_START_REQUEST 的 value = uint16 大端长度 + UTF-8 stream URL。 */
bool parseStreamStartValue(const std::vector<uint8_t> &value, std::string &streamUrl);

/* RECORD_START_REQUEST 的 value = 一个 uint64 大端 deviceId，长度必须恰好 8 字节。 */
bool parseRecordStartValue(const std::vector<uint8_t> &value, uint64_t &deviceId);

/* STREAM_STOP_REQUEST / RECORD_STOP_REQUEST 不携带业务字段，value 必须为空。 */
bool isEmptyControlValue(const std::vector<uint8_t> &value);

} // namespace smart_home

#endif // SMART_HOME_STREAM_REQUEST_H
