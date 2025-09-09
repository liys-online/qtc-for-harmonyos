#include <QtCore/QCoreApplication>
#include <QtCore/qglobal.h>
#include <QTimer>
#include "usbmonitor/usbmonitor.h"
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    UsbMonitor::startMonitor();
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, UsbMonitor::instance(), &UsbMonitor::destroy);
    QObject::connect(UsbMonitor::instance(), &UsbMonitor::usbDeviceAdded, qApp, [](const QString &deviceDescription) {
        qDebug() << "USB device added:" << deviceDescription;
    });
    QObject::connect(UsbMonitor::instance(), &UsbMonitor::usbDeviceRemoved, qApp, [](const QString &deviceDescription) {
        qDebug() << "USB device removed:" << deviceDescription;
    });
    QObject::connect(UsbMonitor::instance(), &UsbMonitor::usbDeviceChanged, qApp, [](const QString &deviceDescription) {
        qDebug() << "USB device changed:" << deviceDescription;
    });
    QTimer::singleShot(10000, qApp, [] {
        QCoreApplication::quit();
    });
    return a.exec();
}
