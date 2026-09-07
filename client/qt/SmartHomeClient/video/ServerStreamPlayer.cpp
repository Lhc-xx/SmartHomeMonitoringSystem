#include "ServerStreamPlayer.h"

#include "protocol/media_packet.h"

ServerStreamPlayer::ServerStreamPlayer(std::unique_ptr<smart_home::client::IDecoder> decoder,
                                       QObject *parent)
    : QObject(parent),
      m_decoder(std::move(decoder)),
      m_stopRequested(false),
      m_decoderOpened(false)
{
}

ServerStreamPlayer::~ServerStreamPlayer()
{
    stop();
}

void ServerStreamPlayer::start()
{
    if (m_worker.joinable()) {
        return;
    }
    m_stopRequested = false;
    m_worker = std::thread(&ServerStreamPlayer::workerLoop, this);
    emit stateChanged(QStringLiteral("连接中"));
}

void ServerStreamPlayer::stop()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopRequested) {
            return;
        }
        m_stopRequested = true;
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
            if (m_stopRequested && m_pendingFrames.empty()) {
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
                if (!m_decoder->open(pkt)) {
                    emit errorOccurred(QStringLiteral("解码器初始化失败"));
                    emit stateChanged(QStringLiteral("解码失败"));
                    return;
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
}
