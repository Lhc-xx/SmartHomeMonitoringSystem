#include "AuthHandler.h"
#include "MySQLClient.h"
#include "UserService.h"
#include "config.h"

#include "protocol/AuthProtocol.h"
#include "protocol/ErrorCode.h"
#include "protocol/MessageType.h"
#include "protocol/Protocol.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

bool cleanupUser(smart_home::MySQLClient &mysql, const std::string &username) {
    const std::string escaped = mysql.escape(username);
    return mysql.execute("DELETE FROM user_sessions WHERE user_id IN "
                         "(SELECT id FROM users WHERE username='" + escaped + "')") &&
           mysql.execute("DELETE FROM users WHERE username='" + escaped + "'");
}

TlvMessage makeRequest(uint16_t type, uint32_t requestId,
                       const std::vector<uint8_t> &value) {
    TlvMessage request;
    request.type = type;
    request.requestId = requestId;
    request.value = value;
    return request;
}

} // namespace

int main() {
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

    const std::string username = "b_auth_handler_user";
    const std::string password = "AuthPassword123";
    cleanupUser(mysql, username);
    smart_home::UserService service(mysql);
    smart_home::AuthHandler handler(service);

    std::vector<uint8_t> value;
    if (!AuthProtocol::encodeRegisterRequest(username, password, value)) {
        std::cerr << "[FAIL] register request encode" << std::endl;
        return 1;
    }
    const TlvMessage registerResponse = handler.handle(makeRequest(
        static_cast<uint16_t>(MessageType::REGISTER_REQUEST), 101U, value));
    ErrorCode code = ErrorCode::INTERNAL_ERROR;
    std::string message;
    if (registerResponse.type != static_cast<uint16_t>(MessageType::REGISTER_RESPONSE) ||
        registerResponse.requestId != 101U ||
        !AuthProtocol::decodeRegisterResponse(registerResponse.value, code, message) ||
        code != ErrorCode::SUCCESS) {
        std::cerr << "[FAIL] register handler response" << std::endl;
        cleanupUser(mysql, username);
        return 1;
    }

    if (!AuthProtocol::encodeLoginRequest(username, password, value)) {
        std::cerr << "[FAIL] login request encode" << std::endl;
        cleanupUser(mysql, username);
        return 1;
    }
    const TlvMessage loginResponse = handler.handle(makeRequest(
        static_cast<uint16_t>(MessageType::LOGIN_REQUEST), 102U, value));
    uint64_t userId = 0U;
    std::string token;
    if (loginResponse.type != static_cast<uint16_t>(MessageType::LOGIN_RESPONSE) ||
        loginResponse.requestId != 102U ||
        !AuthProtocol::decodeLoginResponse(loginResponse.value, userId, token, code) ||
        code != ErrorCode::SUCCESS || userId == 0U || token.empty()) {
        std::cerr << "[FAIL] login handler response" << std::endl;
        cleanupUser(mysql, username);
        return 1;
    }

    /* 损坏 value 必须直接返回 INVALID_PACKET，不应访问数据库。 */
    const TlvMessage malformed = handler.handle(makeRequest(
        static_cast<uint16_t>(MessageType::LOGIN_REQUEST), 103U,
        std::vector<uint8_t>(1U, 0xFFU)));
    if (!AuthProtocol::decodeLoginResponse(malformed.value, userId, token, code) ||
        code != ErrorCode::INVALID_PACKET) {
        std::cerr << "[FAIL] malformed login mapping" << std::endl;
        cleanupUser(mysql, username);
        return 1;
    }

    cleanupUser(mysql, username);
    std::cout << "[PASS] AuthHandler register/login" << std::endl;
    return 0;
}
