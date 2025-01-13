#include "harmonydevicemanager.h"

#include "harmonyconfigurations.h"
#include <utils/qtcprocess.h>
#include <QLoggingCategory>
using namespace Utils;
namespace {
static Q_LOGGING_CATEGORY(harmonyDeviceLog, "qtc.harmony.harmonydevice", QtWarningMsg)
}
namespace Ohos::Internal {
HarmonyDeviceManager::HarmonyDeviceManager(QObject *parent)
    : QObject{parent}
{}

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
    process.setStdOutCallback([](const QString &output) {
        // handleDevicesListChange(output);
        qDebug() << Q_FUNC_INFO <<output;
    });
    process.runBlocking();
    // const bool success = process.result() == ProcessResult::FinishedWithSuccess;
    // qDebug() << "Command finshed (sync):" << command.toUserOutput()
    //                           << "Success:" << success
    //                           << "Output:" << process.allRawOutput();

    // };
}
} // namespace Ohos::Internal
