#include "recorder.h"

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

namespace smart_home {

namespace {

// 提取路径的父目录：无 '/' 返回空串（当前目录），根目录返回 "/"。
std::string dirName(const std::string &path) {
    const size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        return std::string();
    }
    if (pos == 0) {
        return "/";
    }
    return path.substr(0, pos);
}

} // namespace

// 递归创建目录（mkdir -p）：目录已存在（且确实是目录）或创建成功均返回 true。
bool ensureDirectoryExists(const std::string &path) {
    if (path.empty()) {
        return true; // 空路径 = 当前目录，视为已存在
    }

    std::string current;
    const size_t n = path.size();
    for (size_t i = 0; i < n; ++i) {
        current.push_back(path[i]);
        // 在每个 '/' 以及路径末尾尝试创建当前前缀目录
        if (path[i] == '/' || i + 1 == n) {
            std::string dir = current;
            while (dir.size() > 1 && dir.back() == '/') {
                dir.pop_back(); // 去掉末尾 '/'
            }
            if (dir.empty() || dir == "/" || dir == "." || dir == "..") {
                continue;
            }
            if (mkdir(dir.c_str(), 0755) != 0) {
                if (errno != EEXIST) {
                    return false;
                }
                struct stat st;
                if (stat(dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
                    return false; // 同名非目录文件
                }
            }
        }
    }
    return true;
}

Recorder::Recorder()
    : _filePath()
    , _bytesWritten(0)
    , _fd(-1) {}

Recorder::~Recorder() {
    close();
}

bool Recorder::open(const std::string &filePath) {
    std::lock_guard<std::mutex> guard(_mutex);
    if (_fd >= 0) {
        ::close(_fd); // 已打开则先关闭旧文件
        _fd = -1;
    }
    _filePath = filePath;
    _bytesWritten = 0;

    // 确保父目录存在（目录检查下沉到写入器，双保险）
    const std::string parent = dirName(filePath);
    if (!parent.empty() && parent != "/") {
        if (!ensureDirectoryExists(parent)) {
            return false;
        }
    }

    _fd = ::open(filePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    return _fd >= 0;
}

bool Recorder::write(const std::vector<uint8_t> &data) {
    std::lock_guard<std::mutex> guard(_mutex);
    if (_fd < 0) {
        return false;
    }
    if (data.empty()) {
        return true; // 空数据视为无操作成功
    }

    size_t remain = data.size();
    const uint8_t *p = data.data();
    while (remain > 0) {
        ssize_t n = ::write(_fd, p, remain);
        if (n < 0) {
            if (errno == EINTR) {
                continue; // 被信号打断，重试
            }
            return false;
        }
        p += static_cast<size_t>(n);
        remain -= static_cast<size_t>(n);
    }
    _bytesWritten += data.size();
    return true;
}

void Recorder::close() {
    std::lock_guard<std::mutex> guard(_mutex);
    if (_fd >= 0) {
        ::close(_fd);
        _fd = -1;
    }
}

bool Recorder::isOpen() const {
    std::lock_guard<std::mutex> guard(_mutex);
    return _fd >= 0;
}

std::size_t Recorder::bytesWritten() const {
    std::lock_guard<std::mutex> guard(_mutex);
    return _bytesWritten;
}

const std::string &Recorder::filePath() const {
    std::lock_guard<std::mutex> guard(_mutex);
    return _filePath;
}

} // namespace smart_home
