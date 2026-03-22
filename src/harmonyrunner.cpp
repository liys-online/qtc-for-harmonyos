#include "harmonyrunner.h"

#include "harmonyconfigurations.h"
#include "harmonydevice.h"
#include "harmonyutils.h"
#include "ohosconstants.h"
#include "ohostr.h"

#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/devicesupport/devicekitaspects.h>
#include <projectexplorer/devicesupport/idevice.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/runcontrol.h>

#include <utils/commandline.h>
#include <utils/id.h>
#include <utils/outputformat.h>
#include <utils/processenums.h>
#include <utils/qtcassert.h>
#include <utils/qtcprocess.h>
#include <utils/store.h>

#include <QtTaskTree/QTaskTree>

using namespace ProjectExplorer;
using namespace Utils;

namespace Ohos::Internal {

namespace {

QStringList postQuitShellCommandsFromRunControl(const RunControl *runControl)
{
    if (!runControl)
        return {};
    const Store sd = runControl->settingsData(Id(Constants::HARMONY_POSTFINISHSHELLCMDLIST));
    QStringList lines;
    for (auto it = sd.constBegin(); it != sd.constEnd(); ++it) {
        const QVariant &v = it.value();
        if (v.canConvert<QStringList>())
            lines = v.toStringList();
        else if (v.canConvert<QString>())
            lines = v.toString().split('\n');
        if (!lines.isEmpty())
            break;
    }
    QStringList out;
    for (QString line : std::as_const(lines)) {
        line = line.trimmed();
        if (!line.isEmpty())
            out.append(line);
    }
    return out;
}

QString deviceSerialForRun(RunControl *runControl, const BuildConfiguration *bc)
{
    QString serial = deviceSerialNumber(bc);
    if (!serial.isEmpty())
        return serial;
    if (Kit *kit = bc->kit()) {
        if (const IDevice::ConstPtr dev = RunDeviceKitAspect::device(kit))
            serial = HarmonyDevice::harmonyDeviceInfoFromDevice(dev).serialNumber;
    }
    return serial;
}

void runPostQuitShellCommandsOnDevice(RunControl *runControl)
{
    QTC_ASSERT(runControl, return);

    const QStringList cmds = postQuitShellCommandsFromRunControl(runControl);
    if (cmds.isEmpty())
        return;

    BuildConfiguration *bc = runControl->buildConfiguration();
    if (!bc)
        return;

    const QString serial = deviceSerialForRun(runControl, bc);
    if (serial.isEmpty()) {
        runControl->postMessage(
            Tr::tr("Skipping post-quit on-device shell commands: no device serial."),
            ErrorMessageFormat);
        return;
    }

    const FilePath hdc = HarmonyConfig::hdcToolPath();
    if (!hdc.isExecutableFile()) {
        runControl->postMessage(
            Tr::tr("Skipping post-quit on-device shell commands: hdc executable not found."),
            ErrorMessageFormat);
        return;
    }

    runControl->postMessage(Tr::tr("Running post-quit on-device shell commands…"), NormalMessageFormat);

    for (const QString &oneCmd : cmds) {
        QStringList args = hdcSelector(serial);
        args << QStringLiteral("shell") << oneCmd;

        Process process;
        process.setCommand(CommandLine{hdc, args});
        process.setWorkingDirectory(hdc.parentDir());
        process.runBlocking(std::chrono::seconds(60));

        if (process.result() == ProcessResult::FinishedWithSuccess) {
            runControl->postMessage(
                Tr::tr("Post-quit command finished: %1").arg(oneCmd), NormalMessageFormat);
        } else {
            runControl->postMessage(
                Tr::tr("Post-quit command failed: %1 — %2")
                    .arg(oneCmd, process.exitMessage()),
                ErrorMessageFormat);
        }
    }
}

void stopHarmonyApplicationOnDevice(RunControl *runControl)
{
    QTC_ASSERT(runControl, return);

    BuildConfiguration *bc = runControl->buildConfiguration();
    if (!bc)
        return;

    const QString pkg = packageName(bc);
    if (pkg.isEmpty()) {
        runControl->postMessage(
            Tr::tr("Cannot stop application on device: bundle name is unknown."),
            ErrorMessageFormat);
        return;
    }

    const QString serial = deviceSerialForRun(runControl, bc);
    if (serial.isEmpty()) {
        runControl->postMessage(
            Tr::tr("Cannot stop application on device: no device serial (deploy or select a run device)."),
            ErrorMessageFormat);
        return;
    }

    const FilePath hdc = HarmonyConfig::hdcToolPath();
    if (!hdc.isExecutableFile()) {
        runControl->postMessage(Tr::tr("Cannot stop application on device: hdc executable not found."),
                                ErrorMessageFormat);
        return;
    }

    QStringList args = hdcSelector(serial);
    args << QStringLiteral("shell") << QStringLiteral("aa") << QStringLiteral("force-stop") << pkg;

    Process process;
    process.setCommand(CommandLine{hdc, args});
    process.setWorkingDirectory(hdc.parentDir());
    process.runBlocking(std::chrono::seconds(15));

    if (process.result() == ProcessResult::FinishedWithSuccess) {
        runControl->postMessage(
            Tr::tr("Requested force-stop on device for \"%1\".").arg(pkg),
            NormalMessageFormat);
    } else {
        runControl->postMessage(
            Tr::tr("force-stop failed for \"%1\": %2").arg(pkg, process.exitMessage()),
            ErrorMessageFormat);
    }
}

class HarmonyProcessRunnerFactory final : public RunWorkerFactory
{
public:
    HarmonyProcessRunnerFactory()
    {
        setId("Harmony.ProcessRunner");
        setRecipeProducer([](RunControl *runControl) {
            return runControl->processRecipe(runControl->processTask(
                [runControl](Process &process) {
                    // New lambda each run: UniqueConnection would not dedupe; clear our receiver first.
                    QObject::disconnect(runControl, &RunControl::canceled, runControl, nullptr);
                    QObject::connect(runControl,
                                     &RunControl::canceled,
                                     runControl,
                                     [runControl] { stopHarmonyApplicationOnDevice(runControl); });
                    // After the hdc session ends (Stop or error), run post-quit device commands like Android.
                    QObject::connect(&process,
                                     &Process::done,
                                     runControl,
                                     [runControl] { runPostQuitShellCommandsOnDevice(runControl); },
                                     Qt::SingleShotConnection);
                    return QtTaskTree::SetupResult::Continue;
                }));
        });
        addSupportedRunMode(ProjectExplorer::Constants::NORMAL_RUN_MODE);
        setSupportedRunConfigs({Ohos::Constants::HARMONY_RUNCONFIG_ID});
        setExecutionType(ProjectExplorer::Constants::STDPROCESS_EXECUTION_TYPE_ID);
    }
};

} // namespace

void setupHarmonyRunWorker()
{
    static HarmonyProcessRunnerFactory theHarmonyRunWorkerFactory;
}

} // namespace Ohos::Internal
