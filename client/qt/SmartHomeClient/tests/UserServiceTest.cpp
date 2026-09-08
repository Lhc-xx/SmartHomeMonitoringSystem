#include <QByteArray>
#include <QElapsedTimer>
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
    /* 连接仍在异步建立时点击登录，请求必须等待连接成功后自动发送。 */
    void queuesLoginUntilTcpConnected();
    /* 登录连接断开后，旧 userId/token 必须失效，防止重连时误用旧会话。 */
    void disconnectClearsAuthenticatedState();
    /* 认证请求必须串行排队，快速 PTZ 松开产生的 stop 不能被丢弃。 */
    void queuesAuthenticatedRequestsAndPtzStop();

private:
    /* 以下辅助函数显式写入大端整数，保证测试响应与跨端 TLV 契约一致。 */
    static void appendUint16BE(QByteArray &buffer, quint16 value);
    static void appendUint32BE(QByteArray &buffer, quint32 value);
    static void appendUint64BE(QByteArray &buffer, quint64 value);
    static quint32 readRequestId(const QByteArray &packet);
    static QByteArray makeLoginResponse(quint32 requestId);
    static QByteArray makeControlResponse(quint16 type, quint32 requestId);
    static QByteArray makeDeviceListResponse(quint32 requestId);
    static bool takePacket(QTcpSocket *peer, QByteArray &buffer,
                           ClientProtocol::Packet &packet);
    /* 该函数内部消费字节，因此不能直接作为 QTRY_VERIFY 的表达式重复求值。 */
    static bool waitForPacket(QTcpSocket *peer, QByteArray &buffer,
                              ClientProtocol::Packet &packet, int timeoutMs);
};

void UserServiceTest::appendUint16BE(QByteArray &buffer, quint16 value)
{
    buffer.append(static_cast<char>((value >> 8) & 0xFFU));
    buffer.append(static_cast<char>(value & 0xFFU));
}

