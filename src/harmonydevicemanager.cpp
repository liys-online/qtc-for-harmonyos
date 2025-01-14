#include "harmonydevicemanager.h"

#include "harmonyconfigurations.h"
#include "harmonydevice.h"
#include "ohosconstants.h"
#include <projectexplorer/devicesupport/devicemanager.h>
#include <utils/qtcprocess.h>
#include <QLoggingCategory>
using namespace Utils;
using namespace ProjectExplorer;
namespace {
static Q_LOGGING_CATEGORY(harmonyDeviceLog, "qtc.harmony.harmonydevice", QtWarningMsg)
}
namespace Ohos::Internal {
static constexpr char ipRegexStr[] = "(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})";
static const QRegularExpression ipRegex = QRegularExpression(ipRegexStr);
static constexpr char wifiDevicePort[] = "5555";
HarmonyDeviceManager::HarmonyDeviceManager(QObject *parent)
    : QObject{parent}
{}
static void handleDevicesListChange(const QString &serialNumber)
{
    DeviceManager *const devMgr = DeviceManager::instance();
    QString serial = serialNumber;
    serial.remove("\n").remove("\t");
    qDebug() << Q_FUNC_INFO << serial;
    if (serial == "[Empty]") {
        return;
    }
    QString displayName = HarmonyConfig::getDeviceName(serial);
    qDebug() << Q_FUNC_INFO << displayName;
    const bool isEmulator = displayName.startsWith("emulator");
    if (isEmulator) {
        // TODO: Implement emulator support
        qDebug() << Q_FUNC_INFO << QString("The simulator is not yet supported \"%1\".").arg(serial);
    }
    else{
        const Id id = Id(Constants::HARMONY_DEVICE_ID).withSuffix(":").withSuffix(serial);
        static const auto ipRegex = QRegularExpression(ipRegexStr + QStringLiteral(":(\\d{1,5})"));
        if (ipRegex.match(serial).hasMatch())
            displayName += QLatin1String(" (WiFi)");
        if (IDevice::ConstPtr dev = devMgr->find(id)) {
            // DeviceManager doens't seem to have a way to directly update the name, if the name
            // of the device has changed, remove it and register it again with the new name.
            if (dev->displayName() == displayName)
                devMgr->setDeviceState(id, IDevice::DeviceReadyToUse);
            else
                devMgr->removeDevice(id);
        } else {
            HarmonyDevice *newDev = new HarmonyDevice();
            newDev->setupId(IDevice::AutoDetected, id);
            newDev->setDisplayName(displayName);
            newDev->setMachineType(IDevice::Hardware);
            newDev->setDeviceState(IDevice::DeviceReadyToUse);

            newDev->setExtraData(Constants::HarmonySerialNumber, serial);
            newDev->setExtraData(Constants::HarmonyCpuAbi, HarmonyConfig::getAbis(serial));
            newDev->setExtraData(Constants::HarmonySdk, HarmonyConfig::getSDKVersion(serial));
            qCDebug(harmonyDeviceLog, "Registering new Harmony device id \"%s\".",
                    newDev->id().toString().toUtf8().data());
            devMgr->addDevice(IDevice::ConstPtr(newDev));
        }
    }
}
void HarmonyDeviceManager::setupDevicesWatcher()
{
    qDebug() << Q_FUNC_INFO;
    if (!HarmonyConfig::hdcToolPath().exists())
    {
        qCDebug(harmonyDeviceLog) << "Cannot start HDC device watcher"
                                  <<  "because hdc path does not exist.";
        return;
    }
    if (m_hdcDeviceWatcherRunner.isRunning()) {
        qCDebug(harmonyDeviceLog) << "HDC device watcher is already running.";
        return;
    }

    // const auto onSetup = [](Process &process) {
    Process process;
    const CommandLine command{HarmonyConfig::hdcToolPath(), {"list", "targets"}};
    process.setCommand(command);
    // process.runBlocking(30s, EventLoopMode::On);
    process.setWorkingDirectory(command.executable().parentDir());
    // process.setEnvironment(AndroidConfig::toolsEnvironment());
    process.setStdErrLineCallback([](const QString &error) {
        qCDebug(harmonyDeviceLog) << "HDC device watcher error" << error; });
    process.setStdOutLineCallback(handleDevicesListChange);
    process.runBlocking();
    // const bool success = process.result() == ProcessResult::FinishedWithSuccess;
    // qDebug() << "Command finshed (sync):" << command.toUserOutput()
    //                           << "Success:" << success
    //                           << "Output:" << process.allRawOutput();

    // };
}
} // namespace Ohos::Internal
