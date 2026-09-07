#include "protocol/RecordProtocol.h"
#include "protocol/Protocol.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace {

/* 录像协议使用手工大端函数，避免不同平台对网络字节序头文件的依赖。 */
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

/* 协议字符串长度为 uint16，编码前用字节数验证，不能静默截断。 */
bool appendString(std::vector<uint8_t> &buffer, const std::string &text) {
  if (text.size() > static_cast<std::size_t>(std::numeric_limits<uint16_t>::max())) {
    return false;
  }
  appendUint16BE(buffer, static_cast<uint16_t>(text.size()));
  buffer.insert(buffer.end(), text.begin(), text.end());
  return true;
}

/* 声明长度大于剩余字节数时立即视为截断，避免迭代器越界。 */
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

bool isEncodable(const RecordInfo &record) {
  const std::size_t limit = std::numeric_limits<uint16_t>::max();
  return record.filePath.size() <= limit && record.startTime.size() <= limit &&
         record.endTime.size() <= limit;
}

/* 录像响应由数据库结果组成，编码前逐项检查统一 value 上限，避免内存放大。 */
bool canAppend(std::size_t current, std::size_t additional) {
  const std::size_t limit = static_cast<std::size_t>(MAX_TLV_BODY_SIZE);
  return additional <= limit && current <= limit - additional;
}

}  // namespace

bool RecordProtocol::encodeRecordQueryRequest(
    uint64_t userId, const std::string &token, uint64_t deviceId,
    const std::string &startTime, const std::string &endTime,
    std::vector<uint8_t> &value) {
  value.clear();
  if (token.size() > static_cast<std::size_t>(std::numeric_limits<uint16_t>::max()) ||
      startTime.size() > static_cast<std::size_t>(std::numeric_limits<uint16_t>::max()) ||
      endTime.size() > static_cast<std::size_t>(std::numeric_limits<uint16_t>::max())) {
    return false;
  }
  value.reserve(28U + token.size() + startTime.size() + endTime.size());
  appendUint64BE(value, userId);
  appendString(value, token);
  appendUint64BE(value, deviceId);
  appendString(value, startTime);
  return appendString(value, endTime);
}

bool RecordProtocol::decodeRecordQueryRequest(
    const std::vector<uint8_t> &value, uint64_t &userId, std::string &token,
    uint64_t &deviceId, std::string &startTime, std::string &endTime) {
  std::size_t position = 0U;
  uint64_t decodedUserId = 0U;
  uint64_t decodedDeviceId = 0U;
  std::string decodedToken;
  std::string decodedStartTime;
  std::string decodedEndTime;
  if (!readUint64BE(value, position, decodedUserId) ||
      !readString(value, position, decodedToken) ||
      !readUint64BE(value, position, decodedDeviceId) ||
      !readString(value, position, decodedStartTime) ||
      !readString(value, position, decodedEndTime) || position != value.size()) return false;
  userId = decodedUserId;
  token = decodedToken;
  deviceId = decodedDeviceId;
  startTime = decodedStartTime;
  endTime = decodedEndTime;
  return true;
}

bool RecordProtocol::encodeRecordQueryResponse(
    ErrorCode errorCode, const std::vector<RecordInfo> &records,
    std::vector<uint8_t> &value) {
  value.clear();
  if (records.size() > static_cast<std::size_t>(std::numeric_limits<uint16_t>::max())) {
    return false;
  }
  std::size_t bodySize = 6U;  // errorCode:uint32 + count:uint16
  if (!canAppend(0U, bodySize)) return false;
  for (std::vector<RecordInfo>::const_iterator it = records.begin();
       it != records.end(); ++it) {
    if (!isEncodable(*it)) return false;
    const std::size_t itemSize = 16U + 2U + it->filePath.size()
        + 2U + it->startTime.size() + 2U + it->endTime.size();
    if (!canAppend(bodySize, itemSize)) return false;
    bodySize += itemSize;
  }
  value.reserve(bodySize);
  appendUint32BE(value, static_cast<uint32_t>(static_cast<int32_t>(errorCode)));
  appendUint16BE(value, static_cast<uint16_t>(records.size()));
  for (std::vector<RecordInfo>::const_iterator it = records.begin();
       it != records.end(); ++it) {
    /* 每项固定先写双 uint64，再写文件路径、开始时间和结束时间。 */
    appendUint64BE(value, it->id);
    appendUint64BE(value, it->deviceId);
    appendString(value, it->filePath);
    appendString(value, it->startTime);
    appendString(value, it->endTime);
  }
  return true;
}

bool RecordProtocol::decodeRecordQueryResponse(
    const std::vector<uint8_t> &value, ErrorCode &errorCode,
    std::vector<RecordInfo> &records) {
  if (value.size() > static_cast<std::size_t>(MAX_TLV_BODY_SIZE)) return false;
  std::size_t position = 0U;
  uint32_t rawCode = 0U;
  uint16_t count = 0U;
  if (!readUint32BE(value, position, rawCode) ||
      !readUint16BE(value, position, count)) return false;

  std::vector<RecordInfo> decoded;
  decoded.reserve(count);
  for (uint16_t index = 0U; index < count; ++index) {
    RecordInfo record;
    if (!readUint64BE(value, position, record.id) ||
        !readUint64BE(value, position, record.deviceId) ||
        !readString(value, position, record.filePath) ||
        !readString(value, position, record.startTime) ||
        !readString(value, position, record.endTime)) return false;
    decoded.push_back(record);
  }
  /* 精确消费是业务 value 的边界：一个额外字节也视为非法。 */
  if (position != value.size()) return false;
  errorCode = static_cast<ErrorCode>(static_cast<int32_t>(rawCode));
  records.swap(decoded);
  return true;
}
