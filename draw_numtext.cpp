#include "draw_numtext.h"
#include <QPainter>

#define NUM_WIDTH 9
#define NUM_HEIGHT 13

NumTextWidget::NumTextWidget(QWidget *parent)
    : QWidget(parent), res(1), blinking(0) {
    fontImage.load("../numbers.bmp"); // Load your font image here
}

void NumTextWidget::setParameters(const QString &text, int res, int blinking) {
    this->text = text;
    this->res = res;
    this->blinking = blinking;
    update(); // Request a repaint
}

void NumTextWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter painter(this);

    QString numbers;
    if (res == 3) {
        if (blinking == 0) {
            numbers = "      ";
        } else if (blinking >= 60) {
            numbers = text;
        }
    } else if (res == 1) {
        numbers = text;
    }

    if (!numbers.isEmpty() && (res == 1 || res == 3)) {
        int x = 0; // Starting x position
        int y = 0; // Starting y position
        for (const QChar &c : numbers) {
            QRect src_rect;
            if (c >= '0' && c <= '9') {
                int char_index = c.unicode() - '0';
                src_rect = QRect(char_index * NUM_WIDTH, 0, NUM_WIDTH, NUM_HEIGHT);
            } else if (c == '-') {
                src_rect = QRect(27, 6, 9, 1);  // Specific coordinates for dash
            } else if (c == ':') {
                int char_index = 11;
                src_rect = QRect(char_index * NUM_WIDTH, 0, NUM_WIDTH, NUM_HEIGHT);
            } else if (c == ' ') {
                int char_index = 12;  // Assuming 12th position is for space
                src_rect = QRect(char_index * NUM_WIDTH, 0, NUM_WIDTH, NUM_HEIGHT);
            }

            if (!src_rect.isNull()) {
                QRect dst_rect(x, y, src_rect.width(), src_rect.height());
                painter.drawImage(dst_rect, fontImage, src_rect);
                if (c == ':') {
                    x -= 6;  // Decrement x position for colon
                }
                x += NUM_WIDTH + 3;  // Increment x position for next character
            }
        }
    }
}