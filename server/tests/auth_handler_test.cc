#include "AuthHandler.h"
#include "MySQLClient.h"
#include "UserService.h"
#include "config.h"

#include "protocol/AuthProtocol.h"
#include "protocol/ErrorCode.h"
#include "protocol/MessageType.h"
#include "protocol/Protocol.h"

#include <mysql/mysql.h>

#include <iostream>
#include <string>
#include <vector>

namespace {

/*
 * 将固定的测试用户名包装为作用域资源。
 *
 * AuthHandler 集成测试必须使用真实 MySQL，测试开始前和结束后都要
 * 删除 test_user，避免本次测试污染后续运行，也避免历史残留数据
 * 干扰“首次注册成功”的断言。
 */
class TestUserCleanup {
public:
    TestUserCleanup(smart_home::MySQLClient &mysql,
                    const std::string &username)
        : _mysql(mysql),
          _username(mysql.escape(username)) {}

    ~TestUserCleanup() {
        /*
         * 即使测试中途 return，析构函数仍会执行，确保测试账号不会
         * 留在 users 表中。
         */
        cleanup();
    }

    bool cleanup() {
        return _mysql.execute(
            "DELETE FROM users "
            "WHERE username='"
            + _username
            + "'"
        );
    }

private:
    /* 真实数据库连接由测试入口创建并持有，清理器只借用它。 */
    smart_home::MySQLClient &_mysql;

    /* 已经通过 MySQLClient::escape 转义后的测试用户名。 */
    std::string _username;

};

/*
 * 构造一条完整的 REGISTER_REQUEST 内部消息。
 *
 * TLV 外层只由 TlvMessage 表示；value 的内部编码仍完全交给
 * AuthProtocol，测试不自行重写或假设认证协议格式。
 */
bool makeRegisterRequest(const std::string &username,
                         const std::string &password,
                         uint32_t requestId,
                         TlvMessage &request) {
    request.type = static_cast<uint16_t>(MessageType::REGISTER_REQUEST);
    request.requestId = requestId;

    return AuthProtocol::encodeRegisterRequest(
        username,
        password,
        request.value
    );
}

/*
 * 验证 AuthHandler 返回的是 REGISTER_RESPONSE，并通过 AuthProtocol
 * 解码统一 ErrorCode。这样覆盖了处理层到业务协议的真实输出链路。
 */
bool decodeRegisterResponse(const TlvMessage &response,
                            ErrorCode &code) {
    if(response.type !=
       static_cast<uint16_t>(MessageType::REGISTER_RESPONSE)) {
        return false;
    }

    return AuthProtocol::decodeRegisterResponse(response.value, code);
}

/*
 * 在最终清理后读取 users 表，确认测试账号已经真正不存在。
 *
 * DELETE 成功只能证明 SQL 已被服务器接受；通过 SELECT 回读可以保证
 * 测试不会把残留 test_user 误报告为成功。
 */
bool testUserExists(smart_home::MySQLClient &mysql,
                    const std::string &username,
                    bool &exists) {
    const std::string escapedUsername = mysql.escape(username);
    MYSQL_RES *result = mysql.query(
        "SELECT id FROM users "
        "WHERE username='"
        + escapedUsername
        + "'"
    );

    if(result == nullptr) {
        return false;
    }

    exists = mysql_num_rows(result) > 0;
    mysql_free_result(result);

    return true;
}

} // anonymous namespace

