#ifndef DRAW_NUMTEXT_H
#define DRAW_NUMTEXT_H

#include <QPainter>
#include <QImage>
#include <QString>

void draw_numtext(QPainter *painter, QImage *fontImage, const QString &text, int x, int y, int res, int blinking);

#endif // DRAW_NUMTEXT_H
