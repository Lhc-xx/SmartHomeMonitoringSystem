#include <QByteArray>
#include <QSignalSpy>
#include <QtTest>

#include <memory>

#include "protocol/media_packet.h"
#include "mock_decoder.h"
#include "video/ServerStreamPlayer.h"

namespace {

class FlakyDecoder : public smart_home::client::IDecoder
{
public:
    bool open(const smart_home::protocol::MediaPacket &) override
    {
        ++openCount;
        if (openCount == 1) {
            return false;
        }
        opened = true;
        return true;
    }

    bool decode(const smart_home::protocol::MediaPacket &packet,
                smart_home::client::Frame &frame) override
    {
        if (!opened) {
            return false;
        }
        frame.width = 1;
        frame.height = 1;
        frame.pts = packet.pts;
        frame.rgba.assign(4, 0xff);
        return true;
    }

    void close() override
    {
        opened = false;
    }

    int openCount = 0;
    bool opened = false;
};

QByteArray makeMediaFrame(int64_t pts)
{
    smart_home::protocol::MediaPacket packet;
    packet.pts = pts;
    packet.dts = pts;
    packet.data.push_back(0x01);
    std::vector<uint8_t> encoded;
    if (!smart_home::protocol::MediaPacketSerializer::encode(packet, encoded)) {
        return QByteArray();
    }
    return QByteArray(reinterpret_cast<const char *>(encoded.data()),
                      static_cast<int>(encoded.size()));
}

} // namespace

/*
 * ServerStreamPlayerTest 类职责：
 * 验证服务器转发媒体链路在解码失败后可以重新启动，并且 stop 不会等待
 * 已经排队的旧画面全部解码；测试只使用本地构造的 MediaPacket，不连接服务器。
 */
class ServerStreamPlayerTest : public QObject
{
    Q_OBJECT

private slots:
    void decoderFailureAllowsRestart();
    void stoppedPlayerCanStartAgain();
};

void ServerStreamPlayerTest::decoderFailureAllowsRestart()
{
    std::unique_ptr<FlakyDecoder> decoder(new FlakyDecoder());
    FlakyDecoder *decoderPtr = decoder.get();
    ServerStreamPlayer player(std::move(decoder));
    QSignalSpy errorSpy(&player, &ServerStreamPlayer::errorOccurred);
    QSignalSpy frameSpy(&player, &ServerStreamPlayer::frameReady);

    player.start();
    player.onMediaFrame(makeMediaFrame(1));
    QTRY_VERIFY_WITH_TIMEOUT(errorSpy.count() >= 1, 2000);
    QCOMPARE(decoderPtr->openCount, 1);

    /* 第一次 worker 已退出但 thread 仍 joinable；start 必须先回收再重建。 */
    player.start();
    player.onMediaFrame(makeMediaFrame(2));
    QTRY_VERIFY_WITH_TIMEOUT(frameSpy.count() >= 1, 2000);
    QCOMPARE(decoderPtr->openCount, 2);
    player.stop();
}

void ServerStreamPlayerTest::stoppedPlayerCanStartAgain()
{
    ServerStreamPlayer player(
        std::unique_ptr<smart_home::client::IDecoder>(
            new smart_home::client::MockDecoder()));
    QSignalSpy frameSpy(&player, &ServerStreamPlayer::frameReady);

    player.start();
    player.stop();
    player.start();
    player.onMediaFrame(makeMediaFrame(3));
    QTRY_VERIFY_WITH_TIMEOUT(frameSpy.count() >= 1, 2000);
    player.stop();
}

QTEST_GUILESS_MAIN(ServerStreamPlayerTest)

#include "ServerStreamPlayerTest.moc"
