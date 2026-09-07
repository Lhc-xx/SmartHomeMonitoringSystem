#include "protocol/MessageType.h"
#include "protocol/Protocol.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

/*
 * 手工构造固定头，避免测试通过调用待测编码器而掩盖解码器的大端错误。
 */
std::vector<uint8_t> makeHeader(uint16_t type, uint16_t version,
                                uint32_t length, uint32_t requestId) {
  std::vector<uint8_t> bytes;
  bytes.push_back(static_cast<uint8_t>(type >> 8));
  bytes.push_back(static_cast<uint8_t>(type));
  bytes.push_back(static_cast<uint8_t>(version >> 8));
  bytes.push_back(static_cast<uint8_t>(version));
  bytes.push_back(static_cast<uint8_t>(length >> 24));
  bytes.push_back(static_cast<uint8_t>(length >> 16));
  bytes.push_back(static_cast<uint8_t>(length >> 8));
  bytes.push_back(static_cast<uint8_t>(length));
  bytes.push_back(static_cast<uint8_t>(requestId >> 24));
  bytes.push_back(static_cast<uint8_t>(requestId >> 16));
  bytes.push_back(static_cast<uint8_t>(requestId >> 8));
  bytes.push_back(static_cast<uint8_t>(requestId));
  return bytes;
}

void testGoldenBytesAndDecodedStatus() {
  /* 固定字节序列覆盖 type、version、length 与 requestId 的大端顺序。 */
  TlvMessage message;
  message.type = static_cast<uint16_t>(MessageType::REGISTER_REQUEST);
  message.requestId = 0x01020304U;
  message.value.push_back(0xAA);
  message.value.push_back(0xBB);

  const std::vector<uint8_t> expected = {
      0x10, 0x01, 0x00, 0x01, 0x00, 0x00,
      0x00, 0x02, 0x01, 0x02, 0x03, 0x04,
      0xAA, 0xBB};
  std::vector<uint8_t> buffer = TlvProtocol::encode(message);
  assert(buffer == expected);

  TlvMessage decoded;
  assert(TlvProtocol::decode(buffer, decoded) == TlvDecodeStatus::Decoded);
  assert(buffer.empty());
  assert(decoded.type == message.type);
  assert(decoded.version == PROTOCOL_VERSION);
  assert(decoded.requestId == message.requestId);
  assert(decoded.value == message.value);
}

void testHalfPacketAndStickyPackets() {
  /* 半包必须保留原缓冲区，待后续网络数据到达后才能成功解析。 */
  TlvMessage first;
  first.type = static_cast<uint16_t>(MessageType::LOGIN_REQUEST);
  first.requestId = 11;
  first.value.assign(3, 0x7A);
  const std::vector<uint8_t> firstPacket = TlvProtocol::encode(first);

  std::vector<uint8_t> buffer(firstPacket.begin(), firstPacket.begin() + 7);
  TlvMessage decoded;
  assert(TlvProtocol::decode(buffer, decoded) == TlvDecodeStatus::Incomplete);
  assert(buffer.size() == 7);

  buffer.insert(buffer.end(), firstPacket.begin() + 7, firstPacket.end());
  TlvMessage second;
  second.type = static_cast<uint16_t>(MessageType::DEVICE_LIST_REQUEST);
  second.requestId = 12;
  const std::vector<uint8_t> secondPacket = TlvProtocol::encode(second);
  buffer.insert(buffer.end(), secondPacket.begin(), secondPacket.end());

  assert(TlvProtocol::decode(buffer, decoded) == TlvDecodeStatus::Decoded);
  assert(decoded.requestId == 11);
  assert(TlvProtocol::tryDecode(buffer, decoded));
  assert(decoded.requestId == 12);
  assert(buffer.empty());
}

void testPtzMessagesAreRecognized() {
  /* PTZ 也走同一套 TLV 外层；白名单遗漏会让服务端无法解析云台请求。 */
  TlvMessage message;
  message.type = static_cast<uint16_t>(MessageType::PTZ_CONTROL_REQUEST);
  message.requestId = 99U;
  message.value = {0x01, 0x02, 0x03};
  std::vector<uint8_t> buffer = TlvProtocol::encode(message);
  assert(!buffer.empty());

  TlvMessage decoded;
  assert(TlvProtocol::decode(buffer, decoded) == TlvDecodeStatus::Decoded);
  assert(decoded.type == message.type);
  assert(decoded.requestId == message.requestId);
  assert(decoded.value == message.value);

  message.type = static_cast<uint16_t>(MessageType::PTZ_CONTROL_RESPONSE);
  buffer = TlvProtocol::encode(message);
  assert(!buffer.empty());
  assert(TlvProtocol::decode(buffer, decoded) == TlvDecodeStatus::Decoded);
  assert(decoded.type == message.type);
}

void testRejectedHeaderStates() {
  /* 未知 type、错误 version、超大 body 分别必须返回可区分的状态。 */
  TlvMessage decoded;
  std::vector<uint8_t> unknown = makeHeader(0x9999, PROTOCOL_VERSION, 0, 1);
  assert(TlvProtocol::decode(unknown, decoded) == TlvDecodeStatus::UnknownMessage);

  std::vector<uint8_t> unsupported = makeHeader(
      static_cast<uint16_t>(MessageType::REGISTER_REQUEST), 2, 0, 1);
  assert(TlvProtocol::decode(unsupported, decoded) ==
         TlvDecodeStatus::UnsupportedVersion);

  std::vector<uint8_t> oversized = makeHeader(
      static_cast<uint16_t>(MessageType::REGISTER_REQUEST), PROTOCOL_VERSION,
      MAX_TLV_BODY_SIZE + 1U, 1);
  assert(TlvProtocol::decode(oversized, decoded) ==
         TlvDecodeStatus::BodyTooLarge);

  /* 对恶意声明长度的编码请求也必须拒绝，不能把 size_t 静默截断为 uint32_t。 */
  TlvMessage huge;
  huge.type = static_cast<uint16_t>(MessageType::REGISTER_REQUEST);
  huge.value.resize(static_cast<std::size_t>(MAX_TLV_BODY_SIZE) + 1U);
  assert(TlvProtocol::encode(huge).empty());
}

}  // namespace

int main() {
  testGoldenBytesAndDecodedStatus();
  testHalfPacketAndStickyPackets();
  testPtzMessagesAreRecognized();
  testRejectedHeaderStates();
  std::cout << "[PASS] protocol_test" << std::endl;
  return 0;
}
