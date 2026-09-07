#include "VideoWidget.h"

#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>

VideoWidget::VideoWidget(QWidget *parent)
    : QWidget(parent),
      m_channelName(QStringLiteral("未命名通道")),
      m_state(QStringLiteral("等待接入")),
      m_nativeVideo(false),
      m_topOverlay(nullptr),
      m_bottomOverlay(nullptr)
{
    /* 四宫格需要稳定的最小比例，避免窗口缩小时频道标签被完全挤掉。 */
    setMinimumSize(300, 170);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(false);
}

void VideoWidget::setChannelName(const QString &name)
{
    m_channelName = name.trimmed().isEmpty() ? QStringLiteral("未命名通道") : name.trimmed();
    syncOverlayLabels();
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
    syncOverlayLabels();
    update();
}

void VideoWidget::setNativeVideoMode(bool enabled)
{
    m_nativeVideo = enabled;
    if (m_nativeVideo) {
        /*
         * libvlc 需要把画面渲染进一个真实的窗口句柄；开启原生窗口属性并
         * 主动调用 winId() 强制创建 HWND，避免第一次 play 时拿不到句柄。
         */
        setAttribute(Qt::WA_NativeWindow, true);
        winId();

        if (m_topOverlay == nullptr) {
            m_topOverlay = new QLabel(this);
            m_topOverlay->setAttribute(Qt::WA_NativeWindow, true);
            m_topOverlay->setStyleSheet(QStringLiteral(
                "background: rgba(18, 22, 28, 210); color: white; padding: 2px 8px;"));
            m_topOverlay->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        }
        if (m_bottomOverlay == nullptr) {
            m_bottomOverlay = new QLabel(this);
            m_bottomOverlay->setAttribute(Qt::WA_NativeWindow, true);
            m_bottomOverlay->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        }
        m_topOverlay->show();
        m_bottomOverlay->show();
        /* 原生子窗口默认堆叠在控件自绘内容之上，再 raise 保证覆盖在视频之上。 */
        m_topOverlay->raise();
        m_bottomOverlay->raise();
        syncOverlayLabels();
    } else {
        delete m_topOverlay;
        m_topOverlay = nullptr;
        delete m_bottomOverlay;
        m_bottomOverlay = nullptr;
    }
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

    /* VLC 模式下画面由 libvlc 原生窗口绘制，叠加文本由原生 QLabel 负责。 */
    if (m_nativeVideo) {
        return;
    }

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

void VideoWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    syncOverlayLabels();
}

void VideoWidget::syncOverlayLabels()
{
    if (!m_nativeVideo) {
        return;
    }

    if (m_topOverlay != nullptr) {
        m_topOverlay->setGeometry(10, 10, qMax(0, width() - 20), 30);
        m_topOverlay->setText(m_channelName);
    }

    if (m_bottomOverlay != nullptr) {
        const QString status = m_detail.isEmpty() ? m_state : m_state + QStringLiteral(" · ") + m_detail;
        const QColor stateColor = m_state == QStringLiteral("播放中")
            ? QColor(QStringLiteral("#42d392"))
            : QColor(QStringLiteral("#f0b35a"));
        m_bottomOverlay->setStyleSheet(QStringLiteral("color: %1; background: transparent; padding: 2px 8px;")
            .arg(stateColor.name()));
        m_bottomOverlay->setGeometry(20, qMax(0, height() - 42), qMax(0, width() - 40), 24);
        m_bottomOverlay->setText(status);
    }
}
