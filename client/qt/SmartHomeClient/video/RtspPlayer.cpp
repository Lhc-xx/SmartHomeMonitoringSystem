#include "RtspPlayer.h"

#include <QProcess>
#include <QTimer>
#include <QUrl>

namespace {

const int kMaxFrameBufferBytes = 8 * 1024 * 1024;

QString stateText(const QString &state)
{
    return state;
}

}

RtspPlayer::RtspPlayer(QObject *parent)
    : QObject(parent),
      m_process(new QProcess(this)),
      m_reconnectTimer(new QTimer(this)),
      m_frameWatchdog(new QTimer(this)),
      m_backoffSeconds(1),
      m_manualStop(true)
{
    /*
     * QProcess 本身是异步对象，readyRead/finished 信号让两个视频流共享
     * Qt 事件循环而不调用 waitForReadyRead，避免把网络等待带入 UI 线程。
     */
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &RtspPlayer::readStandardOutput);
    connect(m_process,
            static_cast<void (QProcess::*)(QProcess::ProcessError)>(&QProcess::errorOccurred),
            this, &RtspPlayer::handleProcessError);
    connect(m_process,
            static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, &RtspPlayer::handleProcessFinished);

    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout,
            this, &RtspPlayer::restartAfterBackoff);

    /*
     * 某些摄像头在 TCP 已建立但暂时不输出视频时，FFmpeg 进程不会退出。
     * 看门狗限制“连接中”最长等待时间，超时后走统一重连流程，避免界面
     * 永久停在黑屏而没有任何状态反馈。
     */
    m_frameWatchdog->setSingleShot(true);
    connect(m_frameWatchdog, &QTimer::timeout,
            this, &RtspPlayer::handleFrameTimeout);
}

RtspPlayer::~RtspPlayer()
{
    stop();
}

void RtspPlayer::setSource(const QString &url, const QString &ffmpegPath)
{
    stop();
    m_url = url.trimmed();
    m_ffmpegPath = ffmpegPath.trimmed();
    m_backoffSeconds = 1;
}

void RtspPlayer::start()
{
    if (m_url.isEmpty() || QUrl(m_url).scheme().compare(QStringLiteral("rtsp"), Qt::CaseInsensitive) != 0) {
        emit errorOccurred(QStringLiteral("RTSP 地址无效"));
        setState(QStringLiteral("配置错误"));
        return;
    }
    if (m_process->state() == QProcess::Running || m_reconnectTimer->isActive()) {
        return;
    }

    m_manualStop = false;
    m_frameBuffer.clear();
    setState(QStringLiteral("连接中"));
    m_frameWatchdog->start(10000);

    const QString program = m_ffmpegPath.isEmpty() ? QStringLiteral("ffmpeg") : m_ffmpegPath;
    m_process->start(program, buildArguments(m_url));
}

void RtspPlayer::stop()
{
    m_manualStop = true;
    m_reconnectTimer->stop();
    m_frameWatchdog->stop();
    m_frameBuffer.clear();

    if (m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(1000)) {
            m_process->kill();
            m_process->waitForFinished(1000);
        }
    }
    setState(QStringLiteral("已停止"));
}

bool RtspPlayer::isRunning() const
{
    return m_process->state() == QProcess::Running;
}

QStringList RtspPlayer::buildArguments(const QString &url)
{
    /*
     * -rtsp_transport tcp 避免 UDP 丢包；image2pipe/mjpeg 让 Qt 不需要
     * 绑定 FFmpeg ABI。限制帧率和尺寸，防止高码率摄像头拖垮桌面客户端。
     */
    return QStringList()
        << QStringLiteral("-hide_banner")
        << QStringLiteral("-loglevel") << QStringLiteral("error")
        << QStringLiteral("-rtsp_transport") << QStringLiteral("tcp")
        << QStringLiteral("-i") << url
        << QStringLiteral("-an")
        << QStringLiteral("-vf") << QStringLiteral("fps=15,scale=640:-2")
        << QStringLiteral("-f") << QStringLiteral("image2pipe")
        << QStringLiteral("-vcodec") << QStringLiteral("mjpeg")
        << QStringLiteral("-q:v") << QStringLiteral("5")
        << QStringLiteral("pipe:1");
}

