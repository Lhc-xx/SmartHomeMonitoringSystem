#include "FilePlaybackPlayer.h"

#include "RtspPlayer.h"   // 复用 extractJpegFrames（纯函数，已单测）

#include <QProcess>

FilePlaybackPlayer::FilePlaybackPlayer(QObject *parent)
    : QObject(parent),
      m_process(new QProcess(this)),
      m_manualStop(true)
{
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &FilePlaybackPlayer::readStandardOutput);
    connect(m_process,
            static_cast<void (QProcess::*)(QProcess::ProcessError)>(&QProcess::errorOccurred),
            this, &FilePlaybackPlayer::handleProcessError);
    connect(m_process,
            static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, &FilePlaybackPlayer::handleProcessFinished);
}

FilePlaybackPlayer::~FilePlaybackPlayer()
{
    stop();
}

QStringList FilePlaybackPlayer::buildArguments(const QString &path)
{
    /* 本地文件：不解析 RTSP，直接 image2pipe/mjpeg 输出受限尺寸 JPEG。 */
    return QStringList()
        << QStringLiteral("-hide_banner")
        << QStringLiteral("-loglevel") << QStringLiteral("error")
        << QStringLiteral("-i") << path
        << QStringLiteral("-an")
        << QStringLiteral("-vf") << QStringLiteral("fps=15,scale=640:-2")
        << QStringLiteral("-f") << QStringLiteral("image2pipe")
        << QStringLiteral("-vcodec") << QStringLiteral("mjpeg")
        << QStringLiteral("-q:v") << QStringLiteral("5")
        << QStringLiteral("pipe:1");
}

void FilePlaybackPlayer::play(const QString &path, const QString &ffmpegPath)
{
    stop();
    if (path.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("录像文件路径为空"));
        return;
    }

    m_manualStop = false;
    m_frameBuffer.clear();
    emit stateChanged(QStringLiteral("播放中"));

    const QString program = ffmpegPath.isEmpty() ? QStringLiteral("ffmpeg") : ffmpegPath;
    m_process->start(program, buildArguments(path));
}

void FilePlaybackPlayer::stop()
{
    m_manualStop = true;
    m_frameBuffer.clear();
    if (m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(1000)) {
            m_process->kill();
            m_process->waitForFinished(1000);
        }
    }
    emit stateChanged(QStringLiteral("已停止"));
}

bool FilePlaybackPlayer::isRunning() const
{
    return m_process->state() == QProcess::Running;
}

void FilePlaybackPlayer::readStandardOutput()
{
    m_frameBuffer.append(m_process->readAllStandardOutput());
    bool overflowed = false;
    const QList<QByteArray> frames = RtspPlayer::extractJpegFrames(m_frameBuffer, overflowed);
    if (overflowed) {
        emit errorOccurred(QStringLiteral("视频帧缓存超过限制，已丢弃异常数据"));
    }

    for (const QByteArray &encoded : frames) {
        const QImage frame = QImage::fromData(encoded, "JPEG");
        if (frame.isNull()) {
            emit errorOccurred(QStringLiteral("收到无法解码的视频帧"));
            continue;
        }
        emit frameReady(frame);
    }
}

void FilePlaybackPlayer::handleProcessError(QProcess::ProcessError error)
{
    if (m_manualStop) {
        return;
    }
    emit errorOccurred(error == QProcess::FailedToStart
        ? QStringLiteral("FFmpeg 启动失败，请检查本地路径或 PATH")
        : QStringLiteral("FFmpeg 播放进程发生错误"));
    emit stateChanged(QStringLiteral("播放失败"));
}

void FilePlaybackPlayer::handleProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status)
    if (m_manualStop) {
        return;
    }
    if (exitCode == 0) {
        emit stateChanged(QStringLiteral("播放结束"));
    } else {
        emit errorOccurred(QStringLiteral("视频流进程已退出（代码 %1）").arg(exitCode));
        emit stateChanged(QStringLiteral("播放失败"));
    }
}
