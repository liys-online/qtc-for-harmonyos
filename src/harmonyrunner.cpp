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
#include <QtTaskTree/qbarriertask.h>

using namespace ProjectExplorer;
using namespace Utils;

namespace Ohos::Internal {

// ---------------------------------------------------------------------------
// hilog severity → output format
// Typical hilog line (OpenHarmony):
//   "MM-DD HH:MM:SS.mmm  <pid>  <tid> <Level> <domain>/<Tag>: <message>"
// where <Level> is one of D I W E F.
// We scan positions 4-6 of the whitespace-split parts.
//
// Colour mapping (Application Output pane):
//   D  → StdOutFormat       (normal foreground – unobtrusive)
//   I  → StdOutFormat       (normal foreground – most readable)
//   W  → LogMessageFormat   (amber / yellow in most themes)
//   E/F→ ErrorMessageFormat  (red)
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
        case 'W':           return LogMessageFormat;
        case 'D': case 'I': return StdOutFormat;
        default:            break;
        }
    }
    return StdOutFormat;
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

// ---------------------------------------------------------------------------
// P2-14: helper – read a bool/string aspect value from RunControl settings.
// ---------------------------------------------------------------------------
static bool readBoolSetting(RunControl *rc, Id key, bool defaultVal)
{
    const Store sd = rc->settingsData(key);
    for (auto it = sd.constBegin(); it != sd.constEnd(); ++it) {
        if (it.value().isValid())
            return it.value().toBool();
    }
    return defaultVal;
}

static QString readStringSetting(RunControl *rc, Id key)
{
    const Store sd = rc->settingsData(key);
    for (auto it = sd.constBegin(); it != sd.constEnd(); ++it) {
        if (it.value().canConvert<QString>())
            return it.value().toString().trimmed();
    }
    return {};
}

// ---------------------------------------------------------------------------
// P2-14: resolve device serial for hilog (same two-step logic as the main runner).
// If serial is empty we omit "-t" and let hdc choose the default device,
// matching the main runner which also runs without "-t".
// ---------------------------------------------------------------------------
static QString resolveSerial(RunControl *rc)
{
    const BuildConfiguration *bc = rc->buildConfiguration();
    if (!bc)
        return {};
    QString serial = deviceSerialNumber(bc);
    if (serial.isEmpty()) {
        if (Kit *kit = bc->kit()) {
            if (const IDevice::ConstPtr dev = RunDeviceKitAspect::device(kit))
                serial = HarmonyDevice::harmonyDeviceInfoFromDevice(dev).serialNumber;
        }
    }
    return serial;
}

