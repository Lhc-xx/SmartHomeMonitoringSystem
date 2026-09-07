// session_policy_test.cc —— 未登录拦截策略纯逻辑测试（角色 A）
//
// 验证 requiresAuth / responseTypeFor / buildUnauthorizedValue，
// 不依赖 MySQL / log4cpp / epoll，只依赖 common 协议编解码实现。
#include "session_policy.h"

#include "protocol/DeviceProtocol.h"
#include "protocol/RecordProtocol.h"

#include <cstdint>
#include <cstdio>
#include <vector>

using namespace smart_home;

static int g_failures = 0;
static void expect(bool cond, const char *msg) {
    if (cond) {
        std::printf("  [ok]   %s\n", msg);
    } else {
        std::printf("  [FAIL] %s\n", msg);
        ++g_failures;
    }
}

// 把 4 字节大端 int32 还原为主机序 int32。
static int32_t decodeInt32BE(const std::vector<uint8_t> &v) {
    if (v.size() < 4) {
        return -1;
    }
    uint32_t u = (static_cast<uint32_t>(v[0]) << 24) |
                 (static_cast<uint32_t>(v[1]) << 16) |
                 (static_cast<uint32_t>(v[2]) << 8) |
                 (static_cast<uint32_t>(v[3]));
    return static_cast<int32_t>(u);
}

int main() {
    std::printf("=== session_policy test begin ===\n");

    // 1) requiresAuth
    expect(!requiresAuth(MessageType::REGISTER_REQUEST), "register does not require auth");
    expect(!requiresAuth(MessageType::LOGIN_REQUEST), "login does not require auth");
    expect(requiresAuth(MessageType::DEVICE_LIST_REQUEST), "device list requires auth");
    expect(requiresAuth(MessageType::RECORD_QUERY_REQUEST), "record query requires auth");
    expect(requiresAuth(MessageType::STREAM_START_REQUEST), "stream start requires auth");
    expect(requiresAuth(MessageType::STREAM_STOP_REQUEST), "stream stop requires auth");
    expect(requiresAuth(MessageType::RECORD_START_REQUEST), "record start requires auth");
    expect(requiresAuth(MessageType::RECORD_STOP_REQUEST), "record stop requires auth");
    expect(requiresAuth(MessageType::PTZ_CONTROL_REQUEST), "ptz control requires auth");

    // 2) responseTypeFor
    expect(responseTypeFor(MessageType::DEVICE_LIST_REQUEST) ==
               static_cast<uint16_t>(MessageType::DEVICE_LIST_RESPONSE),
           "device list -> device list response");
    expect(responseTypeFor(MessageType::RECORD_QUERY_REQUEST) ==
               static_cast<uint16_t>(MessageType::RECORD_QUERY_RESPONSE),
           "record query -> record query response");
    expect(responseTypeFor(MessageType::STREAM_START_REQUEST) ==
               static_cast<uint16_t>(MessageType::STREAM_START_RESPONSE),
           "stream start -> stream start response");
    expect(responseTypeFor(MessageType::STREAM_STOP_REQUEST) ==
               static_cast<uint16_t>(MessageType::STREAM_STOP_RESPONSE),
           "stream stop -> stream stop response");
    expect(responseTypeFor(MessageType::RECORD_START_REQUEST) ==
               static_cast<uint16_t>(MessageType::RECORD_START_RESPONSE),
           "record start -> record start response");
    expect(responseTypeFor(MessageType::RECORD_STOP_REQUEST) ==
               static_cast<uint16_t>(MessageType::RECORD_STOP_RESPONSE),
           "record stop -> record stop response");
    expect(responseTypeFor(MessageType::PTZ_CONTROL_REQUEST) ==
               static_cast<uint16_t>(MessageType::PTZ_CONTROL_RESPONSE),
           "ptz control -> ptz control response");
    expect(responseTypeFor(MessageType::REGISTER_REQUEST) == 0,
           "register has no gated response type");

    // 3) buildUnauthorizedValue：设备列表（errorCode 前缀格式）
    {
        std::vector<uint8_t> value;
        expect(buildUnauthorizedValue(MessageType::DEVICE_LIST_REQUEST, value),
               "device list unauthorized value built");
        ErrorCode code = ErrorCode::SUCCESS;
        std::vector<DeviceInfo> devices;
        expect(DeviceProtocol::decodeDeviceListResponse(value, code, devices) &&
                   code == ErrorCode::UNAUTHORIZED && devices.empty(),
               "device list unauthorized decodes to UNAUTHORIZED");
    }

    // 4) buildUnauthorizedValue：录像查询
    {
        std::vector<uint8_t> value;
        expect(buildUnauthorizedValue(MessageType::RECORD_QUERY_REQUEST, value),
               "record query unauthorized value built");
        ErrorCode code = ErrorCode::SUCCESS;
        std::vector<RecordInfo> records;
        expect(RecordProtocol::decodeRecordQueryResponse(value, code, records) &&
                   code == ErrorCode::UNAUTHORIZED && records.empty(),
               "record query unauthorized decodes to UNAUTHORIZED");
    }

    // 5) buildUnauthorizedValue：推流 / 停流（4 字节大端 errorCode）
    {
        std::vector<uint8_t> value;
        expect(buildUnauthorizedValue(MessageType::STREAM_START_REQUEST, value),
               "stream start unauthorized value built");
        expect(decodeInt32BE(value) == static_cast<int32_t>(ErrorCode::UNAUTHORIZED),
               "stream start unauthorized == UNAUTHORIZED");
    }
    {
        std::vector<uint8_t> value;
        expect(buildUnauthorizedValue(MessageType::STREAM_STOP_REQUEST, value),
               "stream stop unauthorized value built");
        expect(decodeInt32BE(value) == static_cast<int32_t>(ErrorCode::UNAUTHORIZED),
               "stream stop unauthorized == UNAUTHORIZED");
    }
    {
        std::vector<uint8_t> value;
        expect(buildUnauthorizedValue(MessageType::RECORD_START_REQUEST, value),
               "record start unauthorized value built");
        expect(decodeInt32BE(value) == static_cast<int32_t>(ErrorCode::UNAUTHORIZED),
               "record start unauthorized == UNAUTHORIZED");
    }
    {
        std::vector<uint8_t> value;
        expect(buildUnauthorizedValue(MessageType::RECORD_STOP_REQUEST, value),
               "record stop unauthorized value built");
        expect(decodeInt32BE(value) == static_cast<int32_t>(ErrorCode::UNAUTHORIZED),
               "record stop unauthorized == UNAUTHORIZED");
    }
    {
        std::vector<uint8_t> value;
        expect(buildUnauthorizedValue(MessageType::PTZ_CONTROL_REQUEST, value),
               "ptz control unauthorized value built");
        expect(decodeInt32BE(value) == static_cast<int32_t>(ErrorCode::UNAUTHORIZED),
               "ptz control unauthorized == UNAUTHORIZED");
    }

    // 6) 非受保护请求不构建 value
    {
        std::vector<uint8_t> value;
        expect(!buildUnauthorizedValue(MessageType::REGISTER_REQUEST, value),
               "register not gated -> no value");
        expect(!buildUnauthorizedValue(MessageType::LOGIN_REQUEST, value),
               "login not gated -> no value");
    }

    std::printf("=== session_policy test end ===\n");
    if (g_failures == 0) {
        std::printf("session_policy test passed.\n");
        return 0;
    }
    std::printf("session_policy test FAILED: %d items.\n", g_failures);
    return 1;
}
