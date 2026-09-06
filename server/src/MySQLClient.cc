#include "MySQLClient.h"

#include <mysql/mysql.h>

#include <mutex>

namespace smart_home {

MySQLClient::MySQLClient()
    : _conn(mysql_init(nullptr)), _connected(false), _lastError(), _lastErrno(0U) {
    /* 构造阶段只创建 C API 句柄，不连接服务器，连接参数由启动层决定。 */
    if (_conn == nullptr) {
        _lastError = "mysql_init failed";
    }
}

MySQLClient::~MySQLClient() {
    /* 析构统一释放句柄，避免业务层遗漏 mysql_close。 */
    if (_conn != nullptr) {
        mysql_close(_conn);
        _conn = nullptr;
    }
    _connected = false;
}

bool MySQLClient::connect(const std::string &host,
                          const std::string &user,
                          const std::string &password,
                          const std::string &database,
                          unsigned int port) {
    std::lock_guard<std::mutex> guard(_mutex);
    if (_connected) {
        return true;
    }
    if (_conn == nullptr) {
        _conn = mysql_init(nullptr);
        if (_conn == nullptr) {
            _lastError = "mysql_init failed";
            _lastErrno = 0U;
            return false;
        }
    }

    /* 设置有限连接超时和 utf8mb4，保证中文设备名按统一编码读写。 */
    unsigned int timeout = 5U;
    mysql_options(_conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    mysql_options(_conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    if (mysql_real_connect(_conn, host.c_str(), user.c_str(), password.c_str(),
                           database.c_str(), port, nullptr, 0U) == nullptr) {
        _lastError = mysql_error(_conn);
        _lastErrno = mysql_errno(_conn);
        _connected = false;
        return false;
    }
    _connected = true;
    _lastError.clear();
    _lastErrno = 0U;
    return true;
}

void MySQLClient::close() {
    std::lock_guard<std::mutex> guard(_mutex);
    if (_conn != nullptr) {
        mysql_close(_conn);
        _conn = nullptr;
    }
    _connected = false;
}

bool MySQLClient::isConnection() const {
    std::lock_guard<std::mutex> guard(_mutex);
    return _connected;
}

bool MySQLClient::execute(const std::string &sql) {
    std::lock_guard<std::mutex> guard(_mutex);
    if (!_connected || _conn == nullptr) {
        _lastError = "MySQL is not connected";
        _lastErrno = 0U;
        return false;
    }
    if (mysql_real_query(_conn, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0) {
        _lastError = mysql_error(_conn);
        _lastErrno = mysql_errno(_conn);
        return false;
    }
    _lastError.clear();
    _lastErrno = 0U;
    return true;
}

MYSQL_RES *MySQLClient::query(const std::string &sql) {
    std::lock_guard<std::mutex> guard(_mutex);
    if (!_connected || _conn == nullptr) {
        _lastError = "MySQL is not connected";
        _lastErrno = 0U;
        return nullptr;
    }
    if (mysql_real_query(_conn, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0) {
        _lastError = mysql_error(_conn);
        _lastErrno = mysql_errno(_conn);
        return nullptr;
    }
    MYSQL_RES *result = mysql_store_result(_conn);
    if (result == nullptr && mysql_field_count(_conn) != 0U) {
        _lastError = mysql_error(_conn);
        _lastErrno = mysql_errno(_conn);
        return nullptr;
    }
    _lastError.clear();
    _lastErrno = 0U;
    return result;
}

std::string MySQLClient::escape(const std::string &value) {
    std::lock_guard<std::mutex> guard(_mutex);
    if (!_connected || _conn == nullptr) {
        _lastError = "MySQL is not connected";
        _lastErrno = 0U;
        return std::string();
    }
    /* 调用 MySQL 的转义函数，禁止业务字符串直接拼入 SQL。 */
    std::string escaped(value.size() * 2U + 1U, '\0');
    const unsigned long length = mysql_real_escape_string(
        _conn, &escaped[0], value.c_str(), static_cast<unsigned long>(value.size()));
    escaped.resize(static_cast<std::size_t>(length));
    _lastError.clear();
    _lastErrno = 0U;
    return escaped;
}

std::string MySQLClient::lastError() const {
    std::lock_guard<std::mutex> guard(_mutex);
    return _lastError;
}

bool MySQLClient::beginTransaction() {
    std::lock_guard<std::mutex> guard(_mutex);
    if (!_connected || _conn == nullptr) {
        _lastError = "MySQL is not connected";
        _lastErrno = 0U;
        return false;
    }
    if (mysql_real_query(_conn, "START TRANSACTION", 17U) != 0) {
        _lastError = mysql_error(_conn);
        _lastErrno = mysql_errno(_conn);
        return false;
    }
    _lastError.clear();
    _lastErrno = 0U;
    return true;
}

bool MySQLClient::commit() {
    std::lock_guard<std::mutex> guard(_mutex);
    if (!_connected || _conn == nullptr) {
        _lastError = "MySQL is not connected";
        _lastErrno = 0U;
        return false;
    }
    if (mysql_commit(_conn) != 0) {
        _lastError = mysql_error(_conn);
        _lastErrno = mysql_errno(_conn);
        return false;
    }
    _lastError.clear();
    _lastErrno = 0U;
    return true;
}

bool MySQLClient::rollback() {
    std::lock_guard<std::mutex> guard(_mutex);
    if (!_connected || _conn == nullptr) {
        _lastError = "MySQL is not connected";
        _lastErrno = 0U;
        return false;
    }
    if (mysql_rollback(_conn) != 0) {
        _lastError = mysql_error(_conn);
        _lastErrno = mysql_errno(_conn);
        return false;
    }
    _lastError.clear();
    _lastErrno = 0U;
    return true;
}

unsigned int MySQLClient::lastErrno() const {
    std::lock_guard<std::mutex> guard(_mutex);
    return _lastErrno;
}

} // namespace smart_home
