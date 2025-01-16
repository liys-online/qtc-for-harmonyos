#ifndef USBMONITOR_H
#define USBMONITOR_H

#include <QThread>

class UsbMonitorPrivate;
class UsbMonitor : public QThread
{
    Q_OBJECT
public:
    static UsbMonitor *instance();
    static void destroy();
    void setBlank(int blank);
    static void startMonitor();
    static void stopMonitor();
protected:
    void run() override;

signals:
    void usbDeviceAdded(const QString &deviceDescription);
    void usbDeviceRemoved(const QString &deviceDescription);
    void usbDeviceChanged(const QString &deviceDescription);

private:
    explicit UsbMonitor(QObject *parent = nullptr);
    ~UsbMonitor();
    UsbMonitorPrivate * const m_p;
};

#endif // USBMONITOR_H
