#ifndef MYSQL_CLIENT_H
#define MYSQL_CLIENT_H

#include <mutex>
#include <string>

/*
 * 头文件只声明 MySQL C API 的不透明句柄，避免把 mysql.h 中的宏传播到协议层。
 * 完整定义仅由 MySQLClient.cc 和确实需要读取结果集的实现文件包含。
 */
struct MYSQL;
struct MYSQL_RES;

namespace smart_home {

/* MySQLClient 是数据库访问的唯一封装，业务层不直接操作 MYSQL 连接句柄。 */
class MySQLClient {
public:
    /* 构造时创建句柄但不连接，连接参数由启动配置提供。 */
    MySQLClient();
    ~MySQLClient();

    MySQLClient(const MySQLClient &) = delete;
    MySQLClient &operator=(const MySQLClient &) = delete;

    /* 建立连接；默认连接超时为 5 秒。 */
    bool connect(const std::string &host,
                 const std::string &user,
                 const std::string &password,
                 const std::string &database,
                 unsigned int port = 3306U);
    void close();
    bool isConnection() const;

    /* execute 用于 INSERT/UPDATE/DELETE 等无结果集语句。 */
    bool execute(const std::string &sql);

    /* query 返回 MYSQL_RES，调用方必须在使用完毕后 mysql_free_result。 */
    MYSQL_RES *query(const std::string &sql);

    /* 转义用户输入，防止直接拼接 SQL 造成注入。 */
    std::string escape(const std::string &value);

    std::string lastError() const;

    /* 显式事务接口用于登录会话等多步写入的原子性。 */
    bool beginTransaction();
    bool commit();
    bool rollback();

    /* 返回最近一次 MySQL C API 错误码，用于映射重复用户名等业务错误。 */
    unsigned int lastErrno() const;

private:
    /*
     * 以下辅助函数均要求调用方已经持有 _mutex。
     * 将重连集中在一个入口，避免 query/execute 各自实现时出现行为不一致。
     */
    bool reconnectLocked();
    bool startTransactionLocked();
    bool isConnectionError(unsigned int errorCode) const;
    void setErrorLocked(const std::string &message, unsigned int errorCode);

    MYSQL *_conn;
    bool _connected;
    bool _reconnectEnabled;
    bool _inTransaction;

    /* 保存连接参数只用于进程内自动重连，密码不会写入日志或仓库。 */
    std::string _host;
    std::string _user;
    std::string _password;
    std::string _database;
    unsigned int _port;

    std::string _lastError;
    unsigned int _lastErrno;
    mutable std::mutex _mutex;
};

} // namespace smart_home

#endif // MYSQL_CLIENT_H
