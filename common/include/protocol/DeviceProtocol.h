#ifndef DEVICE_PROTOCOL_H
#define DEVICE_PROTOCOL_H

#include "protocol/ErrorCode.h"

#include <cstdint>
#include <string>
#include <vector>

/*
 * 设备清单中的一项。字符串保存 UTF-8 原始字节，长度以协议中的 uint16 字节数计，
 * 而不是按 Unicode 字符数量计数。
 */
struct DeviceInfo {
  uint64_t id = 0U;
  std::string deviceName;
  std::string deviceType;
  std::string status;
};

/* 设备业务 value 的编解码器；最外层消息封装仍由 TlvProtocol 负责。 */
class DeviceProtocol {
 public:
  /* 请求：userId:uint64 + tokenLen:uint16 + token。 */
  static bool encodeDeviceListRequest(uint64_t userId, const std::string &token,
                                      std::vector<uint8_t> &value);
  static bool decodeDeviceListRequest(const std::vector<uint8_t> &value,
                                      uint64_t &userId, std::string &token);

  /* 响应：errorCode:int32 + count:uint16 + 每项 id 与三段 uint16 字符串。 */
  static bool encodeDeviceListResponse(ErrorCode errorCode,
                                       const std::vector<DeviceInfo> &devices,
                                       std::vector<uint8_t> &value);
  static bool decodeDeviceListResponse(const std::vector<uint8_t> &value,
                                       ErrorCode &errorCode,
                                       std::vector<DeviceInfo> &devices);
};

#endif
