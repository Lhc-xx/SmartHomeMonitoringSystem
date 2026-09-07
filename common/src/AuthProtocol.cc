#include "protocol/AuthProtocol.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace {

/* 认证 value 与外层 TLV 一样固定使用网络大端，不能依赖主机字节序。 */
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

/* 每次读取均先检查剩余字节数，避免 position 加法溢出和越界访问。 */
bool readUint16BE(const std::vector<uint8_t> &buffer, std::size_t &position,
                  uint16_t &value) {
  if (position > buffer.size() || buffer.size() - position < 2U) {
    return false;
  }
  value = static_cast<uint16_t>((static_cast<uint16_t>(buffer[position]) << 8) |
                                buffer[position + 1U]);
  position += 2U;
  return true;
}

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

bool readUint64BE(const std::vector<uint8_t> &buffer, std::size_t &position,
                  uint64_t &value) {
  if (position > buffer.size() || buffer.size() - position < 8U) {
    return false;
  }
  value = 0U;
  for (std::size_t index = 0U; index < 8U; ++index) {
    value = (value << 8) | static_cast<uint64_t>(buffer[position + index]);
  }
  position += 8U;
  return true;
}

/* 写入 uint16 长度前校验 string 的字节数，UTF-8 多字节字符按真实字节数计数。 */
bool appendLengthString(std::vector<uint8_t> &buffer, const std::string &text) {
  if (text.size() > static_cast<std::size_t>(std::numeric_limits<uint16_t>::max())) {
    return false;
  }
  appendUint16BE(buffer, static_cast<uint16_t>(text.size()));
  buffer.insert(buffer.end(), text.begin(), text.end());
  return true;
}

/* 读取长度字符串；不足 declaredLength 字节立即失败，不接受截断数据。 */
bool readLengthString(const std::vector<uint8_t> &buffer, std::size_t &position,
                      std::string &text) {
  uint16_t length = 0U;
  if (!readUint16BE(buffer, position, length) ||
      buffer.size() - position < static_cast<std::size_t>(length)) {
    return false;
  }
  text.assign(buffer.begin() + static_cast<std::ptrdiff_t>(position),
              buffer.begin() + static_cast<std::ptrdiff_t>(position + length));
  position += static_cast<std::size_t>(length);
  return true;
}

/* 双字符串请求的统一实现，确保注册与登录不因复制代码出现格式漂移。 */
bool encodeCredentials(const std::string &username, const std::string &password,
                       std::vector<uint8_t> &value) {
  /* 失败时也清空输出，避免调用方误把上一次成功请求重新发送。 */
  value.clear();
  if (username.size() > static_cast<std::size_t>(std::numeric_limits<uint16_t>::max()) ||
      password.size() > static_cast<std::size_t>(std::numeric_limits<uint16_t>::max())) {
    return false;
  }
  value.reserve(4U + username.size() + password.size());
  return appendLengthString(value, username) && appendLengthString(value, password);
}

/* 解析成功的必要条件是所有字节恰好被两个字段消费，尾随数据必须拒绝。 */
bool decodeCredentials(const std::vector<uint8_t> &value, std::string &username,
                       std::string &password) {
  std::size_t position = 0U;
  std::string decodedUsername;
  std::string decodedPassword;
  if (!readLengthString(value, position, decodedUsername) ||
      !readLengthString(value, position, decodedPassword) ||
      position != value.size()) {
    return false;
  }
  username = decodedUsername;
  password = decodedPassword;
  return true;
}

}  // namespace

bool AuthProtocol::encodeRegisterRequest(const std::string &username,
                                         const std::string &password,
                                         std::vector<uint8_t> &value) {
  return encodeCredentials(username, password, value);
}

bool AuthProtocol::decodeRegisterRequest(const std::vector<uint8_t> &value,
                                         std::string &username,
                                         std::string &password) {
  return decodeCredentials(value, username, password);
}

std::vector<uint8_t> AuthProtocol::encodeRegisterResponse(
    ErrorCode code, const std::string &message) {
  std::vector<uint8_t> value;
  if (message.size() > static_cast<std::size_t>(std::numeric_limits<uint16_t>::max())) {
    return value;
  }
  value.reserve(6U + message.size());
  appendUint32BE(value, static_cast<uint32_t>(static_cast<int32_t>(code)));
  appendLengthString(value, message);
  return value;
}

std::vector<uint8_t> AuthProtocol::encodeRegisterResponse(ErrorCode code) {
  /*
   * 保留旧版单参数接口的 4 字节布局，避免已有调用方因新增 message
   * 字段而改变字节流；需要发送 REGISTER_RESPONSE 完整契约时使用上面的
   * 带 message 重载。解码器同时兼容这两种布局。
   */
  std::vector<uint8_t> value;
  value.reserve(4U);
  appendUint32BE(value, static_cast<uint32_t>(static_cast<int32_t>(code)));
  return value;
}

bool AuthProtocol::decodeRegisterResponse(const std::vector<uint8_t> &value,
                                          ErrorCode &code,
                                          std::string &message) {
  std::size_t position = 0U;
  uint32_t rawCode = 0U;
  std::string decodedMessage;
  if (!readUint32BE(value, position, rawCode)) {
    return false;
  }
  /* 兼容历史 4 字节响应；新编码接口始终写出零长度 message 字段。 */
  if (position != value.size() &&
      (!readLengthString(value, position, decodedMessage) || position != value.size())) {
    return false;
  }
  code = static_cast<ErrorCode>(static_cast<int32_t>(rawCode));
  message = decodedMessage;
  return true;
}

bool AuthProtocol::decodeRegisterResponse(const std::vector<uint8_t> &value,
                                          ErrorCode &code) {
  std::string ignoredMessage;
  return decodeRegisterResponse(value, code, ignoredMessage);
}

bool AuthProtocol::encodeLoginRequest(const std::string &username,
                                      const std::string &password,
                                      std::vector<uint8_t> &value) {
  return encodeCredentials(username, password, value);
}

bool AuthProtocol::decodeLoginRequest(const std::vector<uint8_t> &value,
                                      std::string &username,
                                      std::string &password) {
  return decodeCredentials(value, username, password);
}

bool AuthProtocol::encodeLoginResponse(uint64_t userId, const std::string &token,
                                       ErrorCode errorCode,
                                       std::vector<uint8_t> &value) {
  value.clear();
  if (token.size() > static_cast<std::size_t>(std::numeric_limits<uint16_t>::max())) {
    return false;
  }
  value.reserve(14U + token.size());
  appendUint64BE(value, userId);
  appendLengthString(value, token);
  appendUint32BE(value, static_cast<uint32_t>(static_cast<int32_t>(errorCode)));
  return true;
}

bool AuthProtocol::decodeLoginResponse(const std::vector<uint8_t> &value,
                                       uint64_t &userId, std::string &token,
                                       ErrorCode &errorCode) {
  std::size_t position = 0U;
  uint64_t decodedUserId = 0U;
  uint32_t rawCode = 0U;
  std::string decodedToken;
  if (!readUint64BE(value, position, decodedUserId) ||
      !readLengthString(value, position, decodedToken) ||
      !readUint32BE(value, position, rawCode) || position != value.size()) {
    return false;
  }
  userId = decodedUserId;
  token = decodedToken;
  errorCode = static_cast<ErrorCode>(static_cast<int32_t>(rawCode));
  return true;
}
