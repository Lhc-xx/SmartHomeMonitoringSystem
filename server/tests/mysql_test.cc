#include "MySQLClient.h"
#include "config.h"

#include <mysql/mysql.h>

#include <iostream>
#include <string>

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
