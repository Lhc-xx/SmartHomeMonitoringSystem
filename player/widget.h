#ifndef WIDGET_H
#define WIDGET_H
#include "VLCKits.h"

#include <QWidget>

#include <memory>

using std::unique_ptr;

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

private slots:
    void on_btnOpen_clicked();

private:
    Ui::Widget *ui;
    unique_ptr<VLCKits> _pVLCKits;

};
#endif // WIDGET_H
