#include "harmonydevicemanager.h"

#include "harmonyconfigurations.h"
#include "harmonydevice.h"
#include "ohosconstants.h"
#include <projectexplorer/devicesupport/devicemanager.h>
#include <utils/qtcprocess.h>
#include <usbmonitor/usbmonitor.h>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QTimer>

#include <utils/processenums.h>

using namespace Utils;
using namespace ProjectExplorer;
namespace {
static Q_LOGGING_CATEGORY(harmonyDeviceLog, "qtc.harmony.harmonydevice", QtWarningMsg)
}
namespace Ohos::Internal {
static constexpr char ipRegexStr[] = "(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})";
static const auto ipRegex = QRegularExpression(ipRegexStr + QStringLiteral(":(\\d{1,5})"));
static constexpr char wifiDevicePort[] = "5555";
HarmonyDeviceManager::HarmonyDeviceManager(QObject *parent)
    : QObject{parent}
{}

HarmonyDeviceManager::~HarmonyDeviceManager()
{
    UsbMonitor::destroy();

}
static void handleErrLineOutput(const QString &error)
{
    qCDebug(harmonyDeviceLog) << "HDC device watcher error" << error;
}

static QStringList splitHdcTargetLine(const QString &line)
{
    QString s = line;
    s.remove('\r');
    s = s.trimmed();
    if (s.isEmpty())
        return {};
    // 旧版 hdc 多为制表符；新版常见多空格对齐
    if (s.contains(QLatin1Char('\t')))
        return s.split(QLatin1Char('\t'), Qt::SkipEmptyParts);
    static const QRegularExpression wsSplit(QStringLiteral("\\s+"));
    return s.split(wsSplit, Qt::SkipEmptyParts);
}

static bool looksLikeHdcListHeader(const QStringList &parts)
{
    if (parts.isEmpty())
        return true;
    const QString a = parts.constFirst();
    const QStringList headers{QStringLiteral("SN"),       QStringLiteral("Identifier"),
                              QStringLiteral("DEVICE"),    QStringLiteral("Device"),
                              QStringLiteral("DevSerial"), QStringLiteral("Serial")};
    for (const QString &h : headers) {
        if (a.compare(h, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

static void handleDevicesListChange(const QString &serialNumber)
{
    DeviceManager *const devMgr = DeviceManager::instance();
    const QStringList serialBits = splitHdcTargetLine(serialNumber);
    // 仅需：序列号 / 连接类型 / 状态（旧代码误要求 ≥5 列，导致新版 hdc 输出全部被丢弃）
    if (serialBits.size() < 3)
        return;
    if (looksLikeHdcListHeader(serialBits))
        return;

    QString dirtySerial = serialBits.constFirst();
    if (dirtySerial == QLatin1String("[Empty]") || dirtySerial.isEmpty()) {
        return;
    }
    if (serialBits.at(1) == QLatin1String("UART")) {
        return;
    }
    QString displayName = HarmonyConfig::getDeviceName(dirtySerial);
    if (displayName.contains(QLatin1String("[Fail][E000004]"))) {
        qCWarning(harmonyDeviceLog) << "Failed to query device list, retrying in 1 second.";
        QTimer::singleShot(1000, instance(), &HarmonyDeviceManager::queryDevice);
        return;
    }
    if (displayName.isEmpty()) {
        displayName = HarmonyConfig::getProductModel(dirtySerial);
        if (displayName.isEmpty())
            displayName = dirtySerial;
    }
    const QString stateStr = serialBits.at(2).trimmed();
    IDevice::DeviceState state;
    if (stateStr == QLatin1String("Connected") || stateStr == QStringLiteral("已连接"))
        state = IDevice::DeviceReadyToUse;
    else if (stateStr == QLatin1String("Offline") || stateStr == QStringLiteral("未连接")
             || stateStr == QStringLiteral("断开"))
        state = IDevice::DeviceConnected;
    else
        state = IDevice::DeviceDisconnected;
    qCDebug(harmonyDeviceLog) << "Detected device:" << displayName;
    const bool isEmulator = displayName.startsWith(QStringLiteral("emulator"), Qt::CaseInsensitive)
                            || dirtySerial.contains(QStringLiteral("emulator"), Qt::CaseInsensitive);
    if (isEmulator) {
        // TODO: Implement emulator support
        qCDebug(harmonyDeviceLog) << QString("The simulator is not yet supported \"%1\".")
                                         .arg(dirtySerial);
    }
    else{
        const Id id = Id(Constants::HARMONY_DEVICE_ID).withSuffix(":").withSuffix(dirtySerial);

        if (IDevice::ConstPtr dev = devMgr->find(id)) {
            if (displayName.isEmpty())
            {
                displayName = dev->displayName();
            }
            else
            {
                if (ipRegex.match(dirtySerial).hasMatch())
                {
                    displayName += QLatin1String(" (WiFi)");
                }
            }
            // DeviceManager doens't seem to have a way to directly update the name, if the name
            // of the device has changed, remove it and register it again with the new name.
            if (dev->displayName() == displayName)
                devMgr->setDeviceState(id, state);
            else
                devMgr->removeDevice(id);
        } else {
            HarmonyDevice *newDev = new HarmonyDevice();
            newDev->setupId(IDevice::AutoDetected, id);
            newDev->setDisplayName(displayName);
            newDev->setMachineType(IDevice::Hardware);
            newDev->setDeviceState(state);

            newDev->setExtraData(Constants::HarmonySerialNumber, dirtySerial);
            {
                const QString abiRaw = HarmonyConfig::getAbis(dirtySerial);
                QStringList abis = abiRaw.split(QRegularExpression(QStringLiteral("[,\\s]+")),
                                                Qt::SkipEmptyParts);
                for (QString &a : abis)
                    a = a.trimmed();
                newDev->setExtraData(Constants::HarmonyCpuAbi, abis);
            }
            newDev->setExtraData(Constants::HarmonySdk, HarmonyConfig::getSDKVersion(dirtySerial));
            qCDebug(harmonyDeviceLog, "Registering new Harmony device id \"%s\".", newDev->id().name().data());
            devMgr->addDevice(IDevice::Ptr(newDev));
        }
    }
}
void HarmonyDeviceManager::queryDevice()
{
    qCDebug(harmonyDeviceLog) << "Querying HarmonyOS devices...";
    const FilePath hdc = HarmonyConfig::hdcToolPath();
    if (!hdc.isExecutableFile())
        return;

    Process process;
    const CommandLine command{hdc, {QStringLiteral("list"), QStringLiteral("targets"),
                                    QStringLiteral("-v")}};
    process.setCommand(command);
    process.setWorkingDirectory(command.executable().parentDir());
    process.runBlocking();

    if (process.result() != ProcessResult::FinishedWithSuccess) {
        qCWarning(harmonyDeviceLog) << "hdc list targets failed:" << process.exitMessage();
    }

    for (const QString &errLine : process.stdErrLines())
        handleErrLineOutput(errLine);
    for (const QString &outLine : process.stdOutLines())
        handleDevicesListChange(outLine);
}

void HarmonyDeviceManager::setupDevicesWatcher()
{
    qCDebug(harmonyDeviceLog) << "Setting up HDC device watcher.";
    if (!HarmonyConfig::hdcToolPath().isExecutableFile()) {
        qCDebug(harmonyDeviceLog)
            << QStringLiteral(
                   "HDC device watcher not started: no hdc executable found. "
                   "Set default SDK or add OpenHarmony SDK paths in Harmony settings "
                   "(hdc is under <sdk>/<api>/toolchains or …/toolchains).");
        return;
    }
    if (UsbMonitor::instance()->isRunning()) {
        qCDebug(harmonyDeviceLog) << "HDC device watcher is already running.";
        return;
    }
    queryDevice();
    connect(UsbMonitor::instance(), &UsbMonitor::usbDeviceChanged, this, &HarmonyDeviceManager::queryDevice);
    UsbMonitor::startMonitor();
}
} // namespace Ohos::Internal
