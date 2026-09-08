#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "video/FilePlaybackPlayer.h"

/*
 * FilePlaybackPlayerTest 只验证本地录像播放的 ffmpeg 参数和路径边界，不启动
 * ffmpeg、不访问网络；缺失文件用临时目录构造，符合离线测试约束。
 */
class FilePlaybackPlayerTest : public QObject
{
    Q_OBJECT

private slots:
    void buildsLocalFileArguments();
    void rejectsMissingLocalFileBeforeStartingProcess();
};

void FilePlaybackPlayerTest::buildsLocalFileArguments()
{
    const QStringList args = FilePlaybackPlayer::buildArguments(
        QStringLiteral("/data/1/seg_00000.ts"));

    QVERIFY(args.contains(QStringLiteral("-i")));
    QCOMPARE(args.at(args.indexOf(QStringLiteral("-i")) + 1), QStringLiteral("/data/1/seg_00000.ts"));
    QVERIFY(args.contains(QStringLiteral("-f")));
    QCOMPARE(args.at(args.indexOf(QStringLiteral("-f")) + 1), QStringLiteral("image2pipe"));
    QVERIFY(args.contains(QStringLiteral("-vcodec")));
    QCOMPARE(args.at(args.indexOf(QStringLiteral("-vcodec")) + 1), QStringLiteral("mjpeg"));
    QVERIFY(args.contains(QStringLiteral("-an")));
    QVERIFY(!args.contains(QStringLiteral("-rtsp_transport")));
    QCOMPARE(args.last(), QStringLiteral("pipe:1"));
}

void FilePlaybackPlayerTest::rejectsMissingLocalFileBeforeStartingProcess()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    /* 该路径故意不存在，模拟服务端返回但客户端无法访问的录像路径。 */
    const QString missingPath = temporaryDirectory.path()
        + QStringLiteral("/missing-record.ts");
    FilePlaybackPlayer player;
    QSignalSpy errors(&player, &FilePlaybackPlayer::errorOccurred);

    /* 使用不存在的 ffmpeg 名称，确保测试不会意外启动真实播放器进程。 */
    player.play(missingPath, QStringLiteral("smarthome-test-ffmpeg-not-found"));

    QCOMPARE(errors.count(), 1);
    QCOMPARE(errors.at(0).at(0).toString(),
             QStringLiteral("录像文件不存在或不可读：%1").arg(missingPath));
    QVERIFY(!player.isRunning());
}

QTEST_GUILESS_MAIN(FilePlaybackPlayerTest)

#include "FilePlaybackPlayerTest.moc"
