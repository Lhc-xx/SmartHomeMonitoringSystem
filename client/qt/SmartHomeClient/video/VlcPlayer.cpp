#include "VlcPlayer.h"

#include <cstdio>

namespace {

QString formatTime(libvlc_time_t seconds)
{
    const int hh = static_cast<int>(seconds / 3600);
    const int mm = static_cast<int>((seconds % 3600) / 60);
    const int ss = static_cast<int>(seconds % 60);
    char buffer[64] = {0};
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hh, mm, ss);
    return QString::fromLatin1(buffer);
}

QString stateText(libvlc_state_t state)
{
    switch (state) {
    case libvlc_Playing:
        return QStringLiteral("播放中");
    case libvlc_Paused:
        return QStringLiteral("已暂停");
    case libvlc_Stopped:
        return QStringLiteral("已停止");
    case libvlc_Ended:
        return QStringLiteral("播放结束");
    case libvlc_Error:
        return QStringLiteral("播放错误");
    default:
        return QStringLiteral("未知状态");
    }
}

}

VlcPlayer::VlcPlayer(QObject *parent)
    : QObject(parent),
      m_instance(nullptr),
      m_media(nullptr),
      m_mediaPlayer(nullptr),
      m_eventManager(nullptr),
      m_totalSeconds(0)
{
}

VlcPlayer::~VlcPlayer()
{
    stop();
    if (m_mediaPlayer != nullptr) {
        libvlc_media_player_release(m_mediaPlayer);
        m_mediaPlayer = nullptr;
    }
    if (m_media != nullptr) {
        libvlc_media_release(m_media);
        m_media = nullptr;
    }
    if (m_instance != nullptr) {
        libvlc_release(m_instance);
        m_instance = nullptr;
    }
}

void VlcPlayer::handleEvent(const libvlc_event_t *event, void *data)
{
    VlcPlayer *player = static_cast<VlcPlayer *>(data);
    if (player == nullptr) {
        return;
    }

    switch (event->type) {
    case libvlc_MediaPlayerPositionChanged:
        player->updatePosition();
        player->updateTime();
        break;
    case libvlc_MediaPlayerTimeChanged:
        player->updateTime();
        break;
    case libvlc_MediaPlayerAudioVolume:
        player->updateVolume();
        break;
    case libvlc_MediaPlayerPlaying:
        player->applyState(libvlc_Playing);
        break;
    case libvlc_MediaPlayerPaused:
        player->applyState(libvlc_Paused);
        break;
    case libvlc_MediaPlayerStopped:
        player->applyState(libvlc_Stopped);
        break;
    case libvlc_MediaPlayerEndReached:
        player->applyState(libvlc_Ended);
        break;
    case libvlc_MediaPlayerEncounteredError:
        player->applyState(libvlc_Error);
        emit player->errorOccurred(QStringLiteral("媒体播放器遇到错误"));
        break;
    default:
        break;
    }
}

bool VlcPlayer::init()
{
    if (m_instance != nullptr) {
        return true;
    }

    m_instance = libvlc_new(0, nullptr);
    if (m_instance == nullptr) {
        emit errorOccurred(QStringLiteral("无法创建 libvlc 实例"));
        return false;
    }

    m_mediaPlayer = libvlc_media_player_new(m_instance);
    if (m_mediaPlayer == nullptr) {
        libvlc_release(m_instance);
        m_instance = nullptr;
        emit errorOccurred(QStringLiteral("无法创建 libvlc media player"));
        return false;
    }

    m_eventManager = libvlc_media_player_event_manager(m_mediaPlayer);
    if (m_eventManager == nullptr) {
        libvlc_media_player_release(m_mediaPlayer);
        m_mediaPlayer = nullptr;
        libvlc_release(m_instance);
        m_instance = nullptr;
        emit errorOccurred(QStringLiteral("无法获取 libvlc 事件管理器"));
        return false;
    }

    libvlc_event_attach(m_eventManager, libvlc_MediaPlayerPositionChanged, handleEvent, this);
    libvlc_event_attach(m_eventManager, libvlc_MediaPlayerTimeChanged, handleEvent, this);
    libvlc_event_attach(m_eventManager, libvlc_MediaPlayerAudioVolume, handleEvent, this);
    libvlc_event_attach(m_eventManager, libvlc_MediaPlayerPlaying, handleEvent, this);
    libvlc_event_attach(m_eventManager, libvlc_MediaPlayerPaused, handleEvent, this);
    libvlc_event_attach(m_eventManager, libvlc_MediaPlayerStopped, handleEvent, this);
    libvlc_event_attach(m_eventManager, libvlc_MediaPlayerEndReached, handleEvent, this);
    libvlc_event_attach(m_eventManager, libvlc_MediaPlayerEncounteredError, handleEvent, this);
    return true;
}

