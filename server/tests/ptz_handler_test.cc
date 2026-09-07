// ptz_handler_test.cc —— PTZ 目标与动作白名单测试
//
// 只构造非法请求验证 fail-closed 行为，不发起任何 HTTP 请求，也不访问真实摄像头。

#include "PtzHandler.h"
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

} // namespace

int main() {
    smart_home::PtzHandler handler("test-secret", "192.168.2.100,192.168.2.160");
    bool ok = true;
    ok = ok && expectInvalid(handler, "http://127.0.0.1", "up", "start");
    ok = ok && expectInvalid(handler, "http://192.168.2.100@127.0.0.1", "up", "start");
    ok = ok && expectInvalid(handler, "file:///etc/passwd", "up", "start");
    ok = ok && expectInvalid(handler, "http://192.168.2.100/api?next=", "up", "start");
    ok = ok && expectInvalid(handler, "http://192.168.2.100", "zoom", "start");
    ok = ok && expectInvalid(handler, "http://192.168.2.100", "stop", "start");
    ok = ok && expectInvalid(handler, "http://192.168.2.100", "up", "pause");
    if (!ok) {
        std::fprintf(stderr, "PTZ allowlist validation failed\n");
        return 1;
    }
    std::printf("ptz_handler test passed.\n");
    return 0;
}