int main() {
    std::cout
        << "================================"
        << std::endl;

    std::cout
        << "AuthHandler Test"
        << std::endl;

    std::cout
        << "================================"
        << std::endl;

    /*
     * 集成测试复用服务器本地配置，因此数据库连接参数不写入测试代码。
     * 真实密码仍只保留在被 Git 忽略的 server.conf 中。
     */
    smart_home::Config config;
    if(!config.load("server/conf/server.conf")) {
        std::cerr
            << "[FAIL] config load failed"
            << std::endl;
        return 1;
    }

    /* 建立真实 MySQL 连接，禁止用 mock 替代 UserService 的依赖。 */
    smart_home::MySQLClient mysql;
    if(!mysql.connect(
            config.mysqlHost(),
            config.mysqlUser(),
            config.mysqlPassword(),
            config.mysqlDatabase(),
            static_cast<unsigned int>(config.mysqlPort())
        )) {
        std::cerr
            << "[FAIL] MySQL connect failed: "
            << mysql.lastError()
            << std::endl;
        return 1;
    }

    /*
     * AuthHandler 仅借用 UserService；UserService 再借用已连接的
     * MySQLClient，完整复现服务器未来的依赖方向。
     */
    smart_home::UserService service(mysql);
    smart_home::AuthHandler handler(service);

    const std::string username = "test_user";
    const std::string password = "123456";
    TestUserCleanup cleanup(mysql, username);

    /* 测试数据清理失败时，不能继续执行会受旧数据影响的断言。 */
    if(!cleanup.cleanup()) {
        std::cerr
            << "[FAIL] test data cleanup failed: "
            << mysql.lastError()
            << std::endl;
        return 1;
    }

    /*
     * 测试 1：有效 REGISTER_REQUEST 应被解码、注册并编码为成功响应。
     */
    TlvMessage request;
    if(!makeRegisterRequest(username, password, 1001, request)) {
        std::cerr
            << "[FAIL] register request encode failed"
            << std::endl;
        return 1;
    }

    const TlvMessage successResponse = handler.handle(request);
    ErrorCode code = ErrorCode::INTERNAL_ERROR;
    if(!decodeRegisterResponse(successResponse, code) ||
       successResponse.requestId != request.requestId ||
       code != ErrorCode::SUCCESS) {
        std::cerr
            << "[FAIL] register success"
            << std::endl;
        return 1;
    }

    std::cout
        << "[PASS] register success"
        << std::endl;

    /*
     * 测试 2：同一 REGISTER_REQUEST 再次进入真实 UserService 时，
     * 应由业务层返回 USER_ALREADY_EXISTS，并由处理层原样编码。
     */
    const TlvMessage duplicateResponse = handler.handle(request);
    if(!decodeRegisterResponse(duplicateResponse, code) ||
       duplicateResponse.requestId != request.requestId ||
       code != ErrorCode::USER_ALREADY_EXISTS) {
        std::cerr
            << "[FAIL] duplicate username"
            << std::endl;
        return 1;
    }

    std::cout
        << "[PASS] duplicate username"
        << std::endl;

    /*
     * 测试 3：value 只声明用户名长度却没有提供完整用户名和密码长度，
     * 这是非法认证载荷，必须在进入 UserService 之前返回 INVALID_PACKET。
     */
    TlvMessage invalidRequest;
    invalidRequest.type =
        static_cast<uint16_t>(MessageType::REGISTER_REQUEST);
    invalidRequest.requestId = 1002;
    invalidRequest.value.push_back(0x00);
    invalidRequest.value.push_back(0x09);
    invalidRequest.value.push_back('b');
    invalidRequest.value.push_back('a');
    invalidRequest.value.push_back('d');

    const TlvMessage invalidResponse = handler.handle(invalidRequest);
    if(!decodeRegisterResponse(invalidResponse, code) ||
       invalidResponse.requestId != invalidRequest.requestId ||
       code != ErrorCode::INVALID_PACKET) {
        std::cerr
            << "[FAIL] invalid packet"
            << std::endl;
        return 1;
    }

    std::cout
        << "[PASS] invalid packet"
        << std::endl;

    /*
     * 在成功输出前显式清理，若 DELETE 失败则将失败暴露给调用者，
     * 而不是把污染的数据库状态伪装成通过。
     */
    bool userExists = false;
    if(!cleanup.cleanup() ||
       !testUserExists(mysql, username, userExists) ||
       userExists) {
        std::cerr
            << "[FAIL] final test data cleanup failed: "
            << mysql.lastError()
            << std::endl;
        return 1;
    }

    std::cout
        << "AuthHandler test passed."
        << std::endl;

    return 0;
}
