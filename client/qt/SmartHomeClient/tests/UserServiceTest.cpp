#include <QByteArray>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

#include "network/TcpClient.h"
#include "service/UserService.h"

/*
 * UserServiceTest 类职责：
 *
 * 使用仅监听 127.0.0.1 的临时 TCP 服务验证客户端认证状态机。
 * 测试不启动项目服务器、不访问真实网络，也不保存任何账号、密码或 token。
 */
class UserServiceTest : public QObject
{
    Q_OBJECT

private slots:
    /* 登录连接断开后，旧 userId/token 必须失效，防止重连时误用旧会话。 */
    void disconnectClearsAuthenticatedState();

private:
    /* 以下辅助函数显式写入大端整数，保证测试响应与跨端 TLV 契约一致。 */
    static void appendUint16BE(QByteArray &buffer, quint16 value);
    static void appendUint32BE(QByteArray &buffer, quint32 value);
    static void appendUint64BE(QByteArray &buffer, quint64 value);
    static quint32 readRequestId(const QByteArray &packet);
    static QByteArray makeLoginResponse(quint32 requestId);
};

void UserServiceTest::appendUint16BE(QByteArray &buffer, quint16 value)
{
    buffer.append(static_cast<char>((value >> 8) & 0xFFU));
    buffer.append(static_cast<char>(value & 0xFFU));
}

void UserServiceTest::appendUint32BE(QByteArray &buffer, quint32 value)
{
    buffer.append(static_cast<char>((value >> 24) & 0xFFU));
    buffer.append(static_cast<char>((value >> 16) & 0xFFU));
    buffer.append(static_cast<char>((value >> 8) & 0xFFU));
    buffer.append(static_cast<char>(value & 0xFFU));
}

void UserServiceTest::appendUint64BE(QByteArray &buffer, quint64 value)
{
    for (int shift = 56; shift >= 0; shift -= 8) {
        buffer.append(static_cast<char>((value >> shift) & 0xFFULL));
    }
}

quint32 UserServiceTest::readRequestId(const QByteArray &packet)
{
    /* TLV Header 的 RequestId 位于固定偏移 8，短包返回零并让后续断言失败。 */
    if (packet.size() < 12) {
        return 0;
    }
    const uchar *data = reinterpret_cast<const uchar *>(packet.constData());
    return (static_cast<quint32>(data[8]) << 24)
        | (static_cast<quint32>(data[9]) << 16)
        | (static_cast<quint32>(data[10]) << 8)
        | static_cast<quint32>(data[11]);
}

QByteArray UserServiceTest::makeLoginResponse(quint32 requestId)
{
    /* LOGIN_RESPONSE value 顺序：userId、token length-string、ErrorCode。 */
    QByteArray body;
    appendUint64BE(body, 42U);
    const QByteArray token("local-test-token");
    appendUint16BE(body, static_cast<quint16>(token.size()));
    body.append(token);
    appendUint32BE(body, 0U);

    QByteArray packet;
    appendUint16BE(packet, 0x1102U);
    appendUint16BE(packet, 1U);
    appendUint32BE(packet, static_cast<quint32>(body.size()));
    appendUint32BE(packet, requestId);
    packet.append(body);
    return packet;
}

void UserServiceTest::disconnectClearsAuthenticatedState()
{
    QTcpServer localServer;
    QVERIFY2(localServer.listen(QHostAddress::LocalHost, 0),
             "本机临时 TCP 服务监听失败");

    TcpClient tcpClient;
    UserService service(&tcpClient);
    QSignalSpy connectedSpy(&tcpClient, &TcpClient::connected);
    QSignalSpy disconnectedSpy(&tcpClient, &TcpClient::disconnected);
    QSignalSpy loginSuccessSpy(&service, &UserService::loginSuccess);
    QSignalSpy requestFailedSpy(&service, &UserService::requestFailed);

    tcpClient.connectServer(QStringLiteral("127.0.0.1"), localServer.serverPort());
    QTRY_COMPARE_WITH_TIMEOUT(connectedSpy.count(), 1, 2000);
    QVERIFY(localServer.hasPendingConnections());
    QTcpSocket *peer = localServer.nextPendingConnection();
    QVERIFY(peer != nullptr);

    service.loginUser(QStringLiteral("local-user"), QStringLiteral("local-password"));
    QTRY_VERIFY_WITH_TIMEOUT(peer->bytesAvailable() > 0, 2000);
    const QByteArray loginRequest = peer->readAll();
    const quint32 requestId = readRequestId(loginRequest);
    QVERIFY(requestId != 0U);

    QCOMPARE(peer->write(makeLoginResponse(requestId)) > 0, true);
    peer->flush();
    QTRY_COMPARE_WITH_TIMEOUT(loginSuccessSpy.count(), 1, 2000);
    QCOMPARE(loginSuccessSpy.at(0).at(0).toULongLong(), static_cast<qulonglong>(42U));

    /* 服务端主动断开后，再发资源请求必须先报告“请登录”，不能继续使用旧 token。 */
    peer->disconnectFromHost();
    QTRY_COMPARE_WITH_TIMEOUT(disconnectedSpy.count(), 1, 2000);
    requestFailedSpy.clear();
    service.requestDeviceList();
    QCOMPARE(requestFailedSpy.count(), 1);
    QCOMPARE(requestFailedSpy.takeFirst().at(0).toString(),
             QStringLiteral("获取设备列表失败：请先成功登录。"));
}

QTEST_GUILESS_MAIN(UserServiceTest)

#include "UserServiceTest.moc"
