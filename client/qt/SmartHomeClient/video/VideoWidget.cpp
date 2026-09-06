#include "VideoWidget.h"

#include <QPainter>
#include <QPaintEvent>

VideoWidget::VideoWidget(QWidget *parent)
    : QWidget(parent)
{
    /* 为后续视频画面预留一个可见且稳定的最小显示区域。 */
    setMinimumSize(320, 180);
}

void VideoWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    /*
     * 当前阶段只提供视觉占位，不保留任何图像缓存或解码状态。
     * 后续接入 FFmpeg 时可在该位置替换为视频帧绘制。
     */
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    painter.setPen(Qt::white);
    painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("Video Area"));
}
