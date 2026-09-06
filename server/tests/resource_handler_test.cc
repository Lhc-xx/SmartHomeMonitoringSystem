#include "MySQLClient.h"
#include "ResourceHandler.h"
#include "UserService.h"
#include "config.h"

#include "protocol/DeviceProtocol.h"
#include "protocol/ErrorCode.h"
#include "protocol/MessageType.h"
#include "protocol/RecordProtocol.h"

#include <mysql/mysql.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

/*
 * 测试结束时按外键依赖顺序清理会话、录像、设备和用户，
 * 这样重复运行集成测试不会因为上一次残留数据而误判。
 */
bool cleanupUser(smart_home::MySQLClient &mysql, const std::string &username) {
    const std::string escaped = mysql.escape(username);
    return mysql.execute("DELETE FROM user_sessions WHERE user_id IN "
                         "(SELECT id FROM users WHERE username='" + escaped + "')") &&
           mysql.execute("DELETE FROM records WHERE device_id IN "
                         "(SELECT id FROM devices WHERE user_id IN "
                         "(SELECT id FROM users WHERE username='" + escaped + "'))") &&
           mysql.execute("DELETE FROM devices WHERE user_id IN "
                         "(SELECT id FROM users WHERE username='" + escaped + "')") &&
           mysql.execute("DELETE FROM users WHERE username='" + escaped + "'");
}

/* 查询刚注册用户的数据库 id，后续设备归属测试需要使用该 id。 */
bool findUserId(smart_home::MySQLClient &mysql, const std::string &username,
                uint64_t &userId) {
    MYSQL_RES *result = mysql.query("SELECT id FROM users WHERE username='" +
                                    mysql.escape(username) + "'");
    if (result == nullptr) {
        return false;
    }
    MYSQL_ROW row = mysql_fetch_row(result);
    const bool found = row != nullptr && row[0] != nullptr;
    if (found) {
        userId = static_cast<uint64_t>(std::strtoull(row[0], nullptr, 10));
    }
    mysql_free_result(result);
    return found;
}

} // namespace

int main() {
    /* 资源处理测试依赖本地 MySQL 配置；缺少配置时明确报告环境阻断。 */
    smart_home::Config config;
    if (!config.load("server/conf/server.conf")) {
        std::cerr << "[FAIL] config load failed" << std::endl;
        return 1;
    }
    smart_home::MySQLClient mysql;
    if (!mysql.connect(config.mysqlHost(), config.mysqlUser(), config.mysqlPassword(),
                       config.mysqlDatabase(), static_cast<unsigned int>(config.mysqlPort()))) {
        std::cerr << "[FAIL] MySQL connect failed: " << mysql.lastError() << std::endl;
        return 1;
    }

    /* 使用专用用户名，避免与其他测试的固定数据相互干扰。 */
    const std::string owner = "b_resource_owner";
    const std::string other = "b_resource_other";
    const std::string password = "ResourcePassword123";
    cleanupUser(mysql, owner);
    cleanupUser(mysql, other);
    smart_home::UserService users(mysql);
    if (users.registerUser(owner, password) != ErrorCode::SUCCESS ||
        users.registerUser(other, password) != ErrorCode::SUCCESS) {
        std::cerr << "[FAIL] registration setup" << std::endl;
        cleanupUser(mysql, owner);
        cleanupUser(mysql, other);
        return 1;
    }
    const smart_home::LoginResult login = users.loginUser(owner, password);
    uint64_t ownerId = 0U;
    uint64_t otherId = 0U;
    if (login.code != ErrorCode::SUCCESS || login.token.empty() ||
        !findUserId(mysql, owner, ownerId) || !findUserId(mysql, other, otherId)) {
        std::cerr << "[FAIL] login setup" << std::endl;
        cleanupUser(mysql, owner);
        cleanupUser(mysql, other);
        return 1;
    }

    /*
     * 数据库 status 是 INT，1 表示在线；DeviceService 读取后再按协议字符串发送。
     * 测试数据必须遵守表字段类型，避免严格 SQL 模式拒绝文本状态值。
     */
    if (!mysql.execute("INSERT INTO devices(user_id,device_name,device_type,status) VALUES(" +
                       std::to_string(ownerId) + ",'owner-device','camera',1)") ||
        !mysql.execute("INSERT INTO devices(user_id,device_name,device_type,status) VALUES(" +
                       std::to_string(otherId) + ",'other-device','camera',1)")) {
        std::cerr << "[FAIL] device setup" << std::endl;
        cleanupUser(mysql, owner);
        cleanupUser(mysql, other);
        return 1;
    }

    MYSQL_RES *result = mysql.query("SELECT id FROM devices WHERE user_id=" +
                                    std::to_string(ownerId) +
                                    " AND device_name='owner-device'");
    if (result == nullptr) {
        cleanupUser(mysql, owner);
        cleanupUser(mysql, other);
        return 1;
    }
    MYSQL_ROW row = mysql_fetch_row(result);
    const uint64_t deviceId = row != nullptr && row[0] != nullptr
                                  ? static_cast<uint64_t>(std::strtoull(row[0], nullptr, 10))
                                  : 0U;
    mysql_free_result(result);
    if (deviceId == 0U) {
        cleanupUser(mysql, owner);
        cleanupUser(mysql, other);
        return 1;
    }

    /* 通过真实 handler 验证 token 鉴权、设备隔离以及空录像查询。 */
    smart_home::ResourceHandler handler(mysql);
    TlvMessage request;
    request.type = static_cast<uint16_t>(MessageType::DEVICE_LIST_REQUEST);
    request.requestId = 201U;
    DeviceProtocol::encodeDeviceListRequest(ownerId, login.token, request.value);
    const TlvMessage response = handler.handle(request);
    ErrorCode code = ErrorCode::INTERNAL_ERROR;
    std::vector<DeviceInfo> devices;
    const bool deviceOk = response.type ==
                              static_cast<uint16_t>(MessageType::DEVICE_LIST_RESPONSE) &&
                          response.requestId == request.requestId &&
                          DeviceProtocol::decodeDeviceListResponse(response.value, code, devices) &&
                          code == ErrorCode::SUCCESS && devices.size() == 1U &&
                          devices[0].id == deviceId;

    request.type = static_cast<uint16_t>(MessageType::RECORD_QUERY_REQUEST);
    request.requestId = 202U;
    RecordProtocol::encodeRecordQueryRequest(ownerId, login.token, deviceId,
                                             std::string(), std::string(), request.value);
    const TlvMessage recordResponse = handler.handle(request);
    std::vector<RecordInfo> records;
    const bool recordOk = recordResponse.type ==
                              static_cast<uint16_t>(MessageType::RECORD_QUERY_RESPONSE) &&
                          RecordProtocol::decodeRecordQueryResponse(recordResponse.value, code,
                                                                     records) &&
                          code == ErrorCode::SUCCESS && records.empty();

    cleanupUser(mysql, owner);
    cleanupUser(mysql, other);
    if (!deviceOk || !recordOk) {
        std::cerr << "[FAIL] resource ownership or empty-query behavior" << std::endl;
        return 1;
    }
    std::cout << "[PASS] ResourceHandler device/record metadata" << std::endl;
    return 0;
}
