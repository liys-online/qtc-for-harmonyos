#include "harmonyrunner.h"

#include "harmonyconfigurations.h"
#include "harmonydevice.h"
#include "harmonylogcategories.h"
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

// ---------------------------------------------------------------------------
// hilog severity → output format
// Typical hilog line (OpenHarmony):
//   "MM-DD HH:MM:SS.mmm  <pid>  <tid> <Level> <domain>/<Tag>: <message>"
// where <Level> is one of D I W E F.
// We scan positions 4-6 of the whitespace-split parts.
// ---------------------------------------------------------------------------
static OutputFormat hilogLineFormat(const QString &line)
{
    const QStringList parts = line.split(u' ', Qt::SkipEmptyParts);
    for (int i = 3; i < qMin(int(parts.size()), 8); ++i) {
        const QString &p = parts.at(i);
        QChar sc;
        if (p.size() == 1)
            sc = p.at(0);
        else if (p.size() >= 2 && p.at(1) == u'/')
            sc = p.at(0);
        else
            continue;
        switch (sc.toLatin1()) {
        case 'E': case 'F': return ErrorMessageFormat;
        case 'W':           return StdErrFormat;
        case 'D': case 'I': return LogMessageFormat;
        default:            break;
        }
    }
    return LogMessageFormat;
}

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
    qCDebug(harmonyRunLog) << "Registering Harmony process run worker factory.";
    static HarmonyProcessRunnerFactory theHarmonyRunWorkerFactory;
}

// ---------------------------------------------------------------------------
// P2-14: hilog streaming RunWorker
// Attaches an auxiliary `hdc shell hilog [filter]` process to the run control
// and forwards each output line to the Application Output panel.
// This factory does NOT set setExecutionType(), so Qt Creator treats it as a
// supplementary worker whose exit does not stop the primary run control.
// ---------------------------------------------------------------------------

namespace {

class HarmonyHilogRunWorkerFactory final : public RunWorkerFactory
{
public:
    HarmonyHilogRunWorkerFactory()
    {
        setId("Harmony.HilogReader");
        setRecipeProducer([](RunControl *runControl) {
            using namespace QtTaskTree;

            // --- read hilogEnabled ---
            bool enabled = true;
            {
                const Store sd = runControl->settingsData(Id(Constants::HARMONY_HILOG_ENABLED));
                for (auto it = sd.constBegin(); it != sd.constEnd(); ++it) {
                    if (it.value().isValid()) { enabled = it.value().toBool(); break; }
                }
            }

            // --- read optional filter string ---
            QString filter;
            {
                const Store sd = runControl->settingsData(Id(Constants::HARMONY_HILOG_FILTER));
                for (auto it = sd.constBegin(); it != sd.constEnd(); ++it) {
                    if (it.value().canConvert<QString>()) {
                        filter = it.value().toString().trimmed();
                        break;
                    }
                }
            }

            // --- resolve device serial ---
            const BuildConfiguration *bc = runControl->buildConfiguration();
            QString serial;
            if (bc) {
                serial = deviceSerialNumber(bc);
                if (serial.isEmpty()) {
                    if (Kit *kit = bc->kit()) {
                        if (const IDevice::ConstPtr dev = RunDeviceKitAspect::device(kit))
                            serial = HarmonyDevice::harmonyDeviceInfoFromDevice(dev).serialNumber;
                    }
                }
            }
            const FilePath hdc = HarmonyConfig::hdcToolPath();

            // All condition checks are deferred into the processTask setup so that the outer
            // lambda has a single return path (required for compiler return-type deduction).
            // Note: serial may be empty here if no deploy was done in the current session.
            // In that case we omit "-t <serial>" and let hdc target the default device,
            // matching the same behaviour as the main HarmonyProcessRunnerFactory.
            const bool canStream = enabled && hdc.isExecutableFile();

            return runControl->processRecipe(runControl->processTask(
                [runControl, canStream, hdc, serial, filter](Process &process) {
                    if (!canStream) {
                        if (!hdc.isExecutableFile())
                            qCDebug(harmonyRunLog) << "hilog reader skipped: hdc not found at" << hdc;
                        return SetupResult::StopWithSuccess;
                    }

                    // Build: hdc [-t <serial>] shell hilog [filter]
                    QStringList args;
                    if (!serial.isEmpty())
                        args = hdcSelector(serial);
                    args << QStringLiteral("shell") << QStringLiteral("hilog");
                    if (!filter.isEmpty())
                        args << filter;
                    process.setCommand(CommandLine{hdc, args});

                    // Stream stdout lines with severity-aware colouring.
                    process.setStdOutCallback([runControl](const QString &line) {
                        runControl->postMessage(line, hilogLineFormat(line));
                    });

                    // hilog writes everything to stdout; forward stderr as errors just in case.
                    process.setStdErrCallback([runControl](const QString &line) {
                        runControl->postMessage(line, ErrorMessageFormat);
                    });

                    runControl->postMessage(Tr::tr("Harmony: hilog streaming started."),
                                            NormalMessageFormat);
                    return SetupResult::Continue;
                }));
        });
        addSupportedRunMode(ProjectExplorer::Constants::NORMAL_RUN_MODE);
        setSupportedRunConfigs({Ohos::Constants::HARMONY_RUNCONFIG_ID});
    }
};

} // namespace

void setupHarmonyHilogWorker()
{
    qCDebug(harmonyRunLog) << "Registering Harmony hilog reader worker factory.";
    static HarmonyHilogRunWorkerFactory theHarmonyHilogRunWorkerFactory;
}

} // namespace Ohos::Internal
