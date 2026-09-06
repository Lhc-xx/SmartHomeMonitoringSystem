#include "MySQLClient.h"
#include "UserService.h"
#include "config.h"

#include "protocol/ErrorCode.h"

#include <mysql/mysql.h>

#include <ctime>
#include <iostream>
#include <string>

namespace {

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

    const std::string username = "b_user_service_" + std::to_string(std::time(nullptr));
    const std::string password = "PasswordForTest123";
    cleanupUser(mysql, username);
    smart_home::UserService service(mysql);

    if (service.registerUser(username, password) != ErrorCode::SUCCESS ||
        service.registerUser(username, password) != ErrorCode::USER_ALREADY_EXISTS) {
        std::cerr << "[FAIL] register or duplicate registration" << std::endl;
        cleanupUser(mysql, username);
        return 1;
    }

    /* 直接检查数据库字段，确认密码没有以明文写入。 */
    MYSQL_RES *result = mysql.query(
        "SELECT password_hash,salt FROM users WHERE username='" + mysql.escape(username) + "'");
    if (result == nullptr) {
        std::cerr << "[FAIL] credential query" << std::endl;
        cleanupUser(mysql, username);
        return 1;
    }
    MYSQL_ROW row = mysql_fetch_row(result);
    const bool credentialOk = row != nullptr && row[0] != nullptr && row[1] != nullptr &&
                              std::string(row[0]) != password && !std::string(row[1]).empty();
    mysql_free_result(result);
    if (!credentialOk) {
        std::cerr << "[FAIL] plaintext credential check" << std::endl;
        cleanupUser(mysql, username);
        return 1;
    }

    const smart_home::LoginResult login = service.loginUser(username, password);
    const smart_home::LoginResult wrongPassword = service.loginUser(username, "wrong-password");
    const smart_home::LoginResult missingUser = service.loginUser("not-existing-user", password);
    if (login.code != ErrorCode::SUCCESS || login.userId == 0U || login.token.empty() ||
        wrongPassword.code != ErrorCode::PASSWORD_ERROR ||
        missingUser.code != ErrorCode::USER_NOT_FOUND) {
        std::cerr << "[FAIL] login result mapping" << std::endl;
        cleanupUser(mysql, username);
        return 1;
    }

    /* 会话表只允许 token 摘要落库，明文 token 不应出现在 token_hash 字段。 */
    result = mysql.query("SELECT token_hash FROM user_sessions WHERE user_id=" +
                         std::to_string(login.userId));
    if (result == nullptr) {
        std::cerr << "[FAIL] session query" << std::endl;
        cleanupUser(mysql, username);
        return 1;
    }
    row = mysql_fetch_row(result);
    const bool tokenHashOk = row != nullptr && row[0] != nullptr &&
                             std::string(row[0]) != login.token &&
                             std::string(row[0]).size() == 128U;
    mysql_free_result(result);
    cleanupUser(mysql, username);
    if (!tokenHashOk) {
        std::cerr << "[FAIL] token plaintext persisted" << std::endl;
        return 1;
    }

    std::cout << "[PASS] UserService register/login" << std::endl;
    return 0;
}
