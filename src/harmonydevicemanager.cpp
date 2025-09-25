#include "harmonydevicemanager.h"

#include "harmonyconfigurations.h"
#include "harmonydevice.h"
#include "ohosconstants.h"
#include <projectexplorer/devicesupport/devicemanager.h>
#include <coreplugin/messagemanager.h>
#include <utils/qtcprocess.h>
#include <usbmonitor/usbmonitor.h>
#include <QLoggingCategory>
#include <QTimer>
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
static void  handleErrLineOutput(const QString &error)
{
    qCDebug(harmonyDeviceLog) << "HDC device watcher error" << error;
}
static void handleDevicesListChange(const QString &serialNumber)
{
    DeviceManager *const devMgr = DeviceManager::instance();
    QString temp = serialNumber;
    temp.remove("\n").remove("\r");
    const QStringList serialBits = temp.split("\t", Qt::SplitBehaviorFlags::SkipEmptyParts);
    if (serialBits.isEmpty() || serialBits.size() < 5) {
        return;
    }
    QString dirtySerial = serialBits.first();
    if (dirtySerial == "[Empty]" || dirtySerial.isEmpty()) {
        return;
    }
    if (serialBits.at(1) == "UART") {
        return;
    }
    QString displayName = HarmonyConfig::getDeviceName(dirtySerial);
    if (displayName.contains("[Fail][E000004]")) {
        qDebug() << Q_FUNC_INFO << "Failed to query device list, retrying in 1 second.";
        QTimer::singleShot(1000, instance(), &HarmonyDeviceManager::queryDevice);
        return;
    }
    const QString stateStr = serialBits.at(2).trimmed();
    IDevice::DeviceState state;
    if (stateStr == "Connected")
        state = IDevice::DeviceReadyToUse;
    else if (stateStr == "Offline")
        state = IDevice::DeviceConnected;
    else
        state = IDevice::DeviceDisconnected;
    qDebug() << Q_FUNC_INFO << displayName;
    const bool isEmulator = displayName.startsWith("emulator");
    if (isEmulator) {
        // TODO: Implement emulator support
        qDebug() << Q_FUNC_INFO << QString("The simulator is not yet supported \"%1\".").arg(dirtySerial);
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
            newDev->setExtraData(Constants::HarmonyCpuAbi, HarmonyConfig::getAbis(dirtySerial));
            newDev->setExtraData(Constants::HarmonySdk, HarmonyConfig::getSDKVersion(dirtySerial));
            qCDebug(harmonyDeviceLog, "Registering new Harmony device id \"%s\".", newDev->id().name().data());
            devMgr->addDevice(IDevice::Ptr(newDev));
        }
    }
}
void HarmonyDeviceManager::queryDevice()
{
    Core::MessageManager::writeSilently(tr("Querying HarmonyOS devices..."));
    Process process;
    const CommandLine command{HarmonyConfig::hdcToolPath(), {"list", "targets", "-v"}};
    process.setCommand(command);
    process.setWorkingDirectory(command.executable().parentDir());
    process.setStdErrLineCallback(handleErrLineOutput);
    process.setStdOutLineCallback(handleDevicesListChange);
    process.runBlocking();
}

void HarmonyDeviceManager::setupDevicesWatcher()
{
    qDebug() << Q_FUNC_INFO;
    if (!HarmonyConfig::hdcToolPath().exists())
    {
        Core::MessageManager::writeSilently("Cannot start HDC device watcher, because hdc path does not exist.");
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
