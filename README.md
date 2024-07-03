MPDamp
======

mpd client that resembles Winamp Classic accurately

Forked off of sofuture's mpdvz for the mere reason of getting audio data from mpd to visualize in a Winamp-like visualizer

![screenshot](https://raw.githubusercontent.com/0x5066/MPDamp/master/screenshot.png)

instructions
------------

put this in your mpd.conf:

    audio_output {
        type    "fifo"
        name    "my_fifo"
        path    "/tmp/mpd.fifo"
        format  "44100:16:1"
    }

build (and optionally run):

    make && ./mpdamp

run:

    ./mpdamp [path to mpd fifo]

    path defaults to /tmp/mpd.fifo

etc
---

inspiration and initial code from: http://stackoverflow.com/questions/21762412/mpd-fifo-python-audioop-arduino-and-voltmeter-faking-a-vu-meter
