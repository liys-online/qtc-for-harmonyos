#include "usbmonitor.h"
#include <QDebug>
#include <libusb.h>
#include <QTimer>
class UsbMonitorPrivate
{
public:
    UsbMonitorPrivate() : ctx(nullptr)
    {
        if (libusb_init(&ctx) != LIBUSB_SUCCESS) {
            qWarning() << "Failed to initialize libusb!";
            ctx = nullptr;
        }
    }
    ~UsbMonitorPrivate()
    {
        if (ctx) {
            libusb_exit(ctx);
        }
    }
    libusb_context *ctx;
    int blank = 100;
    static UsbMonitor *m_instance;
};
UsbMonitor *UsbMonitorPrivate::m_instance = nullptr;
UsbMonitor *UsbMonitor::instance()
{
    UsbMonitorPrivate::m_instance = UsbMonitorPrivate::m_instance ? UsbMonitorPrivate::m_instance : new UsbMonitor;
    return UsbMonitorPrivate::m_instance;
}

void UsbMonitor::destroy()
{
    if (UsbMonitorPrivate::m_instance) {
        delete UsbMonitorPrivate::m_instance;
        UsbMonitorPrivate::m_instance = nullptr;
    }
}

void UsbMonitor::setBlank(int blank)
{
    m_p->blank = blank;
}

void UsbMonitor::startMonitor()
{
    UsbMonitor::instance()->start();
}

void UsbMonitor::stopMonitor()
{
    UsbMonitor::instance()->quit();
    UsbMonitor::instance()->wait();
}

UsbMonitor::UsbMonitor(QObject *parent)
    : QThread(parent),
    m_p(new UsbMonitorPrivate)
{

}

UsbMonitor::~UsbMonitor()
{
    delete m_p;
    if(isRunning()) {
        quit();
        wait();
    }
}

void UsbMonitor::run()
{
    if (!m_p->ctx) return;

    QSet<QString> previousDevices;

    while (true) {
        QSet<QString> currentDevices;
        libusb_device **devices;
        ssize_t deviceCount = libusb_get_device_list(m_p->ctx, &devices);

        for (ssize_t i = 0; i < deviceCount; ++i)
        {
            libusb_device_descriptor desc;
            if (libusb_get_device_descriptor(devices[i], &desc) == 0)
            {
                QString deviceInfo = QString("Vendor ID: %1, Product ID: %2")
                .arg(desc.idVendor, 4, 16, QChar('0'))
                    .arg(desc.idProduct, 4, 16, QChar('0'));
                currentDevices.insert(deviceInfo);
            }
        }

        for (const auto &device : currentDevices) {
            if (!previousDevices.contains(device)) {
                QTimer::singleShot(3000, this, [this, device] {
                    emit usbDeviceChanged(device);
                    emit usbDeviceAdded(device);
                });
            }
        }

        for (const auto &device : previousDevices) {
            if (!currentDevices.contains(device)) {
                QTimer::singleShot(3000, this, [this, device] {
                    emit usbDeviceChanged(device);
                    emit usbDeviceRemoved(device);
                });
            }
        }

        previousDevices = currentDevices;
        libusb_free_device_list(devices, 1);

        QThread::msleep(m_p->blank);
    }
}
