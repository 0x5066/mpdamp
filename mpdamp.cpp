#include "mpdamp.h"
#include "visualizer.h"
#include "draw_numtext.h"
#include "bitmaptext.h"

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

mpdamp::mpdamp(QWidget *parent)
    : QWidget(parent) {
    // Load the background image
    backgroundImage.load("../main.bmp");
    fontImage.load("../numbers.bmp");
    titleBarImage.load("../titlebar.bmp");
    bitmaptextImage.load("../text.bmp");

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
    connect(visualizerTimer, &QTimer::timeout, this, &mpdamp::updateVisualizer);
    visualizerTimer->start(16); // Approximately 60 fps

    // init and start new timer
    metadataTimer = new QTimer(this);
    connect(metadataTimer, &QTimer::timeout, this, &mpdamp::updateMetadata);
    metadataTimer->start(16); 

    // init mpd connection
    conn = mpd_connection_new(NULL, 0, 0);
    if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
        fprintf(stderr, "Error connecting to MPD: %s\n", mpd_connection_get_error_message(conn));
        mpd_connection_free(conn);
    }

    // Track focus changes
    setFocusPolicy(Qt::StrongFocus);
    installEventFilter(this);
}

mpdamp::~mpdamp() {
    if (pipeFd != -1) {
        ::close(pipeFd);
    }
    fftw_destroy_plan(p);
    fftw_free(in);
    fftw_free(out);
    if (conn) mpd_connection_free(conn);
}

bool mpdamp::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::FocusIn) {
        isActive = true;
        update();
    } else if (event->type() == QEvent::FocusOut) {
        isActive = false;
        update();
    }
    return QWidget::eventFilter(obj, event);
}

void mpdamp::paintEvent(QPaintEvent *event) {
    QPainter painter(this);

    // Draw the background image
    painter.drawImage(0, 0, backgroundImage);

    // Draw the title bar
    QRect titleBarRect = isActive ? QRect(27, 0, 275, 14) : QRect(27, 15, 275, 14);
    painter.drawImage(0, 0, titleBarImage, titleBarRect.x(), titleBarRect.y(), titleBarRect.width(), titleBarRect.height());

    // Draw the visualizer
    drawVisualizer(&painter);

    // Draw the number text
    drawNumText(&painter);

    drawSongText(&painter);
    drawKHz(&painter);
    drawBitrate(&painter);
}

void mpdamp::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && event->pos().y() <= 14) { // Only drag if clicked within the title bar area
        dragging = true;
        dragStartPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}

void mpdamp::mouseMoveEvent(QMouseEvent *event) {
    if (dragging) {
        move(event->globalPos() - dragStartPosition);
        event->accept();
    }
}

void mpdamp::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        dragging = false;
        event->accept();
    }
}

std::string getFilenameFromURI(const std::string& uri) {
    // Find the last slash in the URI
    size_t lastSlashPos = uri.find_last_of('/');
    if (lastSlashPos != std::string::npos) {
        // Extract the filename from the URI
        return uri.substr(lastSlashPos + 1);
    }
    // If no slash is found, return the original URI (unlikely case)
    return uri;
}

const char *format_time2(unsigned int time, char *buffer) {
    unsigned int minutes = time / 60;
    unsigned int seconds = time % 60;
    sprintf(buffer, "%u:%02u", minutes, seconds);

    return buffer;
}

unsigned int getCurrentSongID(mpd_connection *conn) {
    struct mpd_status *status = mpd_run_status(conn);
    unsigned int song_id = mpd_status_get_song_id(status);
    mpd_status_free(status);
    return song_id;
}

