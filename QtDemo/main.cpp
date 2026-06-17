// main.cpp — entry point for the Qt GUI demo.
#include <QApplication>
#include <QImage>
#include <QMetaType>

#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // QImage is delivered across the grab thread -> GUI thread boundary via a
    // queued signal connection, so register it as a metatype to be safe.
    qRegisterMetaType<QImage>("QImage");

    MainWindow w;
    w.resize(1000, 760);
    w.show();
    return app.exec();
}
