// Copyright (C) 2026 Li-Yaosong.
// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "harmonydebugsupport.h"

#include "harmonyconfigurations.h"
#include "harmonyutils.h"
#include "hdcsocketclient.h"
#include "ohosconstants.h"
#include "ohostr.h"

#include <debugger/debuggerengine.h>
#include <debugger/debuggerruncontrol.h>

#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/runconfiguration.h>
#include <projectexplorer/runcontrol.h>

#include <coreplugin/icore.h>

#include <utils/commandline.h>
#include <utils/filepath.h>
#include <utils/id.h>
#include <utils/outputformat.h>
#include <utils/processhandle.h>
#include <utils/qtcprocess.h>
#include <utils/store.h>

#include <QtTaskTree/qtasktree.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QProcess>
#include <QTcpServer>
#include <QHostAddress>
#include <QTimer>

#include <atomic>
#include <memory>

using namespace Debugger;
using namespace ProjectExplorer;
using namespace QtTaskTree;
using namespace Utils;
using namespace std::chrono_literals;

namespace Ohos::Internal {

/*
** 内部辅助函数
*/

/*
** 查找指定 OHOS triple 的 lldb-server 二进制文件。
** 依据可用的 CLI 脚本（ohos_lldb_cli_debug.sh），设备侧正确的 lldb-server 路径为：
**   <sdk>/native/llvm/lib/clang/<ver>/bin/<triple>/lldb-server
** hms/native/lldb 路径为华为签名版本，仅在严格用户镜像中需要，优先使用标准 clang 树路径。
** 搜索顺序：
**   1. 已注册的 OpenHarmony SDK：<sdk>/native/llvm/lib/clang/<ver>/bin/<triple>/lldb-server
**   2. DevEco Studio openharmony 路径：openharmony/native 下的相同结构
**   3. DevEco Studio hms 路径：hms/native/lldb/<triple>/lldb-server
**   4. 已注册 SDK 的 hms 路径（最后备选）
*/
static FilePath lldbServerForAbi(const QString &abiTriple)
{
    /*
    ** DevEco Studio openharmony/native 路径
    */
    if (const FilePath tools = HarmonyConfig::devecoToolsLocation(); !tools.isEmpty()) {
        const FilePath devecoNdk = tools.parentDir() / "sdk" / "default" / "openharmony" / "native";

        /*
        ** DevEco Studio hms 路径（华为签名，用于严格用户镜像）
        */
        const FilePath hmsCand = tools.parentDir()
                                 / "sdk" / "default" / "hms" / "native" / "lldb"
                                 / abiTriple / "lldb-server";
        if (hmsCand.exists())
            return hmsCand;
    }
    return {};
}

/*
** 构建 LLDB platform 连接 URL。
** 格式：unix-abstract-connect://[SERIAL]<socketDir>/<socketName>
** [SERIAL] 选择器用于告知 OHOS LLDB 通过 hdc 路由到指定设备。
** socketDir 必须以 '/' 开头，结果为设备选择器后跟抽象套接字路径，例如：
**   unix-abstract-connect://[FMR0224xxx]/com.example.app/platform-123.sock
** lldb-server --listen 参数使用：
**   unix-abstract://<socketDir>/<socketName>（两个斜杠 + 开头 / = 共三个斜杠）
*/
static QString ohosAbstractSocketConnectUrl(const QString &serial,
                                            const QString &socketDir,
                                            const QString &socketName)
{
    return QStringLiteral("unix-abstract-connect://[%1]%2/%3").arg(serial, socketDir, socketName);
}

struct HdcResult {
    bool ok = false;
    QString output;
    QString error;
};

/* ** 通过 hdc 在设备上执行 shell 命令（优先使用 socket，失败时回退到 CLI）。 */
static HdcResult hdcShell(const QString &serial, const QString &shellLine, int timeoutMs = 30000)
{
    const FilePath hdc = HarmonyConfig::hdcToolPath();
    QStringList args = hdcSelector(serial);
    args << QStringLiteral("shell") << shellLine;
    const HdcShellSyncResult r = HdcSocketClient::runSyncWithCliFallback(
        serial,
        QStringLiteral("shell ") + shellLine,
        hdc.toUserOutput(),
        args,
        timeoutMs);
    return {r.isOk(), r.standardOutput, r.errorMessage};
}

/*
** 使用 "hdc file send" 将本地文件推送到设备。
*/
static HdcResult hdcFileSend(const QString &serial,
                              const FilePath &local,
                              const QString &remote,
                              int timeoutMs = 120000)
{
    const FilePath hdc = HarmonyConfig::hdcToolPath();
    QStringList args = hdcSelector(serial);
    args << QStringLiteral("file") << QStringLiteral("send")
         << local.nativePath() << remote;
    Process proc;
    proc.setCommand(CommandLine{hdc, args});
    proc.start();
    if (!proc.waitForFinished(std::chrono::milliseconds(timeoutMs))) {
        proc.kill();
        return {false, {}, QStringLiteral("hdc file send timed out after %1 ms").arg(timeoutMs)};
    }
    const bool ok = proc.result() == ProcessResult::FinishedWithSuccess;
    return {ok, proc.allOutput(), ok ? QString() : proc.errorString()};
}

/*
** 在后台执行 hdc shell 命令（即发即忘模式）。
** 用于 `aa start -D`、`aa process -D` 等在设备侧长期运行的命令：
** 这些命令会持续占用 hdc 连接，不能使用同步调用等待返回。
*/
static void fireHdcShell(RunControl *runControl,
                          const QString &serial,
                          const QString &shellCmd)
{
    auto *client = new HdcSocketClient(runControl); // NOSONAR (cpp:S5025) - parented, will auto-delete on stopped
    /*
    ** 调试结束时关闭连接，设备侧命令随之收到 SIGHUP
    */
    QObject::connect(runControl, &RunControl::stopped, client, &HdcSocketClient::stop);
    QObject::connect(client, &HdcSocketClient::finished, client, &QObject::deleteLater);
    client->start(serial, QStringLiteral("shell ") + shellCmd);
}

/*
** 轮询设备上的 "ps -ef" 直到找到目标包名，返回 PID；失败时返回 0。
** OHOS shell 返回 \r\n —— 在主机侧去除 \r（设备上没有 `tr` 命令）。
*/
static qint64 pollAppPid(const QString &serial, const QString &bundle, int maxAttempts = 20)
{
    for (int i = 0; i < maxAttempts; ++i) {
        if (i > 0) {
            /*
            ** 用嵌套 QEventLoop 代替 QThread::msleep，保持 Qt 主线程事件循环可用
            */
            QEventLoop loop;
            QTimer::singleShot(500, &loop, &QEventLoop::quit);
            loop.exec();
        }
        const HdcResult r = hdcShell(serial, QStringLiteral("ps -ef"), 10000);
        if (!r.ok)
            continue;
        for (QString line : r.output.split(u'\n')) {
            line.remove(u'\r');
            if (!line.contains(bundle))
                continue;
            /*
            ** ps -ef 列顺序：UID PID PPID C STIME TTY TIME CMD ...
            */
            const QStringList fields = line.split(u' ', Qt::SkipEmptyParts);
            if (fields.size() < 2)
                continue;
            bool ok = false;
            const qint64 pid = fields.at(1).toLongLong(&ok);
            if (ok && pid > 0)
                return pid;
        }
    }
    return 0;
}

/*
** 在本地绑定端口 0 并立即释放，返回空闲端口号。
*/
static quint16 findFreePort()
{
    QTcpServer s;
    s.listen(QHostAddress::LocalHost, 0);
    const quint16 port = s.serverPort();
    s.close();
    return port;
}

/*
** 计算调试会话的 solib 搜索路径。
*/
static FilePaths computeSolibPaths(const RunControl *runControl)
{
    FilePaths solibPaths;
    const BuildConfiguration *bc = runControl->buildConfiguration();
    if (!bc)
        return solibPaths;
    if (const FilePath bd = buildDirectory(bc); !bd.isEmpty()) {
        if (const FilePath cmakeObj = bd / "ohpro/entry/build/default/intermediates/cmake/default/obj/arm64-v8a"; cmakeObj.isReadableDir())
            solibPaths.append(cmakeObj);
        if (const FilePath cmakeLibs = bd / "ohpro/entry/build/default/intermediates/libs/default/arm64-v8a"; cmakeLibs.isReadableDir())
            solibPaths.append(cmakeLibs);
        solibPaths.append(bd);
    }
    if (const RunConfiguration *rc = bc->activeRunConfiguration()) {
        const FilePath wd = rc->buildTargetInfo().workingDirectory;
        if (!wd.isEmpty())
            solibPaths.append(wd);
    }
    FilePath::removeDuplicates(solibPaths);
    return solibPaths;
}

/*
** 验证调试先决条件；失败时向面板输出错误消息并返回 false。
*/
static bool validateDebugPrerequisites(RunControl *runControl,
                                        BuildConfiguration *&outBc,
                                        QString &outSerial,
                                        QString &outBundle)
{
    outBc = runControl->buildConfiguration();
    if (!outBc) {
        runControl->postMessage(Tr::tr("Harmony DAP debug: no build configuration."), ErrorMessageFormat);
        return false;
    }
    if (!HarmonyConfig::hdcToolPath().isExecutableFile()) {
        runControl->postMessage(Tr::tr("Harmony DAP debug: hdc was not found (check HarmonyOS kit settings)."), ErrorMessageFormat);
        return false;
    }
    outSerial = harmonyEffectiveDeviceSerial(outBc);
    if (outSerial.isEmpty()) {
        runControl->postMessage(Tr::tr("Harmony DAP debug: no target device serial (is a device connected?)."), ErrorMessageFormat);
        return false;
    }
    outBundle = packageName(outBc);
    if (outBundle.isEmpty()) {
        runControl->postMessage(Tr::tr("Harmony DAP debug: bundle/package name is unknown (check project settings)."), ErrorMessageFormat);
        return false;
    }
    return true;
}

/*
** 将首选 ABI 映射到 OHOS triple；未知 ABI 时发出警告并回退到 aarch64。
*/
static QString resolveOhosTriple(RunControl *runControl, const BuildConfiguration *bc)
{
    const QString preferredAbi = hapDevicePreferredAbi(bc);
    if (preferredAbi.contains(QLatin1String("aarch64")) || preferredAbi.contains(QLatin1String("arm64")))
        return QStringLiteral("aarch64-linux-ohos");
    if (!preferredAbi.isEmpty() && preferredAbi.contains(QLatin1String("arm")))
        return QStringLiteral("arm-linux-ohos");
    if (preferredAbi.contains(QLatin1String("x86_64")))
        return QStringLiteral("x86_64-linux-ohos");
    runControl->postMessage(
        Tr::tr("Harmony DAP debug: unknown ABI '%1', defaulting to aarch64-linux-ohos.").arg(preferredAbi),
        NormalMessageFormat);
    return QStringLiteral("aarch64-linux-ohos");
}

/*
** 从 RunControl 设置中读取 startup-break 开关（默认 true）。
*/
static bool readStartupBreak(const RunControl *runControl)
{
    const Store sd = runControl->settingsData(Id(Constants::HARMONY_DEBUG_STARTUP_BREAK));
    for (auto it = sd.constBegin(); it != sd.constEnd(); ++it) {
        if (it.value().isValid())
            return it.value().toBool();
    }
    return true;
}

/*
** 构造 "aa start" 命令行字符串。
*/
static QString buildAaStartCmd(const QString &bundle, const QString &ability, bool startupBreak)
{
    if (startupBreak) {
        return ability.isEmpty()
            ? QString("aa start -D -b %1").arg(bundle)
            : QString("aa start -D -a %1 -b %2").arg(ability, bundle);
    }
    return ability.isEmpty()
        ? QString("aa start -b %1").arg(bundle)
        : QString("aa start -a %1 -b %2").arg(ability, bundle);
}

/*
** 终止上次会话残留的 arktsdebugbridge 进程。
*/
static void killStaleBridgeProcess()
{
    Process killStaleBridge;
#ifdef Q_OS_WIN
    killStaleBridge.setCommand(CommandLine{FilePath::fromString("taskkill"), {"/F", "/IM", "arktsdebugbridge.exe"}});
#else
    killStaleBridge.setCommand(CommandLine{FilePath::fromString("pkill"), {"-9", "arktsdebugbridge"}});
#endif
    killStaleBridge.start();
    killStaleBridge.waitForFinished(3s);
}

/*
** ArkTS 端口转发 + WebSocket 桥接进程结果。
*/
struct ArkTsBridgeSetup {
    quint16 localWsPort{};
    quint16 localPandaPort{};
    QString wsSignalFile;
    qint64  bridgePid{};
    QString tmpDir;
};

/*
** 执行步骤 5b：ArkTS 端口转发并启动 arktsdebugbridge（startup-break 模式专用）。
*/
static ArkTsBridgeSetup setupArkTsBridge(RunControl *runControl,
                                          const QString &serial,
                                          qint64 pid,
                                          const QString &bundle)
{
    ArkTsBridgeSetup result;
    result.localWsPort    = findFreePort();
    result.localPandaPort = findFreePort();

    const FilePath hdc2 = HarmonyConfig::hdcToolPath();

    QStringList fportArgs1 = hdcSelector(serial);
    fportArgs1 << QStringLiteral("fport")
               << QStringLiteral("tcp:%1").arg(result.localWsPort)
               << QStringLiteral("ark:%1@%2").arg(pid).arg(bundle);
    Process fport1;
    fport1.setCommand({hdc2, fportArgs1});
    fport1.start();
    fport1.waitForFinished(std::chrono::seconds(10));
    runControl->postMessage(
        Tr::tr("Harmony DAP debug: fport tcp:%1 ark:%2@%3 → %4")
            .arg(result.localWsPort).arg(pid).arg(bundle).arg(fport1.allOutput().trimmed()),
        NormalMessageFormat);

    QStringList fportArgs2 = hdcSelector(serial);
    fportArgs2 << QStringLiteral("fport")
               << QStringLiteral("tcp:%1").arg(result.localPandaPort)
               << QStringLiteral("ark:%1@Debugger").arg(pid);
    Process fport2;
    fport2.setCommand({hdc2, fportArgs2});
    fport2.start();
    fport2.waitForFinished(std::chrono::seconds(10));
    runControl->postMessage(
        Tr::tr("Harmony DAP debug: fport tcp:%1 ark:%2@Debugger → %3")
            .arg(result.localPandaPort).arg(pid).arg(fport2.allOutput().trimmed()),
        NormalMessageFormat);

    result.tmpDir = QDir::tempPath()
        + QStringLiteral("/harmony_debug_%1").arg(QDateTime::currentMSecsSinceEpoch());
    QDir().mkpath(result.tmpDir);
    result.wsSignalFile = result.tmpDir + QStringLiteral("/signal");

    if (const QString bridgeBin = Core::ICore::libexecPath(
                QStringLiteral("arktsdebugbridge")).toUrlishString();
            QProcess::startDetached(
                bridgeBin,
                {QString::number(result.localWsPort),
                 QString::number(result.localPandaPort),
                 result.wsSignalFile},
                QString(),
                &result.bridgePid)) {
        runControl->postMessage(
            Tr::tr("Harmony DAP debug: bridge started (PID %1), connecting to ws://127.0.0.1:%2…")
                .arg(result.bridgePid).arg(result.localWsPort),
            NormalMessageFormat);
        QEventLoop l;
        QTimer::singleShot(2000, &l, &QEventLoop::quit);
        l.exec();
    } else {
        runControl->postMessage(
            Tr::tr("Harmony DAP debug: bridge binary not found (%1). "
                   "ArkTS startup-break may not work.").arg(bridgeBin),
            NormalMessageFormat);
    }
    return result;
}

/*
** 轮询设备上的 lldb-server 进程（最多 8 秒），确认就绪后返回 true。
*/
static bool pollLldbServerReady(RunControl *runControl,
                                 const QString &serial,
                                 const QString &socketName)
{
    const QString lldbPsCmd = QStringLiteral(
        "ps -ef 2>/dev/null | grep lldb-server | grep '%1' | grep -v grep").arg(socketName);
    constexpr int kLldbPollIntervalMs  = 500;
    constexpr int kLldbPollMaxAttempts = 16;
    for (int attempt = 0; attempt < kLldbPollMaxAttempts; ++attempt) {
        QEventLoop l;
        QTimer::singleShot(kLldbPollIntervalMs, &l, &QEventLoop::quit);
        l.exec();
        const QString lldbPsOut = hdcShell(serial, lldbPsCmd, 5000).output.trimmed();
        if (!lldbPsOut.isEmpty()) {
            runControl->postMessage(
                Tr::tr("Harmony DAP debug: lldb-server confirmed (attempt %1): %2")
                    .arg(attempt + 1).arg(lldbPsOut),
                NormalMessageFormat);
            return true;
        }
    }
    return false;
}

/*
** 按 PID 终止 bridge 进程（bridgePid <= 0 时为空操作）。
*/
static void killBridgeProcess(qint64 bridgePid)
{
    if (bridgePid <= 0)
        return;
    Process killBridge;
#ifdef Q_OS_WIN
    killBridge.setCommand(CommandLine{FilePath::fromString("taskkill"), {"/F", "/PID", QString::number(bridgePid)}});
#else
    killBridge.setCommand(CommandLine{FilePath::fromString("kill"), {QString::number(bridgePid)}});
#endif
    killBridge.start();
    killBridge.waitForFinished(3s);
}

/*
** 调试会话共享状态（由预启动任务写入，供调试器修改器和会话清理使用）。
** 任务树保证 preLaunch 顺序完成后才调用调试器修改器，无需使用原子类型。
*/
struct HarmonyDebugState {
    QString socketUrl;
    qint64  pid{0};
    QString wsSignalFile;
    quint16 localWsPort{};
    quint16 localPandaPort{};
    qint64  bridgePid{};
    QString serial;
    QString tmpDir;
};

/*
** 预启动任务主体：执行调试步骤 1-6 并填充 state。
** 注意：所有验证均在此函数内执行，以确保错误信息显示在
** "应用程序输出"面板中（该面板仅在 RunControl::start() 之后打开）。
*/
static DoneResult runPreLaunchTask(RunControl *runControl, HarmonyDebugState &state)
{
    runControl->postMessage(Tr::tr("Harmony DAP debug: preparing…"), NormalMessageFormat);

    BuildConfiguration *bc = nullptr;
    QString serial;
    QString bundle;
    if (!validateDebugPrerequisites(runControl, bc, serial, bundle))
        return DoneResult::Error;
    state.serial = serial;

    const QString ohosTriple = resolveOhosTriple(runControl, bc);
    const FilePath lldbServer = lldbServerForAbi(ohosTriple);
    if (!lldbServer.exists()) {
        runControl->postMessage(
            Tr::tr("Harmony DAP debug: lldb-server not found for triple '%1' "
                   "(searched DevEco SDK and registered NDK paths).").arg(ohosTriple),
            ErrorMessageFormat);
        return DoneResult::Error;
    }

    const QString ability    = defaultHarmonyAbilityName(bc);
    const QString remoteRoot = QStringLiteral("/data/local/tmp/debugserver/") + bundle;
    const QString remoteLldbServer = remoteRoot + QStringLiteral("/lldb-server");

    QString socketDir = u'/' + bundle;
    if (socketDir.size() > 70)
        socketDir = u'/' + socketDir.right(69);
    const QString socketName = QStringLiteral("platform-%1.sock")
                                   .arg(QDateTime::currentMSecsSinceEpoch());
    state.socketUrl = ohosAbstractSocketConnectUrl(serial, socketDir, socketName);

    const bool startupBreak = readStartupBreak(runControl);

    /* ** 步骤 1：强制停止残留实例 + 终止残留 bridge */
    runControl->postMessage(Tr::tr("Harmony DAP debug: step 1 — force stop"), NormalMessageFormat);
    hdcShell(serial, QStringLiteral("aa force-stop '") + bundle + u'\'', 10000);
    { QEventLoop l; QTimer::singleShot(400, &l, &QEventLoop::quit); l.exec(); }
    killStaleBridgeProcess();

    /* ** 步骤 2：创建远程调试目录 */
    runControl->postMessage(Tr::tr("Harmony DAP debug: step 2 — prepare remote dir"), NormalMessageFormat);
    if (const HdcResult mkRes = hdcShell(serial, QString("mkdir -p '%1' && chmod 757 '%1'").arg(remoteRoot)); !mkRes.ok) {
        runControl->postMessage(
            Tr::tr("Harmony DAP debug: failed to create remote dir: %1").arg(mkRes.error), ErrorMessageFormat);
        return DoneResult::Error;
    }

    /* ** 步骤 3：推送 lldb-server */
    runControl->postMessage(Tr::tr("Harmony DAP debug: step 3 — push lldb-server"), NormalMessageFormat);
    hdcShell(serial,
        QStringLiteral("for p in $(ps -ef | grep lldb-server | grep -v grep | awk '{print $2}');"
                       " do kill -9 $p 2>/dev/null || true; done; sleep 0.3"),
        10000);
    if (const HdcResult sendRes = hdcFileSend(serial, lldbServer, remoteLldbServer); !sendRes.ok) {
        runControl->postMessage(
            Tr::tr("Harmony DAP debug: failed to push lldb-server: %1").arg(sendRes.error), ErrorMessageFormat);
        return DoneResult::Error;
    }
    hdcShell(serial, QString("chmod 755 '%1'; rm -f '%2'/*.log").arg(remoteLldbServer, remoteRoot), 10000);
    runControl->postMessage(
        Tr::tr("Harmony DAP debug: lldb-server pushed from: %1").arg(lldbServer.toUserOutput()), NormalMessageFormat);

    /* ** 步骤 4：启动应用 */
    runControl->postMessage(
        Tr::tr("Harmony DAP debug: step 4 — %1")
            .arg(startupBreak ? Tr::tr("aa start -D (startup-break mode)") : Tr::tr("aa start")),
        NormalMessageFormat);
    if (const HdcResult startRes = hdcShell(serial, buildAaStartCmd(bundle, ability, startupBreak), 60000); !startRes.ok)
        runControl->postMessage(Tr::tr("Harmony DAP debug: aa start warning: %1").arg(startRes.error), NormalMessageFormat);
    { QEventLoop l; QTimer::singleShot(500, &l, &QEventLoop::quit); l.exec(); }

    /* ** 步骤 5：轮询 PID */
    runControl->postMessage(Tr::tr("Harmony DAP debug: step 5 — poll app PID"), NormalMessageFormat);
    state.pid = pollAppPid(serial, bundle, 20);
    if (state.pid == 0) {
        runControl->postMessage(
            Tr::tr("Harmony DAP debug: could not determine app PID after launch."), ErrorMessageFormat);
        return DoneResult::Error;
    }
    runControl->postMessage(Tr::tr("Harmony DAP debug: app PID %1.").arg(state.pid), NormalMessageFormat);

    /* ** 步骤 5b：ArkTS 端口转发 + bridge（仅 startup-break 模式） */
    if (startupBreak) {
        runControl->postMessage(
            Tr::tr("Harmony DAP debug: step 5b — ArkTS port-forward + bridge"), NormalMessageFormat);
        const ArkTsBridgeSetup bs = setupArkTsBridge(runControl, serial, state.pid, bundle);
        state.localWsPort    = bs.localWsPort;
        state.localPandaPort = bs.localPandaPort;
        state.wsSignalFile   = bs.wsSignalFile;
        state.bridgePid      = bs.bridgePid;
        state.tmpDir         = bs.tmpDir;
    }

    /* ** 步骤 6：注入 lldb-server */
    runControl->postMessage(Tr::tr("Harmony DAP debug: step 6 — inject lldb-server"), NormalMessageFormat);
    const QString lldbListenUrl = QStringLiteral("unix-abstract://") + socketDir + u'/' + socketName;
    const QString aaProcessCmd = ability.isEmpty()
        ? QString("aa process -b %1 -D \"%2 platform --listen %3 --log-file %4/platform.log\"")
              .arg(bundle, remoteLldbServer, lldbListenUrl, remoteRoot)
        : QString("aa process -a %1 -b %2 -D \"%3 platform --listen %4 --log-file %5/platform.log\"")
              .arg(ability, bundle, remoteLldbServer, lldbListenUrl, remoteRoot);
    runControl->postMessage(Tr::tr("Harmony DAP debug: injecting: %1").arg(aaProcessCmd), NormalMessageFormat);
    if (const HdcResult processRes = hdcShell(serial, aaProcessCmd, 30000); !processRes.ok) {
        runControl->postMessage(
            Tr::tr("Harmony DAP debug: aa process warning: %1").arg(processRes.error), NormalMessageFormat);
    }

    /* ** 轮询 lldb-server 进程稳定 */
    if (!pollLldbServerReady(runControl, serial, socketName)) {
        runControl->postMessage(
            Tr::tr("Harmony DAP debug: WARNING — lldb-server with socket '%1' not seen after 8 s. "
                   "Check the HAP is a debug build and SELinux allows /data/local/tmp/debugserver/.").arg(socketName),
            ErrorMessageFormat);
    } else {
        QEventLoop l;
        QTimer::singleShot(500, &l, &QEventLoop::quit);
        l.exec();
    }

    runControl->postMessage(
        Tr::tr("Harmony DAP debug: connecting — socket=%1").arg(state.socketUrl), NormalMessageFormat);
    return DoneResult::Success;
}

/*
** 构建调试任务树（在 RunControl::start() 后执行）。
** 通过 shared_ptr<HarmonyDebugState> 在 preLaunch、cleanup 和调试器修改器之间共享状态，
** 避免在 setRecipeProducer lambda 内出现深层嵌套逻辑。
*/
static Group buildDebugRecipe(RunControl *runControl)
{
    auto state = std::make_shared<HarmonyDebugState>();

    DebuggerRunParameters rp = DebuggerRunParameters::fromRunControl(runControl);
    rp.setStartMode(AttachToRemoteServer);
    rp.setSkipDebugServer(true);
    rp.setUseExtendedRemote(true);
    rp.setLldbPlatform(QStringLiteral("remote-ohos"));
    rp.setUseContinueInsteadOfRun(true);
    rp.setInferiorExecutable({});
    rp.setSymbolFile({});

    const FilePaths solibPaths = computeSolibPaths(runControl);
    if (!solibPaths.isEmpty())
        rp.setSolibSearchPath(solibPaths);

    const auto preLaunch = QSyncTask([runControl, state] {
        return runPreLaunchTask(runControl, *state);
    });

    QObject::connect(runControl, &RunControl::stopped, runControl, [state] {
        killBridgeProcess(state->bridgePid);
        if (!state->tmpDir.isEmpty())
            QDir(state->tmpDir).removeRecursively();
    });

    return Group{
        preLaunch,
        debuggerRecipe(runControl, rp, [state](DebuggerRunParameters &params) {
            params.setRemoteChannel(state->socketUrl);
            params.setAttachPid(ProcessHandle(state->pid));
            /*
            ** 将信号文件路径通过 workingDirectory 字段传递给 lldbbridge.py。
            ** LLDB attach 模式下 workingDirectory 不用于启动进程，可安全复用。
            ** lldbbridge.py 的 OHOS runEngine 块将在 attach 成功后读取此路径并创建文件，
            ** 触发 Bridge → ArkTS → Runtime.runIfWaitingForDebugger 解锁链。
            */
            if (!state->wsSignalFile.isEmpty()) {
                auto inferior = params.inferior();
                inferior.workingDirectory = Utils::FilePath::fromString(state->wsSignalFile);
                params.setInferior(inferior);
            }
            params.setCommandsAfterConnect(QString());
        })
    };
}

/*
** HarmonyDebugWorkerFactory
*/

class HarmonyDebugWorkerFactory final : public RunWorkerFactory
{
public:
    HarmonyDebugWorkerFactory()
    {
        setId("Harmony.DebugWorker");
        setRecipeProducer([](RunControl *runControl) {
            return buildDebugRecipe(runControl);
        });
        addSupportedRunMode(ProjectExplorer::Constants::DEBUG_RUN_MODE);
        addSupportedRunConfig(Constants::HARMONY_RUNCONFIG_ID);
    }
};

void setupHarmonyDebugWorker()
{
    static HarmonyDebugWorkerFactory theHarmonyDebugWorkerFactory;
}

} // namespace Ohos::Internal
