#include "config.h"
#include "MySQLClient.h"
#include "UserService.h"

#include "protocol/ErrorCode.h"

#include <mysql/mysql.h>

#include <ctime>
#include <iostream>
#include <sstream>
#include <string>

#include <unistd.h>


/*
 * 创建一个每次测试都不同的用户名。
 *
 * 防止：
 *
 * 上一次测试留下的数据
 * 影响下一次测试。
 *
 * 例如：
 *
 * b_test_1720000000_1234
 */
static std::string
makeTestUsername()
{
    std::ostringstream oss;


    oss
        << "b_test_"
        << std::time(nullptr)
        << "_"
        << getpid();


    return oss.str();
}



int main()
{
    std::cout
        << "=========================================="
        << std::endl;

    std::cout
        << "       UserService Register Test"
        << std::endl;

    std::cout
        << "=========================================="
        << std::endl;



    // ============================================================
    // 第1步：加载配置
    // ============================================================

    smart_home::Config config;


    if(!config.load(
            "server/conf/server.conf"
        ))
    {
        std::cerr
            << "[FAIL] config load failed"
            << std::endl;

        return 1;
    }


    std::cout
        << "[PASS] config loaded"
        << std::endl;



    // ============================================================
    // 第2步：连接MySQL
    // ============================================================

    smart_home::MySQLClient mysql;


    if(!mysql.connect(
            config.mysqlHost(),
            config.mysqlUser(),
            config.mysqlPassword(),
            config.mysqlDatabase(),
            static_cast<unsigned int>(
                config.mysqlPort()
            )
        ))
    {
        std::cerr
            << "[FAIL] MySQL connect failed: "
            << mysql.lastError()
            << std::endl;

        return 1;
    }


    std::cout
        << "[PASS] MySQL connected"
        << std::endl;



    // ============================================================
    // 第3步：创建UserService
    // ============================================================

    smart_home::UserService
        userService(mysql);



    // ============================================================
    // 准备测试账号
    // ============================================================

    const std::string username =
        makeTestUsername();


    const std::string password =
        "TestPassword123";


    /*
     * 防止极端情况下同名数据残留。
     */
    std::string escapedUsername =
        mysql.escape(username);


    mysql.execute(
        "DELETE FROM users "
        "WHERE username='"
        + escapedUsername
        + "'"
    );



    // ============================================================
    // 测试1：
    // 正常注册
    // ============================================================

    ErrorCode code =
        userService.registerUser(
            username,
            password
        );


    if(code != ErrorCode::SUCCESS)
    {
        std::cerr
            << "[FAIL] normal register failed, code = "
            << static_cast<int>(code)
            << std::endl;

        return 1;
    }


    std::cout
        << "[PASS] normal register"
        << std::endl;



    // ============================================================
    // 测试2：
    // 重复用户名
    // ============================================================

    code =
        userService.registerUser(
            username,
            password
        );


    if(code !=
       ErrorCode::USER_ALREADY_EXISTS)
    {
        std::cerr
            << "[FAIL] duplicate user test failed, code = "
            << static_cast<int>(code)
            << std::endl;

        return 1;
    }


    std::cout
        << "[PASS] duplicate username"
        << std::endl;



    // ============================================================
    // 测试3：
    // 非法参数
    // ============================================================

    code =
        userService.registerUser(
            "",
            password
        );


    if(code !=
       ErrorCode::INVALID_PARAMETER)
    {
        std::cerr
            << "[FAIL] invalid parameter test failed"
            << std::endl;

        return 1;
    }


    std::cout
        << "[PASS] invalid parameter"
        << std::endl;



    // ============================================================
    // 测试4：
    // 验证数据库没有保存明文密码
    // ============================================================

    MYSQL_RES *result =
        mysql.query(
            "SELECT password_hash,salt "
            "FROM users "
            "WHERE username='"
            + escapedUsername
            + "'"
        );


    if(result == nullptr)
    {
        std::cerr
            << "[FAIL] query registered user failed"
            << std::endl;

        return 1;
    }


    MYSQL_ROW row =
        mysql_fetch_row(result);


    if(row == nullptr ||
       row[0] == nullptr ||
       row[1] == nullptr)
    {
        mysql_free_result(result);

        std::cerr
            << "[FAIL] registered user not found"
            << std::endl;

        return 1;
    }


    std::string passwordHash =
        row[0];


    std::string salt =
        row[1];


    /*
     * 核心验证：
     *
     * 数据库中的password_hash
     * 绝对不能等于用户输入的明文password。
     */
    if(passwordHash == password ||
       passwordHash.empty() ||
       salt.empty())
    {
        mysql_free_result(result);

        std::cerr
            << "[FAIL] password storage test failed"
            << std::endl;

        return 1;
    }


    mysql_free_result(result);

    result = nullptr;


    std::cout
        << "[PASS] password stored as hash + salt"
        << std::endl;



    // ============================================================
    // 测试结束：
    // 删除测试数据
    // ============================================================

    mysql.execute(
        "DELETE FROM users "
        "WHERE username='"
        + escapedUsername
        + "'"
    );


    std::cout
        << "[PASS] test data cleaned"
        << std::endl;


    std::cout
        << "=========================================="
        << std::endl;

    std::cout
        << " UserService register test passed."
        << std::endl;

    std::cout
        << "=========================================="
        << std::endl;


    return 0;
}