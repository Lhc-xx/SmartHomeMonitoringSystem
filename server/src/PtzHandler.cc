// PtzHandler.cc —— 云台控制转发（角色 D 维护）

#include "PtzHandler.h"

#include <arpa/inet.h>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <sstream>
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

std::string trim(const std::string &value) {
    size_t first = 0;
    while (first < value.size()
           && std::isspace(static_cast<unsigned char>(value[first])) != 0) {
        ++first;
    }
    size_t last = value.size();
    while (last > first
           && std::isspace(static_cast<unsigned char>(value[last - 1])) != 0) {
        --last;
    }
    return value.substr(first, last - first);
}

bool isIpv4Literal(const std::string &host) {
    struct in_addr address{};
    return !host.empty() && inet_pton(AF_INET, host.c_str(), &address) == 1;
}

bool parseAndValidateHttpUrl(const std::string &url, std::string &host) {
    if (url.find_first_of("?#\\") != std::string::npos) {
        return false;
    }
    const size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) {
        return false;
    }
    std::string scheme = url.substr(0, schemeEnd);
    for (char &ch : scheme) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    if (scheme != "http" && scheme != "https") {
        return false;
    }

    const size_t authorityStart = schemeEnd + 3;
    const size_t pathStart = url.find('/', authorityStart);
    const std::string authority = url.substr(
        authorityStart,
        pathStart == std::string::npos ? std::string::npos : pathStart - authorityStart);
    if (authority.empty() || authority.find_first_of("?#\\@") != std::string::npos) {
        return false;
    }

    std::string hostPart = authority;
    const size_t colon = authority.rfind(':');
    if (colon != std::string::npos) {
        /* 仅允许 host:port；IPv6 字面量和多余冒号一律拒绝。 */
        if (colon == 0 || authority.find(':') != colon) {
            return false;
        }
        const std::string portText = authority.substr(colon + 1);
        if (portText.empty()) {
            return false;
        }
        char *end = nullptr;
        const long port = std::strtol(portText.c_str(), &end, 10);
        if (end == portText.c_str() || *end != '\0' || port < 1 || port > 65535) {
            return false;
        }
        hostPart = authority.substr(0, colon);
    }
    if (!isIpv4Literal(hostPart)) {
        return false;
    }
    host = hostPart;
    return true;
}

bool isAllowedDirection(const std::string &direction, const std::string &move) {
    if (move == "stop") {
        return direction == "stop";
    }
    if (move != "start") {
        return false;
    }
    return direction == "up" || direction == "down"
        || direction == "left" || direction == "right"
        || direction == "up-left" || direction == "up-right"
        || direction == "down-left" || direction == "down-right";
}

} // namespace

PtzHandler::PtzHandler(const std::string &secret, const std::string &allowedHostsCsv)
    : _client(secret) {
    std::stringstream stream(allowedHostsCsv);
    std::string item;
    while (std::getline(stream, item, ',')) {
        const std::string host = trim(item);
        if (isIpv4Literal(host)) {
            _allowedHosts.insert(host);
        }
    }
}

bool PtzHandler::isAllowedRequest(const std::string &cameraUrl,
                                  const std::string &direction,
                                  const std::string &move) const {
    if (!isAllowedDirection(direction, move)) {
        return false;
    }
    std::string host;
    if (!parseAndValidateHttpUrl(cameraUrl, host)) {
        return false;
    }
    /* 白名单为空时 fail closed，禁止服务端被用作任意内网 HTTP 代理。 */
    return _allowedHosts.find(host) != _allowedHosts.end();
}

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
    } else if (!isAllowedRequest(cameraUrl, direction, move)) {
        errorCode = static_cast<int32_t>(ErrorCode::INVALID_PARAMETER);
    } else if (!_client.control(cameraUrl, direction, move)) {
        errorCode = static_cast<int32_t>(ErrorCode::INTERNAL_ERROR);
        LOG_WARN(("ptz control failed, direction=" + direction
                  + " move=" + move).c_str());
    } else {
        LOG_INFO(("ptz control ok, direction=" + direction
                  + " move=" + move).c_str());
    }

    const uint32_t code = htonl(static_cast<uint32_t>(errorCode));
    const uint8_t *p = reinterpret_cast<const uint8_t *>(&code);
    resp.value.assign(p, p + 4);
    return resp;
}

} // namespace smart_home
