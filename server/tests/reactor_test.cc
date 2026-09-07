// reactor_test.cc —— Reactor 集成测试（角色 A）
//
// 在后台线程启动 Reactor，从客户端真实连接，验证：
//   1. 连接建立（accept）
//   2. 未登录拦截（未登录发 DEVICE_LIST_REQUEST -> UNAUTHORIZED）
//   3. 断线清理（客户端 close 后服务端能继续接受新连接）
//   4. 重连
//   5. 空闲回收（setIdleTimeout 设短超时，连接不发数据被回收）
//   6. 优雅停止（stop -> run 返回）
//
// 不依赖 MySQL：不注入 AuthHandler/ResourceHandler，只验证网络层与未登录拦截。
#include "reactor.h"
#include "protocol/Protocol.h"
#include "protocol/MessageType.h"
#include "protocol/ErrorCode.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

// TlvMessage / TlvProtocol / MessageType 位于全局命名空间

static const int TEST_PORT = 18077;
static int g_failures = 0;

static void expect(bool cond, const char *msg) {
    if (cond) {
        std::printf("  [ok]   %s\n", msg);
    } else {
        std::printf("  [FAIL] %s\n", msg);
        ++g_failures;
    }
}

// 连接服务器，成功返回 fd，失败返回 -1。
static int connectToServer() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(TEST_PORT);
    if (connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

// 发送一个 TLV 请求。
static bool sendRequest(int fd, uint16_t type, uint32_t requestId) {
    TlvMessage req;
    req.type = type;
    req.version = PROTOCOL_VERSION;
    req.requestId = requestId;
    std::vector<uint8_t> buf = TlvProtocol::encode(req);
    if (buf.empty()) {
        return false;
    }
    return send(fd, buf.data(), buf.size(), 0) == static_cast<ssize_t>(buf.size());
}

// 收一个 TLV 响应；返回 false 表示超时/断开。
static bool recvResponse(int fd, TlvMessage &msg) {
    std::vector<uint8_t> buf;
    char tmp[4096];
    while (true) {
        ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
        if (n <= 0) {
            return false;
        }
        buf.insert(buf.end(), tmp, tmp + n);
        std::vector<uint8_t> work = buf;
        if (TlvProtocol::tryDecode(work, msg)) {
            return true;
        }
    }
}

// 读 value 前 4 字节大端 int32。
static int32_t readErrorCode(const TlvMessage &msg) {
    if (msg.value.size() < 4) {
        return -1;
    }
    uint32_t u = (static_cast<uint32_t>(msg.value[0]) << 24) |
                 (static_cast<uint32_t>(msg.value[1]) << 16) |
                 (static_cast<uint32_t>(msg.value[2]) << 8) |
                 (static_cast<uint32_t>(msg.value[3]));
    return static_cast<int32_t>(u);
}

int main() {
    std::printf("=== Reactor integration test begin ===\n");

    // 1. 启动 Reactor（后台线程）
    smart_home::Reactor reactor(2, 100);
    reactor.setIdleTimeout(1); // 空闲回收 1 秒，便于快速验证
    if (!reactor.init("127.0.0.1", TEST_PORT)) {
        std::printf("  [FAIL] reactor init\n");
        return 1;
    }
    std::thread reactorThread([&reactor]() { reactor.run(); });
    usleep(300000); // 等 Reactor 就绪

    // 2. 连接建立
    int fd = connectToServer();
    expect(fd >= 0, "client connect succeeds");
    if (fd < 0) {
        reactor.stop();
        reactorThread.join();
        return 1;
    }

    // 3. 未登录拦截：未登录发 DEVICE_LIST_REQUEST -> UNAUTHORIZED
    {
        expect(sendRequest(fd, static_cast<uint16_t>(MessageType::DEVICE_LIST_REQUEST), 1),
               "send DEVICE_LIST_REQUEST");
        TlvMessage resp;
        expect(recvResponse(fd, resp), "receive response");
        expect(resp.type == static_cast<uint16_t>(MessageType::DEVICE_LIST_RESPONSE),
               "response type is DEVICE_LIST_RESPONSE");
        expect(readErrorCode(resp) == static_cast<int32_t>(ErrorCode::UNAUTHORIZED),
               "unauthenticated request rejected with UNAUTHORIZED");
    }

    // 4. 断线清理 + 重连
    close(fd);
    usleep(200000); // 等 Reactor 处理断开
    {
        int fd2 = connectToServer();
        expect(fd2 >= 0, "reconnect succeeds after disconnect");
        if (fd2 >= 0) {
            close(fd2);
        }
    }

    // 5. 空闲回收：连接后不发数据，超过 idleTimeout 被服务端关闭
    {
        int fd3 = connectToServer();
        expect(fd3 >= 0, "idle-test connect succeeds");
        if (fd3 >= 0) {
            sleep(3); // idleTimeout=1s + 检查周期余量
            char c = 0;
            ssize_t n = recv(fd3, &c, 1, 0);
            expect(n == 0, "idle connection reclaimed by server (peer closed)");
            close(fd3);
        }
    }

    // 6. 优雅停止
    reactor.stop();
    reactorThread.join();
    expect(true, "reactor stopped gracefully");

    std::printf("=== Reactor integration test end ===\n");
    if (g_failures == 0) {
        std::printf("reactor_test passed.\n");
        return 0;
    }
    std::printf("reactor_test FAILED: %d items.\n", g_failures);
    return 1;
}
