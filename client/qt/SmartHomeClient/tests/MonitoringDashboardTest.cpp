#include <QApplication>
#include <QGroupBox>
#include <QHostAddress>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTreeWidget>
#include <QtTest>

#include "ui/MonitoringDashboard.h"
#include "video/VideoWidget.h"

/*
 * MonitoringDashboardTest 验证登录后工作台的四宫格、设备树和云台禁用状态。
 * 测试只构造本地 QWidget，不启动 FFmpeg、不连接真实摄像头或设备 Web API。
 */
class MonitoringDashboardTest : public QObject
{
    Q_OBJECT

private slots:
    void createsFourChannelsAndDeviceTree();
    void hidesRedundantHeaderBranding();
    void recordQueryCarriesSelectedServerDeviceId();
    void recordControlsCarrySelectedServerDeviceId();
    void domeUsesDirectControlWhenConfigured();
    void domeUsesServerForwarderWhenConfigured();
};

namespace {

void sendJsonReply(QTcpSocket *socket, const QByteArray &body)
{
    const QByteArray header = QByteArray("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ")
        + QByteArray::number(body.size()) + QByteArray("\r\nConnection: close\r\n\r\n");
    socket->write(header + body);
    socket->disconnectFromHost();
}

CameraConfig makeDomeConfig(quint16 port, CameraConfig::PtzTransport transport)
{
    CameraConfig config;
    config.name = QStringLiteral("Dome");
    config.type = QStringLiteral("dome");
    config.rtspUrl = QStringLiteral("rtsp://test-user:test-password@camera-b/live/chn=0");
    config.webUrl = QStringLiteral("http://127.0.0.1:%1").arg(port);
    config.user = QStringLiteral("test-user");
    config.password = QStringLiteral("test-password");
    config.enabled = true;
    config.ptzTransport = transport;
    return config;
}

void sendMouseEvent(QPushButton *button, QEvent::Type type)
{
    QMouseEvent event(type, QPointF(8, 8), Qt::LeftButton,
                      type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton,
                      Qt::NoModifier);
    QApplication::sendEvent(button, &event);
}

}

void MonitoringDashboardTest::createsFourChannelsAndDeviceTree()
{
    MonitoringDashboard dashboard;
    const QList<VideoWidget *> videos = dashboard.videoWidgets();
    QCOMPARE(videos.size(), 4);
    QCOMPARE(videos.at(0)->channelName(), QStringLiteral("通道 01 · 枪机"));
    QCOMPARE(videos.at(1)->channelName(), QStringLiteral("通道 02 · 球机"));
    QCOMPARE(dashboard.deviceTree()->topLevelItemCount(), 2);
    QVERIFY(dashboard.ptzPanel() != nullptr);

    const QList<QPushButton *> ptzButtons = dashboard.ptzPanel()->findChildren<QPushButton *>();
    QVERIFY(ptzButtons.size() >= 9);
    for (QPushButton *button : ptzButtons) {
        QVERIFY(!button->isEnabled());
    }
}

void MonitoringDashboardTest::hidesRedundantHeaderBranding()
{
    MonitoringDashboard dashboard;

    // 顶部导航只保留业务入口，避免重复显示登录页已经出现过的品牌文字。
    QVERIFY(dashboard.findChild<QLabel *>(QStringLiteral("dashboardLogo")) == nullptr);
    QVERIFY(dashboard.findChild<QLabel *>(QStringLiteral("dashboardTitle")) == nullptr);
}

void MonitoringDashboardTest::recordQueryCarriesSelectedServerDeviceId()
{
    MonitoringDashboard dashboard;
    ClientProtocol::DeviceInfo device;
    device.id = 42;
    device.name = QStringLiteral("测试设备");
    device.type = QStringLiteral("camera");
    device.status = QStringLiteral("online");
    dashboard.setDevices(QList<ClientProtocol::DeviceInfo>() << device);

    QTreeWidgetItem *serverRoot = dashboard.deviceTree()->findItems(
        QStringLiteral("服务端设备"), Qt::MatchExactly).value(0);
    QVERIFY(serverRoot != nullptr);
    QVERIFY(serverRoot->childCount() == 1);
    dashboard.deviceTree()->setCurrentItem(serverRoot->child(0));

    QPushButton *recordButton = nullptr;
    const QList<QPushButton *> buttons = dashboard.findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        if (button->text() == QStringLiteral("录像查询")) {
            recordButton = button;
            break;
        }
    }
    QVERIFY(recordButton != nullptr);

    QSignalSpy requestSpy(&dashboard, &MonitoringDashboard::requestRecordList);
    recordButton->click();
    QCOMPARE(requestSpy.count(), 1);
    QVERIFY2(requestSpy.at(0).size() == 1,
             "录像查询信号必须携带当前服务端设备 ID");
    QCOMPARE(requestSpy.at(0).at(0).toULongLong(), static_cast<qulonglong>(42));
}

