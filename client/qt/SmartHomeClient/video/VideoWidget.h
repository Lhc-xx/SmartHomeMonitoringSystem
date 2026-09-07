#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QImage>
#include <QString>
#include <QWidget>

class QLabel;
class QResizeEvent;

/*
 * VideoWidget 负责绘制一条监控频道的最新视频帧和状态覆盖层。
 *
 * 播放进程、网络连接和帧解码均由 RtspPlayer 负责；本控件只在 GUI
 * 线程保存 QImage 并重绘，从边界上保证视频异常不会阻塞登录或设备页面。
 *
 * 另外本控件可作为可选 VLC 后端的渲染目标：setNativeVideoMode(true) 会
 * 把控件切换为原生窗口，并把频道 / 状态叠加层改成原生子 QLabel，从而让
 * libvlc 绘制的视频子窗口可以显示在叠加层之下、控件背景之上。
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

    /*
     * 切换是否作为 VLC 原生渲染目标。开启后创建原生窗口与原生叠加标签，
     * 并停止自绘 QImage（由 libvlc 直接绘制）；关闭后恢复自绘路径。
     * 默认关闭，不影响现有 ffmpeg + QImage 播放路径。
     */
    void setNativeVideoMode(bool enabled);

    /* 只读状态用于界面结构测试和工作台决定是否显示等待接入提示。 */
    bool hasFrame() const;
    QString channelName() const;
    QString stateText() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void syncOverlayLabels();

    QString m_channelName;
    QImage m_frame;
    QString m_state;
    QString m_detail;
    bool m_nativeVideo;
    QLabel *m_topOverlay;
    QLabel *m_bottomOverlay;
};

#endif // VIDEOWIDGET_H
