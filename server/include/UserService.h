#ifndef USER_SERVICE_H
#define USER_SERVICE_H

#include "MySQLClient.h"
#include "protocol/ErrorCode.h"

#include <cstddef>
#include <string>

namespace smart_home {


    //UserService
    //用户业务层
    //它负责
    //注册参数校验
    //判断用户名是否存在
    //生产随机salt
    //密码哈希
    //保存用户到MySQL

    //调用关系
    //MessageDispatcher
    //UserService
    //MySQLClient
    //MySQL

    class UserService{
        public:
        //UserService 自己不负责创建数据库连接
        //而是使用外部已经连接好的MySQLClient
        //所以这里使用引用
        explicit UserService(MySQLClient &mysql);

        //用户注册
        //返回整个项目统一的ErrorCode
        //SUCCESS               注册成功
        //INVALID_PARAMETER     用户名/密码不合法
        //USER_ALREADY_EXISTS   用户名已经存在
        //DATABASE_ERROR        数据库查询或插入失败
        //INTERNAL_ERROR        salt/hash 生产失败
        
        ErrorCode registerUser(
            const std::string &username,
            const std::string &password
        );

        private:
        //检查注册参数是否合法
        bool validateRegisterParameter(
            const std::string &username,
            const std::string &password
        )const;

        //查询用户名是否已经存在
        //函数本身返回 true SQL查询成功
        //返回 false  SQL 查询失败
        //true 用户存在
        //false 用户不存在
        bool userExists(const std::string &username,
        bool &exists);

        //生成随机salt
        //最终生成HEX字符串
        //保存到users.salt
        bool generateSalt(std::string &salt);
        
        //使用PBKDF2-HMAC-SHA256
        //对密码进行哈希
        bool hashPassword(
            const std::string &password,
            const std::string &salt,
            std::string &passwordHash
        );

        //将二进制的数据转换成HEX 字符串
        //例如 0x12 0xab 转换成 12ab
        static std::string bytesToHex(
            const unsigned char *data,
            std::size_t length
        );

        private:
        //UserService 不拥有数据库对象
        //他只是使用main或者Server
        //已经创建好的MySQLClient
        MySQLClient &_mysql;

    };
}   //namespace smart_home

#endif