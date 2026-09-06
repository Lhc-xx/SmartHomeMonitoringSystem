#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

#include "video/CameraConfig.h"

/*
 * CameraConfigTest 验证本地摄像头配置的读取和边界校验。
 * 测试只操作临时 INI 文件，不访问摄像头，也不保存任何真实凭据。
 */
class CameraConfigTest : public QObject
{
    Q_OBJECT

private slots:
    void loadsValidGroups();
    void rejectsMissingFile();
    void rejectsInvalidRtspUrl();
};

void CameraConfigTest::loadsValidGroups()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("cameras.ini"));

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&file);
    stream << "[gun]\n"
           << "name=Gun\n"
           << "type=gun\n"
           << "rtspUrl=rtsp://user:password@camera-a/live/chn=0\n"
           << "webUrl=http://camera-a\n"
           << "user=user\n"
           << "password=password\n"
           << "enabled=true\n"
           << "[dome]\n"
           << "name=Dome\n"
           << "type=dome\n"
           << "rtspUrl=rtsp://user:password@camera-b/live/chn=0\n"
           << "webUrl=http://camera-b\n"
           << "enabled=0\n";
    file.close();

    QString error;
    const QList<CameraConfig> configs = loadCameraConfigs(path, &error);
    QCOMPARE(error, QString());
    QCOMPARE(configs.size(), 2);

    bool foundGun = false;
    bool foundDome = false;
    for (const CameraConfig &config : configs) {
        if (config.type == QStringLiteral("gun")) {
            foundGun = true;
            QVERIFY(config.enabled);
        } else if (config.type == QStringLiteral("dome")) {
            foundDome = true;
            QVERIFY(!config.enabled);
        }
    }
    QVERIFY(foundGun);
    QVERIFY(foundDome);
}

void CameraConfigTest::rejectsMissingFile()
{
    QString error;
    const QList<CameraConfig> configs = loadCameraConfigs(QStringLiteral("missing-cameras.ini"), &error);
    QVERIFY(configs.isEmpty());
    QVERIFY(!error.isEmpty());
}

void CameraConfigTest::rejectsInvalidRtspUrl()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("invalid.ini"));

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("[camera]\nname=Camera\ntype=gun\nrtspUrl=http://camera\n");
    file.close();

    QString error;
    const QList<CameraConfig> configs = loadCameraConfigs(path, &error);
    QVERIFY(configs.isEmpty());
    QVERIFY(error.contains(QStringLiteral("RTSP")));
}

QTEST_GUILESS_MAIN(CameraConfigTest)

#include "CameraConfigTest.moc"
