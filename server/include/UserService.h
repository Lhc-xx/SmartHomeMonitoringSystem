#ifndef USER_SERVICE_H
#define USER_SERVICE_H

#include "MySQLClient.h"
#include "protocol/ErrorCode.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace smart_home {

/* 登录结果只在成功时携带明文 token；数据库仅保存 token 摘要。 */
struct LoginResult {
    uint64_t userId;
    std::string token;
    ErrorCode code;
};

/* UserService 封装注册、登录和凭据持久化，屏蔽 SQL 与密码算法细节。 */
class UserService {
public:
    /* 业务层借用启动层创建的数据库连接，不自行管理连接生命周期。 */
    explicit UserService(MySQLClient &mysql);

    /* 注册时校验参数、生成随机 salt、PBKDF2 哈希并写入 users。 */
    ErrorCode registerUser(const std::string &username,
                           const std::string &password);

    /* 登录时验证 PBKDF2 结果，并在事务中写入会话摘要。 */
    LoginResult loginUser(const std::string &username,
                          const std::string &password);

private:
    bool validateRegisterParameter(const std::string &username,
                                   const std::string &password) const;
    bool userExists(const std::string &username, bool &exists);
    bool generateSalt(std::string &salt);
    bool hashPassword(const std::string &password,
                      const std::string &salt,
                      std::string &passwordHash);
    bool hashToken(const std::string &token, std::string &tokenHash);
    static bool constantTimeEquals(const std::string &left,
                                   const std::string &right);
    static std::string bytesToHex(const unsigned char *data, std::size_t length);

    MySQLClient &_mysql;
};

} // namespace smart_home

#endif // USER_SERVICE_H
