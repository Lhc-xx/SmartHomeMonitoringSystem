#include "widget.h"
#include "ui_widget.h"

#include <QDebug>

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
    , _pVLCKits(new VLCKits)
{
    ui->setupUi(this);

    _pVLCKits->initVLC();

}

Widget::~Widget()
{
    delete ui;
}


void Widget::on_btnOpen_clicked()
{
    QString url = ui->urlEdit->text();
    qDebug() << "url:" << url << endl;

    _pVLCKits->playURL(url, (void*)ui->videoWidget->winId());

}
