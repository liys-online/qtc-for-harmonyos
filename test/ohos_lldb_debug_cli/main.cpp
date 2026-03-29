// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "arktsdebugbridge.h"
#include "hdcsocketclient.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTcpServer>
#include <QHostAddress>
#include <QTextStream>
#include <QThread>

#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <vector>
#include <string>

using namespace Ohos::Internal;

/* ** 全局状态 */────

/* ** g_hdcPath 仍需用于仅 CLI 操作：hdc list targets、file send、fport。 */
static QString g_hdcPath;

/* ** 日志 */──

static void log(const QString &msg)
{
    std::cout << "[INFO] " << msg.toStdString() << "\n";
    std::cout.flush();
}

static void err(const QString &msg)
{
    std::cerr << "[ERROR] " << msg.toStdString() << "\n";
    std::cerr.flush();
}

/* ** 命令执行器 */

struct CmdResult
{
    int     exitCode = -1;
    QString out;
    QString errOut;
};

static CmdResult runCmd(const QString &program, const QStringList &args, int timeoutMs = 30000)
{
    QProcess proc;
    proc.start(program, args);
    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        return {-1, {}, QStringLiteral("timeout")};
    }
    auto stripCR = [](QByteArray raw) -> QString {
        return QString::fromLocal8Bit(raw).replace(QLatin1String("\r\n"), QLatin1String("\n"))
                                          .replace(QLatin1Char('\r'), QLatin1Char('\n'));
    };
    return {proc.exitCode(), stripCR(proc.readAllStandardOutput()),
            stripCR(proc.readAllStandardError())};
}

/* ** 为仅 CLI 的 hdc 子命令（file send、fport、list）构建命令行参数。 */
static QStringList hdcCliArgs(const QString &serial, const QStringList &args)
{
    QStringList full;
    if (!serial.isEmpty())
        full << QStringLiteral("-t") << serial;
    full << args;
    return full;
}

/*
** 通过 HdcSocketClient（TCP 守护进程）在设备上执行 shell 命令，
** 当守护进程不可用时自动回退到 hdc CLI 二进制文件。
*/
static HdcShellSyncResult runShell(const QString &serial, const QString &shellCmd,
                                   int timeoutMs = 30000)
{
    const QString daemonCmd = QStringLiteral("shell ") + shellCmd;
    const QStringList cliArgs = hdcCliArgs(serial, {QStringLiteral("shell"), shellCmd});
    return HdcSocketClient::runSyncWithCliFallback(
        serial, daemonCmd, g_hdcPath, cliArgs, timeoutMs);
}

/* ** 端口辅助函数 */

static quint16 findFreePort()
{
    QTcpServer srv;
    srv.listen(QHostAddress::LocalHost, 0);
    quint16 port = srv.serverPort();
    srv.close();
    return port;
}

/* ** hdc 路径检测 */

static QString autoDetectHdc()
{
    /* ** 优先检查 PATH */
    CmdResult r = runCmd(QStringLiteral("which"), {QStringLiteral("hdc")}, 5000);
    if (r.exitCode == 0 && !r.out.trimmed().isEmpty())
        return r.out.trimmed();

    /* ** 回退到已知的 DevEco 安装位置 */
    const QString home = QDir::homePath();
    const QStringList candidates = {
        home + QStringLiteral("/Library/OpenHarmony/Sdk/20/toolchains/hdc"),
        home + QStringLiteral("/Library/OpenHarmony/Sdk/15/toolchains/hdc"),
    };
    for (const QString &c : candidates) {
        if (QFileInfo(c).isExecutable())
            return c;
    }
    return {};
}

/* ** 断点文件解析器 */

/*
** 尝试将裸文件名解析为绝对路径，通过搜索每个符号目录的祖父目录
*/
static QString resolveBreakpointFile(const QString &file, const QStringList &symbolDirs)
{
    if (file.startsWith(QLatin1Char('/')))
        return file; /* ** 已是绝对路径，直接返回 */

    for (const QString &sd : symbolDirs) {
        /* ** 搜索根 = 符号目录的祖父目录 */
        const QString searchRoot = QFileInfo(QFileInfo(sd).dir().absolutePath()).dir().absolutePath();
        if (searchRoot == QLatin1String("/") || searchRoot == QLatin1String("/tmp")
            || searchRoot == QLatin1String("/private/tmp"))
            continue;

        QDirIterator it(searchRoot, {file}, QDir::Files, QDirIterator::Subdirectories);
        if (it.hasNext())
            return it.next();
    }
    return file; /* ** 未找到时原样返回 */
}

