#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QWidget>

/*
 * VideoWidget 类职责：
 *
 * 预留后续 FFmpeg 视频帧显示区域。
 * 当前只绘制 "Video Area" 占位文字，不包含解码、拉流或任何视频业务逻辑。
 */
class VideoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoWidget(QWidget *parent = nullptr);

protected:
    /* 通过绘制事件居中显示占位文本，避免引入额外的 Designer 文件和控件所有权。 */
    void paintEvent(QPaintEvent *event) override;
};

#endif // VIDEOWIDGET_H
