#include "MySQLClient.h"

#include "iostream"
#include <cstddef>
#include <mutex>
#include <mysql/mysql.h>
#include <sstream>

namespace smart_home{

    MySQLClient::MySQLClient()
    :_conn(nullptr)
    ,_connected(false)
    ,_lastError(){
        //链接MYSQL 链接句柄
        //此时只是初始化
        //还没有真正的连接服务器
        _conn = mysql_init(nullptr);
        if(_conn == nullptr){
            _lastError = "mysql_init failed";
        }

    }

    MySQLClient::~MySQLClient(){
        if(_conn != nullptr){
            mysql_close(_conn);
            _conn = nullptr;
        }
        _connected = false;
    }

    bool MySQLClient::connect(const std::string &host,
        const std::string &user,
        const std::string &password,
        const std::string &database,
        unsigned int port){
            //数据库连接是共享资源
            //必须加锁
            std::lock_guard<std::mutex> guard(_mutex);
            
            //已经连接了就不要重新连接
            if(_connected){
                return true;
            }
            //如果句柄不存在，尝试重新创建
            if(_conn == nullptr){
                _conn = mysql_init(nullptr);
                if(_conn == nullptr){
                    _lastError = "mysql_init failed";
                    return false;
                }
            }
            //设置连接超时
            unsigned int timeout = 5;
            mysql_options(_conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

            //指定UTF_8 字符集
            //后面的设备名称等可能包含中文
            mysql_options(_conn,MYSQL_SET_CHARSET_NAME,"utf8mb4");

            //真正的建立TCP/MySQL 连接
            MYSQL *result = mysql_real_connect(_conn, host.c_str(), user.c_str(), password.c_str(), database.c_str(), port, nullptr, 0);
            
            if(result == nullptr){
                _lastError = mysql_error(_conn);
                _connected = false;
                return false;
            }

            _connected = true;
            _lastError.clear();
            return true;
        }

        void MySQLClient::close(){
            std::lock_guard<std::mutex> guard(_mutex);

            if(_conn != nullptr){
                mysql_close(_conn);
                _conn = nullptr;
            }
            _connected = false;
        }

        bool MySQLClient::isConnection() const{
            std::lock_guard<std::mutex> guard(_mutex);
            return _connected;
        }


        bool MySQLClient::execute(const std::string &sql){
            std::lock_guard<std::mutex> guard(_mutex);

            //未连接数据库，不允许执行 SQL
            if(!_connected || _conn == nullptr){
                _lastError = "MySQL is not connected";

                return false;

            }
            //mysql_real_query
            //相比于mysql_query
            //显示的提供SQL 长度
            int ret = mysql_real_query(_conn,
                sql.c_str(),
            static_cast<unsigned long>(sql.size()));

            if(ret != 0){
                _lastError = mysql_error(_conn);
                return false;
            }

            _lastError.clear();
            return true;
        }

        MYSQL_RES *MySQLClient::query(
            const std::string &sql){
                std::lock_guard<std::mutex> guard(_mutex);

                if(!_connected || _conn == nullptr){
                    _lastError = "MySQL is not connected";
                    return nullptr;
                }

                //首先先将SELECT 发送给MySQL
                int ret = mysql_real_query(_conn, sql.c_str(), static_cast<unsigned long>(sql.size()));


                //MySQL C API 大部分的返回约定 和 Linux 一样 0表示成功 非0 表示失败 
                if(ret != 0){
                    _lastError = mysql_error(_conn);
                    return nullptr;
                }

                //mysql_store_result
                //将查询的结果完整的读取到客户端内存
                //后面通过
                //mysql_fetch_row()
                //一行一行的读取
                MYSQL_RES *result = mysql_store_result(_conn);

                //如果SELECT 应该产生结果集
                //但是store_result 返回nullptr
                //说明发生了错误
                if(result == nullptr && mysql_field_count(_conn) != 0){
                    _lastError = mysql_error(_conn);
                    return nullptr;
                }
                _lastError.clear();
                return result;
            }


            std::string MySQLClient::escape(const std::string &value){
                std::lock_guard<std::mutex> guard(_mutex);

                if(!_connected || _conn == nullptr){
                    _lastError = "MySQL is not connected";
                    return "";
                }

                //MySQL 转义之后的字符串
                //最坏的情况下长度接近原来的2倍
                //因此 value.size() * 2 +1
                std::string escaped;

                escaped.resize(value.size()*2+1);
                unsigned long length = mysql_real_escape_string(
                    _conn,
                    &escaped[0],
                    value.c_str(),
                    static_cast<unsigned long>(value.size()));

                //resize() 到实际的长度
                escaped.resize(length);
                _lastError.clear();
                return escaped;
            }

            std::string MySQLClient::lastError() const{
                std::lock_guard<std::mutex> guard(_mutex);
                return _lastError;
            }

}   //namespace smart_home