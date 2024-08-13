#ifndef DRAW_NUMTEXT_H
#define DRAW_NUMTEXT_H

#include <QWidget>
#include <QImage>
#include <QString>

class NumTextWidget : public QWidget {
    Q_OBJECT

public:
    NumTextWidget(QWidget *parent = nullptr);
    void setParameters(const QString &text, int res, int blinking);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage fontImage;
    QString text;
    int res;
    int blinking;
};

#endif // DRAW_NUMTEXT_H
