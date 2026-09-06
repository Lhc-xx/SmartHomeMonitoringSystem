#ifndef AUTH_PROTOCOL_H
#define AUTH_PROTOCOL_H

#include "protocol/ErrorCode.h"

#include <cstdint>
#include <string>
#include <vector>

/*
 * 认证 value 的编解码器。它只处理 TLV 的 value，不负责外层 type、version、
 * length 或 requestId；这些字段由 TlvProtocol 统一处理。
 */
class AuthProtocol {
 public:
  /* 注册请求：usernameLen:uint16 + username + passwordLen:uint16 + password。 */
  static bool encodeRegisterRequest(const std::string &username,
                                    const std::string &password,
                                    std::vector<uint8_t> &value);
  static bool decodeRegisterRequest(const std::vector<uint8_t> &value,
                                    std::string &username,
                                    std::string &password);

  /* 注册响应：code:int32 + messageLen:uint16 + UTF-8 message。 */
  static std::vector<uint8_t> encodeRegisterResponse(ErrorCode code,
                                                     const std::string &message);
  /* 旧单参数接口保留并产生历史 4 字节布局；新响应应使用带 message 的重载。 */
  static std::vector<uint8_t> encodeRegisterResponse(ErrorCode code);
  static bool decodeRegisterResponse(const std::vector<uint8_t> &value,
                                     ErrorCode &code, std::string &message);
  /* 旧解码接口忽略消息文本，兼容历史上仅关心错误码的调用点。 */
  static bool decodeRegisterResponse(const std::vector<uint8_t> &value,
                                     ErrorCode &code);

  /* 登录请求与注册请求的双长度字符串布局相同，但独立命名以表达消息语义。 */
  static bool encodeLoginRequest(const std::string &username,
                                 const std::string &password,
                                 std::vector<uint8_t> &value);
  static bool decodeLoginRequest(const std::vector<uint8_t> &value,
                                 std::string &username,
                                 std::string &password);

  /* 登录响应：userId:uint64 + tokenLen:uint16 + token + errorCode:int32。 */
  static bool encodeLoginResponse(uint64_t userId, const std::string &token,
                                  ErrorCode errorCode,
                                  std::vector<uint8_t> &value);
  static bool decodeLoginResponse(const std::vector<uint8_t> &value,
                                  uint64_t &userId, std::string &token,
                                  ErrorCode &errorCode);
};

#endif
