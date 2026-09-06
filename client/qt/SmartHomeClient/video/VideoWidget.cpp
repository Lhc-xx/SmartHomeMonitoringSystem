#include "VideoWidget.h"

#include <QPainter>
#include <QPaintEvent>

VideoWidget::VideoWidget(QWidget *parent)
    : QWidget(parent),
      m_channelName(QStringLiteral("未命名通道")),
      m_state(QStringLiteral("等待接入"))
{
    /* 四宫格需要稳定的最小比例，避免窗口缩小时频道标签被完全挤掉。 */
    setMinimumSize(300, 170);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(false);
}

void VideoWidget::setChannelName(const QString &name)
{
    m_channelName = name.trimmed().isEmpty() ? QStringLiteral("未命名通道") : name.trimmed();
    update();
}

void VideoWidget::setFrame(const QImage &frame)
{
    m_frame = frame;
    update();
}

void VideoWidget::setState(const QString &state, const QString &detail)
{
    m_state = state.trimmed().isEmpty() ? QStringLiteral("未知状态") : state.trimmed();
    m_detail = detail.trimmed();
    update();
}

bool VideoWidget::hasFrame() const
{
    return !m_frame.isNull();
}

QString VideoWidget::channelName() const
{
    return m_channelName;
}

QString VideoWidget::stateText() const
{
    return m_state;
}

void VideoWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.fillRect(rect(), QColor(QStringLiteral("#20262e")));

    if (!m_frame.isNull()) {
        const QImage scaled = m_frame.scaled(rect().size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        const QPoint topLeft((width() - scaled.width()) / 2, (height() - scaled.height()) / 2);
        painter.drawImage(topLeft, scaled);
    } else {
        painter.setPen(QColor(QStringLiteral("#aeb8c4")));
        painter.drawText(rect(), Qt::AlignCenter, m_state);
    }

    /* 左上角频道标签保证多路预览时用户始终知道当前画面来源。 */
    painter.fillRect(QRect(10, 10, qMin(width() - 20, 220), 30), QColor(18, 22, 28, 210));
    painter.setPen(Qt::white);
    painter.drawText(QRect(20, 10, qMax(0, width() - 40), 30), Qt::AlignVCenter | Qt::AlignLeft,
                     m_channelName);

    const QColor stateColor = m_state == QStringLiteral("播放中")
        ? QColor(QStringLiteral("#42d392"))
        : QColor(QStringLiteral("#f0b35a"));
    painter.setPen(stateColor);
    const QString status = m_detail.isEmpty() ? m_state : m_state + QStringLiteral(" · ") + m_detail;
    painter.drawText(QRect(20, qMax(0, height() - 34), qMax(0, width() - 40), 24),
                     Qt::AlignVCenter | Qt::AlignLeft, status);
}
