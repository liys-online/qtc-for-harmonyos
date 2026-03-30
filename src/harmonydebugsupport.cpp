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
    ** 辅助函数：枚举 clang 版本子目录并检查 lldb-server 是否存在。
    ** ndkRoot 为 "native" 目录（包含 llvm/）。
    */
    const auto tryClangBin = [&](const FilePath &ndkRoot) -> FilePath {
        const FilePath clangBase = ndkRoot / "llvm" / "lib" / "clang";
        const QStringList vers = QDir(clangBase.toUserOutput())
                                     .entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &ver : vers) {
            const FilePath candidate = clangBase / ver / "bin" / abiTriple / "lldb-server";
            if (candidate.isExecutableFile())
                return candidate;
        }
        return {};
    };

    /* ** (1) 已注册的 OpenHarmony SDK —— SDK 15 lldb-server 经确认可用 */
    for (const QString &s : HarmonyConfig::getSdkList()) {
        const FilePath ndk = HarmonyConfig::ndkLocation(FilePath::fromUserInput(s));
        if (const FilePath r = tryClangBin(ndk); !r.isEmpty())
            return r;
    }
    if (const FilePath r = tryClangBin(HarmonyConfig::ndkLocation(HarmonyConfig::defaultSdk())); !r.isEmpty())
        return r;

    /* ** (2) DevEco Studio openharmony/native 路径 */
    const FilePath tools = HarmonyConfig::devecoToolsLocation();
    if (!tools.isEmpty()) {
        const FilePath devecoNdk = tools.parentDir() / "sdk" / "default" / "openharmony" / "native";
        if (const FilePath r = tryClangBin(devecoNdk); !r.isEmpty())
            return r;

        /* ** (3) DevEco Studio hms 路径（华为签名，用于严格用户镜像） */
        const FilePath hmsCand = tools.parentDir()
                                 / "sdk" / "default" / "hms" / "native" / "lldb"
                                 / abiTriple / "lldb-server";
        if (hmsCand.isExecutableFile())
            return hmsCand;
    }

    /* ** (4) 已注册 SDK 的 hms 路径（最后备选） */
    for (const QString &s : HarmonyConfig::getSdkList()) {
        const FilePath c = FilePath::fromUserInput(s) / "hms" / "native" / "lldb" / abiTriple / "lldb-server";
        if (c.isExecutableFile()) return c;
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

/* ** 使用 "hdc file send" 将本地文件推送到设备。 */
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
    auto *client = new HdcSocketClient(runControl);
    /* ** 调试结束时关闭连接，设备侧命令随之收到 SIGHUP */
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
            /* ** 用嵌套 QEventLoop 代替 QThread::msleep，保持 Qt 主线程事件循环可用 */
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
            /* ** ps -ef 列顺序：UID PID PPID C STIME TTY TIME CMD ... */
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

/* ** 在本地绑定端口 0 并立即释放，返回空闲端口号。 */
static quint16 findFreePort()
{
    QTcpServer s;
    s.listen(QHostAddress::LocalHost, 0);
    const quint16 port = s.serverPort();
    s.close();
    return port;
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
        setRecipeProducer([](RunControl *runControl) -> Group {
            /*
            ** 共享持有者：由预启动任务赋值，调试器修改器读取。
            ** 注意：所有验证均推迟到 QSyncTask 函数体内执行，以确保错误信息显示在
            ** “应用程序输出”面板中（该面板仅在 RunControl::start() 之后打开——
            ** 若在此处调用 postMessage() 并返回 {}，用户将看不到任何提示）。
            */
            auto socketUrlHolder = std::make_shared<QString>();
            auto pidHolder = std::make_shared<std::atomic<qint64>>(0);
            /* ** startup-break 模式共享状态 */
            auto wsSignalFileHolder = std::make_shared<QString>();
            auto localWsPortHolder  = std::make_shared<quint16>(0);

            /* ** 基础调试参数（与设备无关） */
            DebuggerRunParameters rp = DebuggerRunParameters::fromRunControl(runControl);
            rp.setStartMode(AttachToRemoteServer);
            rp.setSkipDebugServer(true);
            rp.setUseExtendedRemote(true);
            rp.setLldbPlatform(QStringLiteral("remote-ohos"));
            rp.setUseContinueInsteadOfRun(true);
            /*
            ** 清除运行配置的可执行文件（如 hdc），避免 LLDB 将其作为 ELF/Mach-O
            ** 符号文件读取。符号通过 solib 搜索路径获取；远程可执行文件仅通过
            ** 附加 PID 标识。
            */
            rp.setInferiorExecutable({});
            rp.setSymbolFile({});

            /* ** Solib 路径：尽力而为，非关键，在 bc 可用时于此处计算 */
            {
                FilePaths solibPaths;
                if (BuildConfiguration *bc = runControl->buildConfiguration()) {
                    const FilePath bd = buildDirectory(bc);
                    if (!bd.isEmpty()) {
                        /* ** 优先添加 hvigor cmake 构建目录（与设备上运行的 so build-id 匹配）。
                        ** Qt Creator 独立 cmake 构建产生的 so 与 hvigor 打包进 HAP 的 so
                        ** build-id 不同；若 LLDB 先加载到独立构建的 so，符号地址会有偏移，
                        ** 导致断点落在错误位置、行号与源码不符。
                        ** hvigor cmake obj 目录先加入，确保 LLDB UUID 匹配到正确 binary。 */
                        const FilePath cmakeObj = bd / "ohpro/entry/build/default/intermediates/cmake/default/obj/arm64-v8a";
                        if (cmakeObj.isReadableDir())
                            solibPaths.append(cmakeObj);
                        const FilePath cmakeLibs = bd / "ohpro/entry/build/default/intermediates/libs/default/arm64-v8a";
                        if (cmakeLibs.isReadableDir())
                            solibPaths.append(cmakeLibs);
                        /* ** 再追加构建根目录（包含 Qt for OH 的运行时 so，以及
                        ** 独立 cmake 构建产物——当 hvigor 目录不存在时的后备）。 */
                        solibPaths.append(bd);
                    }
                    if (const RunConfiguration *rc = bc->activeRunConfiguration()) {
                        const FilePath wd = rc->buildTargetInfo().workingDirectory;
                        if (!wd.isEmpty())
                            solibPaths.append(wd);
                    }
                    FilePath::removeDuplicates(solibPaths);
                }
                if (!solibPaths.isEmpty())
                    rp.setSolibSearchPath(solibPaths);
            }

            /* ** 预启动任务（在 RunControl::start() 之后运行 → 面板已打开） */
            const auto preLaunch = QSyncTask([=]() -> DoneResult {
                runControl->postMessage(
                    Tr::tr("Harmony DAP debug: preparing…"),
                    NormalMessageFormat);

                /* ** 验证先决条件 */
                BuildConfiguration *bc = runControl->buildConfiguration();
                if (!bc) {
                    runControl->postMessage(
                        Tr::tr("Harmony DAP debug: no build configuration."),
                        ErrorMessageFormat);
                    return DoneResult::Error;
                }

                const FilePath hdc = HarmonyConfig::hdcToolPath();
                if (!hdc.isExecutableFile()) {
                    runControl->postMessage(
                        Tr::tr("Harmony DAP debug: hdc was not found (check HarmonyOS kit settings)."),
                        ErrorMessageFormat);
                    return DoneResult::Error;
                }

                const QString serial = harmonyEffectiveDeviceSerial(bc);
                if (serial.isEmpty()) {
                    runControl->postMessage(
                        Tr::tr("Harmony DAP debug: no target device serial (is a device connected?)."),
                        ErrorMessageFormat);
                    return DoneResult::Error;
                }

                const QString bundle = packageName(bc);
                if (bundle.isEmpty()) {
                    runControl->postMessage(
                        Tr::tr("Harmony DAP debug: bundle/package name is unknown (check project settings)."),
                        ErrorMessageFormat);
                    return DoneResult::Error;
                }

                /*
                ** 将首选 ABI 映射到 OHOS triple（使用 contains() + 回退机制）。
                ** hapDevicePreferredAbi() 可能返回 Android 风格（如 "arm64-v8a"）、
                ** OHOS triple（如 "aarch64-linux-ohos"）或空字符串。
                */
                const QString preferredAbi = hapDevicePreferredAbi(bc);
                QString ohosTriple;
                if (preferredAbi.contains(QLatin1String("aarch64")) || preferredAbi.contains(QLatin1String("arm64")))
                    ohosTriple = QStringLiteral("aarch64-linux-ohos");
                else if (!preferredAbi.isEmpty() && preferredAbi.contains(QLatin1String("arm")))
                    ohosTriple = QStringLiteral("arm-linux-ohos");
                else if (preferredAbi.contains(QLatin1String("x86_64")))
                    ohosTriple = QStringLiteral("x86_64-linux-ohos");
                else {
                    /* ** 无法确定 ABI —— 回退到 arm64（实体设备几乎总是 ARM64） */
                    runControl->postMessage(
                        Tr::tr("Harmony DAP debug: unknown ABI '%1', defaulting to aarch64-linux-ohos.")
                            .arg(preferredAbi),
                        NormalMessageFormat);
                    ohosTriple = QStringLiteral("aarch64-linux-ohos");
                }

                const FilePath lldbServer = lldbServerForAbi(ohosTriple);
                if (!lldbServer.isExecutableFile()) {
                    runControl->postMessage(
                        Tr::tr("Harmony DAP debug: lldb-server not found for triple '%1' "
                               "(searched DevEco SDK and registered NDK paths).")
                            .arg(ohosTriple),
                        ErrorMessageFormat);
                    return DoneResult::Error;
                }

                /* ** 计算设备路径常量 */
                const QString ability = defaultHarmonyAbilityName(bc);
                const QString remoteRoot = QStringLiteral("/data/local/tmp/debugserver/") + bundle;
                const QString remoteLldbServer = remoteRoot + QStringLiteral("/lldb-server"); /* ** 目标文件名必须为 lldb-server（SELinux 强制要求）*/

                /*
                ** 抽象套接字目录：使用 /<bundle> 前缀，与可用 CLI 脚本保持一致。
                ** 须在 Linux 抽象套接字 108 字节限制内。
                */
                QString socketDir = u'/' + bundle;
                if (socketDir.size() > 70)
                    socketDir = u'/' + socketDir.right(69);

                const QString socketName = QStringLiteral("platform-%1.sock")
                                               .arg(QDateTime::currentMSecsSinceEpoch());
                /*
                ** 连接 URL：unix-abstract-connect://[SERIAL]/bundle/socket.sock
                ** [SERIAL] 是 OHOS LLDB 通过 hdc 选择目标设备的方式。
                */
                *socketUrlHolder = ohosAbstractSocketConnectUrl(serial, socketDir, socketName);

                /* ** 读取 startup-break 选项（默认 true：启动断点模式）。
                ** BoolAspect 默认值为 true，saveToMap 对等于默认值的项不写入 Store，
                ** 因此 Store 中无 key == 用户未修改 == 使用默认值 true。 */
                bool startupBreak = true; /* ** 与 BoolAspect::setDefaultValue(true) 一致 */
                {
                    const Store sd = runControl->settingsData(Id(Constants::HARMONY_DEBUG_STARTUP_BREAK));
                    for (auto it = sd.constBegin(); it != sd.constEnd(); ++it) {
                        if (it.value().isValid()) {
                            startupBreak = it.value().toBool();
                            break;
                        }
                    }
                }

                /* ** 步骤 1：强制停止残留实例 */
                runControl->postMessage(Tr::tr("Harmony DAP debug: step 1 — force stop"), NormalMessageFormat);
                hdcShell(serial, QStringLiteral("aa force-stop '") + bundle + u'\'', 10000);
                { QEventLoop l; QTimer::singleShot(400, &l, &QEventLoop::quit); l.exec(); }

                /* ** 步骤 2：创建远程调试目录 */
                runControl->postMessage(Tr::tr("Harmony DAP debug: step 2 — prepare remote dir"), NormalMessageFormat);
                const HdcResult mkRes = hdcShell(
                    serial,
                    QString("mkdir -p '%1' && chmod 757 '%1'").arg(remoteRoot));
                if (!mkRes.ok) {
                    runControl->postMessage(
                        Tr::tr("Harmony DAP debug: failed to create remote dir: %1").arg(mkRes.error),
                        ErrorMessageFormat);
                    return DoneResult::Error;
                }

                /* ** 步骤 3：杀死残留 lldb-server 实例后直接推送（不使用 mv）。
                ** mv 在 OHOS 上因 SELinux 标签限制无效（hdc file send 设置了安全标签）。
                ** 先按 PID 强制终止残留进程（文件锁），再直接覆盖目标文件名。 */
                runControl->postMessage(Tr::tr("Harmony DAP debug: step 3 — push lldb-server"), NormalMessageFormat);
                hdcShell(serial,
                    QStringLiteral("for p in $(ps -ef | grep lldb-server | grep -v grep | awk '{print $2}');"
                                   " do kill -9 $p 2>/dev/null || true; done; sleep 0.3"),
                    10000);
                const HdcResult sendRes = hdcFileSend(serial, lldbServer, remoteLldbServer);
                if (!sendRes.ok) {
                    runControl->postMessage(
                        Tr::tr("Harmony DAP debug: failed to push lldb-server: %1").arg(sendRes.error),
                        ErrorMessageFormat);
                    return DoneResult::Error;
                }
                hdcShell(serial,
                    QString("chmod 755 '%1'; rm -f '%2'/*.log").arg(remoteLldbServer, remoteRoot),
                    10000);
                runControl->postMessage(
                    Tr::tr("Harmony DAP debug: lldb-server pushed from: %1").arg(lldbServer.toUserOutput()),
                    NormalMessageFormat);

                /* ** 步骤 4：启动应用 */
                runControl->postMessage(
                    Tr::tr("Harmony DAP debug: step 4 — %1")
                        .arg(startupBreak
                             ? Tr::tr("aa start -D (startup-break mode)")
                             : Tr::tr("aa start")),
                    NormalMessageFormat);
                const QString aaStartCmd = startupBreak
                    ? (ability.isEmpty()
                        ? QString("aa start -D -b %1").arg(bundle)
                        : QString("aa start -D -a %1 -b %2").arg(ability, bundle))
                    : (ability.isEmpty()
                        ? QString("aa start -b %1").arg(bundle)
                        : QString("aa start -a %1 -b %2").arg(ability, bundle));
                const HdcResult startRes = hdcShell(serial, aaStartCmd, 60000);
                if (!startRes.ok) {
                    runControl->postMessage(
                        Tr::tr("Harmony DAP debug: aa start warning: %1").arg(startRes.error),
                        NormalMessageFormat); /* ** 非致命错误 */
                }
                { QEventLoop l; QTimer::singleShot(500, &l, &QEventLoop::quit); l.exec(); }

                /* ** 步骤 5：轮询 PID */
                runControl->postMessage(Tr::tr("Harmony DAP debug: step 5 — poll app PID"), NormalMessageFormat);
                const qint64 pid = pollAppPid(serial, bundle, 20);
                if (pid == 0) {
                    runControl->postMessage(
                        Tr::tr("Harmony DAP debug: could not determine app PID after launch."),
                        ErrorMessageFormat);
                    return DoneResult::Error;
                }
                runControl->postMessage(
                    Tr::tr("Harmony DAP debug: app PID %1.").arg(pid),
                    NormalMessageFormat);

                /* ** 步骤 5b：ArkTS 端口转发 + 启动 WebSocket 桥接进程（仅 startup-break 模式） */
                QString wsSignalFile;
                if (startupBreak) {
                    runControl->postMessage(
                        Tr::tr("Harmony DAP debug: step 5b — ArkTS port-forward + bridge"),
                        NormalMessageFormat);

                    const quint16 localWsPort    = findFreePort();
                    const quint16 localPandaPort = findFreePort();
                    *localWsPortHolder = localWsPort;

                    const FilePath hdc2 = HarmonyConfig::hdcToolPath();
                    QStringList fportArgs1 = hdcSelector(serial);
                    fportArgs1 << QStringLiteral("fport")
                               << QStringLiteral("tcp:%1").arg(localWsPort)
                               << QStringLiteral("ark:%1@%2").arg(pid).arg(bundle);
                    Process fport1;
                    fport1.setCommand({hdc2, fportArgs1});
                    fport1.start();
                    fport1.waitForFinished(std::chrono::seconds(10));
                    runControl->postMessage(
                        Tr::tr("Harmony DAP debug: fport tcp:%1 ark:%2@%3 → %4")
                            .arg(localWsPort).arg(pid).arg(bundle).arg(fport1.allOutput().trimmed()),
                        NormalMessageFormat);

                    QStringList fportArgs2 = hdcSelector(serial);
                    fportArgs2 << QStringLiteral("fport")
                               << QStringLiteral("tcp:%1").arg(localPandaPort)
                               << QStringLiteral("ark:%1@Debugger").arg(pid);
                    Process fport2;
                    fport2.setCommand({hdc2, fportArgs2});
                    fport2.start();
                    fport2.waitForFinished(std::chrono::seconds(10));
                    runControl->postMessage(
                        Tr::tr("Harmony DAP debug: fport tcp:%1 ark:%2@Debugger → %3")
                            .arg(localPandaPort).arg(pid).arg(fport2.allOutput().trimmed()),
                        NormalMessageFormat);

                    /* ** 创建临时目录存放信号文件 */
                    const QString tmpDir = QDir::tempPath()
                        + QStringLiteral("/harmony_debug_%1").arg(QDateTime::currentMSecsSinceEpoch());
                    QDir().mkpath(tmpDir);
                    wsSignalFile = tmpDir + QStringLiteral("/signal");
                    *wsSignalFileHolder = wsSignalFile;

                    /* ** 通过 QProcess::startDetached 在独立子进程中启动 ArkTSDebugBridge。
                    ** 二进制安装在 libexec 目录下（与 perfparser、iostool 等同级），
                    ** 使用 ICore::libexecPath() 定位，兼容所有平台和沙盒构建。 */
                    const QString bridgeBin = Core::ICore::libexecPath(
                        QStringLiteral("arktsdebugbridge")).toUrlishString();
                    qint64 bridgePid = 0;
                    const bool bridgeStarted = QProcess::startDetached(
                        bridgeBin,
                        {QString::number(localWsPort),
                         QString::number(localPandaPort),
                         wsSignalFile},
                        QString(),
                        &bridgePid);
                    if (bridgeStarted) {
                        runControl->postMessage(
                            Tr::tr("Harmony DAP debug: bridge started (PID %1), connecting to ws://127.0.0.1:%2…")
                                .arg(bridgePid).arg(localWsPort),
                            NormalMessageFormat);
                        /* ** 等待 2 秒让桥接器完成 L1 WebSocket 握手 */
                        { QEventLoop l; QTimer::singleShot(2000, &l, &QEventLoop::quit); l.exec(); }
                    } else {
                        runControl->postMessage(
                            Tr::tr("Harmony DAP debug: bridge binary not found (%1). "
                                   "ArkTS startup-break may not work.").arg(bridgeBin),
                            NormalMessageFormat); /* ** 非致命：可回退到手动 continue */
                    }
                }

                /* ** 步骤 6：注入 lldb-server（aa process -D） */
                runControl->postMessage(Tr::tr("Harmony DAP debug: step 6 — inject lldb-server"), NormalMessageFormat);
                const QString lldbListenUrl = QStringLiteral("unix-abstract://") + socketDir + u'/' + socketName;
                const QString aaProcessCmd = ability.isEmpty()
                    ? QString("aa process -b %1 -D \"%2 platform --listen %3 --log-file %4/platform.log\"")
                          .arg(bundle, remoteLldbServer, lldbListenUrl, remoteRoot)
                    : QString("aa process -a %1 -b %2 -D \"%3 platform --listen %4 --log-file %5/platform.log\"")
                          .arg(ability, bundle, remoteLldbServer, lldbListenUrl, remoteRoot);
                runControl->postMessage(
                    Tr::tr("Harmony DAP debug: injecting: %1").arg(aaProcessCmd),
                    NormalMessageFormat);
                const HdcResult processRes = hdcShell(serial, aaProcessCmd, 30000);
                if (!processRes.ok) {
                    runControl->postMessage(
                        Tr::tr("Harmony DAP debug: aa process warning: %1").arg(processRes.error),
                        NormalMessageFormat); /* ** 非致命：某些设备在调度前已启动 */
                }

                /* ** 轮询 lldb-server 进程稳定（最多 8 秒） */
                const QString lldbPsCmd = QStringLiteral(
                    "ps -ef 2>/dev/null | grep lldb-server | grep '%1' | grep -v grep")
                    .arg(socketName);
                QString lldbPsOut;
                constexpr int kLldbPollIntervalMs = 500;
                constexpr int kLldbPollMaxAttempts = 16;
                for (int attempt = 0; attempt < kLldbPollMaxAttempts; ++attempt) {
                    { QEventLoop l; QTimer::singleShot(kLldbPollIntervalMs, &l, &QEventLoop::quit); l.exec(); }
                    const HdcResult psRes = hdcShell(serial, lldbPsCmd, 5000);
                    lldbPsOut = psRes.output.trimmed();
                    if (!lldbPsOut.isEmpty()) {
                        runControl->postMessage(
                            Tr::tr("Harmony DAP debug: lldb-server confirmed (attempt %1): %2")
                                .arg(attempt + 1).arg(lldbPsOut),
                            NormalMessageFormat);
                        break;
                    }
                }
                if (lldbPsOut.isEmpty()) {
                    runControl->postMessage(
                        Tr::tr("Harmony DAP debug: WARNING — lldb-server with socket '%1' not seen after 8 s. "
                               "Check the HAP is a debug build and SELinux allows /data/local/tmp/debugserver/.")
                            .arg(socketName),
                        ErrorMessageFormat);
                } else {
                    { QEventLoop l; QTimer::singleShot(500, &l, &QEventLoop::quit); l.exec(); }
                }

                runControl->postMessage(
                    Tr::tr("Harmony DAP debug: connecting — socket=%1").arg(*socketUrlHolder),
                    NormalMessageFormat);
                *pidHolder = pid;
                return DoneResult::Success;
            });

            /*
            ** 任务树：先运行预启动任务，再启动调试器。
            ** 修改器在调试器引擎启动前被调用；socketUrlHolder、pidHolder、
            ** wsSignalFileHolder 均由 preLaunch 任务赋值。
            ** commandsAfterConnect：LLDB attach 之后、用户可操作之前执行的命令：
            **   - SIGILL/SIGRTMIN+1/SIGPIPE 信号处理（与 CLI 脚本一致）
            **   - startup-break 模式：创建信号文件（触发 ArkTS 解锁链），
            **     消息写入内核缓冲区，等待 process continue 后由 ArkTS 读取。
            */
            return Group{
                preLaunch,
                debuggerRecipe(runControl, rp,
                    [socketUrlHolder, pidHolder, wsSignalFileHolder](DebuggerRunParameters &params) {
                        params.setRemoteChannel(*socketUrlHolder);
                        params.setAttachPid(ProcessHandle(static_cast<qint64>(*pidHolder)));

                        /* ** 将信号文件路径通过 workingDirectory 字段传递给 lldbbridge.py。
                        ** LLDB attach 模式下 workingDirectory 不用于启动进程，可安全复用。
                        ** lldbbridge.py 的 OHOS runEngine 块将在 attach 成功后读取此路径并创建文件，
                        ** 触发 Bridge → ArkTS → Runtime.runIfWaitingForDebugger 解锁链。 */
                        if (!wsSignalFileHolder->isEmpty()) {
                            auto inferior = params.inferior();
                            inferior.workingDirectory = Utils::FilePath::fromString(*wsSignalFileHolder);
                            params.setInferior(inferior);
                        }

                        params.setCommandsAfterConnect(QString());
                    })
            };
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
