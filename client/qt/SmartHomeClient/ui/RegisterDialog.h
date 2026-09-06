#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include <QDialog>

class UserService;

QT_BEGIN_NAMESPACE
namespace Ui {
class RegisterDialog;
}
QT_END_NAMESPACE

/*
 * RegisterDialog 类职责：
 *
 * 提供独立的注册表单，负责注册输入校验和注册结果展示。
 * 对话框只依赖 UserService 的业务接口，不构造 TLV、不直接操作 socket，
 * 这样注册窗口关闭后不会改变既有登录和网络层的职责边界。
 */
class RegisterDialog : public QDialog
{
    Q_OBJECT

public:
    /* 由登录页注入认证服务，避免对话框自行创建网络对象。 */
    explicit RegisterDialog(UserService *userService, QWidget *parent = nullptr);
    ~RegisterDialog();

signals:
    /* 注册成功后把用户名交还登录页，便于用户直接输入密码登录。 */
    void registered(const QString &username);

private slots:
    /* 校验输入后提交异步注册请求，避免在 UI 线程中等待网络。 */
    void onRegisterClicked();

    /* 用户取消注册时关闭模态对话框，不触发任何网络请求。 */
    void onCancelClicked();

    /* UserService 返回成功或失败时更新对话框状态。 */
    void onRegisterSuccess();
    void onRegisterFailed(const QString &reason);

private:
    /* Qt Designer 生成的控件访问对象。 */
    Ui::RegisterDialog *ui;

    /* 只保存认证服务的非拥有指针，对话框不负责释放服务对象。 */
    UserService *m_userService;

    /* 保存已通过校验的用户名，成功信号使用它回填登录页。 */
    QString m_pendingUsername;
};

#endif // REGISTERDIALOG_H
