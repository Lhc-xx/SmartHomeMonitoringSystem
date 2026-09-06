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
 * 提供用户名、密码、登录和注册控件，并将注册动作委托给 UserService。
 * 本类只处理界面输入与状态展示，不生成 TLV 数据，也不直接访问网络对象。
 */
class LoginWidget : public QWidget
{
    Q_OBJECT

public:
    /* UserService 由 MainWindow 注入，使界面与认证业务实现解耦。 */
    explicit LoginWidget(UserService *userService, QWidget *parent = nullptr);
    ~LoginWidget();

private slots:
    /* 读取控件内容并调用 UserService，保持注册业务规则在服务层统一处理。 */
    void onRegisterClicked();

    /* 读取账号密码并提交登录请求；具体 TLV 和网络操作仍由 UserService 完成。 */
    void onLoginClicked();

    /* 根据认证服务结果更新界面状态。 */
    void onRegisterSuccess();
    void onRegisterFailed(const QString &reason);
    void onLoginSuccess(quint64 userId);
    void onLoginFailed(const QString &reason);

private:
    /* Qt Designer 从 LoginWidget.ui 生成的控件访问对象。 */
    Ui::LoginWidget *ui;

    /* 仅保存认证服务引用，不拥有也不直接管理网络层。 */
    UserService *m_userService;
};

#endif // LOGINWIDGET_H
