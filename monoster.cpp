#include "monoster.h"
#include <QPainter>
#include <QFontDatabase>

#define MONO_STEREO_WIDTH 29
#define MONO_STEREO_HEIGHT 12

MonosterWidget::MonosterWidget(QWidget *parent)
    : QWidget(parent), channels(0), res(1) {
    monosterImage.load("../monoster.bmp"); // Load your mono/stereo image here
}

void MonosterWidget::setParameters(int channels, int res) {
    this->channels = channels;
    this->res = res;
    update(); // Request a repaint
}

void MonosterWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter painter(this);

    QRect src_mono(MONO_STEREO_WIDTH, 0, MONO_STEREO_WIDTH, MONO_STEREO_HEIGHT);
    QRect src_stereo(0, 0, MONO_STEREO_WIDTH, MONO_STEREO_HEIGHT);

    if (channels == 2) {
        std::swap(src_mono, src_stereo); // Swap rectangles for stereo
    }

    QRect dst_rect(0, 0, src_mono.width(), src_mono.height());
    painter.drawImage(dst_rect, monosterImage, src_mono);

    if (res == 3 && channels > 0) {
        dst_rect.moveLeft(MONO_STEREO_WIDTH + 3); // Increment x position for stereo
        painter.drawImage(dst_rect, monosterImage, src_stereo);
    }
}