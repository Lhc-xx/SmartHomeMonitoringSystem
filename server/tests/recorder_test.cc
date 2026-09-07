// recorder_test.cc —— 录像文件写入器 + 目录检查（角色 A）
//
// 验证 ensureDirectoryExists（递归建目录）与 Recorder 的文件生命周期：
// open / write / close / isOpen / bytesWritten / filePath，并回读校验内容。
// 只依赖标准库 + POSIX，不依赖 MySQL / log4cpp / epoll。
#include "recorder.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <unistd.h> // getpid

using smart_home::Recorder;
using smart_home::ensureDirectoryExists;

static int g_failures = 0;
static void expect(bool cond, const char *msg) {
    if (cond) {
        std::printf("  [ok]   %s\n", msg);
    } else {
        std::printf("  [FAIL] %s\n", msg);
        ++g_failures;
    }
}

static bool dirExists(const std::string &path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

int main() {
    std::printf("=== Recorder test begin ===\n");

    const std::string base = "/tmp/smarthome_rec_test_" + std::to_string(getpid());
    const std::string nested = base + "/a/b/c";
    const std::string recFile = base + "/rec.rec";
    const std::string text = "hello-smart-home-recording";

    // 1) 目录检查：递归创建多级目录
    expect(!dirExists(base), "base dir not exists initially");
    expect(ensureDirectoryExists(nested), "ensureDirectoryExists creates nested dirs");
    expect(dirExists(nested), "nested dir now exists");
    expect(ensureDirectoryExists(nested), "ensureDirectoryExists idempotent (existing)");
    expect(ensureDirectoryExists(""), "empty path treated as current dir");

    // 2) Recorder 初始状态
    {
        Recorder rec;
        expect(!rec.isOpen(), "initially not open");
        expect(rec.bytesWritten() == 0U, "initially 0 bytes");
        expect(rec.filePath().empty(), "initially empty filePath");
    }

    // 3) 打开 + 写入 + 关闭
    {
        Recorder rec;
        expect(rec.open(recFile), "open creates file");
        expect(rec.isOpen(), "isOpen after open");
        expect(rec.filePath() == recFile, "filePath stored");

        std::vector<uint8_t> payload(text.begin(), text.end());
        expect(rec.write(payload), "write payload ok");
        expect(rec.bytesWritten() == text.size(), "bytesWritten == text.size()");

        std::vector<uint8_t> empty;
        expect(rec.write(empty), "write empty is no-op success");
        expect(rec.bytesWritten() == text.size(), "bytesWritten unchanged after empty write");

        rec.close();
        expect(!rec.isOpen(), "not open after close");
        expect(!rec.write(payload), "write after close fails");
    }

    // 4) 回读文件校验内容
    {
        std::ifstream in(recFile.c_str(), std::ios::binary);
        expect(in.good(), "reopen file for read");
        std::string data((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
        expect(data.size() == text.size(), "file size == text.size()");
        expect(data == text, "file content matches");
    }

    // 5) 重新 open 会截断旧内容（bytesWritten 归零）
    {
        Recorder rec;
        expect(rec.open(recFile), "reopen truncates");
        expect(rec.bytesWritten() == 0U, "bytesWritten reset after reopen");
        std::vector<uint8_t> small(5, 0xAB);
        expect(rec.write(small), "write 5 bytes");
        expect(rec.bytesWritten() == 5U, "bytesWritten == 5");
        rec.close();
    }

    // 6) open 一个父目录不存在、但可自动创建的文件
    {
        Recorder rec;
        const std::string autoFile = base + "/auto_dir/x.rec";
        expect(rec.open(autoFile), "open auto-creates parent dir");
        expect(dirExists(base + "/auto_dir"), "parent dir auto-created");
        expect(rec.write(std::vector<uint8_t>(8, 0x01)), "write to auto-created file");
        rec.close();
    }

    // 清理：逆序删除目录与文件（best-effort）
    std::remove(recFile.c_str());
    std::remove((base + "/auto_dir/x.rec").c_str());
    rmdir((base + "/auto_dir").c_str());
    rmdir((base + "/a/b/c").c_str());
    rmdir((base + "/a/b").c_str());
    rmdir((base + "/a").c_str());
    rmdir(base.c_str());

    std::printf("=== Recorder test end ===\n");
    if (g_failures == 0) {
        std::printf("recorder test passed.\n");
        return 0;
    }
    std::printf("recorder test FAILED: %d items.\n", g_failures);
    return 1;
}