/* ** ────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ohos_lldb_debug_cli"));

    /*
    ** 桥接模式提前退出：
    ** 由正常流程通过 QProcess::startDetached() 在独立子进程中启动，
    ** 用于执行两级 ArkTS 解锁协议。
    ** argv: <exe> --bridge-mode <connectPort> <pandaPort> <signalFile>
    */
    if (argc == 5 && QByteArray(argv[1]) == "--bridge-mode") {
        const quint16 connectPort = QByteArray(argv[2]).toUShort();
        const quint16 pandaPort   = QByteArray(argv[3]).toUShort();
        const QString signalFile  = QString::fromLocal8Bit(argv[4]);
        ArkTSDebugBridge bridge;
        QObject::connect(&bridge, &ArkTSDebugBridge::logMessage, [](const QString &m) {
            std::cout << m.toStdString() << "\n";
            std::cout.flush();
        });
        QObject::connect(&bridge, &ArkTSDebugBridge::errorOccurred, [](const QString &m) {
            std::cerr << m.toStdString() << "\n";
            std::cerr.flush();
        });
        QObject::connect(&bridge, &ArkTSDebugBridge::finished, qApp, &QCoreApplication::quit);
        bridge.start(connectPort, pandaPort, signalFile);
        return app.exec();
    }

    /* ** 参数解析 */

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("OHOS C++ command-line debug helper (LLDB)\n\n"
                        "Usage:\n"
                        "  ohos_lldb_debug_cli --bundle <name> --ability <name> [options]\n\n"
                        "Mirrors ohos_lldb_cli_debug.sh. See that script for full documentation."));
    parser.addHelpOption();

    QCommandLineOption bundleOpt(
        QStringLiteral("bundle"),
        QStringLiteral("App bundle name, e.g. com.example.demo"),
        QStringLiteral("name"));
    QCommandLineOption abilityOpt(
        QStringLiteral("ability"),
        QStringLiteral("Ability name, e.g. EntryAbility"),
        QStringLiteral("name"));
    QCommandLineOption serialOpt(
        QStringLiteral("serial"),
        QStringLiteral("Device serial from `hdc list targets`"),
        QStringLiteral("id"));
    QCommandLineOption lldbServerOpt(
        QStringLiteral("lldb-server"),
        QStringLiteral("Local lldb-server path (aarch64-linux-ohos)"),
        QStringLiteral("path"));
    QCommandLineOption lldbBinOpt(
        QStringLiteral("lldb-bin"),
        QStringLiteral("Local lldb binary (must support remote-ohos platform)"),
        QStringLiteral("path"));
    QCommandLineOption symbolDirOpt(
        QStringLiteral("symbol-dir"),
        QStringLiteral("Local symbol search dir (.so). Repeatable."),
        QStringLiteral("path"));
    QCommandLineOption logChannelsOpt(
        QStringLiteral("log-channels"),
        QStringLiteral("LLDB log channels (default: \"lldb process:gdb-remote packets\")"),
        QStringLiteral("value"));
    QCommandLineOption skipForceStopOpt(
        QStringLiteral("skip-force-stop"),
        QStringLiteral("Do not run `aa force-stop <bundle>` before launch"));
    QCommandLineOption startupBreakOpt(
        QStringLiteral("startup-break"),
        QStringLiteral("Hit C++ startup breakpoints (main.cpp etc.) using aa start -D + WebSocket"));
    QCommandLineOption breakOpt(
        QStringLiteral("break"),
        QStringLiteral("Pre-set breakpoint FILE:LINE for --startup-break mode. Repeatable."),
        QStringLiteral("FILE:LINE"));

    parser.addOption(bundleOpt);
    parser.addOption(abilityOpt);
    parser.addOption(serialOpt);
    parser.addOption(lldbServerOpt);
    parser.addOption(lldbBinOpt);
    parser.addOption(symbolDirOpt);
    parser.addOption(logChannelsOpt);
    parser.addOption(skipForceStopOpt);
    parser.addOption(startupBreakOpt);
    parser.addOption(breakOpt);

    parser.process(app);

    if (!parser.isSet(bundleOpt) || !parser.isSet(abilityOpt)) {
        err(QStringLiteral("--bundle and --ability are required"));
        parser.showHelp(1);
    }

    const QString bundle         = parser.value(bundleOpt);
    const QString ability        = parser.value(abilityOpt);
    QString       serial         = parser.value(serialOpt);
    const bool    skipForceStop  = parser.isSet(skipForceStopOpt);
    const bool    startupBreak   = parser.isSet(startupBreakOpt);
    const QStringList startupBPs = parser.values(breakOpt);
    const QStringList symbolDirs = parser.values(symbolDirOpt);
    const QString logChannels    = parser.isSet(logChannelsOpt)
                                       ? parser.value(logChannelsOpt)
                                       : QStringLiteral("lldb process:gdb-remote packets");

    const QString home          = QDir::homePath();
    const QString sdk15Base     = home + QStringLiteral("/Library/OpenHarmony/Sdk/15");
    const QString defaultLldbServer =
        QStringLiteral("/Applications/DevEco-Studio.app/Contents/sdk/default/hms/native/lldb/"
                       "aarch64-linux-ohos/lldb-server");
    const QString defaultLldbBin = sdk15Base + QStringLiteral("/native/llvm/bin/lldb");

    const QString lldbServerLocal =
        parser.isSet(lldbServerOpt) ? parser.value(lldbServerOpt) : defaultLldbServer;
    const QString lldbBin =
        parser.isSet(lldbBinOpt) ? parser.value(lldbBinOpt) : defaultLldbBin;

    /* ** 验证二进制文件 */

    if (!QFile::exists(lldbServerLocal)) {
        err(QStringLiteral("lldb-server not found: ") + lldbServerLocal);
        err(QStringLiteral("Pass --lldb-server <path/to/aarch64-linux-ohos/lldb-server>"));
        return 1;
    }
    {
        const QFileInfo fi(lldbBin);
        if (!fi.exists() || !fi.isExecutable()) {
            err(QStringLiteral("lldb not found or not executable: ") + lldbBin);
            err(QStringLiteral("Pass --lldb-bin <path/to/lldb>  "
                               "(must be from SDK 15 or later that supports remote-ohos)"));
            return 1;
        }
    }

    /* ** 验证 lldb 是否支持 remote-ohos 平台 */
    {
        CmdResult r = runCmd(lldbBin, {QStringLiteral("-o"), QStringLiteral("platform list"),
                                        QStringLiteral("-o"), QStringLiteral("quit")},
                             10000);
        const QString combined = r.out + r.errOut;
        if (!combined.contains(QStringLiteral("remote-ohos"))) {
            err(QStringLiteral("The lldb at '") + lldbBin
                + QStringLiteral("' does not support the remote-ohos platform."));
            err(QStringLiteral("Use SDK 15's lldb: ") + home
                + QStringLiteral("/Library/OpenHarmony/Sdk/15/native/llvm/bin/lldb"));
            return 1;
        }
    }

    /* ** 定位 hdc */

    g_hdcPath = autoDetectHdc();
    if (g_hdcPath.isEmpty()) {
        err(QStringLiteral("hdc not found. Install DevEco Studio or add hdc to PATH."));
        return 1;
    }

    /* ** 验证符号目录 */

    for (const QString &d : symbolDirs) {
        if (!QDir(d).exists()) {
            err(QStringLiteral("symbol dir not found: ") + d);
            return 1;
        }
    }

    /* ** 自动检测设备序列号 */

    if (serial.isEmpty()) {
        CmdResult r = runCmd(g_hdcPath, {QStringLiteral("list"), QStringLiteral("targets")});
        QStringList targets;
        for (const QString &line : r.out.split(QLatin1Char('\n'))) {
            const QString t = line.trimmed();
            if (!t.isEmpty() && t != QLatin1String("Empty"))
                targets << t;
        }
        if (targets.isEmpty()) {
            err(QStringLiteral("No OHOS device found. Check USB/network and run: hdc list targets"));
            return 1;
        }
        if (targets.size() > 1) {
            err(QStringLiteral("Multiple devices found, please pass --serial"));
            for (const QString &t : std::as_const(targets))
                std::cout << t.toStdString() << "\n";
            return 1;
        }
        serial = targets.first();
    }

    log(QStringLiteral("Device serial  : ") + serial);
    log(QStringLiteral("LLDB binary    : ") + lldbBin);
    log(QStringLiteral("lldb-server    : ") + lldbServerLocal);

    /* ** 套接字/路径设置 */

    QString socketDir = QLatin1Char('/') + bundle;
    if (socketDir.size() > 70)
        socketDir = QLatin1Char('/') + socketDir.right(69);

    const QString socketName = QStringLiteral("platform-%1.sock")
                                   .arg(QDateTime::currentSecsSinceEpoch());
    const QString connectUrl =
        QStringLiteral("unix-abstract-connect://[%1]%2/%3").arg(serial, socketDir, socketName);

    const QString remoteRoot = QStringLiteral("/data/local/tmp/debugserver/") + bundle;
    const QString remoteLs   = remoteRoot + QStringLiteral("/lldb-server"); /* ** 目标文件名必须严格为 lldb-server */

    const QString scriptDir     = QCoreApplication::applicationDirPath();
    const QString lldbCmdsFile  = scriptDir + QStringLiteral("/ohos_lldb_cmds.lldb");

    /* ** 步骤 1：强制停止 */

    if (!skipForceStop) {
        log(QStringLiteral("Step 1: aa force-stop"));
        runShell(serial, QStringLiteral("aa force-stop '") + bundle + QLatin1Char('\''));
        QThread::msleep(500);
    }

    /* ** 步骤 2：准备远程目录 */

    log(QStringLiteral("Step 2: prepare remote directory"));
    runShell(serial,
             QStringLiteral("mkdir -p '") + remoteRoot
                 + QStringLiteral("' && chmod 757 '") + remoteRoot + QLatin1Char('\''));

    /*
    ** 步骤 3：推送 lldb-server。
    ** 先终止残留实例（文件锁），然后直接发送到目标名称
    **（mv 技巧在 OHOS 上因 hdc file send 设置 SELinux 标签而无效）。
    */
    log(QStringLiteral("Step 3: push lldb-server"));
    runShell(serial,
             QStringLiteral("for p in $(ps -ef | grep lldb-server | grep -v grep | awk '{print $2}');"
                            " do kill -9 $p 2>/dev/null || true; done; sleep 0.3"));
    /* ** file send 无 socket 等价命令——使用 CLI。 */
    runCmd(g_hdcPath, hdcCliArgs(serial, {QStringLiteral("file"), QStringLiteral("send"),
                                          lldbServerLocal, remoteLs}));
    runShell(serial,
             QStringLiteral("chmod 755 '") + remoteLs
                 + QStringLiteral("'; rm -f '") + remoteRoot + QStringLiteral("'/*.log"));

    /* ** 步骤 4：启动应用 */

    QString clientGdbLog;
    QString wsSignalFile;
    QString localWsPortStr;
    QString localPandaPortStr;

    if (startupBreak) {
        log(QStringLiteral("Step 4: aa start -D -a ") + ability
            + QStringLiteral(" -b ") + bundle + QStringLiteral("  (startup-break mode)"));
        runShell(serial, QStringLiteral("aa start -D -a ") + ability + QStringLiteral(" -b ") + bundle);
    } else {
        log(QStringLiteral("Step 4: aa start -a ") + ability + QStringLiteral(" -b ") + bundle);
        runShell(serial, QStringLiteral("aa start -a ") + ability + QStringLiteral(" -b ") + bundle);
    }

    /*
    ** 步骤 5：轮询应用 PID。
    ** OHOS shell 无 `tr` 命令；在主机侧去除 \r
    **（HdcSocketClient 通过 UTF-8 帧处理此问题）。
    */
    log(QStringLiteral("Step 5: resolving app PID"));
    QString pid;
    for (int i = 0; i < 20 && pid.isEmpty(); ++i) {
        const HdcShellSyncResult ps = runShell(serial, QStringLiteral("ps -ef"));
        for (const QString &line : ps.standardOutput.split(QLatin1Char('\n'))) {
            if (!line.contains(bundle) || line.contains(QLatin1String("grep"))
                || line.contains(QLatin1String("lldb")))
                continue;
            const QStringList parts =
                line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                pid = parts.at(1);
                break;
            }
        }
        if (pid.isEmpty())
            QThread::msleep(500);
    }

    if (pid.isEmpty()) {
        err(QStringLiteral("Could not resolve PID for ") + bundle);
        return 1;
    }
    log(QStringLiteral("App PID: ") + pid);

    /* ** 步骤 5b：ArkTS 调试套接字端口转发（仅限启动断点模式）*/

    if (startupBreak) {
        const quint16 localWsPort    = findFreePort();
        const quint16 localPandaPort = findFreePort();
        localWsPortStr    = QString::number(localWsPort);
        localPandaPortStr = QString::number(localPandaPort);

        log(QStringLiteral("Step 5b: forward ArkTS debug socket → localhost:") + localWsPortStr);

        /* ** fport 无 socket 等价命令——使用 CLI。 */
        runCmd(g_hdcPath, hdcCliArgs(serial, {QStringLiteral("fport"),
                                              QStringLiteral("tcp:") + localWsPortStr,
                                              QStringLiteral("ark:") + pid + QLatin1Char('@') + bundle}));
        log(QStringLiteral("  hdc fport tcp:") + localWsPortStr
            + QStringLiteral(" ark:") + pid + QLatin1Char('@') + bundle
            + QStringLiteral("  → OK"));

        runCmd(g_hdcPath, hdcCliArgs(serial, {QStringLiteral("fport"),
                                              QStringLiteral("tcp:") + localPandaPortStr,
                                              QStringLiteral("ark:") + pid + QStringLiteral("@Debugger")}));
        log(QStringLiteral("  hdc fport tcp:") + localPandaPortStr
            + QStringLiteral(" ark:") + pid + QStringLiteral("@Debugger  → OK"));

        /* ** gdb-remote 日志和 ws 信号文件的临时目录 */
        const QString tmpDir =
            QDir::tempPath() + QStringLiteral("/ohos_lldb_")
            + QString::number(QDateTime::currentMSecsSinceEpoch());
        QDir().mkpath(tmpDir);
        clientGdbLog = tmpDir + QStringLiteral("/gdb-remote-client.log");
        wsSignalFile = tmpDir + QStringLiteral("/signal");

        /*
        ** 通过 startDetached 在独立子进程中启动 ArkTSDebugBridge。
        ** QProcess::startDetached 内部执行 fork+exec，为子进程提供全新的进程映像——
        ** 避免 macOS 上 Qt 基于 CoreFoundation 的事件循环的 fork 安全问题。
        */
        const QString appPath = QCoreApplication::applicationFilePath();
        qint64 bridgePid = 0;
        const bool bridgeStarted = QProcess::startDetached(
            appPath,
            {QStringLiteral("--bridge-mode"), localWsPortStr, localPandaPortStr, wsSignalFile},
            QString(),
            &bridgePid);
        if (!bridgeStarted) {
            err(QStringLiteral("Failed to start ArkTSDebugBridge subprocess"));
            return 1;
        }

        /*
        ** 给桥接进程 2 秒建立 L1 WebSocket，再执行 'process attach'
        **（'process attach' 会冻结 ArkTS 线程。
        */
        log(QStringLiteral("  ArkTSDebugBridge started (PID %1), connecting to ws://127.0.0.1:%2...")
                .arg(bridgePid)
                .arg(localWsPortStr));
        QThread::msleep(2000);
    }

    /*
    ** 步骤 6：注入 lldb-server（aa process -D）。
    ** 在已运行的应用内启动原生调试协进程。
    ** 套接字路径使用 unix-abstract://（两个斜杠），连接 URL 共三个斜杠
    **（→ 抽象套接字 /bundle/name，含一个前导斜杠）。
    */
    log(QStringLiteral("Step 6: aa process -D (inject native debug server)"));
    const QString debugCmd =
        remoteLs + QStringLiteral(" platform --listen unix-abstract://")
        + socketDir + QLatin1Char('/') + socketName
        + QStringLiteral(" --log-file ") + remoteRoot + QStringLiteral("/platform.log");
    const QString aaProcessCmd =
        QStringLiteral("aa process -a ") + ability + QStringLiteral(" -b ") + bundle
        + QStringLiteral(" -D \"") + debugCmd + QLatin1Char('"');
    log(QStringLiteral("hdc -t \"") + serial + QStringLiteral("\" shell \"") + aaProcessCmd + QLatin1Char('"'));
    runShell(serial, aaProcessCmd);

    /* ** 轮询 lldb-server 进程直到稳定（最多 8 秒）*/
    for (int i = 0; i < 16; ++i) {
        const HdcShellSyncResult ps = runShell(
            serial,
            QStringLiteral("ps -ef 2>/dev/null | grep lldb-server | grep '")
                + socketName + QStringLiteral("' | grep -v grep"));
        if (!ps.standardOutput.trimmed().isEmpty()) {
            log(QStringLiteral("  lldb-server process detected for socket ") + socketName);
            break;
        }
        if (i == 15) {
            log(QStringLiteral("  WARN: lldb-server not observed in ps for socket ") + socketName
                + QStringLiteral("; continuing to connect immediately"));
            runShell(serial, QStringLiteral("ps -ef | grep lldb-server | grep -v grep"));
        }
        QThread::msleep(500);
    }
    QThread::msleep(200);

    /* ** 步骤 7：构建 LLDB 初始化文件 */

    QFile cmdsFile(lldbCmdsFile);
    if (!cmdsFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        err(QStringLiteral("Cannot write LLDB init file: ") + lldbCmdsFile);
        return 1;
    }
    QTextStream ts(&cmdsFile);

    ts << "settings set auto-confirm true\n";

    if (startupBreak && !clientGdbLog.isEmpty()) {
        ts << "log enable -T -f \"" << clientGdbLog
           << "\" gdb-remote break packets memory process\n";
    }

    /* ** 符号搜索路径（附加前设置，以便 LLDB 在附加时通过 Build-ID 匹配）*/
    for (const QString &d : symbolDirs) {
        ts << "settings append target.exec-search-paths \"" << d << "\"\n";
        const QString qtObj = d
            + QStringLiteral("/ohpro/entry/build/default/intermediates/cmake/default/obj/arm64-v8a");
        if (QDir(qtObj).exists())
            ts << "settings append target.exec-search-paths \"" << qtObj << "\"\n";
        const QString qtLibs = d
            + QStringLiteral("/ohpro/entry/build/default/intermediates/libs/default/arm64-v8a");
        if (QDir(qtLibs).exists())
            ts << "settings append target.exec-search-paths \"" << qtLibs << "\"\n";
    }

    ts << "platform select remote-ohos\n";
    ts << "platform connect \"" << connectUrl << "\"\n";
    ts << "process attach -p " << pid << "\n";
    ts << "settings set target.process.follow-fork-mode child\n";
    ts << "settings set target.process.thread.step-avoid-regexp ^std::\n";
    ts << "type format add --format boolean jboolean\n";
    ts << "process handle -p true -s false SIGILL\n";
    ts << "process handle -p false -s false -n false SIGRTMIN+1\n";
    ts << "process handle -p true -s false SIGPIPE\n";

    if (startupBreak) {
        /*
        ** 禁用 stop-on-sharedlibrary-events
        */
        ts << "settings set target.process.stop-on-sharedlibrary-events false\n";

        /* ** 附加后的额外符号路径（用于平台插件 .so）*/
        for (const QString &d : symbolDirs) {
            const QString qtObj =
                d + QStringLiteral("/ohpro/entry/build/default/intermediates/cmake/default/"
                                   "obj/arm64-v8a");
            if (QDir(qtObj).exists())
                ts << "settings append target.exec-search-paths \"" << qtObj << "\"\n";
            const QString qtLibs =
                d + QStringLiteral("/ohpro/entry/build/default/intermediates/libs/default/"
                                   "arm64-v8a");
            if (QDir(qtLibs).exists())
                ts << "settings append target.exec-search-paths \"" << qtLibs << "\"\n";
        }

        /* ** 用户断点（FILE:LINE 对）*/
        for (const QString &bp : startupBPs) {
            const int    colon  = bp.lastIndexOf(QLatin1Char(':'));
            const QString bpFile = resolveBreakpointFile(
                colon >= 0 ? bp.left(colon) : bp, symbolDirs);
            const QString bpLine = colon >= 0 ? bp.mid(colon + 1) : QStringLiteral("1");
            ts << "breakpoint set --file \"" << bpFile << "\" --line " << bpLine << "\n";
        }

        /*
        ** 在 process continue 之前创建信号文件（顺序至关重要）：
        ** 信号 → 辅助进程发送 {"type":"connected"} 到 TCP 缓冲区 → ArkTS 恢复 →
        ** 加载原生 .so → main() 运行 → 断点触发。
        */
        ts << "script import os; open('" << wsSignalFile << "', 'w').close()\n";
        ts << "process continue\n";
    }

    cmdsFile.close();

    /* ** 启动断点模式摘要 */

    if (startupBreak) {
        log(QString());
        log(QStringLiteral("startup-break mode active:"));
        log(QStringLiteral("  • ArkTS/JS VM is paused; native main() has NOT yet executed"));
        log(QStringLiteral("  • ArkTS debug socket forwarded → ws://127.0.0.1:") + localWsPortStr);
        log(QStringLiteral("  • host-side ArkTSDebugBridge will:"));
        log(QStringLiteral("    1) send {\"type\":\"connected\"} (Level 1 unblock)"));
        log(QStringLiteral("    2) wait for addInstance, then connect Panda CDT → "
                            "runIfWaitingForDebugger (Level 2)"));
        log(QStringLiteral("  • ArkTS resumes → loads native .so → main() runs → breakpoint fires"));
        log(QString());
        log(QStringLiteral("  stop-on-sharedlibrary-events is DISABLED:"));
        log(QStringLiteral("    LLDB will pause at each shared-library load "
                            "(shows 'shared library event')."));
        log(QStringLiteral("    Type  c  (continue) at each such stop until your own breakpoint fires."));
        log(QString());

        if (!startupBPs.isEmpty()) {
            log(QStringLiteral("  Pre-set breakpoints:"));
            for (const QString &bp : startupBPs) {
                const int    colon  = bp.lastIndexOf(QLatin1Char(':'));
                const QString f = colon >= 0 ? bp.left(colon) : bp;
                const QString l = colon >= 0 ? bp.mid(colon + 1) : QStringLiteral("1");
                log(QStringLiteral("    breakpoint set --file ") + f
                    + QStringLiteral(" --line ") + l);
            }
        } else {
            log(QStringLiteral("  No --break specified → set breakpoints interactively, then:"));
            log(QStringLiteral("    (lldb) breakpoint set --file main.cpp --line 66"));
            log(QStringLiteral("    (lldb) continue"));
            log(QStringLiteral("    (lldb) script import os; open('") + wsSignalFile
                + QStringLiteral("', 'w').close()"));
        }
        log(QString());
    }

    /* ** 步骤 8：启动 LLDB（替换当前进程）*/

    log(QStringLiteral("Connect URL: ") + connectUrl);
    log(QStringLiteral("Starting LLDB (type 'continue' after attach to resume app)"));

    const std::string lldbBinStd   = lldbBin.toStdString();
    const std::string lldbCmdsStd  = lldbCmdsFile.toStdString();

    std::vector<std::string> lldbArgsStd = {lldbBinStd, "-s", lldbCmdsStd};
    std::vector<char *>      lldbArgv;
    for (auto &s : lldbArgsStd)
        lldbArgv.push_back(const_cast<char *>(s.c_str()));
    lldbArgv.push_back(nullptr);

    /*
    ** execvp 用 lldb 替换当前进程映像；WebSocket 辅助子进程
    ** 继续存活（被重新挂载）并运行直到 lldb 退出。
    */
    execvp(lldbArgv[0], lldbArgv.data());

    /* ** 仅在 execvp 失败时才会到达此处 */
    err(QStringLiteral("Failed to exec lldb: ") + lldbBin);
    return 1;
}
