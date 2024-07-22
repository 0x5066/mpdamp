#ifndef MPDAMP_H
#define MPDAMP_H

#include <QWidget>
#include <QImage>
#include <QTimer>
#include <fftw3.h>
#include <mpd/client.h>

const static int BSZ = 1152;
const static int BSZ2 = BSZ / 2;

typedef struct {
    uint8_t r, g, b, a;
} Color;

extern Color colors[];

Color* osccolors(const Color* colors);

class MPDAmp : public QWidget {
    Q_OBJECT

public:
    MPDAmp(QWidget *parent = nullptr);
    ~MPDAmp();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void updateVisualizer();
    void updateElapsedTime(); // Slot for updating elapsed time

private:
    void readPipe();
    void processBuffer();
    void drawVisualizer(QPainter *painter);
    void drawNumText(QPainter *painter);
    QString formatTime(double seconds); // Convert seconds to MM:SS

    int pipeFd;
    short buffer[BSZ]; // Adjust if needed to match actual buffer size
    double fftData[BSZ];
    QImage backgroundImage;
    QImage fontImage; // Declare fontImage here
    QTimer *visualizerTimer;
    QTimer *timeTimer; // Timer for updating elapsed time

    ssize_t read_count;

    // MPD connection
    struct mpd_connection *conn;

    // FFT variables
    fftw_plan p;
    fftw_complex *in, *out;
};

#endif // MPDAMP_H
