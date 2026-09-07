#include <QImage>
#include <QtTest>

#include "video/VideoWidget.h"

/*
 * VideoWidgetTest 验证控件在无帧、有帧和重连状态下的内存状态。
 * 测试不启动 FFmpeg，不访问摄像头；绘制只依赖 Qt 自带 QWidget。
 */
class VideoWidgetTest : public QObject
{
    Q_OBJECT

private slots:
    void startsWaitingForInput();
    void keepsLastFrameDuringReconnect();
};

void VideoWidgetTest::startsWaitingForInput()
{
    VideoWidget widget;
    widget.setChannelName(QStringLiteral("通道 01 · 枪机"));
    QCOMPARE(widget.channelName(), QStringLiteral("通道 01 · 枪机"));
    QCOMPARE(widget.stateText(), QStringLiteral("等待接入"));
    QVERIFY(!widget.hasFrame());
}

void VideoWidgetTest::keepsLastFrameDuringReconnect()
{
    VideoWidget widget;
    const QImage frame(2, 2, QImage::Format_RGB32);
    widget.setFrame(frame);
    QVERIFY(widget.hasFrame());

    widget.setState(QStringLiteral("重连中"), QStringLiteral("等待摄像头响应"));
    QVERIFY(widget.hasFrame());
    QCOMPARE(widget.stateText(), QStringLiteral("重连中"));
}

QTEST_MAIN(VideoWidgetTest)

#include "VideoWidgetTest.moc"
