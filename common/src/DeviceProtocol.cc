#include "protocol/DeviceProtocol.h"
#include "protocol/Protocol.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace {

/* 以下基础函数均显式按网络大端逐字节处理，以保证 Windows/Linux 的线格式一致。 */
void appendUint16BE(std::vector<uint8_t> &buffer, uint16_t value) {
  buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFFU));
  buffer.push_back(static_cast<uint8_t>(value & 0xFFU));
}

void appendUint32BE(std::vector<uint8_t> &buffer, uint32_t value) {
  buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFFU));
  buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFFU));
  buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFFU));
  buffer.push_back(static_cast<uint8_t>(value & 0xFFU));
}

void appendUint64BE(std::vector<uint8_t> &buffer, uint64_t value) {
  for (int shift = 56; shift >= 0; shift -= 8) {
    buffer.push_back(static_cast<uint8_t>((value >> shift) & 0xFFULL));
  }
}

bool readUint16BE(const std::vector<uint8_t> &buffer, std::size_t &position,
                  uint16_t &value) {
  if (position > buffer.size() || buffer.size() - position < 2U) return false;
  value = static_cast<uint16_t>((static_cast<uint16_t>(buffer[position]) << 8) |
                                buffer[position + 1U]);
  position += 2U;
  return true;
}

bool readUint32BE(const std::vector<uint8_t> &buffer, std::size_t &position,
                  uint32_t &value) {
  if (position > buffer.size() || buffer.size() - position < 4U) return false;
  value = (static_cast<uint32_t>(buffer[position]) << 24) |
          (static_cast<uint32_t>(buffer[position + 1U]) << 16) |
          (static_cast<uint32_t>(buffer[position + 2U]) << 8) |
          static_cast<uint32_t>(buffer[position + 3U]);
  position += 4U;
  return true;
}

bool readUint64BE(const std::vector<uint8_t> &buffer, std::size_t &position,
                  uint64_t &value) {
  if (position > buffer.size() || buffer.size() - position < 8U) return false;
  value = 0U;
  for (std::size_t index = 0U; index < 8U; ++index) {
    value = (value << 8) | static_cast<uint64_t>(buffer[position + index]);
  }
  position += 8U;
  return true;
}

/* 字符串字段超过 uint16 能表示的 65535 字节时必须由编码端拒绝。 */
bool appendString(std::vector<uint8_t> &buffer, const std::string &text) {
  if (text.size() > static_cast<std::size_t>(std::numeric_limits<uint16_t>::max())) {
    return false;
  }
  appendUint16BE(buffer, static_cast<uint16_t>(text.size()));
  buffer.insert(buffer.end(), text.begin(), text.end());
  return true;
}

/* 解码端按声明长度读取；宣称的长度超过剩余数据即为截断包。 */
bool readString(const std::vector<uint8_t> &buffer, std::size_t &position,
                std::string &text) {
  uint16_t length = 0U;
  if (!readUint16BE(buffer, position, length) ||
      buffer.size() - position < static_cast<std::size_t>(length)) return false;
  text.assign(buffer.begin() + static_cast<std::ptrdiff_t>(position),
              buffer.begin() + static_cast<std::ptrdiff_t>(position + length));
  position += static_cast<std::size_t>(length);
  return true;
}

/* 所有字符串预校验后再写，避免函数失败时留下半个响应。 */
bool isEncodable(const DeviceInfo &device) {
  const std::size_t limit = std::numeric_limits<uint16_t>::max();
  return device.deviceName.size() <= limit && device.deviceType.size() <= limit &&
         device.status.size() <= limit;
}

/* 累加响应长度前先比较上限，避免 size_t 加法在异常输入下溢出或分配巨量内存。 */
bool canAppend(std::size_t current, std::size_t additional) {
  const std::size_t limit = static_cast<std::size_t>(MAX_TLV_BODY_SIZE);
  return additional <= limit && current <= limit - additional;
}

}  // namespace

bool DeviceProtocol::encodeDeviceListRequest(uint64_t userId,
                                              const std::string &token,
                                              std::vector<uint8_t> &value) {
  value.clear();
  if (token.size() > static_cast<std::size_t>(std::numeric_limits<uint16_t>::max())) {
    return false;
  }
  value.reserve(10U + token.size());
  appendUint64BE(value, userId);
  return appendString(value, token);
}

bool DeviceProtocol::decodeDeviceListRequest(const std::vector<uint8_t> &value,
                                              uint64_t &userId,
                                              std::string &token) {
  std::size_t position = 0U;
  uint64_t decodedUserId = 0U;
  std::string decodedToken;
  if (!readUint64BE(value, position, decodedUserId) ||
      !readString(value, position, decodedToken) || position != value.size()) return false;
  userId = decodedUserId;
  token = decodedToken;
  return true;
}

bool DeviceProtocol::encodeDeviceListResponse(
    ErrorCode errorCode, const std::vector<DeviceInfo> &devices,
    std::vector<uint8_t> &value) {
  value.clear();
  if (devices.size() > static_cast<std::size_t>(std::numeric_limits<uint16_t>::max())) {
    return false;
  }
  std::size_t bodySize = 6U;  // errorCode:uint32 + count:uint16
  if (!canAppend(0U, bodySize)) return false;
  for (std::vector<DeviceInfo>::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    if (!isEncodable(*it)) return false;
    const std::size_t itemSize = 8U + 2U + it->deviceName.size()
        + 2U + it->deviceType.size() + 2U + it->status.size();
    if (!canAppend(bodySize, itemSize)) return false;
    bodySize += itemSize;
  }

  value.reserve(bodySize);
  appendUint32BE(value, static_cast<uint32_t>(static_cast<int32_t>(errorCode)));
  appendUint16BE(value, static_cast<uint16_t>(devices.size()));
  for (std::vector<DeviceInfo>::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    /* 单项顺序与契约一致：id 后严格跟随 name/type/status 三段长度字符串。 */
    appendUint64BE(value, it->id);
    appendString(value, it->deviceName);
    appendString(value, it->deviceType);
    appendString(value, it->status);
  }
  return true;
}

bool DeviceProtocol::decodeDeviceListResponse(
    const std::vector<uint8_t> &value, ErrorCode &errorCode,
    std::vector<DeviceInfo> &devices) {
  if (value.size() > static_cast<std::size_t>(MAX_TLV_BODY_SIZE)) return false;
  std::size_t position = 0U;
  uint32_t rawCode = 0U;
  uint16_t count = 0U;
  if (!readUint32BE(value, position, rawCode) ||
      !readUint16BE(value, position, count)) return false;

  std::vector<DeviceInfo> decoded;
  decoded.reserve(count);
  for (uint16_t index = 0U; index < count; ++index) {
    DeviceInfo device;
    if (!readUint64BE(value, position, device.id) ||
        !readString(value, position, device.deviceName) ||
        !readString(value, position, device.deviceType) ||
        !readString(value, position, device.status)) return false;
    decoded.push_back(device);
  }
  /* count 项后必须正好到达末尾，拒绝尾随字节和拼接错误的业务 value。 */
  if (position != value.size()) return false;
  errorCode = static_cast<ErrorCode>(static_cast<int32_t>(rawCode));
  devices.swap(decoded);
  return true;
}
