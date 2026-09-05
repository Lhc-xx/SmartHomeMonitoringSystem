#ifndef MYSQL_CLIENT_H
#define MYSQL_CLIENT_H

#include <mysql/mysql.h>

#include <mutex>
#include <string>

//MySQL Client 
//服务器数据库的访问封装

//主要的职责
//初始化 MySQL 连接句柄
//建立数据库连接
//执行 INSERT/ UPDATE / DELETE
//执行 SELECT
//SQL 字符串转义
//自动释放数据库资源


//上传业务
// UserService
// DeviceService
// RecordService
//都不应该直接操作MYSQL*
//而是通过统一的 MySQLClient 访问数据库

namespace smart_home {

class MySQLClient{
public:
    //构造函数 RAII
    //创建MYSQL 连接句柄
    MySQLClient();

    //MySQLClient 对象销毁的时候
    //自动的关闭数据库的连接
    ~MySQLClient();

    //MySQL 属于独占资源 禁止拷贝
    MySQLClient(const MySQLClient &) = delete;

    MySQLClient &operator=(const MySQLClient &) = delete;

    //建立独立的数据库连接 
    bool connect(const std::string &host,
                const std::string &user,
                const std::string &password,
                const std::string &database,
                unsigned int port = 3306);
    
    //主动关闭连接
    //一般不需要手动的关闭，因为析构函数会自动的调用
    void close();

    //当前是否已经连接数据库
    bool isConnection() const;

    //执行 INSERT  UPDATE  DELETE
    //成功返回true
    bool execute(const std::string &sql);

    //执行SELECT 查询
    //成功 返回 MYSQL_RES*
    //失败 返回 nullptr
    //使用者使用结束以后必须
    //mysql_free_result(result);
    //否则会造成结果集资源泄露
    MYSQL_RES *query(const std::string &sql);

    //对用户输入进行SQL转义
    //后面注册时 用户可能输入 OR  1=1 --
    //不能直接拼 SQL
    std::string escape(const std::string &value);

    //获得最后一次数据库错误
    std::string lastError() const;

private:
        //MySQL C API 链接句柄
        MYSQL *_conn;

        //当前的链接状态
        bool _connected;

        //最近的一次错误信息
        //保存下来避免后续MySQL操作
        //覆盖原错误
        std::string _lastError;

        //多个 ThreadPoll 里面的工作线程
        //公用一个MySQLClient 时候
        //需要先使用mutex 保证串行访问
        //后期高并发可以替换为  MySQL 链接池
        mutable std::mutex _mutex;

};

} // namespace smart_home

#endif