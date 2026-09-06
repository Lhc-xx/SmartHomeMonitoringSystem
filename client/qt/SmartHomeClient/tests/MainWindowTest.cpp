#include <QCheckBox>
#include <QDateTimeEdit>
#include <QtTest>

#include "MainWindow.h"

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

QTEST_MAIN(MainWindowTest)

#include "MainWindowTest.moc"
