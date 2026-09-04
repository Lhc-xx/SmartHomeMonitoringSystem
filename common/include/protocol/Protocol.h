#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <string>
#include <sys/types.h>
#include <vector>

/*
 * TLV协议版本。
 */
constexpr uint16_t PROTOCOL_VERSION = 1;

// TVL Header 固定长度
//   * type      2 Byte
//   * version   2 Byte
//   * length    4 Byte
//   * requestId 4 Byte

constexpr std::size_t TLV_HEADER_SIZE = 12;

/*
 * 单个业务包最大允许1MB。
 *
 * 客户端即使恶意声明：
 *
 * length = 0xFFFFFFFF
 *
 * Server也必须直接拒绝。
 */
constexpr uint32_t MAX_TLV_BODY_SIZE = 1024 * 1024;

// TLV 消息
//这个结构体只是程序内部表示
//不能把这个struct 直接 send 出去
struct TlvMessage {
  uint16_t type = 0;
  uint16_t version = PROTOCOL_VERSION;
  uint32_t requestId = 0;
  std::vector<uint8_t> value;
};

// TLV协议工具类。
class TlvProtocol {
public:
  //将TlvMessage序列化成网络字节流。
  static std::vector<uint8_t> encode(const TlvMessage &message);

  //尝试从buffer 中解析一个完整的TLV包
  //返回true  成功解析出一个完整包。
  //返回false 当前数据还不够一个完整包。
  //成功解析之后 会从buffer 当中删除已经消费掉的数据
  static bool tryDecode(std::vector<uint8_t> &buffer, TlvMessage &message);
};

#endif