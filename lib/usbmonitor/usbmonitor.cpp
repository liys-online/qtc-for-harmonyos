#include "usbmonitor.h"

#include <QLoggingCategory>
#include <QTimer>

Q_LOGGING_CATEGORY(harmonyUsbMonitorLog, "qtc.harmony.device.usbmonitor", QtWarningMsg)

#if defined(Q_OS_WIN)
#  include <QThread>
#  include <qt_windows.h>
#  include <dbt.h>
#elif defined(Q_OS_LINUX)
#  include <QFileSystemWatcher>
#elif defined(Q_OS_MACOS)
#  include <CoreFoundation/CoreFoundation.h>
#  include <IOKit/IOKitLib.h>
#endif

// ── Windows: dedicated Win32 message-pump thread ──────────────────────────
// Qt's internal event-dispatcher window uses HWND_MESSAGE as its parent,
// making it a "message-only" window. Message-only windows do NOT receive
// WM_DEVICECHANGE broadcasts from the OS. We therefore create our own
// real top-level hidden window in a small background thread so that the
// broadcast reaches us reliably.
#if defined(Q_OS_WIN)
class WinUsbMonitorThread : public QThread
{
public:
    explicit WinUsbMonitorThread(QObject *notifyTarget)
        : m_notifyTarget(notifyTarget) {}

    void stop()
    {
        if (m_hwnd)
            PostMessage(m_hwnd, WM_QUIT, 0, 0);  // breaks GetMessage loop
        wait();
    }

protected:
    void run() override
    {
        static const wchar_t kClass[] = L"QtcHarmonyUsbMonitorWnd";

        HINSTANCE hi = GetModuleHandle(nullptr);

        WNDCLASSEX wc   = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = WndProc;
        wc.hInstance     = hi;
        wc.lpszClassName = kClass;
        // Ignore failure: may already be registered from a previous run.
        RegisterClassEx(&wc);

        // A WS_POPUP window with no parent is a real top-level window that
        // DOES receive WM_DEVICECHANGE broadcasts, unlike HWND_MESSAGE windows.
        m_hwnd = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                kClass, L"",
                                WS_POPUP,
                                0, 0, 0, 0,
                                nullptr, nullptr, hi, nullptr);
        if (!m_hwnd) {
            qCWarning(harmonyUsbMonitorLog)
                << "UsbMonitor: failed to create Win32 window, error"
                << GetLastError();
            UnregisterClass(kClass, hi);
            return;
        }

        // Store 'this' so the static WndProc can reach us.
        SetWindowLongPtr(m_hwnd, GWLP_USERDATA,
                         reinterpret_cast<LONG_PTR>(this));

        qCDebug(harmonyUsbMonitorLog)
            << "UsbMonitor: Win32 window created, pumping messages.";

        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0)) {
            // WM_QUIT posted by stop() exits the loop.
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
        UnregisterClass(kClass, hi);
    }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message,
                                    WPARAM wp, LPARAM lp)
    {
        if (message == WM_DEVICECHANGE
            && (wp == DBT_DEVNODES_CHANGED
                || wp == DBT_DEVICEARRIVAL
                || wp == DBT_DEVICEREMOVECOMPLETE))
        {
            auto *self = reinterpret_cast<WinUsbMonitorThread *>(
                GetWindowLongPtr(hwnd, GWLP_USERDATA));
            if (self && self->m_notifyTarget) {
                // Trigger the debounce timer in the main thread (Qt::QueuedConnection).
                QMetaObject::invokeMethod(self->m_notifyTarget,
                                          "onUsbEvent",
                                          Qt::QueuedConnection);
            }
        }
        return DefWindowProc(hwnd, message, wp, lp);
    }

    QObject *m_notifyTarget = nullptr;
    HWND     m_hwnd         = nullptr;
};
#endif // Q_OS_WIN

// ── macOS: IOKit USB notification thread ─────────────────────────────────
// QCoreApplication does NOT drive CFRunLoopGetMain() continuously, so we
// cannot rely on the main thread's CFRunLoop for IOKit notifications.
// Instead we spin up a dedicated background thread that owns its own
// CFRunLoop, exactly as WinUsbMonitorThread does on Windows.
#if defined(Q_OS_MACOS)
#  include <QMutex>
#  include <QThread>

static void macUsbDeviceCallback(void *refCon, io_iterator_t iterator)
{
    // Drain the iterator — required or future notifications won't fire.
    io_object_t obj;
    while ((obj = IOIteratorNext(iterator)) != 0)
        IOObjectRelease(obj);

    auto *target = static_cast<QObject *>(refCon);
    QMetaObject::invokeMethod(target, "onUsbEvent", Qt::QueuedConnection);
}

class MacUsbMonitorThread : public QThread
{
public:
    explicit MacUsbMonitorThread(QObject *notifyTarget)
        : m_notifyTarget(notifyTarget) {}

