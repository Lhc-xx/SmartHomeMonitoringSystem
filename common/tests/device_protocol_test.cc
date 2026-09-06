#include "protocol/DeviceProtocol.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

int main() {
  /* 请求包含 userId 和 token，截断后的 payload 不能被误接受。 */
  std::vector<uint8_t> request;
  assert(DeviceProtocol::encodeDeviceListRequest(9, "abc", request));
  uint64_t userId = 0;
  std::string token;
  assert(DeviceProtocol::decodeDeviceListRequest(request, userId, token));
  assert(userId == 9 && token == "abc");
  request.pop_back();
  assert(!DeviceProtocol::decodeDeviceListRequest(request, userId, token));

  /* 空设备列表也是完整、合法的响应。 */
  std::vector<uint8_t> response;
  std::vector<DeviceInfo> devices;
  assert(DeviceProtocol::encodeDeviceListResponse(ErrorCode::SUCCESS, devices,
                                                   response));
  ErrorCode code = ErrorCode::INTERNAL_ERROR;
  assert(DeviceProtocol::decodeDeviceListResponse(response, code, devices));
  assert(code == ErrorCode::SUCCESS && devices.empty());

  /* 非空响应验证 64 位 id 与三段 UTF-8 字符串的字节布局可逆。 */
  DeviceInfo device;
  device.id = 0x0102030405060708ULL;
  device.deviceName = "客厅灯";
  device.deviceType = "light";
  device.status = "online";
  devices.push_back(device);
  assert(DeviceProtocol::encodeDeviceListResponse(ErrorCode::SUCCESS, devices,
                                                   response));
  devices.clear();
  assert(DeviceProtocol::decodeDeviceListResponse(response, code, devices));
  assert(devices.size() == 1U && devices[0].id == device.id);
  assert(devices[0].deviceName == device.deviceName);
  assert(devices[0].status == "online");

  response.push_back(0xFF);
  assert(!DeviceProtocol::decodeDeviceListResponse(response, code, devices));

  /* uint16 字段和 count 都有明确上限，编码端不能截断后继续发送。 */
  std::string tooLong(65536U, 'x');
  assert(!DeviceProtocol::encodeDeviceListRequest(1, tooLong, request));
  devices.assign(65536U, DeviceInfo());
  assert(!DeviceProtocol::encodeDeviceListResponse(ErrorCode::SUCCESS, devices,
                                                   response));
  std::cout << "[PASS] device_protocol_test" << std::endl;
  return 0;
}
