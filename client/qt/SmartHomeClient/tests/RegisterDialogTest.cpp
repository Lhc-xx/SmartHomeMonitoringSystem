#include <QApplication>
#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QtTest>

#include "network/TcpClient.h"
#include "service/UserService.h"
#include "ui/LoginWidget.h"
#include "ui/RegisterDialog.h"

/*
 * RegisterDialogTest 类职责：
 *
 * 验证独立注册窗口的控件结构和前置输入校验。
 * 测试使用空 UserService 指针，确保不启动服务器、不发送真实 TCP 请求，
 * 只检查界面在进入业务层之前能否拒绝非法输入。
 */
class RegisterDialogTest : public QObject
{
    Q_OBJECT

private slots:
    void exposesIndependentRegistrationForm();
    void validatesInputBeforeCallingService();
    void loginButtonOpensRegistrationDialog();
};

void RegisterDialogTest::exposesIndependentRegistrationForm()
{
    RegisterDialog dialog(nullptr);

    QVERIFY(dialog.findChild<QLineEdit *>(QStringLiteral("usernameEdit")) != nullptr);
    QVERIFY(dialog.findChild<QLineEdit *>(QStringLiteral("passwordEdit")) != nullptr);
    QVERIFY(dialog.findChild<QLineEdit *>(QStringLiteral("confirmPasswordEdit")) != nullptr);
    QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("registerButton")) != nullptr);
    QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("cancelButton")) != nullptr);
    QCOMPARE(dialog.windowTitle(), QStringLiteral("注册新账号"));
}

void RegisterDialogTest::validatesInputBeforeCallingService()
{
    RegisterDialog dialog(nullptr);
    QLineEdit *username = dialog.findChild<QLineEdit *>(QStringLiteral("usernameEdit"));
    QLineEdit *password = dialog.findChild<QLineEdit *>(QStringLiteral("passwordEdit"));
    QLineEdit *confirmation = dialog.findChild<QLineEdit *>(QStringLiteral("confirmPasswordEdit"));
    QPushButton *submit = dialog.findChild<QPushButton *>(QStringLiteral("registerButton"));
    QLabel *status = dialog.findChild<QLabel *>(QStringLiteral("statusLabel"));

    QVERIFY(username != nullptr);
    QVERIFY(password != nullptr);
    QVERIFY(confirmation != nullptr);
    QVERIFY(submit != nullptr);
    QVERIFY(status != nullptr);

    submit->click();
    QCOMPARE(status->text(), QStringLiteral("请输入用户名。"));

    username->setText(QStringLiteral("alice"));
    submit->click();
    QCOMPARE(status->text(), QStringLiteral("请输入密码。"));

    password->setText(QStringLiteral("secret"));
    confirmation->setText(QStringLiteral("different"));
    submit->click();
    QCOMPARE(status->text(), QStringLiteral("两次密码不一致。"));

    confirmation->setText(QStringLiteral("secret"));
    submit->click();
    QCOMPARE(status->text(), QStringLiteral("认证服务不可用。"));
}

void RegisterDialogTest::loginButtonOpensRegistrationDialog()
{
    TcpClient tcpClient;
    tcpClient.setAutoReconnect(false);
    UserService userService(&tcpClient);
    LoginWidget loginWidget(&userService);
    QPushButton *registerButton = loginWidget.findChild<QPushButton *>(
        QStringLiteral("registerButton"));

    QVERIFY(registerButton != nullptr);
    QCOMPARE(registerButton->text(), QStringLiteral("注册新账号"));

    bool opened = false;
    QTimer::singleShot(0, [&opened]() {
        QDialog *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        opened = dialog != nullptr
            && dialog->windowTitle() == QStringLiteral("注册新账号");
        if (dialog != nullptr) {
            dialog->reject();
        }
    });
    registerButton->click();
    QVERIFY(opened);
}

QTEST_MAIN(RegisterDialogTest)

#include "RegisterDialogTest.moc"
