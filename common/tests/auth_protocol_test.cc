#include "protocol/AuthProtocol.h"
#include "protocol/ErrorCode.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

void testRegisterRequestRejectsTrailingData() {
  /* 注册请求沿用双长度字符串布局，并拒绝一个额外的尾随字节。 */
  std::vector<uint8_t> value;
  assert(AuthProtocol::encodeRegisterRequest("lqw", "123456", value));
  std::string username;
  std::string password;
  assert(AuthProtocol::decodeRegisterRequest(value, username, password));
  assert(username == "lqw");
  assert(password == "123456");

  value.push_back(0x00);
  assert(!AuthProtocol::decodeRegisterRequest(value, username, password));
}

void testRegisterResponseCarriesUtf8Message() {
  /* 响应中的消息长度用 uint16 大端表示，解码必须消费完整 value。 */
  const std::string message = "用户已存在";
  const std::vector<uint8_t> value = AuthProtocol::encodeRegisterResponse(
      ErrorCode::USER_ALREADY_EXISTS, message);
  assert(value.size() == 6U + message.size());
  assert(value[0] == 0x00 && value[1] == 0x00 && value[2] == 0x07 &&
         value[3] == 0xD2);

  ErrorCode code = ErrorCode::SUCCESS;
  std::string decodedMessage;
  assert(AuthProtocol::decodeRegisterResponse(value, code, decodedMessage));
  assert(code == ErrorCode::USER_ALREADY_EXISTS);
  assert(decodedMessage == message);

  std::vector<uint8_t> trailing = value;
  trailing.push_back(0x01);
  assert(!AuthProtocol::decodeRegisterResponse(trailing, code, decodedMessage));
}

void testLegacyRegisterResponseHasEmptyMessage() {
  /* 旧的单参数编码接口仍可调用，但统一产生长度为零的消息字段。 */
  const std::vector<uint8_t> value =
      AuthProtocol::encodeRegisterResponse(ErrorCode::SUCCESS);
  /* 单参数重载用于兼容旧版 4 字节响应；带 message 的新布局已在上例覆盖。 */
  assert(value.size() == 4U);
  ErrorCode code = ErrorCode::INTERNAL_ERROR;
  std::string message = "not-empty";
  assert(AuthProtocol::decodeRegisterResponse(value, code, message));
  assert(code == ErrorCode::SUCCESS);
  assert(message.empty());
}

void testLoginRequestAndResponse() {
  /* 登录请求与注册请求相同，登录响应包含 64 位用户 ID、token 和错误码。 */
  std::vector<uint8_t> request;
  assert(AuthProtocol::encodeLoginRequest("alice", "pw", request));
  std::string username;
  std::string password;
  assert(AuthProtocol::decodeLoginRequest(request, username, password));
  assert(username == "alice" && password == "pw");

  std::vector<uint8_t> response;
  assert(AuthProtocol::encodeLoginResponse(0x0102030405060708ULL, "token",
                                           ErrorCode::SUCCESS, response));
  const std::vector<uint8_t> expectedPrefix = {
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x00, 0x05};
  assert(std::equal(expectedPrefix.begin(), expectedPrefix.end(), response.begin()));
  uint64_t userId = 0;
  std::string token;
  ErrorCode code = ErrorCode::INTERNAL_ERROR;
  assert(AuthProtocol::decodeLoginResponse(response, userId, token, code));
  assert(userId == 0x0102030405060708ULL);
  assert(token == "token");
  assert(code == ErrorCode::SUCCESS);

  response.pop_back();
  assert(!AuthProtocol::decodeLoginResponse(response, userId, token, code));
}

}  // namespace

int main() {
  testRegisterRequestRejectsTrailingData();
  testRegisterResponseCarriesUtf8Message();
  testLegacyRegisterResponseHasEmptyMessage();
  testLoginRequestAndResponse();
  std::cout << "[PASS] auth_protocol_test" << std::endl;
  return 0;
}