void UserServiceTest::queuesLoginUntilTcpConnected()
{
    QTcpServer localServer;
    QVERIFY2(localServer.listen(QHostAddress::LocalHost, 0),
             "本机临时 TCP 服务监听失败");

    TcpClient tcpClient;
    UserService service(&tcpClient);
    QSignalSpy waitingSpy(&service, &UserService::requestWaiting);
    QSignalSpy loginSuccessSpy(&service, &UserService::loginSuccess);

    /* 故意先提交登录，再开始连接，模拟用户启动后立即点击登录。 */
    service.loginUser(QStringLiteral("queued-user"), QStringLiteral("queued-password"));
    QTRY_COMPARE_WITH_TIMEOUT(waitingSpy.count(), 1, 500);
    QCOMPARE(loginSuccessSpy.count(), 0);

    tcpClient.connectServer(QStringLiteral("127.0.0.1"), localServer.serverPort());
    QTRY_VERIFY_WITH_TIMEOUT(localServer.hasPendingConnections(), 2000);
    QTcpSocket *peer = localServer.nextPendingConnection();
    QVERIFY(peer != nullptr);

    QByteArray requestBytes;
    ClientProtocol::Packet request;
    QVERIFY(waitForPacket(peer, requestBytes, request, 2000));
    QCOMPARE(request.type, ClientProtocol::LoginRequest);
    peer->write(makeLoginResponse(request.requestId));
    peer->flush();
    QTRY_COMPARE_WITH_TIMEOUT(loginSuccessSpy.count(), 1, 2000);
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

QByteArray UserServiceTest::makeControlResponse(quint16 type, quint32 requestId)
{
    /* 控制响应统一为一个大端 ErrorCode=SUCCESS 的四字节 body。 */
    QByteArray packet;
    appendUint16BE(packet, type);
    appendUint16BE(packet, 1U);
    appendUint32BE(packet, 4U);
    appendUint32BE(packet, requestId);
    appendUint32BE(packet, 0U);
    return packet;
}

QByteArray UserServiceTest::makeDeviceListResponse(quint32 requestId)
{
    /* 设备列表响应 body = ErrorCode + count；本用例返回空列表即可。 */
    QByteArray packet;
    appendUint16BE(packet, 0x1202U);
    appendUint16BE(packet, 1U);
    appendUint32BE(packet, 6U);
    appendUint32BE(packet, requestId);
    appendUint32BE(packet, 0U);
    appendUint16BE(packet, 0U);
    return packet;
}

bool UserServiceTest::takePacket(QTcpSocket *peer, QByteArray &buffer,
                                 ClientProtocol::Packet &packet)
{
    if (peer == nullptr) {
        return false;
    }
    /* 服务端 QTcpSocket 的 readyRead 可能尚未把内核字节搬进 Qt 缓冲，
       短暂等待可让测试稳定观察到异步发送，不改变生产网络时序。 */
    if (peer->bytesAvailable() == 0) {
        peer->waitForReadyRead(20);
    }
    if (peer->bytesAvailable() > 0) {
        buffer.append(peer->readAll());
    }
    ErrorCode errorCode = ErrorCode::INVALID_PACKET;
    return ClientProtocol::tryTakePacket(buffer, packet, errorCode)
        == ClientProtocol::PacketState::Decoded;
}

bool UserServiceTest::waitForPacket(QTcpSocket *peer, QByteArray &buffer,
                                    ClientProtocol::Packet &packet, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    do {
        if (takePacket(peer, buffer, packet)) {
            return true;
        }
        QTest::qWait(20);
    } while (timer.elapsed() < timeoutMs);
    return takePacket(peer, buffer, packet);
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

void UserServiceTest::queuesAuthenticatedRequestsAndPtzStop()
{
    QTcpServer localServer;
    QVERIFY2(localServer.listen(QHostAddress::LocalHost, 0),
             "本机临时 TCP 服务监听失败");

    TcpClient tcpClient;
    tcpClient.setAutoReconnect(false);
    UserService service(&tcpClient);
    QSignalSpy connectedSpy(&tcpClient, &TcpClient::connected);
    QSignalSpy loginSuccessSpy(&service, &UserService::loginSuccess);

    tcpClient.connectServer(QStringLiteral("127.0.0.1"), localServer.serverPort());
    QTRY_COMPARE_WITH_TIMEOUT(connectedSpy.count(), 1, 2000);
    QVERIFY(localServer.hasPendingConnections());
    QTcpSocket *peer = localServer.nextPendingConnection();
    QVERIFY(peer != nullptr);

    service.loginUser(QStringLiteral("queue-user"), QStringLiteral("queue-password"));
    QTRY_VERIFY_WITH_TIMEOUT(peer->bytesAvailable() > 0, 2000);
    const QByteArray loginRequest = peer->readAll();
    const quint32 loginRequestId = readRequestId(loginRequest);
    QVERIFY(loginRequestId != 0U);
    peer->write(makeLoginResponse(loginRequestId));
    peer->flush();
    QTRY_COMPARE_WITH_TIMEOUT(loginSuccessSpy.count(), 1, 2000);

    QByteArray wire;
    ClientProtocol::Packet request;
    QSignalSpy streamStartedSpy(&service, &UserService::streamStarted);
    service.startStream(QStringLiteral("mock://queue"));
    service.requestDeviceList();
    service.sendPtzControl(QStringLiteral("http://127.0.0.1"),
                           QStringLiteral("up"), QStringLiteral("start"));
    service.sendPtzControl(QStringLiteral("http://127.0.0.1"),
                           QStringLiteral("stop"), QStringLiteral("stop"));
    /* 第一条请求占用等待位，其余请求必须排队而不能立即失败。 */
    QTRY_VERIFY_WITH_TIMEOUT(peer->bytesAvailable() > 0, 2000);
    wire.append(peer->readAll());
    ErrorCode firstParseError = ErrorCode::INVALID_PACKET;
    QCOMPARE(ClientProtocol::tryTakePacket(wire, request, firstParseError),
             ClientProtocol::PacketState::Decoded);
    QCOMPARE(request.type, ClientProtocol::StreamStartRequest);
    const quint32 streamRequestId = request.requestId;
    peer->write(makeControlResponse(ClientProtocol::StreamStartResponseType, streamRequestId));
    peer->flush();
    QTRY_COMPARE_WITH_TIMEOUT(streamStartedSpy.count(), 1, 2000);

    QTRY_VERIFY_WITH_TIMEOUT(peer->bytesAvailable() > 0, 2000);
    QVERIFY(waitForPacket(peer, wire, request, 2000));
    /* stop 是最新的云台状态，必须抢在尚未发送的普通请求前到达服务端。 */
    QCOMPARE(request.type, ClientProtocol::PtzControlRequest);
    QVERIFY(request.body.contains(QByteArray("stop")));
    peer->write(makeControlResponse(ClientProtocol::PtzControlResponseType, request.requestId));
    peer->flush();

    QVERIFY(waitForPacket(peer, wire, request, 2000));
    QCOMPARE(request.type, ClientProtocol::DeviceListRequest);
    const quint32 deviceRequestId = request.requestId;
    peer->write(makeDeviceListResponse(deviceRequestId));
    peer->flush();
}

QTEST_GUILESS_MAIN(UserServiceTest)

#include "UserServiceTest.moc"
