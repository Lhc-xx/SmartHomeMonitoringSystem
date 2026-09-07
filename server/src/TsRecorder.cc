// TsRecorder.cc —— ffmpeg CLI 分段录制 MPEG-TS（角色 C 维护）

#include "TsRecorder.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace smart_home {

namespace {

bool ensureDirectory(const std::string &path) {
    if (path.empty()) {
        return false;
    }
    struct stat st{};
    if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }
    // 递归创建（mkdir -p 语义）：逐级尝试
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        cur.push_back(path[i]);
        if (path[i] == '/' || i + 1 == path.size()) {
            if (!cur.empty() && cur != "/") {
                if (mkdir(cur.c_str(), 0755) != 0 && errno != EEXIST) {
                    return false;
                }
            }
        }
    }
    return true;
}

} // namespace

TsRecorder::TsRecorder() : _pid(-1) {}

TsRecorder::~TsRecorder() {
    stop();
}

bool TsRecorder::start(const std::string &url, const std::string &outputDir, int segmentSec) {
    stop();  // 已在录制则先停

    if (url.empty() || outputDir.empty() || segmentSec <= 0 || segmentSec > 3600) {
        return false;
    }
    if (!ensureDirectory(outputDir)) {
        return false;
    }
    _outputDir = outputDir;

    const std::string pattern = outputDir + "/seg_%05d.ts";
    const std::string segSec = std::to_string(segmentSec);

    /*
     * 通过 close-on-exec 管道确认 execvp 是否真正成功：fork 成功并不等于
     * ffmpeg 已启动，若 PATH 配置错误，父进程必须收到明确失败而不能把
     * “录像已开始”返回给上层。
     */
    int execPipe[2] = {-1, -1};
    if (pipe(execPipe) != 0) {
        return false;
    }
    const int readFlags = fcntl(execPipe[0], F_GETFD, 0);
    const int writeFlags = fcntl(execPipe[1], F_GETFD, 0);
    if (readFlags < 0 || writeFlags < 0
        || fcntl(execPipe[0], F_SETFD, readFlags | FD_CLOEXEC) != 0
        || fcntl(execPipe[1], F_SETFD, writeFlags | FD_CLOEXEC) != 0) {
        close(execPipe[0]);
        close(execPipe[1]);
        return false;
    }

    const pid_t pid = fork();
    if (pid < 0) {
        close(execPipe[0]);
        close(execPipe[1]);
        return false;
    }
    if (pid == 0) {
        close(execPipe[0]);
        // 子进程：ffmpeg 输出丢弃到 /dev/null，避免污染服务器日志
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);

        // -rtsp_transport 只对 rtsp 源有效；本地文件/其它源带上会报错
        std::vector<std::string> args;
        args.push_back("ffmpeg");
        args.push_back("-loglevel");
        args.push_back("error");
        if (url.compare(0, 7, "rtsp://") == 0) {
            args.push_back("-rtsp_transport");
            args.push_back("tcp");
        }
        args.push_back("-i");
        args.push_back(url);
        args.push_back("-c");
        args.push_back("copy");
        args.push_back("-f");
        args.push_back("segment");
        args.push_back("-segment_time");
        args.push_back(segSec);
        args.push_back("-segment_format");
        args.push_back("mpegts");
        args.push_back(pattern);

        std::vector<char *> argv;
        for (std::string &a : args) {
            argv.push_back(const_cast<char *>(a.c_str()));
        }
        argv.push_back(nullptr);
        execvp("ffmpeg", argv.data());
        const int execError = errno;
        /* 管道仍然打开说明 exec 失败；父进程据此把本次 start 判为失败。 */
        (void)!write(execPipe[1], &execError, sizeof(execError));
        _exit(127);  // exec 失败
    }

    close(execPipe[1]);
    int execError = 0;
    ssize_t statusBytes = 0;
    do {
        statusBytes = read(execPipe[0], &execError, sizeof(execError));
    } while (statusBytes < 0 && errno == EINTR);
    close(execPipe[0]);
    if (statusBytes > 0) {
        /* exec 失败的子进程必然很快退出；回收它避免留下僵尸进程。 */
        waitpid(pid, nullptr, 0);
        _outputDir.clear();
        return false;
    }

    _pid = pid;
    return true;
}

void TsRecorder::stop() {
    if (_pid < 0) {
        return;
    }
    const pid_t pid = _pid;
    _pid = -1;
    kill(pid, SIGTERM);      // 让 ffmpeg 优雅写尾
    waitpid(pid, nullptr, 0);
}

bool TsRecorder::isRecording() const {
    return _pid >= 0;
}

std::vector<std::string> TsRecorder::producedFiles() const {
    std::vector<std::string> files;
    DIR *dir = opendir(_outputDir.c_str());
    if (dir == nullptr) {
        return files;
    }
    struct dirent *entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        const std::string name = entry->d_name;
        // 只取本次命名的 seg_*.ts
        if (name.size() >= 8 && name.compare(0, 4, "seg_") == 0
            && name.compare(name.size() - 3, 3, ".ts") == 0) {
            files.push_back(_outputDir + "/" + name);
        }
    }
    closedir(dir);
    std::sort(files.begin(), files.end());
    return files;
}

} // namespace smart_home
