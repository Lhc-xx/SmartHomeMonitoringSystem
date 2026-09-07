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

private:
    static void respond(QTcpSocket *socket, const QByteArray &body);
};

void PtzClientTest::buildsDirectionalQueries()
{
    const QUrlQuery start = PtzClient::buildControlQuery(PtzClient::Direction::UpRight, true);
    QCOMPARE(start.queryItemValue(QStringLiteral("direction")), QStringLiteral("up-right"));
    QCOMPARE(start.queryItemValue(QStringLiteral("move")), QStringLiteral("start"));

    const QUrlQuery stop = PtzClient::buildControlQuery(PtzClient::Direction::Down, false);
    QCOMPARE(stop.queryItemValue(QStringLiteral("direction")), QStringLiteral("stop"));
    QCOMPARE(stop.queryItemValue(QStringLiteral("move")), QStringLiteral("stop"));
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
    QVERIFY(requests.at(1).contains("GET /api/ptz/control?direction=up&move=start"));
    client.stopMove();
    QTRY_VERIFY_WITH_TIMEOUT(requests.size() >= 3, 2000);
    QVERIFY(requests.at(2).contains("GET /api/ptz/control?direction=stop&move=stop"));
    QVERIFY(readySpy.count() >= 1);
}

QTEST_GUILESS_MAIN(PtzClientTest)

#include "PtzClientTest.moc"
