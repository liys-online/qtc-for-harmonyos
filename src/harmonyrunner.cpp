#include "harmonyrunner.h"

#include "harmonyconfigurations.h"
#include "harmonydevice.h"
#include "harmonylogcategories.h"
#include "harmonyutils.h"
#include "hdcsocketclient.h"
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

            // ---- Hilog via direct hdc-daemon socket (P2-14) ----
            // Instead of spawning "hdc.exe shell hilog" as a subprocess (which
            // suffers from host-side pipe buffering), we open a TCP socket to the
            // hdc daemon (port 8710) and use the same binary protocol that DevEco
            // Studio uses.  TCP_NODELAY eliminates Nagle delay, giving us real-time
            // log delivery identical to DevEco.
            //
            // The QSyncTask completes immediately; the socket runs asynchronously
            // via Qt's event loop, parented to RunControl for lifetime management.
            auto hilogTask = QSyncTask(
                [runControl, canStream, hdc, serial, hilogFilter, bundleName] {
                    if (!canStream) {
                        if (!hdc.isExecutableFile())
                            qCDebug(harmonyRunLog)
                                << "hilog reader skipped: hdc not found at" << hdc;
                        return;
                    }

                    if (bundleName.isEmpty()) {
                        runControl->postMessage(
                            Tr::tr("Harmony: hilog skipped – bundle name unknown "
                                   "(cannot filter by PID)."),
                            ErrorMessageFormat);
                        return;
                    }

                    // POSIX shell script that polls pidof every 0.5 s (up to ~15 s)
                    // then exec's hilog -P <PID> for app-only output.
                    QString script;
                    script += QStringLiteral(
                        "PID=; i=0; "
                        "while [ $i -lt 30 ]; do "
                          "PID=$(pidof ");
                    script += bundleName;
                    script += QStringLiteral(
                        "); PID=${PID%% *}; "
                          "test x$PID != x && break; "
                          "PID=; i=$((i+1)); sleep 0.5; "
                        "done; "
                        "test x$PID != x || { echo 'hilog: PID not found for ");
                    script += bundleName;
                    script += QStringLiteral(
                        " after 15 s'; exit 1; }; "
                        "exec hilog -P $PID");
                    if (!hilogFilter.isEmpty()) {
                        script += u' ';
                        script += hilogFilter;
                    }

                    auto *client = new HdcSocketClient(runControl);
                    QObject::connect(client, &HdcSocketClient::lineReceived, runControl,
                        [runControl](const QString &line) {
                            runControl->postMessage(line, hilogLineFormat(line));
                        });
                    QObject::connect(client, &HdcSocketClient::errorOccurred, runControl,
                        [runControl](const QString &msg) {
                            runControl->postMessage(
                                Tr::tr("Harmony hilog socket: %1").arg(msg),
                                ErrorMessageFormat);
                        });
                    QObject::connect(runControl, &RunControl::stopped,
                                     client, &HdcSocketClient::stop);
                    QObject::connect(client, &HdcSocketClient::finished,
                                     client, &QObject::deleteLater);

                    client->start(serial, QStringLiteral("shell ") + script);

                    runControl->postMessage(
                        Tr::tr("Harmony: streaming hilog for %1 via hdc socket "
                               "(waiting for PID\u2026)")
                            .arg(bundleName),
                        NormalMessageFormat);
                });

            // Run main process and hilog socket concurrently.
            // hilogTask (QSyncTask) completes immediately after creating the
            // HdcSocketClient; the socket streams asynchronously until RunControl
            // emits stopped(), at which point HdcSocketClient::stop() closes it.
            //
            // FinishAllAndSuccess: group finishes when mainTask finishes.
            // hilogTask already completed (StopWithSuccess) so only mainTask
            // keeps the group alive.
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
