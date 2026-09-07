#ifndef VLCKITS_H
#define VLCKITS_H

#include <QObject>
#include <vlc/vlc.h>



class VLCKits : public QObject
{
    Q_OBJECT
public:
    explicit VLCKits(QObject *parent = nullptr);
    ~VLCKits();

    bool initVLC();
    bool play(const QString & name, void * hwnd);
    bool playURL(const QString & name, void * hwnd);
    void play();
    void pause();
    void stop();

    libvlc_media_player_t * mediaPlayer() const { return _pMediaPlayer;   }
    libvlc_time_t durations() const {   return _totalSec;   }

    void setTimeSliderPos(int value);
    void setTimeText(const QString & str);
    void setVolumePos(int value);

    void setVolume(int value);
    void setPosition(int value);

signals:
    void sigTimeSliderPos(int value);
    void sigTimeText(const QString & str);
    void sigVolumeSliderPos(int value);

private:
    libvlc_instance_t *         _pInstance = nullptr;
    libvlc_media_t *            _pMedia = nullptr;
    libvlc_media_player_t *     _pMediaPlayer = nullptr;
    libvlc_event_manager_t *    _pEventManager = nullptr;
    libvlc_time_t _totalSec = 0;
};

#endif // VLCKITS_H
