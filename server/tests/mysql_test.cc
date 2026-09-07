#include "MySQLClient.h"
#include "config.h"

#include <mysql/mysql.h>

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace {

/*
 * 用第二条独立连接杀掉被测连接，模拟 MySQL wait_timeout/网络中断。
 * 这比等待数小时更稳定，也不会修改服务器配置。
 */
bool killConnection(const smart_home::Config &config, unsigned long connectionId) {
    MYSQL *control = mysql_init(nullptr);
    if (control == nullptr) {
        return false;
    }
    if (mysql_real_connect(control, config.mysqlHost().c_str(), config.mysqlUser().c_str(),
                           config.mysqlPassword().c_str(), config.mysqlDatabase().c_str(),
                           static_cast<unsigned int>(config.mysqlPort()), nullptr, 0U) == nullptr) {
        mysql_close(control);
        return false;
    }

    std::ostringstream statement;
    statement << "KILL " << connectionId;
    const bool ok = mysql_query(control, statement.str().c_str()) == 0;
    mysql_close(control);
    return ok;
}

} // namespace

int main() {
    /* 集成测试从本地 server.conf 读取凭据，仓库中不保存真实密码。 */
    smart_home::Config config;
    if (!config.load("server/conf/server.conf")) {
        std::cerr << "[FAIL] config load failed" << std::endl;
        return 1;
    }

    smart_home::MySQLClient mysql;
    if (!mysql.connect(config.mysqlHost(), config.mysqlUser(), config.mysqlPassword(),
                       config.mysqlDatabase(), static_cast<unsigned int>(config.mysqlPort()))) {
        std::cerr << "[FAIL] connect: " << mysql.lastError() << std::endl;
        return 1;
    }

    MYSQL_RES *result = mysql.query("SELECT 1");
    if (result == nullptr) {
        std::cerr << "[FAIL] SELECT 1: " << mysql.lastError() << std::endl;
        return 1;
    }
    MYSQL_ROW row = mysql_fetch_row(result);
    const bool selectOk = row != nullptr && row[0] != nullptr && std::string(row[0]) == "1";
    mysql_free_result(result);
    if (!selectOk) {
        std::cerr << "[FAIL] SELECT 1 result" << std::endl;
        return 1;
    }

    /*
     * 回归场景：服务端主动断开当前连接，下一次查询必须自动重连并成功。
     * 这是线上空闲超过 wait_timeout 后注册/登录失败的直接复现。
     */
    result = mysql.query("SELECT CONNECTION_ID()");
    if (result == nullptr) {
        std::cerr << "[FAIL] CONNECTION_ID: " << mysql.lastError() << std::endl;
        return 1;
    }
    row = mysql_fetch_row(result);
    const unsigned long connectionId =
        row != nullptr && row[0] != nullptr
            ? std::strtoul(row[0], nullptr, 10)
            : 0UL;
    mysql_free_result(result);
    if (connectionId == 0UL || !killConnection(config, connectionId)) {
        std::cerr << "[FAIL] could not simulate a dropped MySQL connection" << std::endl;
        return 1;
    }

    result = mysql.query("SELECT 1");
    if (result == nullptr) {
        std::cerr << "[FAIL] automatic reconnect: " << mysql.lastError() << std::endl;
        return 1;
    }
    row = mysql_fetch_row(result);
    const bool reconnectOk = row != nullptr && row[0] != nullptr && std::string(row[0]) == "1";
    mysql_free_result(result);
    if (!reconnectOk) {
        std::cerr << "[FAIL] automatic reconnect result" << std::endl;
        return 1;
    }

    /* 临时 InnoDB 表验证 rollback 和 commit 的可见性。 */
    if (!mysql.execute("CREATE TEMPORARY TABLE mysql_client_transaction_test "
                       "(id INT PRIMARY KEY) ENGINE=InnoDB") ||
        !mysql.beginTransaction() ||
        !mysql.execute("INSERT INTO mysql_client_transaction_test(id) VALUES(1)") ||
        !mysql.rollback()) {
        std::cerr << "[FAIL] rollback path: " << mysql.lastError() << std::endl;
        return 1;
    }
    result = mysql.query("SELECT COUNT(*) FROM mysql_client_transaction_test");
    if (result == nullptr) {
        std::cerr << "[FAIL] rollback query" << std::endl;
        return 1;
    }
    row = mysql_fetch_row(result);
    const bool rollbackOk = row != nullptr && row[0] != nullptr && std::string(row[0]) == "0";
    mysql_free_result(result);
    if (!rollbackOk) {
        std::cerr << "[FAIL] rollback did not discard row" << std::endl;
        return 1;
    }

    if (!mysql.beginTransaction() ||
        !mysql.execute("INSERT INTO mysql_client_transaction_test(id) VALUES(2)") ||
        !mysql.commit()) {
        std::cerr << "[FAIL] commit path: " << mysql.lastError() << std::endl;
        return 1;
    }
    std::cout << "[PASS] MySQL query and transaction" << std::endl;
    return 0;
}
