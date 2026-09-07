#include <QApplication>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
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
};

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

QTEST_MAIN(MonitoringDashboardTest)

#include "MonitoringDashboardTest.moc"
