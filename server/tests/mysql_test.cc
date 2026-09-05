#include "config.h"
#include "MySQLClient.h"

#include <mysql/mysql.h>

#include <iostream>
#include <string>


int main()
{
    std::cout
        << "=========================================="
        << std::endl;

    std::cout
        << "       MySQL Integration Test"
        << std::endl;

    std::cout
        << "=========================================="
        << std::endl;


    // ============================================================
    // 第1步：加载Server本地配置
    // ============================================================

    smart_home::Config config;


    const std::string configPath =
        "server/conf/server.conf";


    if(!config.load(configPath))
    {
        std::cerr
            << "[FAIL] cannot load config file: "
            << configPath
            << std::endl;

        return 1;
    }


    std::cout
        << "[PASS] config loaded"
        << std::endl;



    // ============================================================
    // 第2步：创建MySQLClient
    // ============================================================

    smart_home::MySQLClient mysql;



    // ============================================================
    // 第3步：根据Config提供的信息连接MySQL
    // ============================================================

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
            << "[FAIL] MySQL connect failed"
            << std::endl;


        /*
         * 输出数据库错误。
         *
         * 注意：
         * 这里只打印错误信息，
         * 不打印数据库密码。
         */
        std::cerr
            << "[MySQL] "
            << mysql.lastError()
            << std::endl;


        return 1;
    }


    std::cout
        << "[PASS] MySQL connected"
        << std::endl;


    std::cout
        << "[INFO] host     = "
        << config.mysqlHost()
        << ":"
        << config.mysqlPort()
        << std::endl;


    std::cout
        << "[INFO] database = "
        << config.mysqlDatabase()
        << std::endl;



    // ============================================================
    // 第4步：真正执行一次SELECT
    // ============================================================

    MYSQL_RES *result =
        mysql.query(
            "SELECT 1"
        );


    if(result == nullptr)
    {
        std::cerr
            << "[FAIL] SELECT 1 failed"
            << std::endl;


        std::cerr
            << "[MySQL] "
            << mysql.lastError()
            << std::endl;


        return 1;
    }



    // ============================================================
    // 第5步：从结果集中读取一行
    // ============================================================

    MYSQL_ROW row =
        mysql_fetch_row(result);


    /*
     * SELECT 1
     *
     * 正常情况下：
     *
     * row[0] == "1"
     */
    if(row == nullptr ||
       row[0] == nullptr ||
       std::string(row[0]) != "1")
    {
        std::cerr
            << "[FAIL] invalid SELECT 1 result"
            << std::endl;


        /*
         * MYSQL_RES属于MySQL资源。
         *
         * 使用结束必须释放。
         */
        mysql_free_result(result);


        return 1;
    }



    std::cout
        << "[PASS] SELECT 1"
        << std::endl;



    // ============================================================
    // 第6步：释放查询结果
    // ============================================================

    mysql_free_result(result);

    result = nullptr;



    // ============================================================
    // 第7步：测试结束
    // ============================================================

    /*
     * 这里没有显式调用：
     *
     * mysql.close()
     *
     * 因为mysql是局部对象。
     *
     * main结束：
     *
     * mysql析构
     *
     *      ↓
     *
     * ~MySQLClient()
     *
     *      ↓
     *
     * mysql_close()
     *
     *
     * 这就是RAII。
     */


    std::cout
        << "=========================================="
        << std::endl;

    std::cout
        << " MySQL integration test passed."
        << std::endl;

    std::cout
        << "=========================================="
        << std::endl;


    return 0;
}