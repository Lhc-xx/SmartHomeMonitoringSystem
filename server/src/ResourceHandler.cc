#include "ResourceHandler.h"

#include "protocol/DeviceProtocol.h"
#include "protocol/ErrorCode.h"
#include "protocol/MessageType.h"
#include "protocol/RecordProtocol.h"

#include <mysql/mysql.h>

#include <openssl/evp.h>

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace smart_home {

namespace {

/*
 * 会话表保存的是 SHA-512(token) 的十六进制摘要。摘要在服务端进程内计算，
 * 再作为 token_hash 条件发送给 MySQL，避免把明文 token 放进 SQL/general log。
 */
bool sha512Hex(const std::string &input, std::string &output) {
    unsigned char digest[EVP_MAX_MD_SIZE] = {0};
    unsigned int digestLength = 0U;
    if (EVP_Digest(input.data(), input.size(), digest, &digestLength,
                   EVP_sha512(), nullptr) != 1 || digestLength != 64U) {
        return false;
    }
    static const char kHex[] = "0123456789abcdef";
    output.clear();
    output.reserve(static_cast<std::size_t>(digestLength) * 2U);
    for (unsigned int index = 0U; index < digestLength; ++index) {
        output.push_back(kHex[(digest[index] >> 4U) & 0x0FU]);
        output.push_back(kHex[digest[index] & 0x0FU]);
    }
    return true;
}

/* 从固定位置读取十进制数字，日期校验不依赖本地化或未定义的 atoi 行为。 */
bool readDigits(const std::string &value, std::size_t offset,
                std::size_t count, int &number) {
    if (offset > value.size() || count > value.size() - offset) {
        return false;
    }
    number = 0;
    for (std::size_t index = 0U; index < count; ++index) {
        const unsigned char ch = static_cast<unsigned char>(value[offset + index]);
        if (!std::isdigit(ch)) {
            return false;
        }
        number = number * 10 + static_cast<int>(ch - static_cast<unsigned char>('0'));
    }
    return true;
}

} // namespace

ResourceHandler::ResourceHandler(MySQLClient &mysql)
    : _mysql(mysql), _deviceService(mysql), _recordService(mysql) {}

bool ResourceHandler::hasActiveSession(uint64_t userId, const std::string &token) {
    if (userId == 0U || token.empty()) {
        return false;
    }
    std::string tokenHash;
    if (!sha512Hex(token, tokenHash)) {
        return false;
    }
    /* SQL 只接收摘要，避免 MySQL 查询日志或审计插件记录明文 token。 */
    const std::string escapedTokenHash = _mysql.escape(tokenHash);
    MYSQL_RES *result = _mysql.query(
        "SELECT id FROM user_sessions WHERE user_id=" + std::to_string(userId) +
        " AND token_hash='" + escapedTokenHash +
        "' AND revoked_at IS NULL AND expires_at>UTC_TIMESTAMP() LIMIT 1");
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
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (!readDigits(value, 0U, 4U, year)
        || !readDigits(value, 5U, 2U, month)
        || !readDigits(value, 8U, 2U, day)
        || !readDigits(value, 11U, 2U, hour)
        || !readDigits(value, 14U, 2U, minute)
        || !readDigits(value, 17U, 2U, second)) {
        return false;
    }
    /* MySQL DATETIME 的安全范围为 1000-01-01 至 9999-12-31。 */
    if (year < 1000 || year > 9999 || month < 1 || month > 12
        || hour > 23 || minute > 59 || second > 59) {
        return false;
    }
    const bool leapYear = (year % 400 == 0)
        || (year % 4 == 0 && year % 100 != 0);
    static const int kDaysInMonth[] =
        {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const int maxDay = kDaysInMonth[month] + (month == 2 && leapYear ? 1 : 0);
    return day >= 1 && day <= maxDay;
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
