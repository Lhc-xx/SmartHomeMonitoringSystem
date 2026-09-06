#ifndef LOGINWIDGET_H
#define LOGINWIDGET_H

#include <QWidget>

class UserService;

QT_BEGIN_NAMESPACE
namespace Ui {
class LoginWidget;
}
QT_END_NAMESPACE

/*
 * LoginWidget 类职责：
 *
 * 提供用户名、密码和登录控件，并把注册入口转交给独立的 RegisterDialog。
 * 本类只处理登录表单和页面切换，不生成 TLV 数据，也不直接访问网络对象。
 */
class LoginWidget : public QWidget
{
    Q_OBJECT

public:
    /* UserService 由 MainWindow 注入，使界面与认证业务实现解耦。 */
    explicit LoginWidget(UserService *userService, QWidget *parent = nullptr);
    ~LoginWidget();

private slots:
    /* 打开独立注册对话框，保持登录和注册两套交互流程彼此隔离。 */
    void onRegisterClicked();

    /* 读取账号密码并提交登录请求；具体 TLV 和网络操作仍由 UserService 完成。 */
    void onLoginClicked();

    /* 根据认证服务结果更新界面状态。 */
    void onLoginSuccess(quint64 userId);
    void onLoginFailed(const QString &reason);

private:
    /* Qt Designer 从 LoginWidget.ui 生成的控件访问对象。 */
    Ui::LoginWidget *ui;

    /* 仅保存认证服务引用，不拥有也不直接管理网络层。 */
    UserService *m_userService;
};

#endif // LOGINWIDGET_H
