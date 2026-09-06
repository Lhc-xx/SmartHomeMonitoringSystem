#include "RegisterDialog.h"

#include "ui_RegisterDialog.h"

#include "service/UserService.h"

RegisterDialog::RegisterDialog(UserService *userService, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RegisterDialog)
    , m_userService(userService)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("注册新账号"));
    setModal(true);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    /*
     * 注册窗口沿用登录页的绿色强调色，但用独立卡片和明确标题区分登录流程，
     * 让用户能够一眼看出当前正在创建账号而不是提交登录请求。
     */
    setStyleSheet(QStringLiteral(R"(
        QDialog#RegisterDialog {
            background: #f7faf8;
            color: #21352b;
            font-family: "Microsoft YaHei UI";
        }
        QLabel#dialogTitle {
            color: #176b48;
            font-size: 22px;
            font-weight: 700;
        }
        QLabel#dialogSubtitle {
            color: #71847a;
            font-size: 12px;
        }
        QLabel#fieldLabel {
            color: #315446;
            font-size: 13px;
            font-weight: 600;
        }
        QLineEdit {
            background: #ffffff;
            border: 1px solid #cfd8d3;
            border-radius: 5px;
            padding: 8px 12px;
            min-height: 24px;
            color: #253b31;
            selection-background-color: #a8cf3b;
            selection-color: #17442b;
        }
        QLineEdit:focus {
            border: 1px solid #16875b;
        }
        QPushButton {
            min-height: 28px;
            border-radius: 5px;
            padding: 7px 18px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton#registerButton {
            background: #0c8558;
            color: #ffffff;
            border: 1px solid #0c8558;
        }
        QPushButton#registerButton:hover { background: #109b67; }
        QPushButton#registerButton:pressed { background: #086b47; }
        QPushButton#cancelButton {
            background: #ffffff;
            color: #176b48;
            border: 1px solid #18875b;
        }
        QPushButton#cancelButton:hover { background: #edf8f2; }
        QPushButton:disabled {
            background: #b9c8c0;
            color: #edf5f0;
            border-color: #b9c8c0;
        }
        QLabel#statusLabel {
            color: #c65345;
            min-height: 24px;
        }
    )"));

    connect(ui->registerButton, &QPushButton::clicked,
            this, &RegisterDialog::onRegisterClicked);
    connect(ui->cancelButton, &QPushButton::clicked,
            this, &RegisterDialog::onCancelClicked);

    /*
     * 注册结果只连接到当前对话框，避免登录页和注册页同时消费同一条信号，
     * 也保证错误提示出现在用户正在操作的窗口中。
     */
    if (m_userService != nullptr) {
        connect(m_userService, &UserService::registerSuccess,
                this, &RegisterDialog::onRegisterSuccess);
        connect(m_userService, &UserService::registerFailed,
                this, &RegisterDialog::onRegisterFailed);
    }
}

RegisterDialog::~RegisterDialog()
{
    delete ui;
}

void RegisterDialog::onRegisterClicked()
{
    const QString username = ui->usernameEdit->text().trimmed();
    const QString password = ui->passwordEdit->text();
    const QString confirmation = ui->confirmPasswordEdit->text();

    /* 先在界面层拦截非法输入，避免把明显错误发送到服务端。 */
    if (username.isEmpty()) {
        ui->statusLabel->setText(QStringLiteral("请输入用户名。"));
        ui->usernameEdit->setFocus();
        return;
    }
    if (password.isEmpty()) {
        ui->statusLabel->setText(QStringLiteral("请输入密码。"));
        ui->passwordEdit->setFocus();
        return;
    }
    if (confirmation != password) {
        ui->statusLabel->setText(QStringLiteral("两次密码不一致。"));
        ui->confirmPasswordEdit->setFocus();
        return;
    }
    if (m_userService == nullptr) {
        ui->statusLabel->setText(QStringLiteral("认证服务不可用。"));
        return;
    }

    m_pendingUsername = username;
    ui->registerButton->setEnabled(false);
    ui->cancelButton->setEnabled(false);
    ui->statusLabel->setText(QStringLiteral("正在提交注册请求…"));
    m_userService->registerUser(username, password);
}

void RegisterDialog::onCancelClicked()
{
    reject();
}

void RegisterDialog::onRegisterSuccess()
{
    ui->statusLabel->setText(QStringLiteral("注册成功"));
    emit registered(m_pendingUsername);
    accept();
}

void RegisterDialog::onRegisterFailed(const QString &reason)
{
    ui->registerButton->setEnabled(true);
    ui->cancelButton->setEnabled(true);
    ui->statusLabel->setText(reason);
}