void MonitoringDashboardTest::recordControlsCarrySelectedServerDeviceId()
{
    MonitoringDashboard dashboard;
    ClientProtocol::DeviceInfo device;
    device.id = 42;
    device.name = QStringLiteral("测试设备");
    device.type = QStringLiteral("camera");
    device.status = QStringLiteral("online");
    dashboard.setDevices(QList<ClientProtocol::DeviceInfo>() << device);

    QTreeWidgetItem *serverRoot = dashboard.deviceTree()->findItems(
        QStringLiteral("服务端设备"), Qt::MatchExactly).value(0);
    QVERIFY(serverRoot != nullptr);
    QVERIFY(serverRoot->childCount() == 1);
    dashboard.deviceTree()->setCurrentItem(serverRoot->child(0));

    QPushButton *recordControl = dashboard.findChild<QPushButton *>(
        QStringLiteral("recordControlButton"));
    QVERIFY(recordControl != nullptr);
    QCOMPARE(recordControl->text(), QStringLiteral("开始录像"));

    QSignalSpy startSpy(&dashboard, &MonitoringDashboard::requestRecordStart);
    QSignalSpy stopSpy(&dashboard, &MonitoringDashboard::requestRecordStop);
    recordControl->click();
    QCOMPARE(startSpy.count(), 1);
    QCOMPARE(startSpy.at(0).at(0).toULongLong(), static_cast<qulonglong>(42));

    dashboard.setRecordingActive(true);
    QCOMPARE(recordControl->text(), QStringLiteral("停止录像"));
    recordControl->click();
    QCOMPARE(stopSpy.count(), 1);
}

void MonitoringDashboardTest::domeUsesDirectControlWhenConfigured()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    QList<QByteArray> requests;
    connect(&server, &QTcpServer::newConnection, this, [&server, &requests]() {
        QTcpSocket *socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, socket, [socket, &requests]() {
            const QByteArray request = socket->readAll();
            requests.append(request);
            sendJsonReply(socket, request.contains("/api/ptz/baseConf")
                          ? QByteArray("{\"ptzSpeed\":4,\"steps\":10}") : QByteArray("{}"));
        });
    });

    MonitoringDashboard dashboard;
    int forwardedCount = 0;
    dashboard.setControlForwarder([&forwardedCount](const QString &, const QString &, const QString &) {
        ++forwardedCount;
    });
    dashboard.setCameraConfigs(QList<CameraConfig>() << makeDomeConfig(
        server.serverPort(), CameraConfig::PtzTransport::Direct));

    QTreeWidgetItem *domeItem = dashboard.deviceTree()->findItems(
        QStringLiteral("球机 · 通道 02"), Qt::MatchExactly).value(0);
    QVERIFY(domeItem != nullptr);
    dashboard.deviceTree()->setCurrentItem(nullptr);
    dashboard.deviceTree()->setCurrentItem(domeItem);

    QPushButton *upButton = dashboard.ptzPanel()->findChild<QPushButton *>(
        QStringLiteral("ptzDirectionButton1"));
    QVERIFY(upButton != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(upButton->isEnabled(), 2000);

    sendMouseEvent(upButton, QEvent::MouseButtonPress);
    QTRY_VERIFY_WITH_TIMEOUT(requests.size() >= 2, 2000);
    sendMouseEvent(upButton, QEvent::MouseButtonRelease);
    QTRY_VERIFY_WITH_TIMEOUT(requests.size() >= 3, 2000);

    QVERIFY(requests.at(1).contains("GET /api/ptz/control?direction=up&move=start"));
    QVERIFY(requests.at(2).contains("GET /api/ptz/control?direction=stop&move=stop"));
    QCOMPARE(forwardedCount, 0);
}

void MonitoringDashboardTest::domeUsesServerForwarderWhenConfigured()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    QList<QByteArray> requests;
    connect(&server, &QTcpServer::newConnection, this, [&server, &requests]() {
        QTcpSocket *socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, socket, [socket, &requests]() {
            requests.append(socket->readAll());
            sendJsonReply(socket, QByteArray("{\"ptzSpeed\":4,\"steps\":10}"));
        });
    });

    MonitoringDashboard dashboard;
    QStringList forwarded;
    dashboard.setControlForwarder([&forwarded](const QString &cameraUrl,
                                                const QString &direction,
                                                const QString &move) {
        forwarded << cameraUrl << direction << move;
    });
    dashboard.setCameraConfigs(QList<CameraConfig>() << makeDomeConfig(
        server.serverPort(), CameraConfig::PtzTransport::Server));

    QTreeWidgetItem *domeItem = dashboard.deviceTree()->findItems(
        QStringLiteral("球机 · 通道 02"), Qt::MatchExactly).value(0);
    QVERIFY(domeItem != nullptr);
    dashboard.deviceTree()->setCurrentItem(nullptr);
    dashboard.deviceTree()->setCurrentItem(domeItem);

    QPushButton *upButton = dashboard.ptzPanel()->findChild<QPushButton *>(
        QStringLiteral("ptzDirectionButton1"));
    QVERIFY(upButton != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(upButton->isEnabled(), 2000);

    sendMouseEvent(upButton, QEvent::MouseButtonPress);
    QTRY_COMPARE_WITH_TIMEOUT(forwarded.size(), 3, 2000);
    sendMouseEvent(upButton, QEvent::MouseButtonRelease);
    QTRY_COMPARE_WITH_TIMEOUT(forwarded.size(), 6, 2000);

    QCOMPARE(forwarded.at(0), QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()));
    QCOMPARE(forwarded.at(1), QStringLiteral("up"));
    QCOMPARE(forwarded.at(2), QStringLiteral("start"));
    QCOMPARE(forwarded.at(4), QStringLiteral("stop"));
    QCOMPARE(forwarded.at(5), QStringLiteral("stop"));
    QCOMPARE(requests.size(), 1);
}

QTEST_MAIN(MonitoringDashboardTest)

#include "MonitoringDashboardTest.moc"
