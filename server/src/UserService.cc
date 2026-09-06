#include "UserService.h"

#include "protocol/ErrorCode.h"

#include <mysql/mysql.h>
#include <mysql/mysqld_error.h>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <cstddef>
#include <cstdlib>
#include <string>

namespace smart_home {

namespace {

const int kSaltSize = 16;
const int kHashSize = 32;
const int kTokenRandomSize = 32;
const int kTokenHashSize = 64;
const int kPbkdf2Iterations = 100000;

} // namespace

UserService::UserService(MySQLClient &mysql)
    : _mysql(mysql) {}

bool UserService::validateRegisterParameter(const std::string &username,
                                            const std::string &password) const {
    /* 注册和登录共享长度边界，防止超长输入进入 SQL 或密码派生算法。 */
    return !username.empty() && username.size() <= 64U &&
           !password.empty() && password.size() <= 128U;
}

bool UserService::userExists(const std::string &username, bool &exists) {
    exists = false;
    const std::string escapedUsername = _mysql.escape(username);
    MYSQL_RES *result = _mysql.query(
        "SELECT id FROM users WHERE username='" + escapedUsername + "' LIMIT 1");
    if (result == nullptr) {
        return false;
    }
    exists = mysql_num_rows(result) > 0U;
    mysql_free_result(result);
    return true;
}

std::string UserService::bytesToHex(const unsigned char *data, std::size_t length) {
    static const char kHex[] = "0123456789abcdef";
    std::string result;
    result.reserve(length * 2U);
    for (std::size_t index = 0U; index < length; ++index) {
        result.push_back(kHex[(data[index] >> 4U) & 0x0FU]);
        result.push_back(kHex[data[index] & 0x0FU]);
    }
    return result;
}

bool UserService::generateSalt(std::string &salt) {
    unsigned char bytes[kSaltSize];
    if (RAND_bytes(bytes, kSaltSize) != 1) {
        return false;
    }
    salt = bytesToHex(bytes, kSaltSize);
    return true;
}

bool UserService::hashPassword(const std::string &password,
                               const std::string &salt,
                               std::string &passwordHash) {
    unsigned char hash[kHashSize];
    const int result = PKCS5_PBKDF2_HMAC(
        password.c_str(), static_cast<int>(password.size()),
        reinterpret_cast<const unsigned char *>(salt.data()),
        static_cast<int>(salt.size()), kPbkdf2Iterations, EVP_sha256(),
        kHashSize, hash);
    if (result != 1) {
        return false;
    }
    passwordHash = bytesToHex(hash, kHashSize);
    return true;
}

bool UserService::hashToken(const std::string &token, std::string &tokenHash) {
    unsigned char hash[kTokenHashSize];
    unsigned int hashLength = 0U;
    if (EVP_Digest(token.data(), token.size(), hash, &hashLength, EVP_sha512(), nullptr) != 1 ||
        hashLength != static_cast<unsigned int>(kTokenHashSize)) {
        return false;
    }
    tokenHash = bytesToHex(hash, kTokenHashSize);
    return true;
}

bool UserService::constantTimeEquals(const std::string &left,
                                     const std::string &right) {
    /* 长度不同时不调用比较函数；长度相同时使用 OpenSSL 常量时间比较。 */
    return left.size() == right.size() && !left.empty() &&
           CRYPTO_memcmp(left.data(), right.data(), left.size()) == 0;
}

ErrorCode UserService::registerUser(const std::string &username,
                                    const std::string &password) {
    if (!validateRegisterParameter(username, password)) {
        return ErrorCode::INVALID_PARAMETER;
    }

    bool exists = false;
    if (!userExists(username, exists)) {
        return ErrorCode::DATABASE_ERROR;
    }
    if (exists) {
        return ErrorCode::USER_ALREADY_EXISTS;
    }

    std::string salt;
    std::string passwordHash;
    if (!generateSalt(salt) || !hashPassword(password, salt, passwordHash)) {
        return ErrorCode::INTERNAL_ERROR;
    }

    const std::string sql =
        "INSERT INTO users(username,password_hash,salt) VALUES('" +
        _mysql.escape(username) + "','" + _mysql.escape(passwordHash) + "','" +
        _mysql.escape(salt) + "')";
    if (!_mysql.execute(sql)) {
        /* 并发注册由数据库唯一索引兜底，向协议层映射为明确业务错误。 */
        return _mysql.lastErrno() == ER_DUP_ENTRY
                   ? ErrorCode::USER_ALREADY_EXISTS
                   : ErrorCode::DATABASE_ERROR;
    }
    return ErrorCode::SUCCESS;
}

LoginResult UserService::loginUser(const std::string &username,
                                   const std::string &password) {
    LoginResult result = {0U, std::string(), ErrorCode::INTERNAL_ERROR};
    if (!validateRegisterParameter(username, password)) {
        result.code = ErrorCode::INVALID_PARAMETER;
        return result;
    }

    MYSQL_RES *queryResult = _mysql.query(
        "SELECT id,password_hash,salt FROM users WHERE username='" +
        _mysql.escape(username) + "' LIMIT 1");
    if (queryResult == nullptr) {
        result.code = ErrorCode::DATABASE_ERROR;
        return result;
    }
    MYSQL_ROW row = mysql_fetch_row(queryResult);
    if (row == nullptr) {
        mysql_free_result(queryResult);
        result.code = ErrorCode::USER_NOT_FOUND;
        return result;
    }
    if (row[0] == nullptr || row[1] == nullptr || row[2] == nullptr) {
        mysql_free_result(queryResult);
        result.code = ErrorCode::DATABASE_ERROR;
        return result;
    }

    const uint64_t userId = static_cast<uint64_t>(std::strtoull(row[0], nullptr, 10));
    const std::string storedHash(row[1]);
    const std::string salt(row[2]);
    mysql_free_result(queryResult);

    std::string calculatedHash;
    if (!hashPassword(password, salt, calculatedHash)) {
        result.code = ErrorCode::INTERNAL_ERROR;
        return result;
    }
    if (!constantTimeEquals(calculatedHash, storedHash)) {
        result.code = ErrorCode::PASSWORD_ERROR;
        return result;
    }

    unsigned char tokenBytes[kTokenRandomSize];
    if (RAND_bytes(tokenBytes, kTokenRandomSize) != 1) {
        result.code = ErrorCode::INTERNAL_ERROR;
        return result;
    }
    const std::string token = bytesToHex(tokenBytes, kTokenRandomSize);
    std::string tokenHash;
    if (!hashToken(token, tokenHash)) {
        result.code = ErrorCode::INTERNAL_ERROR;
        return result;
    }

    /* 会话摘要写入和提交处于同一事务，失败时不留下半条会话记录。 */
    if (!_mysql.beginTransaction()) {
        result.code = ErrorCode::DATABASE_ERROR;
        return result;
    }
    const std::string insertSql =
        "INSERT INTO user_sessions(user_id,token_hash,expires_at) VALUES(" +
        std::to_string(userId) + ",'" + _mysql.escape(tokenHash) +
        "',DATE_ADD(UTC_TIMESTAMP(), INTERVAL 24 HOUR))";
    if (!_mysql.execute(insertSql) || !_mysql.commit()) {
        _mysql.rollback();
        result.code = ErrorCode::DATABASE_ERROR;
        return result;
    }

    /* 明文 token 只返回给当前调用方，数据库和日志均不保存它。 */
    result.userId = userId;
    result.token = token;
    result.code = ErrorCode::SUCCESS;
    return result;
}

} // namespace smart_home
