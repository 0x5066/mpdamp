#include "bitmaptext.h"
#include <QPainter>
#include <QChar>
#include <QRect>

#define CHARF_WIDTH 5
#define CHARF_HEIGHT 6
#define FONT_IMAGE_WIDTH 155

BitmapTextWidget::BitmapTextWidget(QWidget *parent)
    : QWidget(parent) {}

void BitmapTextWidget::setText(const QString &text) {
    this->text = text;
    update(); // Request a repaint
}

void BitmapTextWidget::setFontImage(const QImage &image) {
    this->fontImage = image;
    update(); // Request a repaint
}

void BitmapTextWidget::paintEvent(QPaintEvent *event) {
    QPainter painter(this);

    const char *charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ\"@   0123456789….:()-'!_+\\/[]^&%,=$#ÂÖÄ?* ";

    std::string stdText = text.toStdString();
    int x = 0, y = 0; // Start position

    for (const char* c = stdText.c_str(); *c != '\0'; ++c) {
        char upper_char = std::toupper(*c);

        const char *char_pos = strchr(charset, upper_char);
        int index;
        if (char_pos) {
            index = char_pos - charset;
        } else {
            index = 67;
        }

        int col = index % (FONT_IMAGE_WIDTH / CHARF_WIDTH);
        int row = index / (FONT_IMAGE_WIDTH / CHARF_WIDTH);

        switch (*c) {
            case '"': col = 26; row = 0; break;
            case '@': col = 27; row = 0; break;
            case ' ': col = 29; row = 0; break;
            case ':':
            case ';':
            case '|': col = 12; row = 1; break;
            case '(':
            case '{': col = 13; row = 1; break;
            case ')':
            case '}': col = 14; row = 1; break;
            case '-':
            case '~': col = 15; row = 1; break;
            case '`':
            case '\'': col = 16; row = 1; break;
            case '!': col = 17; row = 1; break;
            case '_': col = 18; row = 1; break;
            case '+': col = 19; row = 1; break;
            case '\\': col = 20; row = 1; break;
            case '/': col = 21; row = 1; break;
            case '[': col = 22; row = 1; break;
            case ']': col = 23; row = 1; break;
            case '^': col = 24; row = 1; break;
            case '&': col = 25; row = 1; break;
            case '%': col = 26; row = 1; break;
            case '.':
            case ',': col = 27; row = 1; break;
            case '=': col = 28; row = 1; break;
            case '$': col = 29; row = 1; break;
            case '#': col = 30; row = 1; break;
            case '?': col = 3; row = 2; break;
            case '*': col = 4; row = 2; break;
        }

        QRect srcRect(col * CHARF_WIDTH, row * CHARF_HEIGHT, CHARF_WIDTH, CHARF_HEIGHT);
        QRect dstRect(x, y, CHARF_WIDTH, CHARF_HEIGHT);

        painter.drawImage(dstRect, fontImage, srcRect);
        x += CHARF_WIDTH;
    }
}