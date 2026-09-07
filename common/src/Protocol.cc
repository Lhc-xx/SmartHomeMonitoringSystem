#include "protocol/Protocol.h"

#include "protocol/MessageType.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

/* 将 16 位整数按网络大端逐字节写入，避免依赖 POSIX 的 arpa/inet.h。 */
void appendUint16BE(std::vector<uint8_t> &buffer, uint16_t value) {
  buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFFU));
  buffer.push_back(static_cast<uint8_t>(value & 0xFFU));
}

/* 将 32 位整数按网络大端逐字节写入，Windows 和 Linux 均使用同一实现。 */
void appendUint32BE(std::vector<uint8_t> &buffer, uint32_t value) {
  buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFFU));
  buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFFU));
  buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFFU));
  buffer.push_back(static_cast<uint8_t>(value & 0xFFU));
}

/* 从指定偏移读取 16 位大端数；数据不足时不移动 position。 */
bool readUint16BE(const std::vector<uint8_t> &buffer, std::size_t &position,
                  uint16_t &value) {
  if (position > buffer.size() || buffer.size() - position < 2U) {
    return false;
  }
  value = static_cast<uint16_t>(
      (static_cast<uint16_t>(buffer[position]) << 8) |
      static_cast<uint16_t>(buffer[position + 1U]));
  position += 2U;
  return true;
}

/* 从指定偏移读取 32 位大端数；调用方先保证头部已完整到达。 */
bool readUint32BE(const std::vector<uint8_t> &buffer, std::size_t &position,
                  uint32_t &value) {
  if (position > buffer.size() || buffer.size() - position < 4U) {
    return false;
  }
  value = (static_cast<uint32_t>(buffer[position]) << 24) |
          (static_cast<uint32_t>(buffer[position + 1U]) << 16) |
          (static_cast<uint32_t>(buffer[position + 2U]) << 8) |
          static_cast<uint32_t>(buffer[position + 3U]);
  position += 4U;
  return true;
}

/* 外层 type 只能是 MessageType 中定义的消息类型，不能接受任意整数。 */
bool isKnownMessageType(uint16_t type) {
  switch (static_cast<MessageType>(type)) {
    case MessageType::REGISTER_REQUEST:
    case MessageType::REGISTER_RESPONSE:
    case MessageType::LOGIN_REQUEST:
    case MessageType::LOGIN_RESPONSE:
    case MessageType::DEVICE_LIST_REQUEST:
    case MessageType::DEVICE_LIST_RESPONSE:
    case MessageType::RECORD_QUERY_REQUEST:
    case MessageType::RECORD_QUERY_RESPONSE:
    case MessageType::STREAM_START_REQUEST:
    case MessageType::STREAM_START_RESPONSE:
    case MessageType::STREAM_STOP_REQUEST:
    case MessageType::STREAM_STOP_RESPONSE:
    case MessageType::RECORD_START_REQUEST:
    case MessageType::RECORD_START_RESPONSE:
    case MessageType::RECORD_STOP_REQUEST:
    case MessageType::RECORD_STOP_RESPONSE:
    case MessageType::PTZ_CONTROL_REQUEST:
    case MessageType::PTZ_CONTROL_RESPONSE:
      return true;
  }
  return false;
}

/*
 * 坏包没有同步标记可供可靠恢复，因此丢弃当前缓冲区；继续保留会让事件循环
 * 对相同坏头反复返回同一错误。半包路径不会调用这个函数。
 */
TlvDecodeStatus reject(std::vector<uint8_t> &buffer,
                       TlvDecodeStatus status) {
  buffer.clear();
  return status;
}

}  // namespace

std::vector<uint8_t> TlvProtocol::encode(const TlvMessage &message) {
  /* 在 size_t 转为 uint32_t 前先校验，彻底避免 64 位平台上的静默窄化。 */
  const std::size_t bodySize = message.value.size();
  if (bodySize > static_cast<std::size_t>(MAX_TLV_BODY_SIZE) ||
      bodySize > static_cast<std::size_t>(std::numeric_limits<uint32_t>::max()) ||
      message.version != PROTOCOL_VERSION || !isKnownMessageType(message.type)) {
    return std::vector<uint8_t>();
  }

  const uint32_t bodyLength = static_cast<uint32_t>(bodySize);
  std::vector<uint8_t> buffer;
  buffer.reserve(TLV_HEADER_SIZE + bodySize);
  appendUint16BE(buffer, message.type);
  appendUint16BE(buffer, message.version);
  appendUint32BE(buffer, bodyLength);
  appendUint32BE(buffer, message.requestId);
  buffer.insert(buffer.end(), message.value.begin(), message.value.end());
  return buffer;
}

TlvDecodeStatus TlvProtocol::decode(std::vector<uint8_t> &buffer,
                                    TlvMessage &message) {
  /* 固定头尚未到齐时不能读取任意字段，也绝不能消费半包。 */
  if (buffer.size() < TLV_HEADER_SIZE) {
    return TlvDecodeStatus::Incomplete;
  }

  std::size_t position = 0U;
  uint16_t type = 0U;
  uint16_t version = 0U;
  uint32_t length = 0U;
  uint32_t requestId = 0U;
  if (!readUint16BE(buffer, position, type) ||
      !readUint16BE(buffer, position, version) ||
      !readUint32BE(buffer, position, length) ||
      !readUint32BE(buffer, position, requestId)) {
    /* 防御性分支：前面的固定头检查已保证正常情况下不可达。 */
    return reject(buffer, TlvDecodeStatus::Invalid);
  }

  if (!isKnownMessageType(type)) {
    return reject(buffer, TlvDecodeStatus::UnknownMessage);
  }
  if (version != PROTOCOL_VERSION) {
    return reject(buffer, TlvDecodeStatus::UnsupportedVersion);
  }
  if (length > MAX_TLV_BODY_SIZE) {
    return reject(buffer, TlvDecodeStatus::BodyTooLarge);
  }

  const std::size_t bodyLength = static_cast<std::size_t>(length);
  const std::size_t totalLength = TLV_HEADER_SIZE + bodyLength;
  if (buffer.size() < totalLength) {
    return TlvDecodeStatus::Incomplete;
  }

  /* 先构造临时消息，只有完整成功时才覆盖调用者传入的 message。 */
  TlvMessage decoded;
  decoded.type = type;
  decoded.version = version;
  decoded.requestId = requestId;
  decoded.value.assign(buffer.begin() + static_cast<std::ptrdiff_t>(position),
                       buffer.begin() + static_cast<std::ptrdiff_t>(totalLength));
  message = decoded;
  buffer.erase(buffer.begin(),
               buffer.begin() + static_cast<std::ptrdiff_t>(totalLength));
  return TlvDecodeStatus::Decoded;
}

TlvDecodeStatus TlvProtocol::tryDecodeWithStatus(std::vector<uint8_t> &buffer,
                                                  TlvMessage &message) {
  return decode(buffer, message);
}

bool TlvProtocol::tryDecode(std::vector<uint8_t> &buffer,
                            TlvMessage &message) {
  return decode(buffer, message) == TlvDecodeStatus::Decoded;
}
