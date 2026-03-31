#include <QtCore/QCoreApplication>
#include <QtCore/qglobal.h>
#include <QTimer>
#include <QDebug>

#include "usbmonitor/usbmonitor.h"
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    UsbMonitor::startMonitor();
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, UsbMonitor::instance(), &UsbMonitor::destroy);
    QObject::connect(UsbMonitor::instance(), &UsbMonitor::usbDeviceChanged, qApp, []() {
        qDebug() << "USB device changed";
    });
    QTimer::singleShot(10000, qApp, [] {
        QCoreApplication::quit();
    });
    return a.exec();
}