    void stop()
    {
        m_mutex.lock();
        CFRunLoopRef rl = m_runLoop;
        m_mutex.unlock();
        if (rl)
            CFRunLoopStop(rl);
        wait();
    }

protected:
    void run() override
    {
        IONotificationPortRef notifyPort = IONotificationPortCreate(0);
        if (!notifyPort) {
            qCWarning(harmonyUsbMonitorLog) << "UsbMonitor: IONotificationPortCreate failed";
            return;
        }

        CFRunLoopRef rl = CFRunLoopGetCurrent();
        CFRetain(rl);
        {
            QMutexLocker lk(&m_mutex);
            m_runLoop = rl;
        }

        CFRunLoopSourceRef src = IONotificationPortGetRunLoopSource(notifyPort);
        CFRunLoopAddSource(rl, src, kCFRunLoopDefaultMode);

        io_iterator_t addIter = 0, removeIter = 0;

        // "IOUSBDevice" matches both legacy IOUSBFamily and the
        // IOUSBHostFamily compatibility nub (macOS 10.12+).
        CFMutableDictionaryRef matchAdd = IOServiceMatching("IOUSBDevice");
        IOServiceAddMatchingNotification(notifyPort, kIOFirstMatchNotification,
                                         matchAdd, macUsbDeviceCallback,
                                         m_notifyTarget, &addIter);
        if (addIter) {
            io_object_t obj;
            while ((obj = IOIteratorNext(addIter)) != 0)
                IOObjectRelease(obj);
        }

        CFMutableDictionaryRef matchRemove = IOServiceMatching("IOUSBDevice");
        IOServiceAddMatchingNotification(notifyPort, kIOTerminatedNotification,
                                         matchRemove, macUsbDeviceCallback,
                                         m_notifyTarget, &removeIter);
        if (removeIter) {
            io_object_t obj;
            while ((obj = IOIteratorNext(removeIter)) != 0)
                IOObjectRelease(obj);
        }

        qCDebug(harmonyUsbMonitorLog) << "UsbMonitor: IOKit monitor thread running.";
        CFRunLoopRun(); // blocks until stop() calls CFRunLoopStop()

        if (addIter)    IOObjectRelease(addIter);
        if (removeIter) IOObjectRelease(removeIter);
        IONotificationPortDestroy(notifyPort);

        QMutexLocker lk(&m_mutex);
        CFRelease(m_runLoop);
        m_runLoop = nullptr;
    }

private:
    QObject     *m_notifyTarget = nullptr;
    QMutex       m_mutex;
    CFRunLoopRef m_runLoop      = nullptr;
};
#endif // Q_OS_MACOS

// ── Private data ──────────────────────────────────────────────────────────
class UsbMonitorPrivate
{
public:
    bool running = false;
    std::unique_ptr<QTimer> debounceTimer = nullptr;

#if defined(Q_OS_WIN)
    std::unique_ptr<WinUsbMonitorThread> winThread = nullptr;
#elif defined(Q_OS_LINUX)
    std::unique_ptr<QFileSystemWatcher> watcher = nullptr;
#elif defined(Q_OS_MACOS)
    std::unique_ptr<MacUsbMonitorThread> macThread = nullptr;
#endif
};

// ── Singleton ─────────────────────────────────────────────────────────────
UsbMonitor *UsbMonitor::instance()
{
    static UsbMonitor usbMonitor;
    return &usbMonitor;
}

bool UsbMonitor::isRunning() const
{
    return m_p->running;
}

// ── Slot called (in main thread) when any platform signals a USB change ───
void UsbMonitor::onUsbEvent()
{
    if (m_p->running)
        m_p->debounceTimer->start(300);
}

// ── Public control ────────────────────────────────────────────────────────
void UsbMonitor::startMonitor()
{
    UsbMonitor *inst = UsbMonitor::instance();
    if (inst->m_p->running)
        return;
    inst->m_p->running = true;

#if defined(Q_OS_WIN)
    if (!inst->m_p->winThread) {
        inst->m_p->winThread = std::make_unique<WinUsbMonitorThread>(inst);
        inst->m_p->winThread->start();
        qCDebug(harmonyUsbMonitorLog) << "UsbMonitor: Win32 monitor thread started.";
    }
#elif defined(Q_OS_LINUX)
    if (!inst->m_p->watcher) {
        inst->m_p->watcher = std::make_unique<QFileSystemWatcher>();
        const QString usbPath = QStringLiteral("/sys/bus/usb/devices");
        if (inst->m_p->watcher->addPath(usbPath)) {
            QObject::connect(inst->m_p->watcher.get(), &QFileSystemWatcher::directoryChanged,
                             inst, &UsbMonitor::onUsbEvent);
            qCDebug(harmonyUsbMonitorLog) << "UsbMonitor: watching" << usbPath;
        } else {
            qCWarning(harmonyUsbMonitorLog) << "UsbMonitor: failed to watch" << usbPath;
        }
    }
#elif defined(Q_OS_MACOS)
    if (!inst->m_p->macThread) {
        inst->m_p->macThread = std::make_unique<MacUsbMonitorThread>(inst);
        inst->m_p->macThread->start();
    }
#endif
}

void UsbMonitor::stopMonitor()
{
    UsbMonitor *inst = UsbMonitor::instance();
    inst->m_p->running = false;
    if (inst->m_p->debounceTimer)
        inst->m_p->debounceTimer->stop();

#if defined(Q_OS_WIN)
    if (inst->m_p->winThread) {
        inst->m_p->winThread->stop();
    }
#elif defined(Q_OS_MACOS)
    if (inst->m_p->macThread) {
        inst->m_p->macThread->stop();
    }
#endif
}

// ── Constructor / Destructor ──────────────────────────────────────────────
UsbMonitor::UsbMonitor(QObject *parent)
    : QObject(parent)
    , m_p(QSharedPointer<UsbMonitorPrivate>::create())
{
    m_p->debounceTimer = std::make_unique<QTimer>();
    m_p->debounceTimer->setSingleShot(true);
    connect(m_p->debounceTimer.get(), &QTimer::timeout, this, &UsbMonitor::usbDeviceChanged);
}

UsbMonitor::~UsbMonitor()
{
    qCDebug(harmonyUsbMonitorLog) << "UsbMonitor destructor called.";
    if (m_p->running)
        stopMonitor();
}

