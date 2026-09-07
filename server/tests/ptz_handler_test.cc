// ptz_handler_test.cc —— PTZ 目标与动作白名单测试
//
// 只构造非法请求验证 fail-closed 行为，不发起任何 HTTP 请求，也不访问真实摄像头。

#include "PtzHandler.h"
#include "PtzHttpClient.h"
#include "protocol/ErrorCode.h"
#include "protocol/MessageType.h"

#include <arpa/inet.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

void appendString(std::vector<uint8_t> &value, const std::string &text) {
    const uint16_t length = static_cast<uint16_t>(text.size());
    value.push_back(static_cast<uint8_t>((length >> 8) & 0xff));
    value.push_back(static_cast<uint8_t>(length & 0xff));
    value.insert(value.end(), text.begin(), text.end());
}

TlvMessage makeRequest(const std::string &url,
                       const std::string &direction,
                       const std::string &move) {
    TlvMessage request;
    request.version = PROTOCOL_VERSION;
    request.requestId = 1;
    request.type = static_cast<uint16_t>(MessageType::PTZ_CONTROL_REQUEST);
    appendString(request.value, url);
    appendString(request.value, direction);
    appendString(request.value, move);
    return request;
}

int32_t readError(const TlvMessage &response) {
    if (response.value.size() != 4) {
        return -1;
    }
    const uint32_t code = (static_cast<uint32_t>(response.value[0]) << 24)
        | (static_cast<uint32_t>(response.value[1]) << 16)
        | (static_cast<uint32_t>(response.value[2]) << 8)
        | static_cast<uint32_t>(response.value[3]);
    return static_cast<int32_t>(code);
}

bool expectInvalid(smart_home::PtzHandler &handler,
                   const std::string &url,
                   const std::string &direction,
                   const std::string &move) {
    const TlvMessage response = handler.handle(makeRequest(url, direction, move));
    return readError(response) == static_cast<int32_t>(smart_home::ErrorCode::INVALID_PARAMETER);
}

bool expectDeviceValue(const std::string &direction,
                       const std::string &move,
                       const std::string &expected) {
    return smart_home::PtzHttpClient::deviceValue(direction, move) == expected;
}

} // namespace

int main() {
    /*
     * 摄像头网页端不是旧的 direction/move 参数，而是 value 编码：
     * 斜向使用 1/2/3/4，正向使用 u/d/l/r，停止使用 s。
     * 这里仅测试纯映射函数，不启动 HTTP 服务，也不访问真实摄像头。
     */
    bool mappingOk = true;
    mappingOk = mappingOk && expectDeviceValue("up-left", "start", "1");
    mappingOk = mappingOk && expectDeviceValue("up", "start", "u");
    mappingOk = mappingOk && expectDeviceValue("up-right", "start", "2");
    mappingOk = mappingOk && expectDeviceValue("left", "start", "l");
    mappingOk = mappingOk && expectDeviceValue("right", "start", "r");
    mappingOk = mappingOk && expectDeviceValue("down-left", "start", "3");
    mappingOk = mappingOk && expectDeviceValue("down", "start", "d");
    mappingOk = mappingOk && expectDeviceValue("down-right", "start", "4");
    mappingOk = mappingOk && expectDeviceValue("stop", "stop", "s");
    mappingOk = mappingOk && expectDeviceValue("up", "pause", "");
    if (!mappingOk) {
        std::fprintf(stderr, "PTZ device value mapping failed\n");
        return 1;
    }

    /* 使用文档保留地址 TEST-NET-2，避免测试源码携带现场网段信息。 */
    smart_home::PtzHandler handler("test-secret", "198.51.100.10,198.51.100.11");
    bool ok = true;
    ok = ok && expectInvalid(handler, "http://127.0.0.1", "up", "start");
    ok = ok && expectInvalid(handler, "http://198.51.100.10@127.0.0.1", "up", "start");
    ok = ok && expectInvalid(handler, "file:///etc/passwd", "up", "start");
    ok = ok && expectInvalid(handler, "http://198.51.100.10/api?next=", "up", "start");
    ok = ok && expectInvalid(handler, "http://198.51.100.10", "zoom", "start");
    ok = ok && expectInvalid(handler, "http://198.51.100.10", "stop", "start");
    ok = ok && expectInvalid(handler, "http://198.51.100.10", "up", "pause");
    if (!ok) {
        std::fprintf(stderr, "PTZ allowlist validation failed\n");
        return 1;
    }
    std::printf("ptz_handler test passed.\n");
    return 0;
}
