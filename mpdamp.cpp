#include "mpdamp.h"
#include "visualizer.h"
#include "draw_numtext.h"

#include <QPainter>
#include <QTimer>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <cmath>
#include <QTime>

Color colors[] = {
    {0, 0, 0, 255},        // color 0 = black
    {24, 33, 41, 255},     // color 1 = grey for dots
    {239, 49, 16, 255},    // color 2 = top of spec
    {206, 41, 16, 255},    // 3
    {214, 90, 0, 255},     // 4
    {214, 102, 0, 255},    // 5
    {214, 115, 0, 255},    // 6
    {198, 123, 8, 255},    // 7
    {222, 165, 24, 255},   // 8
    {214, 181, 33, 255},   // 9
    {189, 222, 41, 255},   // 10
    {148, 222, 33, 255},   // 11
    {41, 206, 16, 255},    // 12
    {50, 190, 16, 255},    // 13
    {57, 181, 16, 255},    // 14
    {49, 156, 8, 255},     // 15
    {41, 148, 0, 255},     // 16
    {24, 132, 8, 255},     // 17 = bottom of spec
    {255, 255, 255, 255},  // 18 = osc 1
    {214, 214, 222, 255},  // 19 = osc 2 (slightly dimmer)
    {181, 189, 189, 255},  // 20 = osc 3
    {160, 170, 175, 255},  // 21 = osc 4
    {148, 156, 165, 255},  // 22 = osc 5
    {150, 150, 150, 255}   // 23 = analyzer peak dots
};

Color* osccolors(const Color* colors) {
    static Color osc_colors[16];

    osc_colors[0] = colors[21];
    osc_colors[1] = colors[21];
    osc_colors[2] = colors[20];
    osc_colors[3] = colors[20];
    osc_colors[4] = colors[19];
    osc_colors[5] = colors[19];
    osc_colors[6] = colors[18];
    osc_colors[7] = colors[18];
    osc_colors[8] = colors[19];
    osc_colors[9] = colors[19];
    osc_colors[10] = colors[20];
    osc_colors[11] = colors[20];
    osc_colors[12] = colors[21];
    osc_colors[13] = colors[21];
    osc_colors[14] = colors[22];
    osc_colors[15] = colors[22];

    return osc_colors;
}

MPDAmp::MPDAmp(QWidget *parent)
    : QWidget(parent), pipeFd(-1), in(nullptr), out(nullptr), conn(nullptr) {
    // Load the background image
    backgroundImage.load("../main.bmp");
    fontImage.load("../numbers.bmp");

    // Set the window size to the size of the image
    setFixedSize(backgroundImage.size());
    setWindowFlags(Qt::FramelessWindowHint);

    // Open the pipe for reading
    pipeFd = open("/tmp/mpd.fifo", O_RDONLY | O_NONBLOCK);
    if (pipeFd == -1) {
        perror("open");
    }

    // Initialize FFTW
    in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * BSZ);
    out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * BSZ);
    p = fftw_plan_dft_1d(BSZ, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    // Initialize and start the visualizer timer
    visualizerTimer = new QTimer(this);
    connect(visualizerTimer, &QTimer::timeout, this, &MPDAmp::updateVisualizer);
    visualizerTimer->start(16); // Approximately 60 fps

    // init and start new timer
    timeTimer = new QTimer(this);
    connect(timeTimer, &QTimer::timeout, this, &MPDAmp::updateElapsedTime);
    timeTimer->start(100); 

    // init mpd connection
    conn = mpd_connection_new(NULL, 0, 0);
    if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
        fprintf(stderr, "Error connecting to MPD: %s\n", mpd_connection_get_error_message(conn));
        mpd_connection_free(conn);
    }
}

MPDAmp::~MPDAmp() {
    if (pipeFd != -1) {
        ::close(pipeFd);
    }
    fftw_destroy_plan(p);
    fftw_free(in);
    fftw_free(out);
    if (conn) mpd_connection_free(conn);
}

void MPDAmp::paintEvent(QPaintEvent *event) {
    QPainter painter(this);

    // Draw the background image
    painter.drawImage(0, 0, backgroundImage);

    // Draw the visualizer
    drawVisualizer(&painter);

    // Draw the number text
    drawNumText(&painter);
}

void MPDAmp::drawVisualizer(QPainter *painter) {
    painter->save();

    // Translate the painter to the visualization position
    painter->translate(24, 43);

    // Set the clipping region to 76x16
    painter->setClipRect(0, 0, 76, 16);

    // Render the visualization
    Visualizer::render(painter, buffer, fftData, BSZ);

    painter->restore();
}

void MPDAmp::drawNumText(QPainter *painter) {
    painter->save();

    // Translate the painter to the desired text position
    painter->translate(36, 26); // Adjust position as needed

    // Draw the number text
    draw_numtext(painter, &fontImage, formatTime(mpd_status_get_elapsed_time(mpd_run_status(conn))), 0, 0, 1, 0);

    painter->restore();
}

void MPDAmp::updateVisualizer() {
    readPipe();
    processBuffer();
    update();
}

void MPDAmp::updateElapsedTime() {
    update();
}

void MPDAmp::readPipe() {
    if (pipeFd != -1) {
        read_count = read(pipeFd, buffer, sizeof(short) * BSZ);
        if (read_count == -1 && errno != EAGAIN) {
            perror("read");
        } else if (read_count == 0) {
            // EOF
            lseek(pipeFd, 0, SEEK_SET); // Reset to the beginning of the pipe
        }
    }
}

// Function to calculate the Blackman-Harris window coefficient
double blackman_harris(int n, int N) {
    const double a0 = 0.35875;
    const double a1 = 0.48829;
    const double a2 = 0.14128;
    const double a3 = 0.01168;
    return a0 - a1 * cos((2.0 * M_PI * n) / (N - 1))
               + a2 * cos((4.0 * M_PI * n) / (N - 1))
               - a3 * cos((6.0 * M_PI * n) / (N - 1));
}

// C-weighting filter coefficients calculation
double c_weighting(double freq) {
    double f2 = freq * freq;
    // Adjust these constants to change the weighting curve
    double f2_plus_20_6_squared = f2 + 40.6 * 40.6;  // Low-frequency pole
    double f2_plus_12200_squared = f2 + 2200 * 2200;  // High-frequency pole

    // Original constant is 1.0072; lower it to reduce overall high-frequency gain
    double constant = 1.2;  // Adjust this constant to decrease high frequency gain
    
    double r = (f2 * (f2 + 1.94e5)) / (f2_plus_20_6_squared * f2_plus_12200_squared);
    return r * constant;
}

void MPDAmp::processBuffer() {
    // Apply Blackman-Harris window to the input data
    if (read_count > 0) {
        for (int i = 0; i < BSZ; i++) {
            double window = blackman_harris(i, BSZ);
            in[i][0] = buffer[i] * window; // Apply Hann window to real part
            in[i][1] = 0.0;             // Imaginary part remains 0
        }
        fftw_execute(p);

        for (int i = 0; i < BSZ; i++) {
            fftData[i] = sqrt(out[i][0] * out[i][0] + out[i][1] * out[i][1]); // Magnitude
        }
    }
}

QString MPDAmp::formatTime(double seconds) {
    int minutes = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;

    // Format minutes and seconds with leading zeros
    QString formattedTime = QString("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));

    // Ensure the time string has leading spaces for better alignment
    if (minutes < 100) {
        // Add leading space if minutes are less than 100
        return QString(" %1").arg(formattedTime);
    } else {
        // For minutes 100 and above, return the formatted time directly
        return formattedTime;
    }
}