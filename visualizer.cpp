#include "visualizer.h"
#include "mpdamp.h"
#include <cmath>

void Visualizer::render(QPainter *painter, const short *buffer, const double *fftData, int length) {
    painter->fillRect(0, 0, 75, 16, Qt::black);
    Color* osc_colors = osccolors(colors);
    painter->setPen(Qt::NoPen);

    int last_y = 0;

    static int sapeaks[75];
    static char safalloff[75];
    int sadata2[75];
    static float sadata3[75];
    bool sa_thick = true;

    double sample[BSZ];
    int bufferSize = length; // Assuming the length is passed as the size of buffer
    int targetSize = 75;
    int lower = 8192;

    // Calculate C-weighting for each FFT bin frequency
    double c_weighting_factors[bufferSize / 2];
    for (int i = 0; i < bufferSize / 2; ++i) {
        double freq = (double)i * 44100 / bufferSize;
        c_weighting_factors[i] = c_weighting(freq);
    }

    // Apply C-weighting to the FFT data
    double weighted_buffer[bufferSize / 2];
    for (int i = 0; i < bufferSize / 2; ++i) {
        weighted_buffer[i] = fftData[i] * c_weighting_factors[i];
    }

    // Calculate the maximum frequency index (half the buffer size for real-valued FFT)
    int maxFreqIndex = bufferSize / 2;

    // Logarithmic scale factor
    double logMaxFreqIndex = log10(maxFreqIndex);
    double logMinFreqIndex = 0;  // log10(1) == 0

    for (int x = 0; x < targetSize; x++) {
        // Calculate the logarithmic index with intensity adjustment
        double logScaledIndex = logMinFreqIndex + (logMaxFreqIndex - logMinFreqIndex) * x / (targetSize - 1);
        double scaledIndex = pow(10, logScaledIndex / 1.0f);

        int index1 = (int)floor(scaledIndex);
        int index2 = (int)ceil(scaledIndex);

        if (index1 >= maxFreqIndex) {
            index1 = maxFreqIndex - 1;
        }
        if (index2 >= maxFreqIndex) {
            index2 = maxFreqIndex - 1;
        }

        if (index1 == index2) {
            // No interpolation needed, exact match
            sample[x] = weighted_buffer[index1] / lower;
        } else {
            // Perform linear interpolation
            double frac2 = scaledIndex - index1;
            double frac1 = 1.0 - frac2;
            sample[x] = (frac1 * weighted_buffer[index1] + frac2 * weighted_buffer[index2]) / lower;
        }
    }

    // Render oscilloscope
    for (int x = 0; x < 75; x++) {
        int buffer_index = (x * length) / 75;
        int y = (((buffer[buffer_index / 2] + 32768 + 1024) * 2) * 16 / 65536);
        y = (-y) + 23;

        y = y < 0 ? 0 : (y > 16 - 1 ? 16 - 1 : y);

        if (x == 0) {
            last_y = y;
        }

        int top = y;
        int bottom = last_y;
        last_y = y;

        if (bottom < top) {
            int temp = bottom;
            bottom = top;
            top = temp + 1;
        }

        int color_index = (top) % 16;
        Color scope_color = osc_colors[color_index];
        painter->setBrush(QColor(scope_color.r, scope_color.g, scope_color.b, scope_color.a));

        for (int dy = top; dy <= bottom; dy++) {
            painter->drawRect(x, dy, 1, 1);
        }
    }

    // Render spectrum analyzer
    for (int x = 0; x < 75; x++) {
        int i = ((i = x & 0xfffffffc) < 72) ? i : 71; // Limiting i to prevent out of bounds access
        if (sa_thick == true) {
            int uVar12 = (int)((sample[i + 3] + sample[i + 2] + sample[i + 1] + sample[i]) / 4);
            sadata2[x] = uVar12;
        } else {
            sadata2[x] = (int)sample[x];
        }

        safalloff[x] -= 32 / 16.0f;

        if (sadata2[x] < 0) {
            sadata2[x] += 127;
        }
        if (sadata2[x] >= 15) {
            sadata2[x] = 15;
        }

        if (safalloff[x] <= sadata2[x]) {
            safalloff[x] = sadata2[x];
        }

        if (sapeaks[x] <= (int)(safalloff[x] * 256)) {
            sapeaks[x] = safalloff[x] * 256;
            sadata3[x] = 3.0f;
        }

        int intValue2 = -(sapeaks[x] / 256) + 15;
        sapeaks[x] -= (int)sadata3[x];
        sadata3[x] *= 1.05f;
        if (sapeaks[x] < 0) {
            sapeaks[x] = 0;
        }

        if (true) { // VisMode == 0 (default rendering mode)
            if (!((x == i + 3 && x < 72) && sa_thick)) {
                for (int dy = -safalloff[x] + 16; dy <= 17; ++dy) {
                    int color_index = dy + 2; // Assuming dy starts from 0
                    Color scope_color = colors[color_index];
                    painter->setBrush(QColor(scope_color.r, scope_color.g, scope_color.b, scope_color.a));
                    painter->drawRect(x, dy, 1, 1);
                }
            }

            if (!((x == i + 3 && x < 72) && sa_thick)) {
                if (intValue2 < 16) {
                    Color peaksColor = colors[23];
                    painter->setBrush(QColor(peaksColor.r, peaksColor.g, peaksColor.b, peaksColor.a));
                    painter->drawRect(x, intValue2, 1, 1);
                }
            }
        }
    }
}
