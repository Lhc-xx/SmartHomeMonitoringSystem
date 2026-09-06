#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "protocol/ClientProtocol.h"

class DeviceModel;
class QListView;
class QLabel;
class LoginWidget;
class QPushButton;
class RecordModel;
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
    void updateDevices(const QList<ClientProtocol::DeviceInfo> &devices);
    void updateRecords(const QList<ClientProtocol::RecordInfo> &records);
    void showRequestError(const QString &reason);

private:
    QWidget *createDataPage();

    Ui::MainWindow *ui;
    TcpClient *m_tcpClient;
    UserService *m_userService;
    LoginWidget *m_loginWidget;
    QWidget *m_dataPage;
    DeviceModel *m_deviceModel;
    RecordModel *m_recordModel;
    QListView *m_deviceList;
    QLabel *m_dataStatus;
    QPushButton *m_recordQueryButton;
};

#endif // MAINWINDOW_H
