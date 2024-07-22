#ifndef VISUALIZER_H
#define VISUALIZER_H

#include <QPainter>

namespace Visualizer {
    void render(QPainter *painter, const short *buffer, const double *fftData, int length);
}

double c_weighting(double freq);

#endif // VISUALIZER_H
