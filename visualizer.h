#ifndef VISUALIZER_H
#define VISUALIZER_H

#include <QWidget>
#include <QPainter>
#include "fft.h"

namespace Visualizer {

class VisualizerWidget : public QWidget {
    Q_OBJECT

public:
    VisualizerWidget(QWidget *parent = nullptr);
    void setData(const short *buffer, float *fftData, int length);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    const short *buffer;
    float *fftData;
    int length;
};

} // namespace Visualizer

#endif // VISUALIZER_H
