#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QImage>
#include <QString>
#include <QWidget>

/*
 * VideoWidget 负责绘制一条监控频道的最新视频帧和状态覆盖层。
 *
 * 播放进程、网络连接和帧解码均由 RtspPlayer 负责；本控件只在 GUI
 * 线程保存 QImage 并重绘，从边界上保证视频异常不会阻塞登录或设备页面。
 */
class VideoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoWidget(QWidget *parent = nullptr);

    /* 设置频道角标、最新画面及状态文本，均可安全地由 Qt 信号槽调用。 */
    void setChannelName(const QString &name);
    void setFrame(const QImage &frame);
    void setState(const QString &state, const QString &detail = QString());

    /* 只读状态用于界面结构测试和工作台决定是否显示等待接入提示。 */
    bool hasFrame() const;
    QString channelName() const;
    QString stateText() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_channelName;
    QImage m_frame;
    QString m_state;
    QString m_detail;
};

#endif // VIDEOWIDGET_H
