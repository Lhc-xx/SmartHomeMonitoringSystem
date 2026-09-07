#include <QtTest>

#include "video/FilePlaybackPlayer.h"

/*
 * FilePlaybackPlayerTest 只验证本地录像播放的 ffmpeg 参数，不启动 ffmpeg、
 * 不访问任何文件或网络，符合离线测试约束。
 */
class FilePlaybackPlayerTest : public QObject
{
    Q_OBJECT

private slots:
    void buildsLocalFileArguments();
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

QTEST_GUILESS_MAIN(FilePlaybackPlayerTest)

#include "FilePlaybackPlayerTest.moc"
