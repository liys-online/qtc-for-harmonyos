#include "usbmonitor.h"

#include <QLoggingCategory>
#include <QTimer>

Q_LOGGING_CATEGORY(harmonyUsbMonitorLog, "qtc.harmony.device.usbmonitor", QtWarningMsg)

#if defined(Q_OS_WIN)
#  include <QThread>
#  include <qt_windows.h>
#  include <dbt.h>
#elif defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
#  include <QFileSystemWatcher>
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

// ── Private data ──────────────────────────────────────────────────────────
class UsbMonitorPrivate
{
public:
    bool    running       = false;
    QTimer *debounceTimer = nullptr;
    static UsbMonitor *m_instance;

#if defined(Q_OS_WIN)
    WinUsbMonitorThread *winThread = nullptr;
#elif defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    QFileSystemWatcher *watcher = nullptr;
#endif
};

UsbMonitor *UsbMonitorPrivate::m_instance = nullptr;

// ── Singleton ─────────────────────────────────────────────────────────────
UsbMonitor *UsbMonitor::instance()
{
    return UsbMonitorPrivate::m_instance
               ? UsbMonitorPrivate::m_instance
               : new UsbMonitor;
}

void UsbMonitor::destroy()
{
    if (UsbMonitorPrivate::m_instance) {
        delete UsbMonitorPrivate::m_instance;
        UsbMonitorPrivate::m_instance = nullptr;
    }
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
        inst->m_p->winThread = new WinUsbMonitorThread(inst);
        inst->m_p->winThread->start();
        qCDebug(harmonyUsbMonitorLog) << "UsbMonitor: Win32 monitor thread started.";
    }
#elif defined(Q_OS_LINUX)
    if (!inst->m_p->watcher) {
        inst->m_p->watcher = new QFileSystemWatcher(inst);
        const QString usbPath = QStringLiteral("/sys/bus/usb/devices");
        if (inst->m_p->watcher->addPath(usbPath)) {
            QObject::connect(inst->m_p->watcher, &QFileSystemWatcher::directoryChanged,
                             inst, &UsbMonitor::onUsbEvent);
            qCDebug(harmonyUsbMonitorLog) << "UsbMonitor: watching" << usbPath;
        } else {
            qCWarning(harmonyUsbMonitorLog) << "UsbMonitor: failed to watch" << usbPath;
        }
    }
#elif defined(Q_OS_MACOS)
    if (!inst->m_p->watcher) {
        inst->m_p->watcher = new QFileSystemWatcher(inst);
        const QString devPath = QStringLiteral("/dev");
        if (inst->m_p->watcher->addPath(devPath)) {
            QObject::connect(inst->m_p->watcher, &QFileSystemWatcher::directoryChanged,
                             inst, &UsbMonitor::onUsbEvent);
            qCDebug(harmonyUsbMonitorLog) << "UsbMonitor: watching" << devPath;
        } else {
            qCWarning(harmonyUsbMonitorLog) << "UsbMonitor: failed to watch" << devPath;
        }
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
        delete inst->m_p->winThread;
        inst->m_p->winThread = nullptr;
    }
#elif defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    if (inst->m_p->watcher) {
        delete inst->m_p->watcher;
        inst->m_p->watcher = nullptr;
    }
#endif
}

// ── Constructor / Destructor ──────────────────────────────────────────────
UsbMonitor::UsbMonitor(QObject *parent)
    : QObject(parent)
    , m_p(new UsbMonitorPrivate)
{
    m_p->m_instance   = this;
    m_p->debounceTimer = new QTimer(this);
    m_p->debounceTimer->setSingleShot(true);
    connect(m_p->debounceTimer, &QTimer::timeout, this, &UsbMonitor::usbDeviceChanged);
}

UsbMonitor::~UsbMonitor()
{
    qCDebug(harmonyUsbMonitorLog) << "UsbMonitor destructor called.";
    if (m_p->running)
        stopMonitor();
    delete m_p;
}

