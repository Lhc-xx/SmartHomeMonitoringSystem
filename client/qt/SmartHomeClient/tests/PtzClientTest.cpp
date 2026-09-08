#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSignalSpy>
#include <QtTest>

#include "network/PtzClient.h"

/*
 * PtzClientTest 使用本机假 HTTP 服务验证 URL、认证头和开始/停止参数。
 * 测试不会连接真实摄像头，也不会发送任何实体云台动作。
 */
class PtzClientTest : public QObject
{
    Q_OBJECT

private slots:
    void buildsDirectionalQueries();
    void probesAndSendsOnlyToLocalFakeServer();
    void ignoresFinishedFromSupersededProbe();

private:
    static void respond(QTcpSocket *socket, const QByteArray &body);
};

void PtzClientTest::buildsDirectionalQueries()
{
    /*
     * 球机网页端的真实接口使用 channelId/value/speed 三个查询参数：
     * value=u 表示向上，speed=4 是能力接口返回的默认速度。
     * 这里把设备协议写成单测，防止后续又误改回抽象的 direction/move 参数。
     */
    const QUrlQuery start = PtzClient::buildControlQuery(PtzClient::Direction::Up, true);
    QCOMPARE(start.queryItemValue(QStringLiteral("channelId")), QStringLiteral("1"));
    QCOMPARE(start.queryItemValue(QStringLiteral("value")), QStringLiteral("u"));
    QCOMPARE(start.queryItemValue(QStringLiteral("speed")), QStringLiteral("4"));

    const QUrlQuery stop = PtzClient::buildControlQuery(PtzClient::Direction::Down, false);
    QCOMPARE(stop.queryItemValue(QStringLiteral("channelId")), QStringLiteral("1"));
    QCOMPARE(stop.queryItemValue(QStringLiteral("value")), QStringLiteral("s"));
    QCOMPARE(stop.queryItemValue(QStringLiteral("speed")), QStringLiteral("4"));

    /* channelId/speed 可按摄像头配置和能力探测结果覆盖，不能写死在实现内部。 */
    const QUrlQuery custom = PtzClient::buildControlQuery(
        PtzClient::Direction::DownRight, true, 2, 7);
    QCOMPARE(custom.queryItemValue(QStringLiteral("channelId")), QStringLiteral("2"));
    QCOMPARE(custom.queryItemValue(QStringLiteral("value")), QStringLiteral("4"));
    QCOMPARE(custom.queryItemValue(QStringLiteral("speed")), QStringLiteral("7"));
}

void PtzClientTest::respond(QTcpSocket *socket, const QByteArray &body)
{
    const QByteArray header = QByteArray("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ")
        + QByteArray::number(body.size()) + QByteArray("\r\nConnection: close\r\n\r\n");
    socket->write(header + body);
    socket->disconnectFromHost();
}

void PtzClientTest::probesAndSendsOnlyToLocalFakeServer()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    QList<QByteArray> requests;
    connect(&server, &QTcpServer::newConnection, this, [&server, &requests]() {
        QTcpSocket *socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, socket, [socket, &requests]() {
            requests.append(socket->readAll());
            if (requests.size() == 1) {
                PtzClientTest::respond(socket, QByteArray("{\"ptzSpeed\":4,\"steps\":10}"));
            } else {
                PtzClientTest::respond(socket, QByteArray("{}"));
            }
        });
    });

    PtzClient client;
    QSignalSpy readySpy(&client, &PtzClient::ptzReady);
    client.setCamera(QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())),
                     QStringLiteral("test-user"), QStringLiteral("test-password"));
    client.probe();

    QTRY_VERIFY_WITH_TIMEOUT(client.isReady(), 2000);
    QVERIFY(!requests.isEmpty());
    QVERIFY(requests.at(0).startsWith("GET /api/ptz/baseConf HTTP/1.1"));
    QVERIFY(requests.at(0).contains("Authorization: Basic "));

    client.startMove(PtzClient::Direction::Up);
    QTRY_VERIFY_WITH_TIMEOUT(requests.size() >= 2, 2000);
    QVERIFY(requests.at(1).contains("GET /api/ptz/control?channelId=1&value=u&speed=4"));
    client.stopMove();
    QTRY_VERIFY_WITH_TIMEOUT(requests.size() >= 3, 2000);
    QVERIFY(requests.at(2).contains("GET /api/ptz/control?channelId=1&value=s&speed=4"));
    QVERIFY(readySpy.count() >= 1);
}

void PtzClientTest::ignoresFinishedFromSupersededProbe()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    QList<QByteArray> requests;
    connect(&server, &QTcpServer::newConnection, this, [&server, &requests]() {
        QTcpSocket *socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, socket, [socket, &requests]() {
            requests.append(socket->readAll());
            if (requests.size() == 1) {
                /* 第一条探测故意不回复；客户端第二次 probe 会将其取消。 */
                return;
            }
            PtzClientTest::respond(socket, QByteArray("{\"ptzSpeed\":4}"));
        });
    });

    PtzClient client;
    QSignalSpy errorSpy(&client, &PtzClient::errorOccurred);
    client.setCamera(QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())),
                     QStringLiteral("test-user"), QStringLiteral("test-password"));
    client.probe();
    QTRY_COMPARE_WITH_TIMEOUT(requests.size(), 1, 2000);

    /* 第二次探测成功后，旧请求的取消回调不能把 ready 状态改回失败。 */
    client.probe();
    QTRY_VERIFY_WITH_TIMEOUT(client.isReady(), 2000);
    QTest::qWait(100);
    QVERIFY(client.isReady());
    QCOMPARE(errorSpy.count(), 0);
}

QTEST_GUILESS_MAIN(PtzClientTest)

#include "PtzClientTest.moc"
