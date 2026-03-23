// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "harmonydebugsupport.h"

#include "harmonyconfigurations.h"
#include "harmonylogcategories.h"
#include "harmonyutils.h"
#include "ohosconstants.h"
#include "ohostr.h"

#include <debugger/debuggerrunconfigurationaspect.h>
#include <debugger/debuggerruncontrol.h>

#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/runconfiguration.h>
#include <projectexplorer/runcontrol.h>

#include <qtsupport/baseqtversion.h>
#include <qtsupport/qtkitaspect.h>

#include <QtTaskTree/QTaskTree>

#include <utils/commandline.h>
#include <utils/fileutils.h>
#include <utils/hostosinfo.h>
#include <utils/outputformat.h>
#include <utils/processhandle.h>
#include <utils/qtcprocess.h>

#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <QUuid>

#include <chrono>

using namespace Debugger;
using namespace ProjectExplorer;
using namespace QtTaskTree;
using namespace Utils;

using namespace std::chrono_literals;

namespace Ohos::Internal {

namespace {

/** Official §7.2 — host lldb connects through hdc to the device abstract socket. */
static const char kUserAbstractRemoteChannel[] = "unix-abstract-connect:///lldb-server/platform.sock";

/** Every \c intermediates/libs/<product>/<abi>/ that contains at least one \c .so (hvigor layout). */
static void appendHvigorIntermediatesLibDirs(const FilePath &ohproRoot, FilePaths *paths)
{
    const FilePath libsRoot = ohproRoot.pathAppended("entry/build/default/intermediates/libs");
    if (!libsRoot.isReadableDir())
        return;
    const FilePaths products = libsRoot.dirEntries(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const FilePath &prod : products) {
        const FilePaths abis = prod.dirEntries(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const FilePath &abiDir : abis) {
            if (!abiDir.dirEntries({{"*.so"}, QDir::Files | QDir::NoDotAndDotDot}).isEmpty())
                paths->append(abiDir);
        }
    }
}

/** macOS: DWARF or the editor may use \c /private/var/... vs \c /Users/... — add LLDB \c source-map both ways. */
static void appendMacOsCanonicalSourceMaps(DebuggerRunParameters &rp, const FilePath &dir)
{
    if (!HostOsInfo::isMacHost() || dir.isEmpty())
        return;
    const QString fsPath = dir.toFSPathString();
    QFileInfo fi(fsPath);
    if (!fi.exists())
        return;
    const QString canon = fi.canonicalFilePath();
    if (canon.isEmpty())
        return;
    const FilePath canonFp = FilePath::fromString(canon);
    const QString a = dir.path();
    const QString b = canonFp.path();
    if (a == b)
        return;
    rp.insertSourcePath(a, b);
    rp.insertSourcePath(b, a);
}

struct HarmonyUserHapAbstractCtx
{
    FilePath hdc;
    QString serial;
    QString bundle;
    QString ability;
    QString ohosTriple;
    FilePath hostLldbServer;
    FilePath hostHap;
    QString deviceLldbDir;
    QString deviceLldbPath;
    QString deviceHapStageDir;
    QString deviceHapPath;
};

static void fillHarmonyCppSolibAndSysroot(DebuggerRunParameters &rp,
                                          BuildConfiguration *bc,
                                          Kit *kit,
                                          Project *project)
{
    if (!bc || !kit || !rp.isCppDebugging())
        return;

    const FilePath ohpro = harmonyBuildDirectory(bc);

    FilePaths solibSearchPath;
    solibSearchPath.append(buildDirectory(bc));
    solibSearchPath.append(bc->buildDirectory());
    const FilePath cmakeLibDir = bc->buildDirectory().pathAppended("lib");
    if (cmakeLibDir.isReadableDir())
        solibSearchPath.append(cmakeLibDir);
    solibSearchPath.append(ohpro);
    appendHvigorIntermediatesLibDirs(ohpro, &solibSearchPath);

    const QString preferredAbi = harmonyDebuggerPreferredAbi(bc);
    if (!preferredAbi.isEmpty()) {
        const FilePath libDir = ohpro.pathAppended("entry/libs").pathAppended(preferredAbi);
        if (libDir.isReadableDir())
            solibSearchPath.append(libDir);
        const FilePath hvigorInterLibs = ohpro.pathAppended("entry/build/default/intermediates/libs/default")
                                             .pathAppended(preferredAbi);
        if (hvigorInterLibs.isReadableDir())
            solibSearchPath.append(hvigorInterLibs);
    }
    if (const RunConfiguration *arc = bc->activeRunConfiguration()) {
        const FilePath wd = arc->buildTargetInfo().workingDirectory;
        if (!wd.isEmpty())
            solibSearchPath.append(wd);
    }
    if (QtSupport::QtVersion *qtVersion = QtSupport::QtKitAspect::qtVersion(kit))
        solibSearchPath.append(qtVersion->qtSoPaths());
    FilePath::removeDuplicates(solibSearchPath);
    rp.setSolibSearchPath(solibSearchPath);
    qCDebug(harmonyRunLog).noquote() << "Harmony debug solib search path:" << solibSearchPath;

    if (project) {
        rp.addSearchDirectory(project->projectDirectory());
        appendMacOsCanonicalSourceMaps(rp, project->projectDirectory());
    }
    rp.addSearchDirectory(ohpro);
    appendMacOsCanonicalSourceMaps(rp, ohpro);
    rp.addSearchDirectory(bc->buildDirectory());
    appendMacOsCanonicalSourceMaps(rp, bc->buildDirectory());

    FilePath sysRoot = rp.sysRoot();
    if (sysRoot.isEmpty() || !sysRoot.isReadableDir()) {
        const FilePath ndk = FilePath::fromSettings(kit->value(Constants::HARMONY_KIT_NDK));
        if (!ndk.isEmpty()) {
            const FilePath candidates[] = {ndk / "sysroot", ndk / "llvm" / "sysroot"};
            for (const FilePath &c : candidates) {
                if (c.isReadableDir()) {
                    rp.setSysRoot(c);
                    qCDebug(harmonyRunLog).noquote() << "Harmony debug sysroot (NDK fallback):"
                                                    << c.toUserOutput();
                    break;
                }
            }
        }
    }
}

static DebuggerRunParameters harmonyDebuggerRunParametersForUserAbstract(RunControl *runControl)
{
    DebuggerRunParameters rp = DebuggerRunParameters::fromRunControl(runControl);
    // §7.2 uses abstract socket — do not allocate TCP debug channel (would overwrite remoteChannel in fixup).
    rp.setRemoteChannel(QString::fromLatin1(kUserAbstractRemoteChannel));
    rp.setSkipDebugServer(true);
    rp.setLldbPlatform(QStringLiteral("remote-ohos"));
    rp.setStartMode(AttachToRemoteServer);
    rp.setUseContinueInsteadOfRun(true);
    rp.setUseExtendedRemote(true);

    BuildConfiguration *bc = runControl->buildConfiguration();
    Kit *kit = runControl->kit();
    fillHarmonyCppSolibAndSysroot(rp, bc, kit, runControl->project());

    if (bc) {
        const QString pkg = harmonyDebuggerBundleName(bc);
        if (!pkg.isEmpty())
            rp.setDisplayName(pkg);
        ProcessRunData inferior = rp.inferior();
        inferior.command = CommandLine();
        rp.setInferior(inferior);
    }

    return rp;
}

/**
 * TCP debug channel only (setupPortsGatherer). Does **not** push lldb-server or run hdc fport.
 * For rooted/engineering devices after **manual** §7.1 setup, or troubleshooting. Retail = use default §7.2.
 */
static DebuggerRunParameters harmonyDebuggerRunParametersLegacyTcp(RunControl *runControl)
{
    DebuggerRunParameters rp = DebuggerRunParameters::fromRunControl(runControl);
    rp.setupPortsGatherer(runControl);
    rp.setSkipDebugServer(true);
    rp.setLldbPlatform(QStringLiteral("remote-ohos"));
    rp.setStartMode(AttachToRemoteServer);
    rp.setUseContinueInsteadOfRun(true);
    rp.setUseExtendedRemote(true);

    BuildConfiguration *bc = runControl->buildConfiguration();
    Kit *kit = runControl->kit();
    fillHarmonyCppSolibAndSysroot(rp, bc, kit, runControl->project());

    if (bc) {
        const QString pkg = harmonyDebuggerBundleName(bc);
        if (!pkg.isEmpty())
            rp.setDisplayName(pkg);
        ProcessRunData inferior = rp.inferior();
        inferior.command = CommandLine();
        rp.setInferior(inferior);
    }

    return rp;
}

/** §7.2 prep: owns \c Storage<HarmonyUserHapAbstractCtx> as the first group item. */
static Group harmonyUserHapAbstractPrepRecipe(RunControl *runControl, const Storage<HarmonyUserHapAbstractCtx> &ctx)
{
    const auto fail = [runControl](const QString &msg) {
        runControl->postMessage(msg, ErrorMessageFormat);
        return false;
    };

    const auto onPrepare = [runControl, ctx, fail] {
        HarmonyUserHapAbstractCtx *c = ctx.activeStorage();
        BuildConfiguration *bc = runControl->buildConfiguration();
        if (!bc)
            return fail(Tr::tr("Harmony debug (user HAP): no build configuration."));

        c->hdc = HarmonyConfig::hdcToolPath();
        if (!c->hdc.isExecutableFile()) {
            return fail(
                Tr::tr("Harmony debug (user HAP): hdc was not found. Configure Harmony SDK / Preferences."));
        }

        c->serial = harmonyEffectiveDeviceSerial(bc);
        if (c->serial.isEmpty()) {
            return fail(Tr::tr("Harmony debug (user HAP): no device serial. Deploy once or select a run device."));
        }

        c->bundle = harmonyDebuggerBundleName(bc);
        if (c->bundle.isEmpty()) {
            return fail(
                Tr::tr("Harmony debug (user HAP): bundle name is unknown. Check AppScope/app.json5 or run "
                       "configuration overrides."));
        }

        c->ability = harmonyDebuggerAbilityName(bc);

        const QString abi = harmonyDebuggerPreferredAbi(bc);
        c->ohosTriple = HarmonyConfig::ohosLldbTripleForAbi(abi);
        if (c->ohosTriple.isEmpty()) {
            return fail(
                Tr::tr("Harmony debug (user HAP): could not determine OHOS ABI for lldb-server (got \"%1\").\n"
                       "Try: build the project and **Harmony HAP** once; **deploy** to set device ABIs; or "
                       "ensure the **Kit** lists Harmony ABIs / CMake sets **OHOS_ARCH**.")
                    .arg(abi.isEmpty() ? Tr::tr("(empty)") : abi));
        }

        c->hostLldbServer = HarmonyConfig::lldbServerExecutable(c->ohosTriple);
        if (!c->hostLldbServer.isReadableFile()) {
            return fail(Tr::tr("Harmony debug (user HAP): lldb-server for triple \"%1\" was not found under "
                               "the DevEco SDK (expected under sdk/default/hms/native/lldb/…).\n"
                               "Triple derived from ABI: %2")
                           .arg(c->ohosTriple, abi));
        }

        const FilePath ohpro = harmonyBuildDirectory(bc);
        QString hapDiag;
        c->hostHap = findBuiltHapPackage(ohpro, &hapDiag);
        if (c->hostHap.isEmpty() || !c->hostHap.isReadableFile()) {
            return fail(Tr::tr("Harmony debug (user HAP): no readable .hap under \"%1\".\n"
                               "Build the Harmony HAP first.\n\n%2")
                           .arg(ohpro.toUserOutput(),
                                hapDiag.isEmpty() ? Tr::tr("(no diagnostic)") : hapDiag));
        }

        c->deviceLldbDir = QStringLiteral("/data/local/tmp/debugserver/%1").arg(c->bundle);
        c->deviceLldbPath = c->deviceLldbDir + QStringLiteral("/lldb-server");
        c->deviceHapStageDir = QStringLiteral("/data/local/tmp/qtc_hap_%1")
                                   .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        c->deviceHapPath = c->deviceHapStageDir + QLatin1Char('/') + c->hostHap.fileName();

        runControl->postMessage(
            Tr::tr("Harmony debug: preparing user + debug HAP + abstract socket (official §7.2)…"),
            NormalMessageFormat);
        runControl->postMessage(Tr::tr("Device lldb-server path: %1").arg(c->deviceLldbPath),
                                NormalMessageFormat);
        return true;
    };

    const auto hdcShell = [ctx](Process &process, const QStringList &shellArgs) {
        const HarmonyUserHapAbstractCtx *c = ctx.activeStorage();
        CommandLine cmd{c->hdc, hdcSelector(c->serial)};
        cmd.addArg(QStringLiteral("shell"));
        cmd.addArgs(shellArgs);
        process.setCommand(cmd);
        process.setWorkingDirectory(c->hdc.parentDir());
    };

    const auto onFail = [runControl](const Process &p, DoneWith result) {
        if (result != DoneWith::Success) {
            runControl->postMessage(
                Tr::tr("Harmony debug (user HAP): command failed: %1").arg(p.exitMessage()),
                ErrorMessageFormat);
        }
    };
    const CallDoneFlags procDone = CallDone::OnSuccess | CallDone::OnError;

    const auto mkServerDir = [ctx, hdcShell](Process &p) {
        hdcShell(p, {QStringLiteral("mkdir"), QStringLiteral("-p"), ctx.activeStorage()->deviceLldbDir});
    };
    const auto sendServer = [ctx](Process &p) {
        const HarmonyUserHapAbstractCtx *c = ctx.activeStorage();
        CommandLine cmd{c->hdc, hdcSelector(c->serial)};
        cmd.addArg(QStringLiteral("file"));
        cmd.addArg(QStringLiteral("send"));
        cmd.addArg(c->hostLldbServer.nativePath());
        cmd.addArg(c->deviceLldbPath);
        p.setCommand(cmd);
        p.setWorkingDirectory(c->hdc.parentDir());
    };
    const auto chmodServer = [ctx, hdcShell](Process &p) {
        hdcShell(p,
                 {QStringLiteral("chmod"),
                  QStringLiteral("755"),
                  ctx.activeStorage()->deviceLldbPath});
    };
    const auto mkHapDir = [ctx, hdcShell](Process &p) {
        hdcShell(p, {QStringLiteral("mkdir"), QStringLiteral("-p"), ctx.activeStorage()->deviceHapStageDir});
    };
    const auto sendHap = [ctx](Process &p) {
        const HarmonyUserHapAbstractCtx *c = ctx.activeStorage();
        CommandLine cmd{c->hdc, hdcSelector(c->serial)};
        cmd.addArg(QStringLiteral("file"));
        cmd.addArg(QStringLiteral("send"));
        cmd.addArg(c->hostHap.nativePath());
        cmd.addArg(c->deviceHapPath);
        p.setCommand(cmd);
        p.setWorkingDirectory(c->hdc.parentDir());
    };
    const auto bmInstall = [ctx, hdcShell](Process &p) {
        hdcShell(p,
                 {QStringLiteral("bm"),
                  QStringLiteral("install"),
                  QStringLiteral("-p"),
                  ctx.activeStorage()->deviceHapPath});
    };
    const auto aaStart = [ctx, hdcShell](Process &p) {
        const HarmonyUserHapAbstractCtx *c = ctx.activeStorage();
        hdcShell(p,
                 {QStringLiteral("aa"),
                  QStringLiteral("start"),
                  QStringLiteral("-a"),
                  c->ability,
                  QStringLiteral("-b"),
                  c->bundle});
    };
    const auto aaAttach = [ctx, hdcShell](Process &p) {
        hdcShell(p,
                 {QStringLiteral("aa"),
                  QStringLiteral("attach"),
                  QStringLiteral("-b"),
                  ctx.activeStorage()->bundle});
    };
    const auto aaProcessLldb = [ctx, hdcShell](Process &p) {
        const HarmonyUserHapAbstractCtx *c = ctx.activeStorage();
        const QString lldbArg = QStringLiteral("%1 platform --listen unix-abstract:///lldb-server/platform.sock")
                                    .arg(c->deviceLldbPath);
        const QString inner = QStringLiteral("aa process -a %1 -b %2 -D \"%3\" &")
                                  .arg(c->ability, c->bundle, lldbArg);
        hdcShell(p, {QStringLiteral("sh"), QStringLiteral("-c"), inner});
    };

    const auto sleepAfterStart = [] {
        QThread::msleep(800);
        return true;
    };
    const auto sleepAfterLldb = [] {
        QThread::msleep(2000);
        return true;
    };

    const auto forceStopBestEffort = [ctx] {
        const HarmonyUserHapAbstractCtx *c = ctx.activeStorage();
        CommandLine cmd{c->hdc, hdcSelector(c->serial)};
        cmd.addArgs({QStringLiteral("shell"),
                     QStringLiteral("aa"),
                     QStringLiteral("force-stop"),
                     c->bundle});
        Process p;
        p.setCommand(cmd);
        p.setWorkingDirectory(c->hdc.parentDir());
        p.runBlocking(std::chrono::seconds(30));
        return true;
    };

    const auto pickPid = [runControl, ctx, fail] {
        const HarmonyUserHapAbstractCtx *c = ctx.activeStorage();
        const qint64 pid = harmonyQueryApplicationPid(c->hdc, c->serial, c->bundle);
        if (pid <= 0) {
            return fail(
                Tr::tr("Harmony debug (user HAP): could not determine the application PID after start.\n"
                       "Try: hdc shell bm dump -n %1\n"
                       "or hdc shell ps -A | find the process, then report the output format so we can "
                       "improve parsing.")
                    .arg(c->bundle));
        }
        runControl->setAttachPid(ProcessHandle(pid));
        runControl->postMessage(Tr::tr("Harmony debug: attaching LLDB to PID %1.").arg(pid),
                                NormalMessageFormat);
        return true;
    };

    return Group{
        ctx,
        QSyncTask(onPrepare),
        ProcessTask(mkServerDir, onFail, procDone).withTimeout(2min),
        ProcessTask(sendServer, onFail, procDone).withTimeout(3min),
        ProcessTask(chmodServer, onFail, procDone).withTimeout(1min),
        ProcessTask(mkHapDir, onFail, procDone).withTimeout(1min),
        ProcessTask(sendHap, onFail, procDone).withTimeout(5min),
        QSyncTask(forceStopBestEffort),
        ProcessTask(bmInstall, onFail, procDone).withTimeout(5min),
        ProcessTask(aaStart, onFail, procDone).withTimeout(2min),
        QSyncTask(sleepAfterStart),
        ProcessTask(aaAttach, onFail, procDone).withTimeout(2min),
        ProcessTask(aaProcessLldb, onFail, procDone).withTimeout(2min),
        QSyncTask(sleepAfterLldb),
        QSyncTask(pickPid),
    };
}

class HarmonyDebugWorkerFactory final : public RunWorkerFactory
{
public:
    HarmonyDebugWorkerFactory()
    {
        setId("HarmonyDebugWorkerFactory");
        setRecipeProducer([](RunControl *runControl) -> Group {
            const FilePath lldbExe = HarmonyConfig::hostLldbExecutable();
            if (!lldbExe.isExecutableFile()) {
                return runControl->errorTask(
                    Tr::tr("Harmony native debug: host LLDB was not found.\n"
                           "Configure the DevEco Studio installation path under "
                           "Preferences → Harmony, and ensure OpenHarmony SDK "
                           "layout contains openharmony/native/llvm/bin/lldb."));
            }

            if (!runControl->kit()) {
                return runControl->errorTask(
                    Tr::tr("Harmony native debug: the run configuration has no kit."));
            }

            bool cppDebug = true;
            if (const auto *asp = runControl->aspectData<DebuggerRunConfigurationAspect>())
                cppDebug = asp->useCppDebugger;

            const bool legacyTcp = qtcEnvironmentVariableIntValue("HARMONY_DEBUG_LEGACY") != 0;

            const FilePath recommended = HarmonyConfig::hostLldbExecutable();
            if (recommended.isExecutableFile()) {
                runControl->postMessage(
                    Tr::tr("Harmony native debug: use OpenHarmony LLDB from the active kit, e.g.:\n%1\n"
                           "Set it under **Projects → Kits → Debugger**, or launch Qt Creator with "
                           "environment variable **QTC_DEBUGGER_PATH** pointing to that file.")
                        .arg(recommended.toUserOutput()),
                    NormalMessageFormat);
            }

            if (!cppDebug || legacyTcp) {
                if (legacyTcp && cppDebug) {
                    runControl->postMessage(
                        Tr::tr("Harmony debug: **HARMONY_DEBUG_LEGACY** is set — using TCP debug port "
                               "allocation only (no §7.2 HAP prep, **no** automatic lldb-server push or "
                               "**hdc fport**). For retail devices use the default path; this mode is for "
                               "manual §7.1 on rooted/engineering hardware."),
                        NormalMessageFormat);
                }
                DebuggerRunParameters rp = harmonyDebuggerRunParametersLegacyTcp(runControl);
                qCDebug(harmonyRunLog) << "Harmony debug: legacy TCP parameters";
                return debuggerRecipe(runControl, rp);
            }

            const Storage<HarmonyUserHapAbstractCtx> userCtx;
            DebuggerRunParameters rp = harmonyDebuggerRunParametersForUserAbstract(runControl);
            qCDebug(harmonyRunLog) << "Harmony debug: user HAP + abstract socket; remoteChannel"
                                   << kUserAbstractRemoteChannel;

            return Group{
                harmonyUserHapAbstractPrepRecipe(runControl, userCtx),
                debuggerRecipe(runControl, rp),
            };
        });
        addSupportedRunMode(ProjectExplorer::Constants::DEBUG_RUN_MODE);
        setSupportedRunConfigs({Ohos::Constants::HARMONY_RUNCONFIG_ID});
        setExecutionType(ProjectExplorer::Constants::STDPROCESS_EXECUTION_TYPE_ID);
    }
};

} // namespace

void setupHarmonyDebugWorker()
{
    qCDebug(harmonyRunLog) << "Registering Harmony debug worker factory.";
    static HarmonyDebugWorkerFactory theHarmonyDebugWorkerFactory;
}

} // namespace Ohos::Internal
