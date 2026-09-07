// PtzHandler.cc —— 云台控制转发（角色 D 维护）

#include "PtzHandler.h"

#include <arpa/inet.h>
#include <cstdint>
#include <vector>

#include "logger.h"
#include "protocol/ErrorCode.h"
#include "protocol/MessageType.h"

namespace smart_home {

namespace {

bool readLengthString(const std::vector<uint8_t> &value, size_t &offset, std::string &out) {
    if (offset + 2 > value.size()) {
        return false;
    }
    const uint16_t len = static_cast<uint16_t>((value[offset] << 8) | value[offset + 1]);
    offset += 2;
    if (offset + len > value.size()) {
        return false;
    }
    out.assign(reinterpret_cast<const char *>(value.data() + offset), len);
    offset += len;
    return true;
}

} // namespace

PtzHandler::PtzHandler(const std::string &secret) : _client(secret) {}

TlvMessage PtzHandler::handle(const TlvMessage &msg) {
    TlvMessage resp;
    resp.version = PROTOCOL_VERSION;
    resp.requestId = msg.requestId;
    resp.type = static_cast<uint16_t>(MessageType::PTZ_CONTROL_RESPONSE);

    int32_t errorCode = static_cast<int32_t>(ErrorCode::SUCCESS);

    std::string cameraUrl;
    std::string direction;
    std::string move;
    size_t offset = 0;
    if (!readLengthString(msg.value, offset, cameraUrl)
        || !readLengthString(msg.value, offset, direction)
        || !readLengthString(msg.value, offset, move)
        || offset != msg.value.size()) {
        errorCode = static_cast<int32_t>(ErrorCode::INVALID_PARAMETER);
    } else if (cameraUrl.empty() || direction.empty() || move.empty()) {
        errorCode = static_cast<int32_t>(ErrorCode::INVALID_PARAMETER);
    } else if (!_client.control(cameraUrl, direction, move)) {
        errorCode = static_cast<int32_t>(ErrorCode::INTERNAL_ERROR);
        LOG_WARN(("ptz control failed, url=" + cameraUrl + " direction=" + direction
                  + " move=" + move).c_str());
    } else {
        LOG_INFO(("ptz control ok, url=" + cameraUrl + " direction=" + direction
                  + " move=" + move).c_str());
    }

    const uint32_t code = htonl(static_cast<uint32_t>(errorCode));
    const uint8_t *p = reinterpret_cast<const uint8_t *>(&code);
    resp.value.assign(p, p + 4);
    return resp;
}

} // namespace smart_home
