#ifndef MONOSTER_WIDGET_H
#define MONOSTER_WIDGET_H

#include <QWidget>
#include <QImage>
#include <QString>

class MonosterWidget : public QWidget {
    Q_OBJECT

public:
    MonosterWidget(QWidget *parent = nullptr);
    void setParameters(int channels, int res);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage monosterImage;
    int channels;
    int res;
};

#endif // MONOSTER_WIDGET_H