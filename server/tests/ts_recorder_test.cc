// ts_recorder_test.cc —— TsRecorder 子进程启动失败回报测试
//
// 将 PATH 临时改为不存在的目录，验证 execvp("ffmpeg") 失败时 start 返回
// false，而不是把仅 fork 成功误报为“正在录像”。测试不接触真实摄像头。

#include "TsRecorder.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <string>

int main() {
    char directoryTemplate[] = "/tmp/smarthome-ts-recorder-XXXXXX";
    char *directory = mkdtemp(directoryTemplate);
    if (directory == nullptr) {
        std::fprintf(stderr, "mkdtemp failed\n");
        return 1;
    }

    const char *oldPathValue = std::getenv("PATH");
    const std::string oldPath = oldPathValue == nullptr ? std::string() : oldPathValue;
    const bool hadPath = oldPathValue != nullptr;
    setenv("PATH", "/definitely/missing/ffmpeg", 1);

    smart_home::TsRecorder recorder;
    const bool started = recorder.start("mock://not-used", directory, 1);

    if (hadPath) {
        setenv("PATH", oldPath.c_str(), 1);
    } else {
        unsetenv("PATH");
    }
    rmdir(directory);

    if (started || recorder.isRecording()) {
        std::fprintf(stderr, "TsRecorder reported a false successful start\n");
        return 1;
    }
    std::printf("ts_recorder test passed.\n");
    return 0;
}

