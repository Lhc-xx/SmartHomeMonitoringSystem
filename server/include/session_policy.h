#ifndef SESSION_POLICY_H
#define SESSION_POLICY_H

// session_policy.h —— 连接级登录态相关纯策略（角色 A）
//
// 把「哪些消息需要登录」「请求类型如何映射到响应类型」「未登录/会话失效时
// 如何构建 UNAUTHORIZED 响应」这些无副作用决策抽成纯函数，方便不依赖
// MySQL / log4cpp / epoll 的单元测试直接验证，Reactor 也复用同一份逻辑。

#include "protocol/DeviceProtocol.h"
#include "protocol/ErrorCode.h"
#include "protocol/MessageType.h"
#include "protocol/RecordProtocol.h"

#include <arpa/inet.h>   // htonl
#include <cstdint>
#include <vector>

namespace smart_home {

// 需要登录才能访问的消息类型：设备列表 / 录像查询 / 推流 / 停流。
inline bool requiresAuth(MessageType type) {
    return type == MessageType::DEVICE_LIST_REQUEST ||
           type == MessageType::RECORD_QUERY_REQUEST ||
           type == MessageType::STREAM_START_REQUEST ||
           type == MessageType::STREAM_STOP_REQUEST;
}

// 请求类型 -> 响应类型；未知类型返回 0。
inline uint16_t responseTypeFor(MessageType type) {
    switch (type) {
    case MessageType::DEVICE_LIST_REQUEST:
        return static_cast<uint16_t>(MessageType::DEVICE_LIST_RESPONSE);
    case MessageType::RECORD_QUERY_REQUEST:
        return static_cast<uint16_t>(MessageType::RECORD_QUERY_RESPONSE);
    case MessageType::STREAM_START_REQUEST:
        return static_cast<uint16_t>(MessageType::STREAM_START_RESPONSE);
    case MessageType::STREAM_STOP_REQUEST:
        return static_cast<uint16_t>(MessageType::STREAM_STOP_RESPONSE);
    default:
        return 0;
    }
}

// 按响应类型使用对应协议编码，构建 UNAUTHORIZED 响应的 value。
// 设备/录像使用各自的 errorCode 前缀格式；流媒体使用 4 字节大端 errorCode。
inline bool buildUnauthorizedValue(MessageType requestType,
                                   std::vector<uint8_t> &value) {
    switch (requestType) {
    case MessageType::DEVICE_LIST_REQUEST:
        return DeviceProtocol::encodeDeviceListResponse(
            ErrorCode::UNAUTHORIZED, std::vector<DeviceInfo>(), value);
    case MessageType::RECORD_QUERY_REQUEST:
        return RecordProtocol::encodeRecordQueryResponse(
            ErrorCode::UNAUTHORIZED, std::vector<RecordInfo>(), value);
    case MessageType::STREAM_START_REQUEST:
    case MessageType::STREAM_STOP_REQUEST: {
        int32_t code = htonl(static_cast<int32_t>(ErrorCode::UNAUTHORIZED));
        uint8_t *p = reinterpret_cast<uint8_t *>(&code);
        value.assign(p, p + 4);
        return true;
    }
    default:
        return false;
    }
}

} // namespace smart_home

#endif // SESSION_POLICY_H
