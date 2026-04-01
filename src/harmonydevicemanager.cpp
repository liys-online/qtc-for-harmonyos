#include "harmonydevicemanager.h"

#include "harmonyconfigurations.h"
#include "harmonydevice.h"
#include "harmonyhdctargetsparser.h"
#include "harmonylogcategories.h"
#include "harmonyutils.h"
#include "hdcsocketclient.h"
#include "ohosconstants.h"
#include <projectexplorer/devicesupport/devicemanager.h>
#include <usbmonitor/usbmonitor.h>
#include <QRegularExpression>
#include <QTimer>

using namespace Utils;
using namespace ProjectExplorer;

namespace Ohos::Internal {
static constexpr char ipRegexStr[] = "(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})";
static const auto ipRegex = QRegularExpression(ipRegexStr + QStringLiteral(":(\\d{1,5})"));

static IDevice::DeviceState ideviceStateFromHdc(HdcTargetConnectionState s)
{
    switch (s) {
    case HdcTargetConnectionState::ReadyToUse:
        return IDevice::DeviceReadyToUse;
    case HdcTargetConnectionState::ConnectedNotReady:
        return IDevice::DeviceConnected;
    case HdcTargetConnectionState::Disconnected:
        break;
    }
    return IDevice::DeviceDisconnected;
}

HarmonyDeviceManager::HarmonyDeviceManager(QObject *parent)
    : QObject{parent}
{}

HarmonyDeviceManager::~HarmonyDeviceManager()
{
    UsbMonitor::destroy();

}
static void registerHarmonyDevice(DeviceManager *devMgr, const Id &id, const QString &dirtySerial) // NOSONAR (cpp:S3578) - non-const DeviceManager methods called
{
    QString deviceName = HarmonyConfig::getDeviceName(dirtySerial);
    const IDevice::DeviceState state = deviceName.isEmpty()
                                           ? IDevice::DeviceDisconnected
                                           : IDevice::DeviceReadyToUse;

    if (ipRegex.match(dirtySerial).hasMatch())
        deviceName += QLatin1String(" (WiFi)");

    qCDebug(harmonyDeviceLog) << "Detected device:" << deviceName;
    const bool isEmulator = deviceName.startsWith(QStringLiteral("emulator"), Qt::CaseInsensitive)
                            || dirtySerial.contains(QStringLiteral("emulator"), Qt::CaseInsensitive);
    const QString displayName = QString("%1[%2]").arg(deviceName, dirtySerial);
    if (isEmulator) {
        /* ** TODO: 实现模拟器支持 */ // NOSONAR (cpp:S1135)
        qCWarning(harmonyDeviceLog) << QString("The simulator is not yet supported \"%1\".")
                                         .arg(dirtySerial);
        return;
    }
    if (IDevice::ConstPtr dev = devMgr->find(id)) { // NOSONAR (cpp:S5296) - find() is non-static
        devMgr->setDeviceState(id, state); // NOSONAR (cpp:S5296)
    } else {
        if(state == IDevice::DeviceDisconnected) {
            qCWarning(harmonyDeviceLog)
                << QString("Device \"%1\" is not ready to use. It will be registered when it becomes ready.")
                .arg(dirtySerial);
            return;
        }
        IDevice::Ptr newDev = std::make_shared<HarmonyDevice>();
        newDev->setupId(IDevice::AutoDetected, id);
        newDev->setDisplayName(displayName);
        newDev->setMachineType(IDevice::Hardware);
        newDev->setDeviceState(state);

        newDev->setExtraData(Constants::HarmonySerialNumber, dirtySerial);
        newDev->setExtraData(Constants::HarmonyCpuAbi, HarmonyConfig::getAbis(dirtySerial));
        newDev->setExtraData(Constants::HarmonySdk, HarmonyConfig::getSDKVersion(dirtySerial));
        qCDebug(harmonyDeviceLog, "Registering new Harmony device id \"%s\".", newDev->id().name().data());
        devMgr->addDevice(newDev); // NOSONAR (cpp:S5296)
    }
}
static void handleDevicesListChange(const QString &serialNumber)
{
    DeviceManager *const devMgr = DeviceManager::instance(); // NOSONAR (cpp:S3578) - non-const methods are called on devMgr
    const HdcListTargetsParseResult parsed = parseHdcListTargetsLine(serialNumber);
    if (parsed.kind != HdcListTargetsLineKind::DeviceDataRow)
        return;
    IDevice::DeviceState state = ideviceStateFromHdc(
        hdcListTargetsStateToConnectionState(parsed.device.stateRaw));
    const QString &dirtySerial = parsed.device.serial;

    const Id id = Id(Constants::HARMONY_DEVICE_ID).withSuffix(":").withSuffix(dirtySerial);
    if (IDevice::ConstPtr dev = devMgr->find(id)) { // NOSONAR (cpp:S5296)
        devMgr->setDeviceState(id, state); // NOSONAR (cpp:S5296)
    }

    QTimer::singleShot(5000, [devMgr, id, dirtySerial]() {
        registerHarmonyDevice(devMgr, id, dirtySerial);
    });
}
void HarmonyDeviceManager::queryDevice() const
{
    qCDebug(harmonyDeviceLog) << "Querying HarmonyOS devices...";

    const FilePath hdc = HarmonyConfig::hdcToolPath();
    const HdcShellSyncResult r = HdcSocketClient::runSyncWithCliFallback(
        QString(),
        QStringLiteral("list targets -v"),
        hdc.toUserOutput(),
        {QStringLiteral("list"), QStringLiteral("targets"), QStringLiteral("-v")},
        20000);

    if (!r.isOk())
        qCWarning(harmonyDeviceLog) << "hdc list targets failed:" << r.errorMessage;

    QStringList outLines;
    for (QString line : r.standardOutput.split(QRegularExpression(QStringLiteral("[\r\n]")),
                                              Qt::SkipEmptyParts)) {
        line = line.trimmed();
        if (!line.isEmpty())
            outLines.append(line);
    }

    for (const QString &outLine : outLines) {
        if (!outLine.isEmpty())
            handleDevicesListChange(outLine);
    }
}

void HarmonyDeviceManager::setupDevicesWatcher() const
{
    qCDebug(harmonyDeviceLog) << "Setting up HDC device watcher.";
    if (!HarmonyConfig::hdcToolPath().isExecutableFile() && harmonyHdcShellPreferCli()) {
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
