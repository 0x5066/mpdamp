#ifndef MPDAMP_H
#define MPDAMP_H

#include <QWidget>
#include <QImage>
#include <QTimer>
#include <QMouseEvent>
#include <mpd/client.h>
#include "visualizer.h"
#include "draw_numtext.h"
#include "bitmaptext.h"

const static int BSZ = 1152;
const static int BSZ2 = BSZ / 2;

typedef struct {
    uint8_t r, g, b, a;
} Color;

extern Color colors[];

Color* osccolors(const Color* colors);

class mpdamp : public QWidget {
    Q_OBJECT

public:
    mpdamp(QWidget *parent = nullptr);
    ~mpdamp();
    FFT fft;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void updateVisualizer();
    void updateElapsedTime(); // Slot for updating elapsed time
    void updateSongInfo();
    void updateMetadata();

private:
    void readPipe();
    void processBuffer();
    void updateScrollPosition();
    QString formatTime(double seconds); // Convert seconds to MM:SS
    QString songTitle;
    QString bitrate_info;
    QString sample_rate_info;

    int scrollPosition;
    QTimer *scrollTimer;
    QString truncatedSongTitle;
    QString originalSongTitle;

    int pipeFd;
    unsigned int current_song_id;
    short buffer[BSZ]; // Adjust if needed to match actual buffer size
    double fftData[BSZ];
    QImage backgroundImage;
    QImage fontImage; // Declare fontImage here
    QImage bitmaptextImage;
    QImage titleBarImage;
    QTimer *visualizerTimer;
    Visualizer::VisualizerWidget *visualizerWidget;
    NumTextWidget *numTextWidget;
    QTimer *timeTimer; // Timer for updating elapsed time
    QTimer *metadataTimer;

    ssize_t read_count;

    // MPD connection
    struct mpd_connection *conn;

    float in_wavedata[BSZ];
    float out_spectraldata[1152];

    // Title bar variables
    bool isActive;
    bool dragging;
    QPoint dragStartPosition;

    BitmapTextWidget *songTextWidget;
    BitmapTextWidget *kHzWidget;
    BitmapTextWidget *bitrateWidget;
};

double c_weighting(double freq);

#endif // MPDAMP_H
