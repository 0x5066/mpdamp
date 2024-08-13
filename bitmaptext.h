#ifndef BITMAPTEXT_H
#define BITMAPTEXT_H

#include <QWidget>
#include <QImage>
#include <QString>

class BitmapTextWidget : public QWidget {
    Q_OBJECT

public:
    explicit BitmapTextWidget(QWidget *parent = nullptr);
    void setText(const QString &text);
    void setFontImage(const QImage &image);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString text;
    QImage fontImage;
};

#endif // BITMAPTEXT_H
