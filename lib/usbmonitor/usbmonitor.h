#ifndef USBMONITOR_H
#define USBMONITOR_H

#include <QObject>

class UsbMonitorPrivate;
class UsbMonitor : public QObject
{
    Q_OBJECT
public:
    static UsbMonitor *instance();
    bool isRunning() const;
    static void startMonitor();
    static void stopMonitor();

signals:
    void usbDeviceChanged();

private:
    explicit UsbMonitor(QObject *parent = nullptr);
    ~UsbMonitor();
    Q_INVOKABLE void onUsbEvent();   // called cross-thread via QMetaObject::invokeMethod
    std::unique_ptr<UsbMonitorPrivate> m_p = nullptr;
};

#endif // USBMONITOR_H
