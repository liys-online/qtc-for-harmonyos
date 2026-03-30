#ifndef USBMONITOR_H
#define USBMONITOR_H

#include <QObject>

class UsbMonitorPrivate;
class UsbMonitor : public QObject
{
    Q_OBJECT
public:
    static UsbMonitor *instance();
    static void destroy();
    bool isRunning() const;
    static void startMonitor();
    static void stopMonitor();

signals:
    void usbDeviceChanged();

private:
    explicit UsbMonitor(QObject *parent = nullptr);
    ~UsbMonitor();
    Q_INVOKABLE void onUsbEvent();   // called cross-thread via QMetaObject::invokeMethod
    UsbMonitorPrivate * const m_p;
};

#endif // USBMONITOR_H