void mpdamp::updateSongInfo() {
    int res = 1;
    char time_str2[6];
    current_song_id = getCurrentSongID(conn);
    if (!conn) return;

    struct mpd_song *current_song = mpd_run_current_song(conn);
    if (!current_song) {
        songTitle = ("No current song");
        bitrate_info = ("  0");
        sample_rate_info = (" 0");
        return;
    }

    const char *title = mpd_song_get_tag(current_song, MPD_TAG_TITLE, 0);
    const char *artist = mpd_song_get_tag(current_song, MPD_TAG_ARTIST, 0);
    const char *album = mpd_song_get_tag(current_song, MPD_TAG_ALBUM, 0);
    const char *track = mpd_song_get_tag(current_song, MPD_TAG_UNKNOWN, 0);
    std::string filename = getFilenameFromURI(mpd_song_get_uri(current_song));

    struct mpd_status *status2 = mpd_run_status(conn);
    if (!status2) {
        mpd_song_free(current_song);
        return;
    }

    float total_time = mpd_status_get_total_time(status2);

    int bitrate = mpd_status_get_kbit_rate(status2);
    const struct mpd_audio_format *audio_format = mpd_status_get_audio_format(status2);
    int sample_rate = (audio_format != nullptr) ? audio_format->sample_rate : 0;
    int channels = (audio_format != nullptr) ? audio_format->channels : 0;

    if (title && artist) {
        songTitle = QString("%1. %2 - %3 (%4)").arg(QString::number(current_song_id)).arg(QString::fromUtf8(artist)).arg(QString::fromUtf8(title)).arg(QString::fromUtf8(format_time2(total_time, time_str2)));
    } else if (title) {
        songTitle = QString("%1. %2 (%3)").arg(QString::number(current_song_id)).arg(QString::fromUtf8(title)).arg(QString::fromUtf8(format_time2(total_time, time_str2)));
    } else {
        songTitle = QString("%1. %2 (%3)").arg(QString::number(current_song_id)).arg(QString::fromUtf8(filename.c_str())).arg(QString::fromUtf8(format_time2(total_time, time_str2)));
    }

    if (bitrate > 0) {
        if (bitrate < 10) {
            bitrate_info = QString("  %1").arg(bitrate);
        } else if (bitrate < 100) {
            bitrate_info = QString(" %1").arg(bitrate);
        } else if (bitrate > 1000) {
            QString bitrate_str = QString::number(bitrate);
            bitrate_info = bitrate_str.mid(0, 2) + "H";
        } else {
            bitrate_info = QString::number(bitrate);
        }
    } else {
        if (res == 1 || res == 3) {
            bitrate_info = "  0";
        }
    }

    if (sample_rate > 0) {
        if (sample_rate < 10) {
            sample_rate_info = QString(" %1").arg(sample_rate / 1000.0, 0, 'f', 1);
        } else if (sample_rate / 1000 > 100) {
            QString sample_rate_str = QString::number(sample_rate / 1000.0, 'f', 1);
            sample_rate_info = sample_rate_str.mid(1, 3);
        } else {
            sample_rate_info = QString::number(sample_rate / 1000.0, 'f', 1);
        }
    } else {
        if (res == 1 || res == 3) {
            sample_rate_info = " 0";
        }
    }

    mpd_status_free(status2);
    mpd_song_free(current_song);
}

void mpdamp::drawVisualizer(QPainter *painter) {
    painter->save();

    // Translate the painter to the visualization position
    painter->translate(24, 43);

    // Set the clipping region to 76x16
    painter->setClipRect(0, 0, 76, 16);

    // Render the visualization
    Visualizer::render(painter, buffer, fftData, BSZ);

    painter->restore();
}

void mpdamp::drawNumText(QPainter *painter) {
    painter->save();

    // Translate the painter to the desired text position
    painter->translate(36, 26); // Adjust position as needed

    // Draw the number text
    draw_numtext(painter, &fontImage, formatTime(mpd_status_get_elapsed_time(mpd_run_status(conn))), 0, 0, 1, 0);

    painter->restore();
}

void mpdamp::drawSongText(QPainter *painter) {
    painter->save();

    painter->translate(111, 27);

    painter->setClipRect(0, 0, 154, 6);

    draw_bitmaptext(painter, &bitmaptextImage, songTitle, 0, 0);

    painter->restore();
}

void mpdamp::drawKHz(QPainter *painter) {
    painter->save();

    painter->translate(156, 43);

    painter->setClipRect(0, 0, 10, 6);

    draw_bitmaptext(painter, &bitmaptextImage, sample_rate_info, 0, 0);

    painter->restore();
}

void mpdamp::drawBitrate(QPainter *painter) {
    painter->save();

    painter->translate(111, 43);

    painter->setClipRect(0, 0, 15, 6);

    draw_bitmaptext(painter, &bitmaptextImage, bitrate_info, 0, 0);

    painter->restore();
}

void mpdamp::updateVisualizer() {
    readPipe();
    processBuffer();
    update();
}

void mpdamp::updateElapsedTime() {
    update();
}

void mpdamp::updateMetadata() {
    updateSongInfo();
    update();
}

void mpdamp::readPipe() {
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

void mpdamp::processBuffer() {
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

QString mpdamp::formatTime(double seconds) {
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