class HarmonyProcessRunnerFactory final : public RunWorkerFactory
{
public:
    HarmonyProcessRunnerFactory()
    {
        setId("Harmony.ProcessRunner");
        setRecipeProducer([](RunControl *runControl) {
            using namespace QtTaskTree;

            // ---- Main process task ----
            // Mirrors the original single-factory recipe: cancel → aa force-stop,
            // done → post-quit shell commands.
            auto mainTask = runControl->processTask(
                [runControl](Process &process) {
                    // Clear any stale handler from a previous run before reconnecting.
                    QObject::disconnect(runControl, &RunControl::canceled, runControl, nullptr);
                    QObject::connect(runControl,
                                     &RunControl::canceled,
                                     runControl,
                                     [runControl] { stopHarmonyApplicationOnDevice(runControl); });
                    QObject::connect(&process,
                                     &Process::done,
                                     runControl,
                                     [runControl] { runPostQuitShellCommandsOnDevice(runControl); },
                                     Qt::SingleShotConnection);
                    return SetupResult::Continue;
                });

            // ---- Hilog sidecar task (P2-14) ----
            const bool hilogEnabled =
                readBoolSetting(runControl, Id(Constants::HARMONY_HILOG_ENABLED), true);
            const QString hilogFilter =
                readStringSetting(runControl, Id(Constants::HARMONY_HILOG_FILTER));
            const QString serial  = resolveSerial(runControl);
            const FilePath hdc    = HarmonyConfig::hdcToolPath();
            const bool canStream  = hilogEnabled && hdc.isExecutableFile();

            // Resolve bundle name for PID-based filtering.
            // Without PID filtering, hilog dumps the entire device log (thousands
            // of lines/sec) and overwhelms the UI event loop, freezing Qt Creator.
            QString bundleName;
            {
                QString ov = readStringSetting(
                    runControl, Id(Constants::HARMONY_RUN_BUNDLE_OVERRIDE));
                if (!ov.isEmpty()) {
                    bundleName = ov;
                } else if (const BuildConfiguration *bc = runControl->buildConfiguration()) {
                    bundleName = packageName(bc);
                }
            }

            auto hilogTask = runControl->processTask(
                [runControl, canStream, hdc, serial, hilogFilter, bundleName](Process &process) {
                    if (!canStream) {
                        if (!hdc.isExecutableFile())
                            qCDebug(harmonyRunLog)
                                << "hilog reader skipped: hdc not found at" << hdc;
                        return SetupResult::StopWithSuccess;
                    }

                    if (bundleName.isEmpty()) {
                        runControl->postMessage(
                            Tr::tr("Harmony: hilog skipped – bundle name unknown "
                                   "(cannot filter by PID)."),
                            ErrorMessageFormat);
                        return SetupResult::StopWithSuccess;
                    }

                    // Build a POSIX shell script that:
                    //   1. Polls pidof <bundle> up to 15 s, waiting for the app to start.
                    //   2. Runs  hilog -P <PID> [user_filter]  once the PID is known.
                    // The script deliberately avoids double-quote characters so that
                    // Windows → QProcess → hdc.exe argument quoting stays reliable.
                    QString script;
                    script += QStringLiteral(
                        "PID=; i=0; "
                        "while [ $i -lt 15 ]; do "
                          "PID=$(pidof ");
                    script += bundleName;
                    script += QStringLiteral(
                        "); PID=${PID%% *}; "
                          "test x$PID != x && break; "
                          "PID=; i=$((i+1)); sleep 1; "
                        "done; "
                        "test x$PID != x && exec hilog -P $PID");
                    if (!hilogFilter.isEmpty()) {
                        script += u' ';
                        script += hilogFilter;
                    }
                    script += QStringLiteral(
                        " || echo 'hilog: PID not found for ");
                    script += bundleName;
                    script += QStringLiteral(" after 15 attempts'");

                    CommandLine cmd{hdc};
                    if (!serial.isEmpty())
                        cmd.addArgs(hdcSelector(serial));
                    cmd.addArg("shell");
                    cmd.addArg(script);
                    process.setCommand(cmd);

                    // Device-side hilog output is always UTF-8.
                    process.setUtf8Codec();

                    // Per-line callback with severity-based colouring.
                    // PID filtering reduces volume to app-only logs (typically
                    // 1-50 lines/s), safe for per-line postMessage calls.
                    process.setStdOutLineCallback([runControl](const QString &line) {
                        runControl->postMessage(line, hilogLineFormat(line));
                    });

                    process.setStdErrLineCallback([runControl](const QString &line) {
                        runControl->postMessage(line, ErrorMessageFormat);
                    });

                    runControl->postMessage(
                        Tr::tr("Harmony: streaming hilog for %1 (waiting for PID…)")
                            .arg(bundleName),
                        NormalMessageFormat);
                    return SetupResult::Continue;
                });

            // Run main and hilog concurrently.
            // Qt Creator's createMainRecipe() uses exactly ONE factory per run-mode/config
            // combination, so there can only be one RunWorkerFactory here. Both processes
            // are therefore embedded in the same recipe Group.
            //
            // FinishAllAndSuccess:
            //   • Hilog exits early (disabled / hdc missing) → main keeps running.
            //   • Main exits (user Stop → both hdc processes killed via setupCanceler) →
            //     hilog also exits → group finishes.
            //   • Device disconnects → both hdc processes fail → group finishes.
            return Group{
                parallel,
                workflowPolicy(WorkflowPolicy::FinishAllAndSuccess),
                When(mainTask, &Process::started) >> Do {
                    QSyncTask([runControl] { runControl->reportStarted(); })
                },
                hilogTask
            };
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

} // namespace Ohos::Internal
