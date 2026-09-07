#include "ServerStreamPlayer.h"

#include "protocol/media_packet.h"

namespace {
/* 控制排队深度，优先保留最新画面，避免网络抖动时延迟无限累积。 */
const size_t kMaxPendingFrames = 8;
}

ServerStreamPlayer::ServerStreamPlayer(std::unique_ptr<smart_home::client::IDecoder> decoder,
                                       QObject *parent)
    : QObject(parent),
      m_decoder(std::move(decoder)),
      m_stopRequested(false),
      m_decoderOpened(false),
      m_workerRunning(false)
{
}

ServerStreamPlayer::~ServerStreamPlayer()
{
    stop();
}

void ServerStreamPlayer::start()
{
    /* 解码失败后线程已退出但 std::thread 仍 joinable，先回收才能允许重启。 */
    if (m_worker.joinable() && !m_workerRunning.load()) {
        m_worker.join();
    }
    if (m_worker.joinable()) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopRequested = false;
        m_pendingFrames.clear();
        m_reassembler.reset();
        m_decoderOpened = false;
        m_workerRunning = true;
    }
    m_worker = std::thread(&ServerStreamPlayer::workerLoop, this);
    emit stateChanged(QStringLiteral("连接中"));
}

void ServerStreamPlayer::stop()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopRequested && !m_worker.joinable()) {
            return;
        }
        m_stopRequested = true;
        /* 停止是立即生效的控制操作，不再把积压画面全部解码后才退出。 */
        m_pendingFrames.clear();
    }
    m_cv.notify_all();
    if (m_worker.joinable()) {
        m_worker.join();
    }
    emit stateChanged(QStringLiteral("已停止"));
}

void ServerStreamPlayer::onMediaFrame(const QByteArray &frameBytes)
{
    if (frameBytes.isEmpty()) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopRequested) {
            return;
        }
        if (m_pendingFrames.size() >= kMaxPendingFrames) {
            /* 丢弃最旧帧，保持预览低延迟；下一帧仍按原顺序解码。 */
            m_pendingFrames.pop_front();
        }
        m_pendingFrames.push_back(frameBytes);
    }
    m_cv.notify_one();
}

void ServerStreamPlayer::workerLoop()
{
    while (true) {
        QByteArray frame;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]() { return m_stopRequested || !m_pendingFrames.empty(); });
            if (m_stopRequested) {
                break;
            }
            frame = m_pendingFrames.front();
            m_pendingFrames.pop_front();
        }

        /*
         * 把（可能跨 TCP 分片到达的）媒体帧字节喂给重组器，再逐个取出完整包。
         * 帧边界由 MediaPacketSerializer 的帧长前缀 + 魔数保证。
         */
        m_reassembler.feed(reinterpret_cast<const uint8_t *>(frame.constData()),
                           static_cast<size_t>(frame.size()));
        smart_home::protocol::MediaPacket pkt;
        while (m_reassembler.nextPacket(pkt)) {
            if (!m_decoderOpened) {
                if (!m_decoder || !m_decoder->open(pkt)) {
                    emit errorOccurred(QStringLiteral("解码器初始化失败"));
                    emit stateChanged(QStringLiteral("解码失败"));
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        m_stopRequested = true;
                        m_pendingFrames.clear();
                    }
                    break;
                }
                m_decoderOpened = true;
            }

            smart_home::client::Frame f;
            if (m_decoder->decode(pkt, f)) {
                /*
                 * QImage 直接引用 f.rgba 的缓冲，这里 copy() 做深拷贝，
                 * 避免 f 出作用域后悬空；随后跨线程排队到 GUI 线程绘制。
                 */
                const QImage image(f.rgba.data(), f.width, f.height, QImage::Format_RGBA8888);
                emit frameReady(image.copy());
                emit stateChanged(QStringLiteral("播放中"));
            }
        }
    }

    if (m_decoderOpened) {
        m_decoder->close();
        m_decoderOpened = false;
    }
    m_workerRunning = false;
}
