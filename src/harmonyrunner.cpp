#include "harmonyrunner.h"

#include "harmonyconfigurations.h"
#include "harmonydevice.h"
#include "harmonylogcategories.h"
#include "harmonymainrunsockettask.h"
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
#include <QtTaskTree/qtasktree.h>

#include <chrono>
#include <functional>

using namespace ProjectExplorer;
using namespace Utils;

namespace Ohos::Internal {

/*
** hilog 严重性 → 输出格式
** 典型 hilog 行格式（OpenHarmony）：
**   "MM-DD HH:MM:SS.mmm  <pid>  <tid> <Level> <domain>/<Tag>: <message>"
** 其中 <Level> 为 D I W E F 之一。
** 扫描空格分割后的第 4–6 个部分。
**
** 颜色映射（应用输出面板）：
**   D  → StdOutFormat（正常前景—不显眼）
**   I  → StdOutFormat（正常前景—最易阅读）
**   W  → LogMessageFormat（大多数主题中为璐色/黄色）
**   E/F → ErrorMessageFormat（红色）
*/
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

struct HdcShellOutcome {
    bool ok = false;
    QString errorDetail;
};

/*
** P2-15 阶段二：socket 优先 + hdc.exe 回退已实现于 HdcSocketClient::runSyncWithCliFallback。
*/
static HdcShellOutcome runHdcShellSocketThenCli(RunControl *runControl,
                                                const QString &serial,
                                                const QString &shellLine,
                                                std::chrono::seconds timeout)
{
    HdcShellOutcome out;
    const QString wireCmd = QStringLiteral("shell ") + shellLine;
    const FilePath hdc = HarmonyConfig::hdcToolPath();
    QStringList args = hdcSelector(serial);
    args << QStringLiteral("shell") << shellLine;
    std::function<void(const QString &)> notifier;
    if (runControl) {
        notifier = [runControl](const QString &m) {
            runControl->postMessage(m, NormalMessageFormat);
        };
    }
    const HdcShellSyncResult r = HdcSocketClient::runSyncWithCliFallback(
        serial,
        wireCmd,
        hdc.toUserOutput(),
        args,
        int(timeout.count() * 1000),
        notifier,
        {});
    out.ok = r.isOk();
    out.errorDetail = r.isOk() ? QString() : r.errorMessage;
    return out;
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

    if (harmonyHdcShellPreferCli()) {
        const FilePath hdc = HarmonyConfig::hdcToolPath();
        if (!hdc.isExecutableFile()) {
            runControl->postMessage(
                Tr::tr("Skipping post-quit on-device shell commands: hdc executable not found."),
                ErrorMessageFormat);
            return;
        }
    }

    runControl->postMessage(Tr::tr("Running post-quit on-device shell commands…"), NormalMessageFormat);

    for (const QString &oneCmd : cmds) {
        const HdcShellOutcome r = runHdcShellSocketThenCli(runControl, serial, oneCmd,
                                                           std::chrono::seconds(60));
        if (r.ok) {
            runControl->postMessage(
                Tr::tr("Post-quit command finished: %1").arg(oneCmd), NormalMessageFormat);
        } else {
            runControl->postMessage(
                Tr::tr("Post-quit command failed: %1 — %2").arg(oneCmd, r.errorDetail),
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

    if (harmonyHdcShellPreferCli()) {
        const FilePath hdc = HarmonyConfig::hdcToolPath();
        if (!hdc.isExecutableFile()) {
            runControl->postMessage(Tr::tr("Cannot stop application on device: hdc executable not found."),
                                    ErrorMessageFormat);
            return;
        }
    }

    const QString shellLine = QStringLiteral("aa force-stop ") + pkg;
    const HdcShellOutcome r = runHdcShellSocketThenCli(runControl, serial, shellLine,
                                                     std::chrono::seconds(15));
    if (r.ok) {
        runControl->postMessage(
            Tr::tr("Requested force-stop on device for \"%1\".").arg(pkg),
            NormalMessageFormat);
    } else {
        runControl->postMessage(
            Tr::tr("force-stop failed for \"%1\": %2").arg(pkg, r.errorDetail),
            ErrorMessageFormat);
    }
}

/*
** P2-14：辅助函数—从 RunControl 设置中读取 bool/string 类型的 Aspect 値。
*/
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

/*
** P2-14：解析 hilog 的设备序列号（与主运行器相同的两步逻辑）。
** 序列号为空时省略 "-t"，让 hdc 选择默认设备（主运行器也是这样做的）。
*/
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

/*
** P2-15 阶段三：将设备 shell 脚本作为 hdc argv 中 "shell" 后的参数（见 HarmonyRunConfiguration）。
*/
static QString extractHarmonyDeviceShellScript(const RunControl *rc)
{
    if (!rc)
        return {};
    const CommandLine cmd = rc->commandLine();
    const QString exeName = cmd.executable().fileName();
    if (!exeName.contains(QLatin1String("hdc"), Qt::CaseInsensitive))
        return {};
    const QStringList args = cmd.splitArguments();
    const int shellIdx = args.indexOf(QStringLiteral("shell"));
    if (shellIdx < 0 || shellIdx + 1 >= args.size())
        return {};
    return QStringList(args.mid(shellIdx + 1)).join(u' ');
}

class HarmonyProcessRunnerFactory final : public RunWorkerFactory
{
public:
    HarmonyProcessRunnerFactory()
    {
        setId("Harmony.ProcessRunner");
        setRecipeProducer([](RunControl *runControl) {
            using namespace QtTaskTree;

            const QString deviceSerial = resolveSerial(runControl);
            const QString deviceScript = extractHarmonyDeviceShellScript(runControl);
            const bool useMainSocket = !harmonyHdcShellPreferCli() && !deviceScript.isEmpty();

            /* ** Hilog 伴随任务（P2-14） */
            const bool hilogEnabled =
                readBoolSetting(runControl, Id(Constants::HARMONY_HILOG_ENABLED), true);
            const QString hilogFilter =
                readStringSetting(runControl, Id(Constants::HARMONY_HILOG_FILTER));
            const FilePath hdc    = HarmonyConfig::hdcToolPath();
            const bool canStream  = hilogEnabled && hdc.isExecutableFile();

            /* ** 解析包名用于基于 PID 的过滤。
            ** 不过滤 PID 时，hilog 会输出整个设备日志（每秒天级行数），
            ** 将过载 UI 事件循环并导致 Qt Creator 卡顿。 */
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

            /* ** 通过 hdc 守护进程直接实现 hilog 流式传输（P2-14）
            ** 直接向 hdc 守护进程（端口 8710）开启 TCP socket，
            ** 使用 DevEco Studio 相同的二进制协议。TCP_NODELAY 消除 Nagle 延迟，
            ** 实现与 DevEco 相同的实时日志投递。
            ** QSyncTask 立即完成；socket 通过 Qt 事件循环异步运行，
            ** 以 RunControl 为父对象管理生命周期。 */
            auto hilogTask = QSyncTask(
                [runControl, canStream, hdc, deviceSerial, hilogFilter, bundleName] {
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

                    /* ** POSIX shell 脚本：每 0.5s 轮询 pidof（最多约 15s），
                    ** 然后 exec hilog -P <PID> 以只输出应用日志。 */
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

                    auto *client = new HdcSocketClient(runControl); // NOSONAR (cpp:S5025) - parented, will auto-delete
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

                    client->start(deviceSerial, QStringLiteral("shell ") + script);

                    runControl->postMessage(
                        Tr::tr("Harmony: streaming hilog for %1 via hdc socket "
                               "(waiting for PID\u2026)")
                            .arg(bundleName),
                        NormalMessageFormat);
                });

            /* ** 并行运行主会话和 hilog socket。
            ** hilogTask（QSyncTask）创建 HdcSocketClient 后立即完成；
            ** socket 异步流式传输，直到 RunControl 发出 stopped()。
            ** FinishAllAndSuccess：主任务完成时组即完成；
            ** hilogTask 已完成（StopWithSuccess）所以只有主任务保持组活跃。
            ** P2-15 阶段三：默认主会话 = hdc 守护进程 TCP；
            ** QTC_HARMONY_HDC_USE_CLI / CLI 优先模式或脚本缺失时 → hdc.exe QProcess。 */
            const auto startedBarrier = [&](auto mainTask, auto startedSignal) {
                return Group{
                    parallel,
                    workflowPolicy(WorkflowPolicy::FinishAllAndSuccess),
                    When(mainTask, startedSignal) >> Do {
                        QSyncTask([runControl] { runControl->reportStarted(); })
                    },
                    hilogTask
                };
            };

            if (useMainSocket) {
                const auto mainSocketTask = QCustomTask<HarmonyMainRunSocketTask>(
                    [runControl, deviceSerial, deviceScript](HarmonyMainRunSocketTask &t) {
                        QObject::disconnect(runControl, &RunControl::canceled, runControl, nullptr);
                        QObject::connect(runControl,
                                         &RunControl::canceled,
                                         runControl,
                                         [runControl] { stopHarmonyApplicationOnDevice(runControl); });
                        QObject::connect(runControl, &RunControl::canceled, &t,
                                         [&t] { t.stopShell(); });
                        QObject::connect(runControl, &RunControl::stopped, &t,
                                         [&t] { t.stopShell(); });
                        QObject::connect(&t,
                                         &HarmonyMainRunSocketTask::done,
                                         runControl,
                                         [runControl](QtTaskTree::DoneResult) {
                                             runPostQuitShellCommandsOnDevice(runControl);
                                         },
                                         Qt::SingleShotConnection);
                        t.setContext(runControl, deviceSerial, deviceScript);
                        runControl->postMessage(
                            Tr::tr("Starting Harmony application via hdc daemon socket\u2026"),
                            NormalMessageFormat);
                        return SetupResult::Continue;
                    });
                return startedBarrier(mainSocketTask,
                                      &HarmonyMainRunSocketTask::sessionStarted);
            }

            const auto mainTask = runControl->processTask(
                [runControl](Process &process) {
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
            return startedBarrier(mainTask, &Process::started);
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
