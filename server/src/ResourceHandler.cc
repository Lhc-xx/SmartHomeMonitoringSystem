#include "ResourceHandler.h"

#include "protocol/DeviceProtocol.h"
#include "protocol/ErrorCode.h"
#include "protocol/MessageType.h"
#include "protocol/RecordProtocol.h"

#include <mysql/mysql.h>

#include <cctype>
#include <string>
#include <vector>

namespace smart_home {

ResourceHandler::ResourceHandler(MySQLClient &mysql)
    : _mysql(mysql), _deviceService(mysql), _recordService(mysql) {}

bool ResourceHandler::hasActiveSession(uint64_t userId, const std::string &token) {
    if (userId == 0U || token.empty()) {
        return false;
    }
    /* 数据库只保存 SHA-512 摘要，明文 token 仅在本次查询内存中存在。 */
    const std::string escapedToken = _mysql.escape(token);
    MYSQL_RES *result = _mysql.query(
        "SELECT id FROM user_sessions WHERE user_id=" + std::to_string(userId) +
        " AND token_hash=SHA2('" + escapedToken +
        "',512) AND revoked_at IS NULL AND expires_at>UTC_TIMESTAMP() LIMIT 1");
    if (result == nullptr) {
        return false;
    }
    const bool active = mysql_num_rows(result) == 1U;
    mysql_free_result(result);
    return active;
}

bool ResourceHandler::isValidDateTime(const std::string &value) {
    if (value.size() != 19U || value[4] != '-' || value[7] != '-' ||
        value[10] != ' ' || value[13] != ':' || value[16] != ':') {
        return false;
    }
    for (std::size_t index = 0U; index < value.size(); ++index) {
        if (index == 4U || index == 7U || index == 10U || index == 13U || index == 16U) {
            continue;
        }
        if (!std::isdigit(static_cast<unsigned char>(value[index]))) {
            return false;
        }
    }
    return true;
}

TlvMessage ResourceHandler::handle(const TlvMessage &request) {
    if (request.type == static_cast<uint16_t>(MessageType::DEVICE_LIST_REQUEST)) {
        return handleDeviceList(request);
    }
    if (request.type == static_cast<uint16_t>(MessageType::RECORD_QUERY_REQUEST)) {
        return handleRecordQuery(request);
    }

    /* 未知资源消息使用设备响应承载 UNKNOWN_MESSAGE，保持已有响应类型约定。 */
    TlvMessage response;
    response.type = static_cast<uint16_t>(MessageType::DEVICE_LIST_RESPONSE);
    response.requestId = request.requestId;
    DeviceProtocol::encodeDeviceListResponse(ErrorCode::UNKNOWN_MESSAGE,
                                             std::vector<DeviceInfo>(), response.value);
    return response;
}

TlvMessage ResourceHandler::handleDeviceList(const TlvMessage &request) {
    uint64_t userId = 0U;
    std::string token;
    ErrorCode code = ErrorCode::INVALID_PACKET;
    std::vector<DeviceInfo> devices;

    if (DeviceProtocol::decodeDeviceListRequest(request.value, userId, token)) {
        if (userId == 0U || token.empty()) {
            code = ErrorCode::INVALID_PARAMETER;
        } else if (!hasActiveSession(userId, token)) {
            code = ErrorCode::UNAUTHORIZED;
        } else if (!_deviceService.listByUser(userId, devices)) {
            code = ErrorCode::DATABASE_ERROR;
        } else {
            code = ErrorCode::SUCCESS;
        }
    }

    TlvMessage response;
    response.type = static_cast<uint16_t>(MessageType::DEVICE_LIST_RESPONSE);
    response.requestId = request.requestId;
    DeviceProtocol::encodeDeviceListResponse(code, devices, response.value);
    return response;
}

TlvMessage ResourceHandler::handleRecordQuery(const TlvMessage &request) {
    uint64_t userId = 0U;
    uint64_t deviceId = 0U;
    std::string token;
    std::string startTime;
    std::string endTime;
    ErrorCode code = ErrorCode::INVALID_PACKET;
    std::vector<RecordInfo> records;

    if (RecordProtocol::decodeRecordQueryRequest(request.value, userId, token, deviceId,
                                                 startTime, endTime)) {
        /* 空时间范围表示查询全部录像，转换为可比较的 DATETIME 边界。 */
        if (startTime.empty()) {
            startTime = "1000-01-01 00:00:00";
        }
        if (endTime.empty()) {
            endTime = "9999-12-31 23:59:59";
        }
        if (userId == 0U || deviceId == 0U || token.empty() ||
            !isValidDateTime(startTime) || !isValidDateTime(endTime) || startTime > endTime) {
            code = ErrorCode::INVALID_PARAMETER;
        } else if (!hasActiveSession(userId, token)) {
            code = ErrorCode::UNAUTHORIZED;
        } else if (!_recordService.queryByDevice(userId, deviceId, startTime, endTime, records)) {
            code = ErrorCode::DATABASE_ERROR;
        } else {
            code = ErrorCode::SUCCESS;
        }
    }

    TlvMessage response;
    response.type = static_cast<uint16_t>(MessageType::RECORD_QUERY_RESPONSE);
    response.requestId = request.requestId;
    RecordProtocol::encodeRecordQueryResponse(code, records, response.value);
    return response;
}

} // namespace smart_home
