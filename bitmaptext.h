#ifndef BITMAPTEXT_H
#define BITMAPTEXT_H

#include <QPainter>
#include <QImage>
#include <QString>

void draw_bitmaptext(QPainter *painter, QImage *fontImage, const QString &text, int x, int y);

#endif // BITMAPTEXT_H
