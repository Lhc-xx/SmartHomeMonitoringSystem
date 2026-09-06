#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstddef>
#include <cstdint>
#include <vector>

/*
 * 外层 TLV 的当前协议版本。任何非该版本的网络包都不能交给业务层解释，
 * 这样能避免不同版本把同一段 value 当成不同字段而造成错误处理。
 */
constexpr uint16_t PROTOCOL_VERSION = 1;

/* type(2) + version(2) + length(4) + requestId(4) 的固定网络头大小。 */
constexpr std::size_t TLV_HEADER_SIZE = 12U;

/*
 * 单个业务 value 的硬上限为 1 MiB。该限制同时用于编码和解码，
 * 防止长度字段被恶意伪造后触发超大内存分配或无限等待数据。
 */
constexpr uint32_t MAX_TLV_BODY_SIZE = 1024U * 1024U;

/*
 * 仅在进程内保存已拆开的消息；绝不能直接把此结构体的内存布局发送到网络，
 * 因为 C++ 结构体存在字节序、填充和 ABI 差异。
 */
struct TlvMessage {
  uint16_t type = 0;
  uint16_t version = PROTOCOL_VERSION;
  uint32_t requestId = 0;
  std::vector<uint8_t> value;
};

/*
 * 带原因的解码结果。Incomplete 不消费输入；Decoded 只消费一个完整包；
 * 其余错误会丢弃当前缓冲区，以避免调用方循环解码同一个坏头而忙等。
 */
enum class TlvDecodeStatus {
  Incomplete,
  Invalid,
  BodyTooLarge,
  UnsupportedVersion,
  UnknownMessage,
  Decoded
};

class TlvProtocol {
 public:
  /* 将合法的进程内消息编码为固定大端 TLV 字节；非法消息返回空向量。 */
  static std::vector<uint8_t> encode(const TlvMessage &message);

  /*
   * 解码一个包并返回细粒度状态。该接口适合网络层把协议错误映射到 ErrorCode，
   * 同时仍保留 buffer，以便半包到齐后重试。
   */
  static TlvDecodeStatus decode(std::vector<uint8_t> &buffer,
                                TlvMessage &message);

  /* 对 decode 的显式别名，便于调用点直接表达“尝试并取得状态”的语义。 */
  static TlvDecodeStatus tryDecodeWithStatus(std::vector<uint8_t> &buffer,
                                             TlvMessage &message);

  /*
   * 兼容旧调用点：只有完整、合法包被解析时返回 true，其他所有状态返回 false。
   */
  static bool tryDecode(std::vector<uint8_t> &buffer, TlvMessage &message);
};

#endif
