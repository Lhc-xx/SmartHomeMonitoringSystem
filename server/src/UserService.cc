#include "UserService.h"
#include "MySQLClient.h"
#include "protocol/ErrorCode.h"

#include <cstddef>
#include <mysql/mysql.h>

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <sstream>
#include <string>

namespace smart_home {

// salt 随机字节数
// 16 Byte = 128 bit 随机salt

//转HEX以后
// 32个字符
// users.salt VARCHAR(64)
//足够保存
static const int SALT_SIZE = 16;

//最终的密码摘要
// SHA256 = 32 Byte
//转HEX 以后
// 64 个字符
// users.password_hash VARCHAR(256)
//足够保存
static const int HASH_SIZE = 32;

/*
 * PBKDF2迭代次数。
 *
 * 这里作为当前MVP版本固定参数。
 *
 * 它不是“越大绝对越好”，
 * 生产系统应该根据实际服务器性能和
 * 安全要求确定并支持升级。
 */

static const int PBKDF2_ITERATIONS = 100000;

UserService::UserService(MySQLClient &mysql) : _mysql(mysql) {}

//注册参数校验
bool UserService::validateRegisterParameter(const std::string &username,
                                            const std::string &password) const {
  // username 数据库定义
  // VARCHAR(64)
  //所以最大不能超过64
  if (username.empty() || username.size() > 64) {
    return false;
  }
  //密码不能是空字符串
  //同时给业务层设置一个合理的上限
  //防止客户端发送超长无意义的参数
  if (password.empty() || password.size() > 128) {
    return false;
  }
  return true;
}

//查询用户名是否存在
bool UserService::userExists(const std::string &username, bool &exists) {
  //默认不存在
  exists = false;

  //用户输入绝对不能直接拼接进SQL
  //防止SQL注入攻击 OR 1=1
  //所以必须先通过MySQLClient::escape()
  //进行转义
  std::string escapedUsername = _mysql.escape(username);

  //查询用户名
  // LIMIT 1
  //只关心有没有 ，不需要查询所有的数据
  std::string sql = "SELECT id "
                    "FROM users "
                    "WHERE username='" 
                    + escapedUsername +
                    "' "
                    "LIMIT 1";

  MYSQL_RES *result = _mysql.query(sql);
  // result == nullptr
  //表示数据库查询失败
  if (result == nullptr) {
    return false;
  }
  exists = mysql_num_rows(result) > 0;

  // mysql_store_result(result);
  //创建的MYSQL_RES资源
  //使用之后必须释放
  mysql_free_result(result);
  result = nullptr;
  return true;
}

//二进制--->HEX
std::string UserService::bytesToHex(const unsigned char *data,
                                    std::size_t length) {
  // HEX 字符表
  static const char HEX[] = "0123456789abcdef";

  std::string result;

  //一个Byte 会变成两个HEX 字符
  //所以提前 reserve
  result.reserve(length * 2);

  for (std::size_t i = 0; i < length; ++i) {
    unsigned char byte = data[i];
    result.push_back(HEX[(byte >> 4) & 0x0f]);

    result.push_back(HEX[byte & 0x0F]);
  }
  return result;
}

//生成随机salt
bool UserService::generateSalt(std::string &salt) {
  unsigned char buffer[SALT_SIZE];

  /*
   * RAND_bytes()
   *
   * 使用OpenSSL安全随机数生成器。
   *
   * 返回1：
   * 成功
   */
  if (RAND_bytes(buffer, SALT_SIZE) != 1) {
    return false;
  }

  /*
   * 随机二进制不能直接保存VARCHAR，
   *
   * 所以转换成HEX。
   */
  salt = bytesToHex(buffer, SALT_SIZE);

  return true;
}

bool UserService::hashPassword(const std::string &password,
                               const std::string &salt,
                               std::string &passwordHash) {
  unsigned char hash[HASH_SIZE];

  /*
   * PKCS5_PBKDF2_HMAC
   *
   * 输入：
   *
   * password
   * salt
   * iteration
   * SHA256
   *
   * 输出：
   *
   * 32 Byte hash
   */
  int ret =
      PKCS5_PBKDF2_HMAC(password.c_str(),

                        static_cast<int>(password.size()),

                        reinterpret_cast<const unsigned char *>(salt.data()),

                        static_cast<int>(salt.size()),

                        PBKDF2_ITERATIONS,

                        EVP_sha256(),

                        HASH_SIZE,

                        hash);

  /*
   * 返回1代表成功。
   */
  if (ret != 1) {
    return false;
  }

  /*
   * 32 Byte二进制hash
   *
   * 转：
   *
   * 64字符HEX。
   */
  passwordHash = bytesToHex(hash, HASH_SIZE);

  return true;
}

//用户注册主业务
ErrorCode UserService::registerUser(const std::string &username,
                                    const std::string &password) {
  //第一步参数检查
  if (!validateRegisterParameter(username, password)) {
    return ErrorCode::INVALID_PARAMETER;
  }
  //第二步判断用户是否存在
  bool exists = false;
  if (!userExists(username, exists)) {
    //查询失败
    return ErrorCode::DATABASE_ERROR;
  }

  if (exists) {
    return ErrorCode::USER_ALREADY_EXISTS;
  }

  //第三步，生成随机的salt
  std::string salt;
  if (!generateSalt(salt)) {
    return ErrorCode::INTERNAL_ERROR;
  }

  //第四步，密码哈希
  std::string passwordHash;
  if (!hashPassword(password, salt, passwordHash)) {
    return ErrorCode::INTERNAL_ERROR;
  }

  //第五步 SQL 转义
  std::string escapedUsername = _mysql.escape(username);

  std::string escapedHash = _mysql.escape(passwordHash);

  std::string escapedSalt = _mysql.escape(salt);

  //第6 步 ,插入users表
  std::string sql = "INSERT INTO users"
                    "(username,password_hash,salt)"
                    "VALUES('" +
                    escapedUsername + "','" + escapedHash + "','" +
                    escapedSalt + "')";

  if(!_mysql.execute(sql)){
    //注意：
    //users.username 本身还有UNIQUE 约束
    //前面的SELECT 属于业务层提前判断
    //UNIQUE 属于数据库最后的一道保护
    //当前MVP 如果 INSERT 失败
    //统一返回 DATABASE_ERROR
    //后续可以根据mysql_errno()
    //将1062进一步映射成
    //USER_ALREADY_EXISTS
    return ErrorCode::DATABASE_ERROR;
  }

  //注册成功
  return ErrorCode::SUCCESS;
}

} // namespace smart_home