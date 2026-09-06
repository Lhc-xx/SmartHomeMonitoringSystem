#include "LoginWidget.h"

#include "ui_LoginWidget.h"

#include "service/UserService.h"

LoginWidget::LoginWidget(UserService *userService, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LoginWidget)
    , m_userService(userService)
{
    /*
     * 控件层次与 objectName 均由 Qt Designer 文件定义，
     * 这样后续调整布局无需在 C++ 中手工创建和管理控件。
    */
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("Smart Home Monitoring System"));
    ui->statusLabel->clear();

    /*
     * 参考授权页面的纵向结构，但不复制其绿色整页背景。
     * 页面保持纯白，只在品牌标识、焦点边框和主按钮中使用绿色作为强调色。
     */
    setStyleSheet(QStringLiteral(R"(
        QWidget#LoginWidget {
            background: #ffffff;
            color: #21352b;
            font-family: "Microsoft YaHei UI";
        }
        QFrame#loginCard {
            background: transparent;
            border: none;
        }
        QLineEdit {
            background: #fafafa;
            border: 1px solid #cfd8d3;
            border-radius: 4px;
            padding: 9px 14px;
            min-height: 24px;
            color: #253b31;
            selection-background-color: #a8cf3b;
            selection-color: #17442b;
        }
        QLineEdit:focus {
            border: 1px solid #16875b;
            background: #ffffff;
        }
        QPushButton {
            min-height: 24px;
            border-radius: 4px;
            padding: 9px 18px;
            font-size: 14px;
            font-weight: 600;
        }
        QPushButton#loginButton {
            background: #0c8558;
            color: #ffffff;
            border: 1px solid #0c8558;
        }
        QPushButton#loginButton:hover {
            background: #109b67;
        }
        QPushButton#loginButton:pressed {
            background: #086b47;
        }
        QPushButton#registerButton {
            background: #ffffff;
            color: #176b48;
            border: 1px solid #18875b;
        }
        QPushButton#registerButton:hover {
            background: #edf8f2;
        }
        QPushButton#registerButton:pressed {
            background: #dcefe5;
        }
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
            this, &LoginWidget::onRegisterClicked);
    connect(ui->loginButton, &QPushButton::clicked,
            this, &LoginWidget::onLoginClicked);

    /*
     * 服务层通过信号返回异步结果，界面不等待网络操作，
     * 从而保持 Qt 主事件循环可响应用户操作。
     */
    if (m_userService != nullptr) {
        connect(m_userService, &UserService::registerSuccess,
                this, &LoginWidget::onRegisterSuccess);
        connect(m_userService, &UserService::registerFailed,
                this, &LoginWidget::onRegisterFailed);
        connect(m_userService, &UserService::loginSuccess,
                this, &LoginWidget::onLoginSuccess);
        connect(m_userService, &UserService::loginFailed,
                this, &LoginWidget::onLoginFailed);
    } else {
        ui->registerButton->setEnabled(false);
        ui->statusLabel->setText(QStringLiteral("认证服务初始化失败。"));
    }
}

LoginWidget::~LoginWidget()
{
    /* ui 不是 QObject，需由窗口析构函数显式释放。 */
    delete ui;
}

void LoginWidget::onRegisterClicked()
{
    if (m_userService == nullptr) {
        ui->statusLabel->setText(QStringLiteral("认证服务不可用。"));
        return;
    }

    /* 用户名去除输入两端的无意空格；密码保持原样，避免静默修改密码内容。 */
    const QString username = ui->usernameEdit->text().trimmed();
    const QString password = ui->passwordEdit->text();

    ui->statusLabel->setText(QStringLiteral("正在提交注册请求…"));
    m_userService->registerUser(username, password);
}

void LoginWidget::onLoginClicked()
{
    if (m_userService == nullptr) {
        ui->statusLabel->setText(QStringLiteral("认证服务不可用。"));
        return;
    }
    /* 用户名去除两端误输入空格，密码保持字节语义，避免静默改变口令。 */
    const QString username = ui->usernameEdit->text().trimmed();
    const QString password = ui->passwordEdit->text();
    ui->statusLabel->setText(QStringLiteral("正在提交登录请求…"));
    m_userService->loginUser(username, password);
}

void LoginWidget::onRegisterSuccess()
{
    ui->statusLabel->setText(QStringLiteral("注册成功"));
}

void LoginWidget::onRegisterFailed(const QString &reason)
{
    ui->statusLabel->setText(reason);
}

void LoginWidget::onLoginSuccess(quint64 userId)
{
    /* 只确认登录结果，不在界面展示或记录 token。 */
    ui->statusLabel->setText(QStringLiteral("登录成功，用户 ID：%1").arg(userId));
}

void LoginWidget::onLoginFailed(const QString &reason)
{
    ui->statusLabel->setText(reason);
}
