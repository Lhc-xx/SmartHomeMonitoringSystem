// connection_output_test.cc —— Connection 非阻塞发送队列测试
//
// 通过本机 socketpair 模拟慢客户端，验证业务线程追加的大块数据能够由
// flushOutput 分多次写完，并且不会丢字节、乱序或把 EPOLLOUT 长期保持开启。

#include "connection.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

bool setNonBlocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool drainAvailable(int fd, std::vector<uint8_t> &received) {
    uint8_t buffer[8192];
    while (true) {
        const ssize_t count = recv(fd, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (count > 0) {
            received.insert(received.end(), buffer, buffer + count);
            continue;
        }
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
            return true;
        }
        return count == 0;
    }
}

} // namespace

int main() {
    int sockets[2] = {-1, -1};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0
        || !setNonBlocking(sockets[0]) || !setNonBlocking(sockets[1])) {
        std::fprintf(stderr, "socketpair/nonblocking setup failed: %s\n", std::strerror(errno));
        return 1;
    }

    int sendBufferSize = 4096;
    setsockopt(sockets[0], SOL_SOCKET, SO_SNDBUF,
               &sendBufferSize, sizeof(sendBufferSize));

    smart_home::Connection connection(sockets[0]);
    bool writeEnabled = false;
    int enableCount = 0;
    int disableCount = 0;
    connection.setWriteInterestCallback([&](bool enabled) {
        writeEnabled = enabled;
        enabled ? ++enableCount : ++disableCount;
    });

    const size_t expectedSize = 1024U * 1024U;
    std::vector<uint8_t> expected(expectedSize);
    for (size_t index = 0; index < expected.size(); ++index) {
        expected[index] = static_cast<uint8_t>(index % 251U);
    }
    if (connection.sendData(expected) != expected.size() || !writeEnabled) {
        std::fprintf(stderr, "sendData did not queue the complete payload\n");
        close(sockets[1]);
        return 1;
    }

    std::vector<uint8_t> received;
    received.reserve(expected.size());
    for (int attempt = 0; received.size() < expected.size() && attempt < 20000; ++attempt) {
        const smart_home::Connection::FlushResult result = connection.flushOutput();
        if (result == smart_home::Connection::FlushResult::Fatal) {
            std::fprintf(stderr, "flushOutput returned Fatal\n");
            close(sockets[1]);
            return 1;
        }
        if (!drainAvailable(sockets[1], received)) {
            std::fprintf(stderr, "peer closed before receiving payload\n");
            close(sockets[1]);
            return 1;
        }
        if (received.size() < expected.size()) {
            /* Pending 表示内核缓冲已满；下轮继续 flush，模拟慢客户端。 */
            usleep(1000);
        }
    }

    const bool exact = received == expected;
    const bool callbackState = !writeEnabled && enableCount == 1 && disableCount == 1;
    close(sockets[1]);
    if (!exact || !callbackState) {
        std::fprintf(stderr, "payload/callback verification failed\n");
        return 1;
    }
    std::printf("connection_output test passed.\n");
    return 0;
}
