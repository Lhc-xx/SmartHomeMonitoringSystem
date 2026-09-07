#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "protocol/ClientProtocol.h"

class DeviceModel;
class FilePlaybackPlayer;
class QCheckBox;
class QDateTimeEdit;
class QListView;
class QLabel;
class LoginWidget;
class MonitoringDashboard;
class QPushButton;
class RecordModel;
class ServerStreamPlayer;
class TcpClient;
class UserService;
class QWidget;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/*
 * MainWindow 负责对象组装和登录后的 B 数据页切换。
 * 数据页只展示设备及录像元数据，不创建播放控件，也不触及 FFmpeg。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void showDataPage(quint64 userId);
    void requestDevices();
    void requestRecords();
    void requestRecordsForDevice(quint64 deviceId);
    void updateDevices(const QList<ClientProtocol::DeviceInfo> &devices);
    void updateRecords(const QList<ClientProtocol::RecordInfo> &records);
    void showRequestError(const QString &reason);
    void handlePlaybackRequest(quint64 deviceId);
    void handleRecordStart(quint64 deviceId);
    void handleRecordStop();

private:
    QWidget *createDataPage();

    Ui::MainWindow *ui;
    TcpClient *m_tcpClient;
    UserService *m_userService;
    LoginWidget *m_loginWidget;
    MonitoringDashboard *m_dashboard;
    ServerStreamPlayer *m_serverStreamPlayer;
    FilePlaybackPlayer *m_playbackPlayer;
    QList<ClientProtocol::RecordInfo> m_records;
    bool m_pendingPlayback;
    QWidget *m_dataPage;
    DeviceModel *m_deviceModel;
    RecordModel *m_recordModel;
    QListView *m_deviceList;
    QLabel *m_dataStatus;
    QPushButton *m_recordQueryButton;
    QCheckBox *m_allTimeCheckBox;
    QDateTimeEdit *m_recordStartEdit;
    QDateTimeEdit *m_recordEndEdit;
};

#endif // MAINWINDOW_H