bool VlcPlayer::playUrl(const QString &url, void *hwnd)
{
    if (m_instance == nullptr && !init()) {
        return false;
    }
    if (m_mediaPlayer == nullptr) {
        emit errorOccurred(QStringLiteral("media player 未初始化"));
        return false;
    }

    libvlc_media_t *media = libvlc_media_new_location(m_instance, url.toUtf8().constData());
    if (media == nullptr) {
        emit errorOccurred(QStringLiteral("无法创建媒体地址：%1").arg(url));
        return false;
    }

    if (m_media != nullptr) {
        libvlc_media_release(m_media);
    }
    m_media = media;

    libvlc_media_player_set_media(m_mediaPlayer, m_media);
    if (hwnd != nullptr) {
        libvlc_media_player_set_hwnd(m_mediaPlayer, hwnd);
    }
    if (libvlc_media_player_play(m_mediaPlayer) < 0) {
        emit errorOccurred(QStringLiteral("播放失败：%1").arg(url));
        return false;
    }
    return true;
}

bool VlcPlayer::playFile(const QString &path, void *hwnd)
{
    if (m_instance == nullptr && !init()) {
        return false;
    }
    if (m_mediaPlayer == nullptr) {
        emit errorOccurred(QStringLiteral("media player 未初始化"));
        return false;
    }

    libvlc_media_t *media = libvlc_media_new_path(m_instance, path.toUtf8().constData());
    if (media == nullptr) {
        emit errorOccurred(QStringLiteral("无法打开媒体文件：%1").arg(path));
        return false;
    }

    if (m_media != nullptr) {
        libvlc_media_release(m_media);
    }
    m_media = media;

    libvlc_media_parse(m_media);
    m_totalSeconds = libvlc_media_get_duration(m_media) / 1000;

    libvlc_media_player_set_media(m_mediaPlayer, m_media);
    if (hwnd != nullptr) {
        libvlc_media_player_set_hwnd(m_mediaPlayer, hwnd);
    }
    if (libvlc_media_player_play(m_mediaPlayer) < 0) {
        emit errorOccurred(QStringLiteral("播放失败：%1").arg(path));
        return false;
    }
    return true;
}

void VlcPlayer::play()
{
    if (m_mediaPlayer == nullptr) {
        return;
    }
    const libvlc_state_t state = libvlc_media_player_get_state(m_mediaPlayer);
    if (state == libvlc_Paused || state == libvlc_Stopped) {
        libvlc_media_player_play(m_mediaPlayer);
    }
}

void VlcPlayer::pause()
{
    if (m_mediaPlayer == nullptr) {
        return;
    }
    if (libvlc_media_player_get_state(m_mediaPlayer) == libvlc_Playing) {
        libvlc_media_player_pause(m_mediaPlayer);
    }
}

void VlcPlayer::stop()
{
    if (m_mediaPlayer == nullptr) {
        return;
    }
    const libvlc_state_t state = libvlc_media_player_get_state(m_mediaPlayer);
    if (state == libvlc_Playing || state == libvlc_Paused) {
        libvlc_media_player_stop(m_mediaPlayer);
        emit positionChanged(0);
    }
}

void VlcPlayer::setVolume(int volume)
{
    if (m_mediaPlayer == nullptr) {
        return;
    }
    libvlc_audio_set_volume(m_mediaPlayer, volume);
}

void VlcPlayer::setPosition(int percent)
{
    if (m_mediaPlayer == nullptr) {
        return;
    }
    libvlc_media_player_set_position(m_mediaPlayer, percent / 100.0f);
}

bool VlcPlayer::isInitialized() const
{
    return m_instance != nullptr && m_mediaPlayer != nullptr;
}

void VlcPlayer::updatePosition()
{
    if (m_mediaPlayer == nullptr) {
        return;
    }
    const float position = libvlc_media_player_get_position(m_mediaPlayer);
    emit positionChanged(static_cast<int>(position * 100.0f));
}

void VlcPlayer::updateTime()
{
    if (m_mediaPlayer == nullptr) {
        return;
    }
    const libvlc_time_t current = libvlc_media_player_get_time(m_mediaPlayer) / 1000;
    emit timeTextChanged(QStringLiteral("%1/%2").arg(formatTime(current), formatTime(m_totalSeconds)));
}

void VlcPlayer::updateVolume()
{
    if (m_mediaPlayer == nullptr) {
        return;
    }
    const int volume = libvlc_audio_get_volume(m_mediaPlayer);
    if (volume >= 0) {
        emit volumeChanged(volume);
    }
}

void VlcPlayer::applyState(libvlc_state_t state)
{
    emit stateChanged(stateText(state));
}
