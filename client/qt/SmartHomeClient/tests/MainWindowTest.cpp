#include <QCheckBox>
#include <QDateTimeEdit>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QPushButton>
#include <QtTest>

#include "MainWindow.h"
#include "ui/MonitoringDashboard.h"

/*
 * MainWindowTest 类职责：
 *
 * 验证 B 数据页的录像查询条件真实存在并具备可用的启停状态。
 * 测试将服务器地址覆盖为本机无服务端口，不访问远程服务或摄像头。
 */
class MainWindowTest : public QObject
{
    Q_OBJECT

private slots:
    void recordTimeFilterCanSwitchBetweenAllAndRange();
    /* 顶部“设备数据”入口必须切换到可见的设备/录像数据页。 */
    void dashboardDeviceDataButtonShowsDataPage();
    /* 数据页的返回入口必须能恢复工作台，避免中央控件切换后无法返回。 */
    void metadataPageCanReturnToDashboard();
    /* 服务端返回的 data 相对路径应能映射到 Windows 已挂载目录。 */
    void mapsServerRecordPathToMountedRoot();
    /* Qt Creator 的构建目录变化时，仍能找到项目 conf 下的摄像头配置。 */
    void resolvesCameraConfigFromProjectDirectory();
    /* 显式环境变量路径始终优先，便于部署到其它目录。 */
    void explicitCameraConfigPathWins();
};

void MainWindowTest::recordTimeFilterCanSwitchBetweenAllAndRange()
{
    qputenv("SMARTHOME_SERVER_IP", QByteArray("127.0.0.1"));
    qputenv("SMARTHOME_SERVER_PORT", QByteArray("1"));

    MainWindow window;
    QCheckBox *allTime = window.findChild<QCheckBox *>(QStringLiteral("allTimeCheckBox"));
    QDateTimeEdit *start = window.findChild<QDateTimeEdit *>(QStringLiteral("recordStartEdit"));
    QDateTimeEdit *end = window.findChild<QDateTimeEdit *>(QStringLiteral("recordEndEdit"));

    QVERIFY2(allTime != nullptr, "数据页缺少全部时间复选框");
    QVERIFY2(start != nullptr, "数据页缺少录像开始时间输入框");
    QVERIFY2(end != nullptr, "数据页缺少录像结束时间输入框");
    QVERIFY(allTime->isChecked());
    QVERIFY(!start->isEnabled());
    QVERIFY(!end->isEnabled());

    allTime->setChecked(false);
    QVERIFY(start->isEnabled());
    QVERIFY(end->isEnabled());
    QCOMPARE(start->displayFormat(), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    QCOMPARE(end->displayFormat(), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

void MainWindowTest::dashboardDeviceDataButtonShowsDataPage()
{
    qputenv("SMARTHOME_SERVER_IP", QByteArray("127.0.0.1"));
    qputenv("SMARTHOME_SERVER_PORT", QByteArray("1"));

    MainWindow window;
    MonitoringDashboard *dashboard = window.findChild<MonitoringDashboard *>();
    QVERIFY(dashboard != nullptr);

    QPushButton *deviceButton = nullptr;
    for (QPushButton *button : dashboard->findChildren<QPushButton *>()) {
        if (button->text() == QStringLiteral("设备数据")) {
            deviceButton = button;
            break;
        }
    }
    QVERIFY(deviceButton != nullptr);
    deviceButton->click();

    QVERIFY(window.centralWidget() != nullptr);
    QCOMPARE(window.centralWidget()->objectName(), QStringLiteral("dataPage"));
}

void MainWindowTest::metadataPageCanReturnToDashboard()
{
    qputenv("SMARTHOME_SERVER_IP", QByteArray("127.0.0.1"));
    qputenv("SMARTHOME_SERVER_PORT", QByteArray("1"));

    MainWindow window;
    MonitoringDashboard *dashboard = window.findChild<MonitoringDashboard *>();
    QVERIFY(dashboard != nullptr);

    QPushButton *deviceButton = nullptr;
    for (QPushButton *button : dashboard->findChildren<QPushButton *>()) {
        if (button->text() == QStringLiteral("设备数据")) {
            deviceButton = button;
            break;
        }
    }
    QVERIFY(deviceButton != nullptr);
    deviceButton->click();

    QPushButton *backButton = window.findChild<QPushButton *>(
        QStringLiteral("backToPreviewButton"));
    QVERIFY(backButton != nullptr);
    backButton->click();

    QVERIFY(window.centralWidget() != nullptr);
    QCOMPARE(window.centralWidget()->objectName(), QStringLiteral("monitoringDashboard"));
}

void MainWindowTest::mapsServerRecordPathToMountedRoot()
{
    QCOMPARE(MainWindow::resolveRecordFilePath(
                 QStringLiteral("./data/7_100/segment-000.ts"),
                 QStringLiteral("Z:/SmartHomeRecords")),
             QStringLiteral("Z:/SmartHomeRecords/7_100/segment-000.ts"));
    QCOMPARE(MainWindow::resolveRecordFilePath(
                 QStringLiteral("/opt/smarthome/data/7_100/segment-001.ts"),
                 QStringLiteral("Z:/SmartHomeRecords")),
             QStringLiteral("Z:/SmartHomeRecords/7_100/segment-001.ts"));
}

void MainWindowTest::resolvesCameraConfigFromProjectDirectory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QDir().mkpath(directory.path() + QStringLiteral("/SmartHomeClient/conf")));
    const QString path = directory.path()
        + QStringLiteral("/SmartHomeClient/conf/cameras.local.conf");
    QFile config(path);
    QVERIFY(config.open(QIODevice::WriteOnly));
    config.write("[gun]\nname=Gun\ntype=gun\nrtspUrl=rtsp://camera/live\nenabled=false\n");
    config.close();

    const QString resolved = MainWindow::resolveCameraConfigPath(
        QString(), directory.path() + QStringLiteral("/build-Debug"));
    QCOMPARE(QDir::fromNativeSeparators(resolved),
             QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()));
}

void MainWindowTest::explicitCameraConfigPathWins()
{
    const QString explicitPath = QStringLiteral("Z:/camera/cameras.local.conf");
    QCOMPARE(MainWindow::resolveCameraConfigPath(explicitPath, QStringLiteral("C:/build")),
             explicitPath);
}

QTEST_MAIN(MainWindowTest)

#include "MainWindowTest.moc"
