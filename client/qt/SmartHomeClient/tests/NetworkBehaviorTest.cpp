#include <QCoreApplication>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>
#include <cstdio>

#include "network/TcpClient.h"
#include "service/UserService.h"

/*
 * 覆盖角色 A 的两项客户端网络行为：
 *   1. 断线自动重连：连接丢失后触发 reconnecting 信号，并最终重连成功；
 *   2. 请求超时：服务器接受连接但从不回复时，UserService 用定时器终止等待。
 *
 * 采用与 ClientProtocolTest 相同的普通 main() 断言风格，不依赖 QtTest 测试运行器，
 * 仅用 QCoreApplication + QTest::qWait 驱动事件循环，全部基于本机回环与临时端口。
 */

namespace {

bool expectTrue(bool condition, const char *message)
{
    if (condition) {
        std::printf("PASS: %s\n", message);
        return true;
    }
    std::fprintf(stderr, "FAIL: %s\n", message);
    return false;
}

bool testReconnectAfterConnectionLost()
{
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) {
        std::fprintf(stderr, "FAIL: server listen failed\n");
        return false;
    }
    const quint16 port = server.serverPort();

    TcpClient client;
    client.setReconnectDelay(1);   // 加速退避，避免测试变慢
    QSignalSpy connectedSpy(&client, &TcpClient::connected);
    QSignalSpy reconnectingSpy(&client, &TcpClient::reconnecting);

    client.connectServer(QStringLiteral("127.0.0.1"), port);
    QTest::qWait(1000);
    if (!expectTrue(connectedSpy.count() >= 1, "首次连接成功")) return false;
    if (!expectTrue(server.hasPendingConnections(), "服务器收到首条连接")) return false;
    QTcpSocket *first = server.nextPendingConnection();
    if (!expectTrue(first != nullptr, "取出首条连接套接字")) return false;

    /* 服务器断开这条连接但保持监听，客户端应触发重连并最终重新建立连接。 */
    first->abort();
    QTest::qWait(2000);
    if (!expectTrue(reconnectingSpy.count() >= 1, "触发重连信号")) return false;
    if (!expectTrue(connectedSpy.count() >= 2, "重连成功（第二次连接）")) return false;
    if (!expectTrue(server.hasPendingConnections(), "服务器收到重连连接")) return false;
    return true;
}

bool testRequestTimesOutWhenServerIsSilent()
{
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) {
        std::fprintf(stderr, "FAIL: server listen failed\n");
        return false;
    }
    const quint16 port = server.serverPort();

    TcpClient client;
    client.setAutoReconnect(false);  // 本用例只关心超时，避免重连干扰
    UserService service(&client);
    service.setRequestTimeout(100);  // 100ms 超时
    QSignalSpy connectedSpy(&client, &TcpClient::connected);
    QSignalSpy registerFailedSpy(&service, &UserService::registerFailed);

    client.connectServer(QStringLiteral("127.0.0.1"), port);
    QTest::qWait(1000);
    if (!expectTrue(connectedSpy.count() >= 1, "超时用例首次连接成功")) return false;
    QTcpSocket *accepted = server.nextPendingConnection();
    if (!expectTrue(accepted != nullptr, "服务器接受连接")) return false;

    /* 服务器接受连接但从不回复，注册请求应在超时后失败。 */
    service.registerUser(QStringLiteral("alice"), QStringLiteral("secret"));
    QTest::qWait(1000);
    if (!expectTrue(registerFailedSpy.count() >= 1, "注册请求超时失败")) return false;
    const QString reason = registerFailedSpy.at(0).at(0).toString();
    if (!expectTrue(reason.contains(QStringLiteral("超时")), "超时原因包含“超时”")) return false;
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);

    bool allPassed = true;
    allPassed = testReconnectAfterConnectionLost() && allPassed;
    allPassed = testRequestTimesOutWhenServerIsSilent() && allPassed;

    std::printf(allPassed ? "ALL TESTS PASSED\n" : "SOME TESTS FAILED\n");
    return allPassed ? 0 : 1;
}
