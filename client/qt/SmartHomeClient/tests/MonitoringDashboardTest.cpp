#include <QApplication>
#include <QGroupBox>
#include <QPushButton>
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

QTEST_MAIN(MonitoringDashboardTest)

#include "MonitoringDashboardTest.moc"
