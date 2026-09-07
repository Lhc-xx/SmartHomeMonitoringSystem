#include "VLCKits.h"
#include <QDebug>

VLCKits::VLCKits(QObject *parent) : QObject(parent)
{
}

VLCKits::~VLCKits()
{
    libvlc_media_player_release(_pMediaPlayer);
    libvlc_media_release(_pMedia);
    libvlc_release(_pInstance);
}

static void processPosition(VLCKits * pkits)
{
    //百分比
    float pos = libvlc_media_player_get_position(pkits->mediaPlayer());
    pkits->setTimeSliderPos(pos * 100);

    libvlc_time_t totalSecs = pkits->durations();
    //qDebug() << "total secs: " << totalSecs;
    libvlc_time_t curSecs = libvlc_media_player_get_time(pkits->mediaPlayer()) / 1000;
    //qDebug() << "cur secs: " << curSecs;
    int hh = curSecs / 3600;
    int mm = curSecs % 3600 / 60;
    int ss = curSecs % 60;
    int HH = totalSecs / 3600;
    int MM = totalSecs % 3600 / 60;
    int SS = totalSecs % 60;
    char buff[64] = {0};
    sprintf(buff, "%02d:%02d:%02d/%02d:%02d:%02d",
            hh, mm, ss, HH, MM, SS);
    QString str(buff);
    pkits->setTimeText(str);
}

static void processVolume(VLCKits * pkits)
{
    int val = libvlc_audio_get_volume(pkits->mediaPlayer());
    qDebug() << "volume:" << val;
    if(val == -1) {
        return;
    }
    pkits->setVolumePos(val);
}

static void vlc_callback( const struct libvlc_event_t *p_event, void *p_data )
{
    VLCKits * pkits = static_cast<VLCKits*>(p_data);
    if(pkits) {
        switch(p_event->type) {
        case libvlc_MediaPlayerPositionChanged://播放位置变化时
            processPosition(pkits); break;
        case libvlc_MediaPlayerAudioVolume://音量变化时
            processVolume(pkits); break;
        }
    }
}

bool VLCKits::initVLC()
{
    _pInstance = libvlc_new(0, nullptr);
    if(_pInstance) {
        _pMediaPlayer = libvlc_media_player_new(_pInstance);
        if(_pMediaPlayer) {
            _pEventManager = libvlc_media_player_event_manager(_pMediaPlayer);
            if(_pEventManager) {
                //注册媒体播放器的事件处理器
                //1. 位置发生改变时
                libvlc_event_attach(_pEventManager, libvlc_MediaPlayerPositionChanged,
                                    vlc_callback, this);
                //2. 声音变化时
                libvlc_event_attach(_pEventManager, libvlc_MediaPlayerAudioVolume,
                                    vlc_callback, this);
                return true;
            } else {
                libvlc_media_player_release(_pMediaPlayer);
                libvlc_release(_pInstance);
                return false;
            }
        } else {
            libvlc_release(_pInstance);
            return false;
        }
    } else {
        return false;
    }
}

bool VLCKits::play(const QString &name, void *hwnd)
{
    //1. 设置媒体
    _pMedia = libvlc_media_new_path(_pInstance, name.toStdString().c_str());
    if(!_pMedia) {
        return false;
    }
    //得到了媒体文件后
    //2. 解析文件
    libvlc_media_parse(_pMedia);
    _totalSec = libvlc_media_get_duration(_pMedia) / 1000;

    //3. 设置媒体播放器
    libvlc_media_player_set_media(_pMediaPlayer, _pMedia);

    //4. 设置播放窗口句柄
    libvlc_media_player_set_hwnd(_pMediaPlayer, hwnd);
    //5. 播放视频
    int ret = libvlc_media_player_play(_pMediaPlayer);
    if(ret < 0) {
        return false;
    } else
        return true;
}

bool VLCKits::playURL(const QString &name, void *hwnd)
{
    _pMedia = libvlc_media_new_location(_pInstance, name.toStdString().c_str());
    if(!_pMedia) {
        return false;
    }
    libvlc_media_player_set_media(_pMediaPlayer, _pMedia);

    libvlc_media_player_set_hwnd(_pMediaPlayer, hwnd);
    int ret = libvlc_media_player_play(_pMediaPlayer);
    if(ret < 0) {
        return false;
    } else
        return true;
}

void VLCKits::play()
{
    libvlc_state_t state = libvlc_media_player_get_state(_pMediaPlayer);
    if(state == libvlc_Paused ||
        state == libvlc_Stopped) {
        libvlc_media_player_play(_pMediaPlayer);
    }
}

void VLCKits::pause()
{
    libvlc_state_t state = libvlc_media_player_get_state(_pMediaPlayer);
    if(state == libvlc_Playing) {
        libvlc_media_player_pause(_pMediaPlayer);
    }
}

void VLCKits::stop()
{
    libvlc_state_t state = libvlc_media_player_get_state(_pMediaPlayer);
    if(state == libvlc_Playing ||
        state == libvlc_Paused) {
        libvlc_media_player_stop(_pMediaPlayer);
        //重置进度条的位置
        emit sigTimeSliderPos(0);
    }
}

void VLCKits::setTimeSliderPos(int value)
{   //发送信号
    emit sigTimeSliderPos(value);
}

void VLCKits::setTimeText(const QString &str)
{   //发送信号
    emit sigTimeText(str);
}

void VLCKits::setVolumePos(int value)
{
    emit sigVolumeSliderPos(value);
}

void VLCKits::setVolume(int value)
{
    libvlc_audio_set_volume(_pMediaPlayer, value);
}

void VLCKits::setPosition(int value)
{
    libvlc_media_player_set_position(_pMediaPlayer, value / 100.0);
}






