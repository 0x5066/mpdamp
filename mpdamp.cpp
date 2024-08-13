#include "mpdamp.h"
#include "visualizer.h"

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

    visualizerWidget = new Visualizer::VisualizerWidget(this);
    visualizerWidget->setGeometry(24, 43, 75, 16); // Position and size as needed

    numTextWidget = new NumTextWidget(this);
    numTextWidget->setGeometry(36, 26, 100, 13); // Adjust the position and size as needed

    songTextWidget = new BitmapTextWidget(this);
    songTextWidget->setFontImage(bitmaptextImage);
    songTextWidget->setGeometry(111, 27, 200, 20); // Adjust the position and size as needed

    kHzWidget = new BitmapTextWidget(this);
    kHzWidget->setFontImage(bitmaptextImage);
    kHzWidget->setGeometry(156, 43, 100, 20); // Adjust the position and size as needed

    bitrateWidget = new BitmapTextWidget(this);
    bitrateWidget->setFontImage(bitmaptextImage);
    bitrateWidget->setGeometry(111, 43, 100, 20); // Adjust the position and size as needed

    // Set the window size to the size of the image
    setFixedSize(backgroundImage.size());
    setWindowFlags(Qt::FramelessWindowHint);

    // Open the pipe for reading
    pipeFd = open("/tmp/mpd.fifo", O_RDONLY | O_NONBLOCK);
    if (pipeFd == -1) {
        perror("open");
    }

    // Step 1: Initialize FFT
    fft;
    int samples_in = BSZ / 2;
    int samples_out = BSZ/4;
    fft.Init(samples_in, samples_out, 1, 1.0f);

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

    scrollTimer = new QTimer(this);
    connect(scrollTimer, &QTimer::timeout, this, &mpdamp::updateScrollPosition);
    scrollTimer->start(350); // Adjust the interval as needed (e.g., 100 ms)

    // Track focus changes
    setFocusPolicy(Qt::StrongFocus);
    installEventFilter(this);
    songTextWidget->setText(originalSongTitle.left(31));
}

void mpdamp::updateScrollPosition() {
    QString longTitle = QString(originalSongTitle) + " *** " + originalSongTitle.left(30);
    if (longTitle.length() > 31) {
        scrollPosition = (scrollPosition + 1) % (longTitle.length() - 31 + 1);
        QString truncatedTitle = longTitle.mid(scrollPosition, 31);
        songTextWidget->setText(truncatedTitle);
    } else {
        songTextWidget->setText(originalSongTitle.left(31));
    }
}

mpdamp::~mpdamp() {
    if (pipeFd != -1) {
        ::close(pipeFd);
    }
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
        originalSongTitle = ("mpdamp 0.0");
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
        originalSongTitle = QString("%1. %2 - %3 (%4)").arg(QString::number(current_song_id)).arg(QString::fromUtf8(artist)).arg(QString::fromUtf8(title)).arg(QString::fromUtf8(format_time2(total_time, time_str2)));
    } else if (title) {
        originalSongTitle = QString("%1. %2 (%3)").arg(QString::number(current_song_id)).arg(QString::fromUtf8(title)).arg(QString::fromUtf8(format_time2(total_time, time_str2)));
    } else {
        originalSongTitle = QString("%1. %2 (%3)").arg(QString::number(current_song_id)).arg(QString::fromUtf8(filename.c_str())).arg(QString::fromUtf8(format_time2(total_time, time_str2)));
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
            sample_rate_info = QString(" %1").arg(sample_rate / 1000, 0, 'f', 0);
        } else if (sample_rate / 1000 > 100) {
            QString sample_rate_str = QString::number(sample_rate / 1000, 'f', 0);
            sample_rate_info = sample_rate_str.mid(1, 3);
        } else {
            sample_rate_info = QString::number(sample_rate / 1000, 'f', 0);
        }
    } else {
        if (res == 1 || res == 3) {
            sample_rate_info = " 0";
        }
    }

    mpd_status_free(status2);
    mpd_song_free(current_song);
}

void mpdamp::updateVisualizer() {
    readPipe();
    processBuffer();
    visualizerWidget->setData(buffer, out_spectraldata, BSZ);
}

void mpdamp::updateElapsedTime() {
    update();
}

void mpdamp::updateMetadata() {
    updateSongInfo();
    QString elapsedTime = formatTime(mpd_status_get_elapsed_time(mpd_run_status(conn)));
    numTextWidget->setParameters(elapsedTime, 1, 0); // Update with appropriate parameters
    //songTextWidget->setText(truncatedSongTitle);
    kHzWidget->setText(sample_rate_info);
    bitrateWidget->setText(bitrate_info);
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

void mpdamp::processBuffer() {
    for (int i = 0; i < 576; i++) {
        in_wavedata[i] = ((float)buffer[i] / 4096);
    }
    fft.time_to_frequency_domain(in_wavedata, out_spectraldata);
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