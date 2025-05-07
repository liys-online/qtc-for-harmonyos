#include <QtCore/QCoreApplication>
#include <QtCore/qglobal.h>
#include <QTimer>
#include "usbmonitor/usbmonitor.h"
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    UsbMonitor::startMonitor();
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, UsbMonitor::instance(), &UsbMonitor::destroy);
    QObject::connect(UsbMonitor::instance(), &UsbMonitor::usbDeviceAdded, [](const QString &deviceDescription) {
        qDebug() << "USB device added:" << deviceDescription;
    });

    QTimer::singleShot(5000, [] {
        QCoreApplication::quit();
    });
    return a.exec();
}
