#include "harmonydevicemanager.h"

#include "harmonyconfigurations.h"
#include "harmonydevice.h"
#include "harmonyhdctargetsparser.h"
#include "harmonylogcategories.h"
#include "harmonyutils.h"
#include "hdcsocketclient.h"
#include "ohosconstants.h"
#include <projectexplorer/devicesupport/devicemanager.h>
#include <utils/qtcprocess.h>
#include <usbmonitor/usbmonitor.h>
#include <QRegularExpression>
#include <QTimer>

#include <utils/processenums.h>

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
static void handleErrLineOutput(const QString &error)
{
    qCDebug(harmonyDeviceLog) << "HDC device watcher error" << error;
}

static void handleDevicesListChange(const QString &serialNumber)
{
    DeviceManager *const devMgr = DeviceManager::instance();
    const HdcListTargetsParseResult parsed = parseHdcListTargetsLine(serialNumber);
    if (parsed.kind != HdcListTargetsLineKind::DeviceDataRow)
        return;
    IDevice::DeviceState state = ideviceStateFromHdc(
        hdcListTargetsStateToConnectionState(parsed.device.stateRaw));
    const QString &dirtySerial = parsed.device.serial;

    const Id id = Id(Constants::HARMONY_DEVICE_ID).withSuffix(":").withSuffix(dirtySerial);
    if (IDevice::ConstPtr dev = devMgr->find(id)) {
        devMgr->setDeviceState(id, state);
    }


    auto registerDevice = [devMgr, id, dirtySerial]() {
        QString deviceName = HarmonyConfig::getDeviceName(dirtySerial);
        IDevice::DeviceState state;
        if (!deviceName.isEmpty()) {
            state = IDevice::DeviceReadyToUse;
        } else {
            state = IDevice::DeviceDisconnected;
        }

        if (ipRegex.match(dirtySerial).hasMatch()) {
            deviceName += QLatin1String(" (WiFi)");
        }

        qCDebug(harmonyDeviceLog) << "Detected device:" << deviceName;
        const bool isEmulator = deviceName.startsWith(QStringLiteral("emulator"), Qt::CaseInsensitive)
                                || dirtySerial.contains(QStringLiteral("emulator"), Qt::CaseInsensitive);
        QString displayName = QString("%1[%2]").arg(deviceName, dirtySerial);
        if (isEmulator) {
            // TODO: Implement emulator support
            qCWarning(harmonyDeviceLog) << QString("The simulator is not yet supported \"%1\".")
                                             .arg(dirtySerial);
        }
        else{
            if (IDevice::ConstPtr dev = devMgr->find(id)) {
                devMgr->setDeviceState(id, state);
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
    };
    QTimer::singleShot(5000, registerDevice);
}
void HarmonyDeviceManager::queryDevice()
{
    qCDebug(harmonyDeviceLog) << "Querying HarmonyOS devices...";

    QStringList outLines;

    if (!harmonyHdcShellPreferCli()) {
        const HdcShellSyncResult sock = HdcSocketClient::runShellSync(
            QString(),
            QStringLiteral("list targets -v"),
            20000);
        if (sock.isOk()) {
            const QString blob = sock.standardOutput;
            for (QString line : blob.split(QRegularExpression(QStringLiteral("[\r\n]")),
                                          Qt::SkipEmptyParts)) {
                line = line.trimmed();
                if (!line.isEmpty())
                    outLines.append(line);
            }
        } else {
            qCDebug(harmonyDeviceLog) << "hdc list targets (socket) failed:" << int(sock.code)
                                      << sock.errorMessage;
        }
    }

    if (outLines.isEmpty()) {
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
        for (const QString &outLine : process.stdOutLines()) {
            if (!outLine.isEmpty())
                outLines.append(outLine);
        }
    }

    for (const QString &outLine : outLines) {
        if (!outLine.isEmpty())
            handleDevicesListChange(outLine);
    }
}

void HarmonyDeviceManager::setupDevicesWatcher()
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
