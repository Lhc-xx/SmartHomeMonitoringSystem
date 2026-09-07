#include "MySQLClient.h"

#include <mysql/mysql.h>

#include <mutex>

namespace {

/*
 * MySQL 客户端库在不同发行版中对错误常量的头文件位置略有差异，
 * 因此这里按官方稳定错误号判断连接级故障：
 * 2003=无法连接服务器，2006=服务器已断开，2013=连接在传输中丢失。
 * 只有这类故障才允许自动重连，SQL 语法/约束错误仍原样返回业务层。
 */
bool isConnectionFailure(unsigned int errorCode) {
    return errorCode == 2003U || errorCode == 2006U || errorCode == 2013U;
}

} // namespace

namespace smart_home {

MySQLClient::MySQLClient()
    : _conn(mysql_init(nullptr)),
      _connected(false),
      _reconnectEnabled(false),
      _inTransaction(false),
      _host(),
      _user(),
      _password(),
      _database(),
      _port(3306U),
      _lastError(),
      _lastErrno(0U) {
    /* 构造阶段只创建 C API 句柄，不连接服务器，连接参数由启动层决定。 */
    if (_conn == nullptr) {
        _lastError = "mysql_init failed";
    }
}

MySQLClient::~MySQLClient() {
    /* 析构统一释放句柄，避免业务层遗漏 mysql_close。 */
    std::lock_guard<std::mutex> guard(_mutex);
    if (_conn != nullptr) {
        mysql_close(_conn);
        _conn = nullptr;
    }
    _connected = false;
    _reconnectEnabled = false;
    _inTransaction = false;
}

void MySQLClient::setErrorLocked(const std::string &message, unsigned int errorCode) {
    _lastError = message;
    _lastErrno = errorCode;
}

bool MySQLClient::isConnectionError(unsigned int errorCode) const {
    return isConnectionFailure(errorCode);
}

bool MySQLClient::reconnectLocked() {
    if (!_reconnectEnabled || _host.empty() || _user.empty() || _database.empty()) {
        setErrorLocked("MySQL reconnect parameters are unavailable", 0U);
        return false;
    }

    /* 关闭已经失效的句柄后重新初始化，避免复用残留的协议状态。 */
    if (_conn != nullptr) {
        mysql_close(_conn);
        _conn = nullptr;
    }
    _connected = false;
    _inTransaction = false;

    _conn = mysql_init(nullptr);
    if (_conn == nullptr) {
        setErrorLocked("mysql_init failed during reconnect", 0U);
        return false;
    }

    unsigned int timeout = 5U;
    mysql_options(_conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    mysql_options(_conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    if (mysql_real_connect(_conn, _host.c_str(), _user.c_str(), _password.c_str(),
                           _database.c_str(), _port, nullptr, 0U) == nullptr) {
        const std::string error = mysql_error(_conn);
        const unsigned int errorCode = mysql_errno(_conn);
        mysql_close(_conn);
        _conn = nullptr;
        setErrorLocked(error, errorCode);
        return false;
    }

    _connected = true;
    _lastError.clear();
    _lastErrno = 0U;
    return true;
}

bool MySQLClient::startTransactionLocked() {
    if (mysql_real_query(_conn, "START TRANSACTION", 17U) != 0) {
        const unsigned int errorCode = mysql_errno(_conn);
        setErrorLocked(mysql_error(_conn), errorCode);
        return false;
    }
    _inTransaction = true;
    _lastError.clear();
    _lastErrno = 0U;
    return true;
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

    /* 保存参数供空闲断线后自动重连；密码只留在内存，不写日志。 */
    _host = host;
    _user = user;
    _password = password;
    _database = database;
    _port = port;
    _reconnectEnabled = true;
    return reconnectLocked();
}

void MySQLClient::close() {
    std::lock_guard<std::mutex> guard(_mutex);
    if (_conn != nullptr) {
        mysql_close(_conn);
        _conn = nullptr;
    }
    _connected = false;
    _reconnectEnabled = false;
    _inTransaction = false;
    _host.clear();
    _user.clear();
    _password.clear();
    _database.clear();
    _port = 3306U;
}

bool MySQLClient::isConnection() const {
    std::lock_guard<std::mutex> guard(_mutex);
    return _connected;
}

bool MySQLClient::execute(const std::string &sql) {
    std::lock_guard<std::mutex> guard(_mutex);
    if ((!_connected || _conn == nullptr) && !reconnectLocked()) {
        return false;
    }

    const bool wasInTransaction = _inTransaction;
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (mysql_real_query(_conn, sql.c_str(), static_cast<unsigned long>(sql.size())) == 0) {
            _lastError.clear();
            _lastErrno = 0U;
            return true;
        }

        const unsigned int errorCode = mysql_errno(_conn);
        setErrorLocked(mysql_error(_conn), errorCode);
        if (attempt != 0 || !isConnectionError(errorCode) || !reconnectLocked()) {
            return false;
        }

        /* 连接在事务中断开后，新连接必须重新开启事务再重试语句。 */
        if (wasInTransaction && !startTransactionLocked()) {
            return false;
        }
    }
    return false;
}

MYSQL_RES *MySQLClient::query(const std::string &sql) {
    std::lock_guard<std::mutex> guard(_mutex);
    if ((!_connected || _conn == nullptr) && !reconnectLocked()) {
        return nullptr;
    }
    const bool wasInTransaction = _inTransaction;
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (mysql_real_query(_conn, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0) {
            const unsigned int errorCode = mysql_errno(_conn);
            setErrorLocked(mysql_error(_conn), errorCode);
            if (attempt == 0 && isConnectionError(errorCode) && reconnectLocked()) {
                if (wasInTransaction && !startTransactionLocked()) {
                    return nullptr;
                }
                continue;
            }
            return nullptr;
        }
        MYSQL_RES *result = mysql_store_result(_conn);
        if (result == nullptr && mysql_field_count(_conn) != 0U) {
            const unsigned int errorCode = mysql_errno(_conn);
            setErrorLocked(mysql_error(_conn), errorCode);
            if (attempt == 0 && isConnectionError(errorCode) && reconnectLocked()) {
                if (wasInTransaction && !startTransactionLocked()) {
                    return nullptr;
                }
                continue;
            }
            return nullptr;
        }
        _lastError.clear();
        _lastErrno = 0U;
        return result;
    }
    return nullptr;
}

std::string MySQLClient::escape(const std::string &value) {
    std::lock_guard<std::mutex> guard(_mutex);
    if ((!_connected || _conn == nullptr) && !reconnectLocked()) {
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
    if ((!_connected || _conn == nullptr) && !reconnectLocked()) {
        return false;
    }
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (startTransactionLocked()) {
            return true;
        }
        const unsigned int errorCode = _lastErrno;
        if (attempt == 0 && isConnectionError(errorCode) && reconnectLocked()) {
            continue;
        }
        return false;
    }
    return false;
}

bool MySQLClient::commit() {
    std::lock_guard<std::mutex> guard(_mutex);
    if (!_connected || _conn == nullptr) {
        _lastError = "MySQL is not connected";
        _lastErrno = 0U;
        return false;
    }
    if (mysql_commit(_conn) != 0) {
        const unsigned int errorCode = mysql_errno(_conn);
        setErrorLocked(mysql_error(_conn), errorCode);
        /* 提交结果未知时不自动重放，避免重复创建会话记录。 */
        if (isConnectionError(errorCode)) {
            _connected = false;
            _inTransaction = false;
        }
        return false;
    }
    _inTransaction = false;
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
        const unsigned int errorCode = mysql_errno(_conn);
        setErrorLocked(mysql_error(_conn), errorCode);
        if (isConnectionError(errorCode)) {
            _connected = false;
        }
        _inTransaction = false;
        return false;
    }
    _inTransaction = false;
    _lastError.clear();
    _lastErrno = 0U;
    return true;
}

unsigned int MySQLClient::lastErrno() const {
    std::lock_guard<std::mutex> guard(_mutex);
    return _lastErrno;
}

} // namespace smart_home
