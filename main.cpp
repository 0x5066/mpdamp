#include <QApplication>
#include "mpdamp.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    MPDAmp window;
    window.show();

    return app.exec();
}
