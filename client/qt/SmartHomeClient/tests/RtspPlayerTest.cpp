#include <QSignalSpy>
#include <QtTest>

#include "video/RtspPlayer.h"

/*
 * RtspPlayerTest 只验证命令构造和 JPEG 字节拆包，不启动 FFmpeg、摄像头
 * 或任何真实网络连接，确保客户端离线构建即可回归播放器核心逻辑。
 */
class RtspPlayerTest : public QObject
{
    Q_OBJECT

private slots:
    void buildsTcpMjpegArguments();
    void extractsSplitAndConcatenatedFrames();
    void limitsMalformedBuffer();
};

void RtspPlayerTest::buildsTcpMjpegArguments()
{
    const QStringList args = RtspPlayer::buildArguments(QStringLiteral("rtsp://camera/live"));
    QVERIFY(args.contains(QStringLiteral("-rtsp_transport")));
    QCOMPARE(args.at(args.indexOf(QStringLiteral("-rtsp_transport")) + 1), QStringLiteral("tcp"));
    QVERIFY(args.contains(QStringLiteral("-f")));
    QCOMPARE(args.at(args.indexOf(QStringLiteral("-f")) + 1), QStringLiteral("image2pipe"));
    QVERIFY(args.contains(QStringLiteral("-vcodec")));
    QCOMPARE(args.at(args.indexOf(QStringLiteral("-vcodec")) + 1), QStringLiteral("mjpeg"));
    QVERIFY(args.contains(QStringLiteral("-an")));
    QCOMPARE(args.last(), QStringLiteral("pipe:1"));
}

void RtspPlayerTest::extractsSplitAndConcatenatedFrames()
{
    QByteArray buffer("noise", 5);
    bool overflowed = false;

    QList<QByteArray> frames = RtspPlayer::extractJpegFrames(buffer, overflowed);
    QVERIFY(frames.isEmpty());
    QVERIFY(!overflowed);

    buffer.append("\xFF\xD8one\xFF", 6);
    frames = RtspPlayer::extractJpegFrames(buffer, overflowed);
    QVERIFY(frames.isEmpty());
    QVERIFY(!overflowed);

    buffer.append("\xD9\xFF\xD8two\xFF\xD9", 8);
    frames = RtspPlayer::extractJpegFrames(buffer, overflowed);
    QCOMPARE(frames.size(), 2);
    QCOMPARE(frames.at(0), QByteArray("\xFF\xD8one\xFF\xD9", 7));
    QCOMPARE(frames.at(1), QByteArray("\xFF\xD8two\xFF\xD9", 7));
    QVERIFY(buffer.isEmpty());
    QVERIFY(!overflowed);
}

void RtspPlayerTest::limitsMalformedBuffer()
{
    QByteArray buffer(8 * 1024 * 1024 + 1, 'x');
    bool overflowed = false;
    const QList<QByteArray> frames = RtspPlayer::extractJpegFrames(buffer, overflowed);
    QVERIFY(frames.isEmpty());
    QVERIFY(overflowed);
    QVERIFY(buffer.isEmpty());
}

QTEST_GUILESS_MAIN(RtspPlayerTest)

#include "RtspPlayerTest.moc"
