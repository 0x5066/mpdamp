#ifndef MPDAMP_H
#define MPDAMP_H

#include <QWidget>
#include <QImage>
#include <QTimer>
#include <QMouseEvent>
#include <fftw3.h>
#include <mpd/client.h>

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
    void drawVisualizer(QPainter *painter);
    void drawNumText(QPainter *painter);
    void drawSongText(QPainter *painter);
    void drawKHz(QPainter *painter);
    void drawBitrate(QPainter *painter);
    QString formatTime(double seconds); // Convert seconds to MM:SS
    QString songTitle;
    QString bitrate_info;
    QString sample_rate_info;

    int pipeFd;
    unsigned int current_song_id;
    short buffer[BSZ]; // Adjust if needed to match actual buffer size
    double fftData[BSZ];
    QImage backgroundImage;
    QImage fontImage; // Declare fontImage here
    QImage bitmaptextImage;
    QImage titleBarImage;
    QTimer *visualizerTimer;
    QTimer *timeTimer; // Timer for updating elapsed time
    QTimer *metadataTimer;

    ssize_t read_count;

    // MPD connection
    struct mpd_connection *conn;

    // FFT variables
    fftw_plan p;
    fftw_complex *in, *out;

    // Title bar variables
    bool isActive;
    bool dragging;
    QPoint dragStartPosition;
};

#endif // MPDAMP_H