QList<QByteArray> RtspPlayer::extractJpegFrames(QByteArray &buffer, bool &overflowed)
{
    overflowed = false;
    QList<QByteArray> frames;
    const QByteArray startMarker("\xFF\xD8", 2);
    const QByteArray endMarker("\xFF\xD9", 2);

    while (true) {
        const int start = buffer.indexOf(startMarker);
        if (start < 0) {
            if (buffer.size() > kMaxFrameBufferBytes) {
                buffer.clear();
                overflowed = true;
                break;
            }
            /* 保留最后一个字节，防止下一个分片补齐 SOI 标记。 */
            if (buffer.size() > 1) {
                buffer.remove(0, buffer.size() - 1);
            }
            break;
        }

        if (start > 0) {
            buffer.remove(0, start);
        }

        const int end = buffer.indexOf(endMarker, 2);
        if (end < 0) {
            if (buffer.size() > kMaxFrameBufferBytes) {
                buffer.clear();
                overflowed = true;
            }
            break;
        }

        const int frameSize = end + endMarker.size();
        if (frameSize > kMaxFrameBufferBytes) {
            buffer.remove(0, frameSize);
            overflowed = true;
            continue;
        }
        frames.append(buffer.left(frameSize));
        buffer.remove(0, frameSize);
    }

    if (buffer.size() > kMaxFrameBufferBytes) {
        buffer.clear();
        overflowed = true;
    }
    return frames;
}

void RtspPlayer::readStandardOutput()
{
    m_frameBuffer.append(m_process->readAllStandardOutput());
    bool overflowed = false;
    const QList<QByteArray> frames = extractJpegFrames(m_frameBuffer, overflowed);
    if (overflowed) {
        emit errorOccurred(QStringLiteral("视频帧缓存超过限制，已丢弃异常数据"));
    }

    for (const QByteArray &encoded : frames) {
        const QImage frame = QImage::fromData(encoded, "JPEG");
        if (frame.isNull()) {
            emit errorOccurred(QStringLiteral("收到无法解码的视频帧"));
            continue;
        }
        m_backoffSeconds = 1;
        m_frameWatchdog->start(10000);
        setState(QStringLiteral("播放中"));
        emit frameReady(frame);
    }
}

void RtspPlayer::handleProcessError(QProcess::ProcessError error)
{
    m_frameWatchdog->stop();
    if (m_manualStop) {
        return;
    }

    const QString reason = error == QProcess::FailedToStart
        ? QStringLiteral("FFmpeg 启动失败，请检查本地路径或 PATH")
        : QStringLiteral("FFmpeg 播放进程发生错误");
    emit errorOccurred(reason);
    setState(QStringLiteral("连接失败"));
    scheduleReconnect();
}

void RtspPlayer::handleProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    m_frameWatchdog->stop();
    if (m_manualStop) {
        return;
    }

    Q_UNUSED(status)
    emit errorOccurred(QStringLiteral("视频流进程已退出（代码 %1）").arg(exitCode));
    setState(QStringLiteral("重连中"));
    scheduleReconnect();
}

void RtspPlayer::handleFrameTimeout()
{
    if (m_manualStop || m_process->state() == QProcess::NotRunning) {
        return;
    }

    /*
     * 只终止当前 FFmpeg 子进程，不触碰摄像头配置或其他频道；finished
     * 信号随后会负责进入退避重连。这样四宫格中的单路故障不会拖垮 UI。
     */
    emit errorOccurred(QStringLiteral("视频流连接超时，准备重连"));
    setState(QStringLiteral("重连中"));
    /* 直接异步终止，避免在 GUI 线程等待子进程退出。 */
    m_process->kill();
    scheduleReconnect();
}

void RtspPlayer::restartAfterBackoff()
{
    if (!m_manualStop) {
        start();
    }
}

void RtspPlayer::setState(const QString &state)
{
    emit stateChanged(stateText(state));
}

void RtspPlayer::scheduleReconnect()
{
    if (m_manualStop || m_reconnectTimer->isActive()) {
        return;
    }

    m_reconnectTimer->start(m_backoffSeconds * 1000);
    m_backoffSeconds = qMin(m_backoffSeconds * 2, 8);
}
