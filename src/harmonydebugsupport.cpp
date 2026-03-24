// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "harmonydebugsupport.h"

#include "harmonyconfigurations.h"
#include "harmonylogcategories.h"
#include "harmonyutils.h"
#include "ohosconstants.h"
#include "ohostr.h"

#include <debugger/debuggerconstants.h>
#include <debugger/debuggerengine.h>
#include <debugger/debuggeritem.h>
#include <debugger/debuggeritemmanager.h>
#include <debugger/debuggerkitaspect.h>
#include <debugger/debuggerrunconfigurationaspect.h>
#include <debugger/debuggerruncontrol.h>

#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/runcontrol.h>

#include <qtsupport/baseqtversion.h>
#include <qtsupport/qtkitaspect.h>

#include <QtTaskTree/QTaskTree>

#include <utils/commandline.h>
#include <utils/fileutils.h>
#include <utils/hostosinfo.h>
#include <utils/outputformat.h>
#include <utils/qtcprocess.h>

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QFile>
#include <QDateTime>

using namespace Debugger;
using namespace ProjectExplorer;
using namespace QtTaskTree;
using namespace Utils;

namespace Ohos::Internal {

namespace {

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

static FilePath pickHarmonySymbolFile(RunControl *runControl, BuildConfiguration *bc)
{
    if (runControl) {
        const FilePath targetFile = runControl->targetFilePath();
        if (targetFile.isReadableFile() && targetFile.fileName().endsWith(".so"))
            return targetFile;
    }

    if (!bc)
        return {};

    const FilePath ohpro = harmonyBuildDirectory(bc);
    FilePaths candidates;

    const FilePath cmakeLibDir = bc->buildDirectory().pathAppended("lib");
    if (cmakeLibDir.isReadableDir())
        candidates.append(cmakeLibDir.dirEntries({{"*.so"}, QDir::Files | QDir::NoDotAndDotDot}));

    const QString preferredAbi = harmonyDebuggerPreferredAbi(bc);
    if (!preferredAbi.isEmpty()) {
        const FilePath entryLibDir = ohpro.pathAppended("entry/libs").pathAppended(preferredAbi);
        if (entryLibDir.isReadableDir())
            candidates.append(entryLibDir.dirEntries({{"*.so"}, QDir::Files | QDir::NoDotAndDotDot}));
    }

    FilePaths hvigorDirs;
    appendHvigorIntermediatesLibDirs(ohpro, &hvigorDirs);
    for (const FilePath &dir : hvigorDirs)
        candidates.append(dir.dirEntries({{"*.so"}, QDir::Files | QDir::NoDotAndDotDot}));

    FilePath::removeDuplicates(candidates);
    if (candidates.isEmpty())
        return {};

    std::sort(candidates.begin(), candidates.end(), [](const FilePath &a, const FilePath &b) {
        return a.fileName() < b.fileName();
    });
    return candidates.constFirst();
}

static void fillHarmonyCppSymbolsAndSysroot(DebuggerRunParameters &rp,
                                            BuildConfiguration *bc,
                                            Kit *kit,
                                            Project *project,
                                            RunControl *runControl)
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
        const FilePath entryLibDir = ohpro.pathAppended("entry/libs").pathAppended(preferredAbi);
        if (entryLibDir.isReadableDir())
            solibSearchPath.append(entryLibDir);
    }

    if (QtSupport::QtVersion *qtVersion = QtSupport::QtKitAspect::qtVersion(kit))
        solibSearchPath.append(qtVersion->qtSoPaths());

    FilePath::removeDuplicates(solibSearchPath);
    rp.setSolibSearchPath(solibSearchPath);

    if (project) {
        rp.addSearchDirectory(project->projectDirectory());
        appendMacOsCanonicalSourceMaps(rp, project->projectDirectory());
    }
    rp.addSearchDirectory(ohpro);
    appendMacOsCanonicalSourceMaps(rp, ohpro);
    rp.addSearchDirectory(bc->buildDirectory());
    appendMacOsCanonicalSourceMaps(rp, bc->buildDirectory());

    const FilePath symbolFile = pickHarmonySymbolFile(runControl, bc);
    if (symbolFile.isReadableFile())
        rp.setSymbolFile(symbolFile);

    FilePath sysRoot = rp.sysRoot();
    if (!sysRoot.isEmpty() && sysRoot.isReadableDir())
        return;

    const FilePath ndk = FilePath::fromSettings(kit->value(Constants::HARMONY_KIT_NDK));
    if (ndk.isEmpty())
        return;

    const FilePath candidates[] = {ndk / "sysroot", ndk / "llvm" / "sysroot"};
    for (const FilePath &c : candidates) {
        if (c.isReadableDir()) {
            rp.setSysRoot(c);
            return;
        }
    }
}

struct HarmonyDapPrepareContext
{
    FilePath hdc;
    QString serial;
    QString bundle;
    QString ability;
    qint64 pid = -1;
    QString ohosTriple;
    FilePath hostLldbServer;
    QString deviceLldbDir;
    QString deviceLldbPath;
    QString remoteAddress;
    QString remoteAddressSimple;
};

static Group harmonyDapPrepRecipe(RunControl *runControl, const Storage<HarmonyDapPrepareContext> &ctx)
{
    const auto fail = [runControl](const QString &msg) {
        runControl->postMessage(msg, ErrorMessageFormat);
        return false;
    };

    const auto onPrepare = [runControl, ctx, fail] {
        HarmonyDapPrepareContext *c = ctx.activeStorage();
        BuildConfiguration *bc = runControl->buildConfiguration();
        if (!bc)
            return fail(Tr::tr("Harmony DAP debug: no build configuration."));

        c->hdc = HarmonyConfig::hdcToolPath();
        if (!c->hdc.isExecutableFile())
            return fail(Tr::tr("Harmony DAP debug: hdc was not found."));

        c->serial = harmonyEffectiveDeviceSerial(bc);
        if (c->serial.isEmpty())
            return fail(Tr::tr("Harmony DAP debug: no target device serial."));

        c->bundle = harmonyDebuggerBundleName(bc);
        if (c->bundle.isEmpty())
            return fail(Tr::tr("Harmony DAP debug: bundle name is unknown."));

        c->ability = harmonyDebuggerAbilityName(bc);
        if (c->ability.isEmpty())
            c->ability = QStringLiteral("EntryAbility");

        const QString abi = harmonyDebuggerPreferredAbi(bc);
        c->ohosTriple = HarmonyConfig::ohosLldbTripleForAbi(abi);
        if (c->ohosTriple.isEmpty())
            return fail(Tr::tr("Harmony DAP debug: cannot determine OHOS ABI/triple."));
        c->hostLldbServer = HarmonyConfig::lldbServerExecutable(c->ohosTriple);
        if (!c->hostLldbServer.isReadableFile())
            return fail(Tr::tr("Harmony DAP debug: lldb-server not found for triple %1.")
                            .arg(c->ohosTriple));

        c->deviceLldbDir = QStringLiteral("/data/local/tmp/debugserver/%1").arg(c->bundle);
        c->deviceLldbPath = c->deviceLldbDir + QStringLiteral("/lldb-server");
        QString sharedRemote = runControl->property("HarmonyRemoteAddress").toString();
        QString sharedRemoteSimple = runControl->property("HarmonyRemoteAddressSimple").toString();
        if (sharedRemote.isEmpty()) {
            const QString sockName = QStringLiteral("platform-harmony.sock");
            sharedRemote = QStringLiteral("unix-abstract-connect://%1/%2/%3")
                               .arg(c->serial, c->bundle, sockName);
            sharedRemoteSimple = QStringLiteral("unix-abstract-connect://%1").arg(sockName);
            runControl->setProperty("HarmonyRemoteAddress", sharedRemote);
            runControl->setProperty("HarmonyRemoteAddressSimple", sharedRemoteSimple);
        }
        c->remoteAddress = sharedRemote;
        c->remoteAddressSimple = sharedRemoteSimple;

        runControl->postMessage(
            Tr::tr("Harmony DAP debug: launching app with 'aa start -D' before attaching adapter."),
            NormalMessageFormat);
        return true;
    };

    const auto aaStartDebug = [ctx](Process &p) {
        const HarmonyDapPrepareContext *c = ctx.activeStorage();
        CommandLine cmd{c->hdc, hdcSelector(c->serial)};
        cmd.addArg(QStringLiteral("shell"));
        cmd.addArgs({QStringLiteral("aa"),
                     QStringLiteral("start"),
                     QStringLiteral("-a"),
                     c->ability,
                     QStringLiteral("-b"),
                     c->bundle,
                     QStringLiteral("-D")});
        p.setCommand(cmd);
        p.setWorkingDirectory(c->hdc.parentDir());
    };

    const auto mkServerDir = [ctx](Process &p) {
        const HarmonyDapPrepareContext *c = ctx.activeStorage();
        CommandLine cmd{c->hdc, hdcSelector(c->serial)};
        cmd.addArgs({QStringLiteral("shell"),
                     QStringLiteral("mkdir"),
                     QStringLiteral("-p"),
                     c->deviceLldbDir});
        p.setCommand(cmd);
        p.setWorkingDirectory(c->hdc.parentDir());
    };

    const auto sendServer = [ctx](Process &p) {
        const HarmonyDapPrepareContext *c = ctx.activeStorage();
        CommandLine cmd{c->hdc, hdcSelector(c->serial)};
        cmd.addArgs({QStringLiteral("file"),
                     QStringLiteral("send"),
                     c->hostLldbServer.nativePath(),
                     c->deviceLldbPath});
        p.setCommand(cmd);
        p.setWorkingDirectory(c->hdc.parentDir());
    };

    const auto chmodServer = [ctx](Process &p) {
        const HarmonyDapPrepareContext *c = ctx.activeStorage();
        CommandLine cmd{c->hdc, hdcSelector(c->serial)};
        cmd.addArgs({QStringLiteral("shell"),
                     QStringLiteral("chmod"),
                     QStringLiteral("755"),
                     c->deviceLldbPath});
        p.setCommand(cmd);
        p.setWorkingDirectory(c->hdc.parentDir());
    };

    const auto aaProcessLldb = [ctx](Process &p) {
        const HarmonyDapPrepareContext *c = ctx.activeStorage();
        const QString listenAddr = c->remoteAddress;
        QString listenAddrRun = listenAddr;
        if (listenAddr.startsWith(QStringLiteral("unix-abstract-connect://"))) {
            // Force a stable device-scoped abstract socket form for lldb-server listen.
            // Some HarmonyOS builds are sensitive to this shape during handshake.
            QString sockName = listenAddr.mid(listenAddr.lastIndexOf(QLatin1Char('/')) + 1);
            if (sockName.isEmpty())
                sockName = QStringLiteral("platform-harmony.sock");
            listenAddrRun = QStringLiteral("unix-abstract://%1/%2/%3")
                                .arg(c->serial, c->bundle, sockName);
        }
        const QString lldbArgServer = QStringLiteral("%1 platform --server --listen %2")
                                          .arg(c->deviceLldbPath, listenAddrRun);
        const QString lldbArgPlain = QStringLiteral("%1 platform --listen %2")
                                         .arg(c->deviceLldbPath, listenAddrRun);
        const QString listenAddrRunBracket = QStringLiteral("unix-abstract://[%1]/%2/%3")
                                                 .arg(c->serial, c->bundle,
                                                      listenAddrRun.mid(listenAddrRun.lastIndexOf(QLatin1Char('/')) + 1));
        const QString lldbArgBracket = QStringLiteral("%1 platform --server --listen %2")
                                           .arg(c->deviceLldbPath, listenAddrRunBracket);
        // aa process often keeps running in the foreground after printing success; run it in the
        // background so we can probe pidof / fall through to alternate listen forms.
        const QString inner = QStringLiteral(
            "echo '[aa-process] try1'; "
            "aa process -a %1 -b %2 -D \"%3\" & "
            "echo '[aa-process] after-aa1-backgrounded'; "
            "sleep 2; "
            "if [ -n \"$(pidof lldb-server 2>/dev/null)\" ]; then "
            "  echo '[aa-process] try1 alive'; "
            "  exit 0; "
            "fi; "
            "echo '[aa-process] try2'; "
            "killall lldb-server 2>/dev/null || true; "
            "sleep 1; "
            "aa process -a %1 -b %2 -D \"%4\" & "
            "echo '[aa-process] after-aa2-backgrounded'; "
            "sleep 2; "
            "if [ -n \"$(pidof lldb-server 2>/dev/null)\" ]; then "
            "  echo '[aa-process] try2 alive'; "
            "  exit 0; "
            "fi; "
            "echo '[aa-process] try3'; "
            "killall lldb-server 2>/dev/null || true; "
            "sleep 1; "
            "aa process -a %1 -b %2 -D \"%5\" & "
            "echo '[aa-process] after-aa3-backgrounded'; "
            "sleep 2; "
            "if [ -n \"$(pidof lldb-server 2>/dev/null)\" ]; then "
            "  echo '[aa-process] try3 alive'; "
            "  exit 0; "
            "fi; "
            "echo '[aa-process] all tries failed to keep lldb-server alive'; "
            "exit 1")
                                  .arg(c->ability, c->bundle, lldbArgServer, lldbArgPlain, lldbArgBracket);
        CommandLine cmd{c->hdc, hdcSelector(c->serial)};
        cmd.addArgs({QStringLiteral("shell"), QStringLiteral("sh"), QStringLiteral("-c"), inner});
        p.setCommand(cmd);
        p.setWorkingDirectory(c->hdc.parentDir());
    };

    const auto killStaleLldbServer = [ctx](Process &p) {
        const HarmonyDapPrepareContext *c = ctx.activeStorage();
        CommandLine cmd{c->hdc, hdcSelector(c->serial)};
        cmd.addArgs({QStringLiteral("shell"),
                     QStringLiteral("sh"),
                     QStringLiteral("-c"),
                     QStringLiteral(
                         "echo '[cleanup] before:'; "
                         "pidof lldb-server 2>/dev/null || true; "
                         "aa force-stop -b %1 >/dev/null 2>&1 || aa force-stop %1 >/dev/null 2>&1 || true; "
                         "killall lldb-server >/dev/null 2>&1 || true; "
                         "for p in $(pidof lldb-server 2>/dev/null); do "
                         "kill -9 \"$p\" >/dev/null 2>&1 || true; "
                         "done; "
                         "sleep 1; "
                         "echo '[cleanup] after:'; "
                         "pidof lldb-server 2>/dev/null || true")
                         .arg(c->bundle)});
        p.setCommand(cmd);
        p.setWorkingDirectory(c->hdc.parentDir());
    };

    const auto verifyLldbServer = [ctx](Process &p) {
        const HarmonyDapPrepareContext *c = ctx.activeStorage();
        CommandLine cmd{c->hdc, hdcSelector(c->serial)};
        cmd.addArgs({QStringLiteral("shell"),
                     QStringLiteral("sh"),
                     QStringLiteral("-c"),
                     QStringLiteral(
                         "PIDS=$(pidof lldb-server 2>/dev/null || true); "
                         "echo \"$PIDS\"; "
                         "echo '---'; "
                         "ps -A | grep lldb-server || true; "
                         "PID=$(echo \"$PIDS\" | awk '{print $1}'); "
                         "echo \"--- cmdline:$PID ---\"; "
                         "[ -n \"$PID\" ] && cat /proc/$PID/cmdline 2>/dev/null | tr '\\000' ' ' || true; "
                         "echo; "
                         "echo '--- unix sockets ---'; "
                         "cat /proc/net/unix 2>/dev/null | grep -E 'platform-|lldb|debugserver|%1' || true")
                         .arg(c->bundle)});
        p.setCommand(cmd);
        p.setWorkingDirectory(c->hdc.parentDir());
    };

    const auto onFail = [runControl](const Process &p, DoneWith result) {
        if (result == DoneWith::Success)
            return;
        runControl->postMessage(
            Tr::tr("Harmony DAP debug: pre-launch command failed: %1")
                .arg(p.exitMessage(Process::FailureMessageFormat::WithStdErr)),
            ErrorMessageFormat);
    };

    const auto onVerifyLldbServer = [runControl](const Process &p, DoneWith result) {
        if (result != DoneWith::Success) {
            runControl->postMessage(
                Tr::tr("Harmony DAP debug: lldb-server verify command failed: %1")
                    .arg(p.exitMessage(Process::FailureMessageFormat::WithStdErr)),
                ErrorMessageFormat);
            return;
        }
        const QString out = p.allOutput();
        const QStringList lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        bool hasPid = false;
        for (const QString &line : lines) {
            const QString t = line.trimmed();
            if (t.isEmpty() || t == QStringLiteral("---"))
                continue;
            bool ok = false;
            t.toLongLong(&ok);
            if (ok) {
                hasPid = true;
                break;
            }
        }
        if (!hasPid) {
            runControl->postMessage(
                Tr::tr("Harmony DAP debug: lldb-server is not visible on device after start.\n"
                       "verify output:\n%1")
                    .arg(out),
                ErrorMessageFormat);
            return;
        }
        runControl->postMessage(
            Tr::tr("Harmony DAP debug: lldb-server verify output:\n%1").arg(out),
            NormalMessageFormat);
    };

    const auto onAaProcessLldb = [runControl](const Process &p, DoneWith result) {
        const QString out = p.allOutput();
        const QString msg = Tr::tr("Harmony DAP debug: aa process result=%1\noutput:\n%2")
                                .arg(result == DoneWith::Success ? QStringLiteral("success")
                                                                 : QStringLiteral("error"),
                                     out);
        runControl->postMessage(msg, result == DoneWith::Success ? NormalMessageFormat : ErrorMessageFormat);
    };

    const auto onKillStaleLldbServer = [runControl](const Process &p, DoneWith result) {
        const QString out = p.allOutput();
        if (result != DoneWith::Success) {
            runControl->postMessage(
                Tr::tr("Harmony DAP debug: stale lldb-server cleanup failed: %1\noutput:\n%2")
                    .arg(p.exitMessage(Process::FailureMessageFormat::WithStdErr), out),
                ErrorMessageFormat);
            return;
        }
        runControl->postMessage(
            Tr::tr("Harmony DAP debug: stale lldb-server cleanup output:\n%1").arg(out),
            NormalMessageFormat);
    };

    const auto pickPid = [runControl, ctx, fail] {
        HarmonyDapPrepareContext *c = ctx.activeStorage();
        QString pidSource;
        c->pid = harmonyQueryApplicationPid(c->hdc, c->serial, c->bundle, &pidSource);
        if (c->pid <= 0) {
            return fail(
                Tr::tr("Harmony DAP debug: could not determine app PID after launch.\n"
                       "Try: hdc -t %1 shell ps -A")
                    .arg(c->serial));
        }
        runControl->setAttachPid(ProcessHandle(c->pid));
        runControl->setProperty("HarmonyRemoteAddress", c->remoteAddress);
        runControl->postMessage(
            Tr::tr("Harmony DAP debug: attach PID %1 (source: %2).")
                .arg(c->pid)
                .arg(pidSource.isEmpty() ? Tr::tr("unknown") : pidSource),
            NormalMessageFormat);
        return true;
    };

    return Group{
        ctx,
        QSyncTask(onPrepare),
        ProcessTask(mkServerDir, onFail, CallDone::OnSuccess | CallDone::OnError)
            .withTimeout(std::chrono::minutes(1)),
        ProcessTask(sendServer, onFail, CallDone::OnSuccess | CallDone::OnError)
            .withTimeout(std::chrono::minutes(2)),
        ProcessTask(chmodServer, onFail, CallDone::OnSuccess | CallDone::OnError)
            .withTimeout(std::chrono::minutes(1)),
        ProcessTask(aaStartDebug, onFail, CallDone::OnSuccess | CallDone::OnError)
            .withTimeout(std::chrono::minutes(2)),
        ProcessTask(killStaleLldbServer, onKillStaleLldbServer, CallDone::OnSuccess | CallDone::OnError)
            .withTimeout(std::chrono::seconds(20)),
        ProcessTask(aaProcessLldb, onAaProcessLldb, CallDone::OnSuccess | CallDone::OnError)
            .withTimeout(std::chrono::minutes(2)),
        ProcessTask(verifyLldbServer, onVerifyLldbServer, CallDone::OnSuccess | CallDone::OnError)
            .withTimeout(std::chrono::seconds(20)),
        QSyncTask(pickPid),
    };
}

static bool debuggerLooksLikeDapAdapter(const FilePath &debugger)
{
    if (!debugger.isExecutableFile())
        return false;
    const QString name = debugger.fileName().toLower();
    if (name.contains(QStringLiteral("harmony-dap-proxy")))
        return true;
    return name.contains(QStringLiteral("lldb-dap")) || name.contains(QStringLiteral("lldb-vscode"));
}

static bool containsHarmonyPlaceholder(const QString &value)
{
    return value.contains(QStringLiteral("${SN}"))
           || value.contains(QStringLiteral("${bundle name}"));
}

static FilePath ensureHarmonyDapProxyScript()
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appData.isEmpty())
        return {};

    const FilePath dir = FilePath::fromUserInput(appData).pathAppended("harmony");
    if (!dir.ensureWritableDir())
        return {};

    const FilePath script = dir.pathAppended("harmony-dap-proxy.py");
    const QByteArray content = R"(#!/usr/bin/env python3
import os
import socket
import subprocess
import sys
import threading
import time
import json
import datetime

PROXY_VERSION = "2026-03-24-v5"

def fail(msg):
    sys.stderr.write(msg + "\n")
    sys.stderr.flush()
    sys.exit(1)

def find_server():
    roots = []
    deveco_tools = os.environ.get("HARMONY_DEVECO_TOOLS", "")
    if deveco_tools:
        roots.append(os.path.join(deveco_tools, "llvm", "server", "dap"))
    roots.append("/Applications/DevEco-Studio.app/Contents/tools/llvm/server/dap")
    roots.append(os.path.expanduser("~/Applications/DevEco-Studio.app/Contents/tools/llvm/server/dap"))
    for root in roots:
        if not os.path.isdir(root):
            continue
        for sub in ("mac", "linux", "win", "windows"):
            d = os.path.join(root, sub)
            if not os.path.isdir(d):
                continue
            for name in os.listdir(d):
                if "dap_server" in name:
                    p = os.path.join(d, name)
                    if os.path.isfile(p) and os.access(p, os.X_OK):
                        return p
    fail("Harmony DAP proxy: dap_server not found")

def pick_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port

def _rewrite_ide_request(body_bytes, tracef):
    try:
        obj = json.loads(body_bytes.decode("utf-8"))
    except Exception:
        return body_bytes
    if obj.get("type") == "request" and obj.get("command") == "initialize":
        args = obj.get("arguments")
        if not isinstance(args, dict):
            args = {}
            obj["arguments"] = args
        if "adapterID" not in args:
            args["adapterID"] = "lldbapi"
            tracef.write(b"[proxy] patched initialize.adapterID=lldbapi\n")
            tracef.flush()
        return json.dumps(obj, separators=(",", ":")).encode("utf-8")
    if obj.get("type") == "request" and obj.get("command") == "attach":
        args = obj.get("arguments")
        if isinstance(args, dict) and "pid" in args:
            try:
                pid = int(args["pid"])
                # Build a DevEco-like attach payload.
                args.clear()
                args["request"] = "attach"
                args["type"] = "cppmi"
                args["name"] = "(lldb) Attach"
                args["externalConsole"] = False
                args["stopAtEntry"] = False
                args["showStaticGlobalVars"] = True
                args["showVisualSTL"] = True
                args["attachStep"] = "connectDbgServer"
                args["remote"] = True
                args["processId"] = str(pid)
                serial = os.environ.get("HARMONY_DEVICE_SERIAL", "").strip()
                bundle = os.environ.get("HARMONY_BUNDLE_NAME", "").strip()
                env_remote = os.environ.get("HARMONY_REMOTE_ADDRESS", "").strip()
                env_remote_simple = os.environ.get("HARMONY_REMOTE_ADDRESS_SIMPLE", "").strip()
                placeholder = ("${SN}" in serial or "${bundle name}" in serial
                               or "${SN}" in bundle or "${bundle name}" in bundle
                               or "${SN}" in env_remote or "${bundle name}" in env_remote)
                if placeholder:
                    tracef.write(("[proxy] ERROR: placeholder detected serial=%r bundle=%r remote=%r\n"
                                  % (serial, bundle, env_remote)).encode("utf-8"))
                    tracef.flush()
                    raise RuntimeError("placeholder remote context")
                remote = ""
                if env_remote:
                    remote = env_remote
                elif env_remote_simple:
                    remote = env_remote_simple
                elif serial and bundle:
                    remote = "unix-abstract-connect://%s/%s/platform-harmony.sock" % (
                        serial,
                        bundle,
                    )
                if not remote:
                    tracef.write(("[proxy] ERROR: no valid remoteAddress/serial/bundle "
                                  "serial=%r bundle=%r remote=%r\n"
                                  % (serial, bundle, env_remote)).encode("utf-8"))
                    tracef.flush()
                    raise RuntimeError("missing remote context")
                if remote:
                    args["remoteAddress"] = remote
                    tracef.write(("[proxy] attach.remoteAddress=%s\n" % remote).encode("utf-8"))
                    tracef.flush()
                args["remotePlatform"] = os.environ.get("HARMONY_REMOTE_PLATFORM", "remote-ohos")
                cwd = os.environ.get("HARMONY_OHPRO_DIR", "").strip()
                if cwd:
                    args["cwd"] = cwd
                args["startupCommands"] = [
                    "settings set auto-confirm true"
                ]
                args["postAttachCommands"] = [
                    "settings set target.process.thread.step-avoid-regexp ^std::"
                ]
                tracef.write(("[proxy] patched attach payload processId=%d\n" % pid).encode("utf-8"))
                tracef.flush()
            except Exception as e:
                tracef.write(("[proxy] FATAL: attach rewrite failed: %s\n" % str(e)).encode("utf-8"))
                tracef.flush()
                fail("Harmony DAP proxy: invalid attach context (serial/bundle/remoteAddress).")
        return json.dumps(obj, separators=(",", ":")).encode("utf-8")
    return body_bytes

def pipe_in_to_sock(sock, tracef):
    fd = sys.stdin.fileno()
    buf = b""
    try:
        while True:
            data = os.read(fd, 4096)
            if not data:
                tracef.write(b"[proxy] IDE stdin EOF\n")
                tracef.flush()
                try:
                    sock.shutdown(socket.SHUT_WR)
                except Exception:
                    pass
                return
            buf += data
            while True:
                sep = buf.find(b"\r\n\r\n")
                if sep < 0:
                    break
                head = buf[:sep]
                content_length = None
                for line in head.split(b"\r\n"):
                    if line.lower().startswith(b"content-length:"):
                        try:
                            content_length = int(line.split(b":", 1)[1].strip())
                        except Exception:
                            content_length = None
                        break
                if content_length is None:
                    # Not a DAP-framed packet, pass through one chunk and reset.
                    tracef.write(("[proxy] IDE->DAP raw %d bytes\n" % len(buf)).encode("utf-8"))
                    tracef.flush()
                    sock.sendall(buf)
                    buf = b""
                    break
                total = sep + 4 + content_length
                if len(buf) < total:
                    break
                body = buf[sep + 4:total]
                body = _rewrite_ide_request(body, tracef)
                out = b"Content-Length: " + str(len(body)).encode("ascii") + b"\r\n\r\n" + body
                tracef.write(("[proxy] IDE->DAP %d bytes\n" % len(out)).encode("utf-8"))
                tracef.flush()
                sock.sendall(out)
                buf = buf[total:]
    except Exception:
        return

def pipe_sock_to_out(sock, tracef):
    out = sys.stdout.buffer
    try:
        while True:
            try:
                data = sock.recv(4096)
            except socket.timeout:
                continue
            if not data:
                return
            tracef.write(("[proxy] DAP->IDE %d bytes\n" % len(data)).encode("utf-8"))
            tracef.flush()
            out.write(data)
            out.flush()
    except Exception:
        return

def main():
    server = find_server()
    port = pick_port()
    logpath = os.environ.get("HARMONY_DAP_LOGPATH", os.path.expanduser("~/Library/Logs/HarmonyDapProxy"))
    os.makedirs(logpath, exist_ok=True)

    cmd = [server, "--debuggertype=lldbapi", "--port=%d" % port, "--logpath=%s" % logpath, "--loglevel=INFO"]
    server_log = os.path.join(logpath, "dap_server.stderr.log")
    proxy_log = os.path.join(logpath, "dap_proxy.trace.log")
    logf = open(server_log, "ab", buffering=0)
    tracef = open(proxy_log, "ab", buffering=0)
    tracef.write(("[proxy] version=%s\n" % PROXY_VERSION).encode("utf-8"))
    tracef.write(("[proxy] start pid=%d\n" % os.getpid()).encode("utf-8"))
    tracef.write(("[proxy] env.remote=%r env.remoteSimple=%r env.serial=%r env.bundle=%r env.utid=%r\n" % (
        os.environ.get("HARMONY_REMOTE_ADDRESS", ""),
        os.environ.get("HARMONY_REMOTE_ADDRESS_SIMPLE", ""),
        os.environ.get("HARMONY_DEVICE_SERIAL", ""),
        os.environ.get("HARMONY_BUNDLE_NAME", ""),
        os.environ.get("HDC_UTID", ""),
    )).encode("utf-8"))
    tracef.flush()
    proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=logf, text=False)
    tracef.write(("[proxy] server pid=%d cmd=%s\n" % (proc.pid, " ".join(cmd))).encode("utf-8"))
    tracef.flush()

    connected = None
    for _ in range(80):
        try:
            s = socket.create_connection(("127.0.0.1", port), timeout=0.2)
            s.settimeout(None)
            connected = s
            tracef.write(("[proxy] connected 127.0.0.1:%d\n" % port).encode("utf-8"))
            tracef.flush()
            break
        except Exception:
            if proc.poll() is not None:
                fail("Harmony DAP proxy: dap_server exited early. See " + server_log)
            time.sleep(0.05)

    if connected is None:
        try:
            logf.close()
        except Exception:
            pass
        proc.kill()
        fail("Harmony DAP proxy: cannot connect to dap_server")

    t1 = threading.Thread(target=pipe_in_to_sock, args=(connected, tracef), daemon=True)
    t2 = threading.Thread(target=pipe_sock_to_out, args=(connected, tracef), daemon=True)
    t1.start()
    t2.start()
    tracef.write(b"[proxy] pipes started\n")
    tracef.flush()
    t1.join()
    t2.join()

    try:
        connected.close()
    except Exception:
        pass

    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=2)
        except Exception:
            proc.kill()
    try:
        logf.close()
    except Exception:
        pass
    try:
        tracef.write(b"[proxy] stop\n")
        tracef.flush()
        tracef.close()
    except Exception:
        pass

if __name__ == "__main__":
    main()
)";

    bool needsUpdate = !script.exists();
    if (!needsUpdate) {
        const Result<QByteArray> current = script.fileContents();
        needsUpdate = !current || *current != content;
    }
    if (needsUpdate) {
        if (!script.writeFileContents(content))
            return {};
        QFile f(script.toFSPathString());
        f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                         | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                         | QFileDevice::ReadOther | QFileDevice::ExeOther);
    }
    return script;
}

static QVariant ensureHarmonyProxyDebuggerItem(const FilePath &proxyScript)
{
    if (!proxyScript.isExecutableFile())
        return {};

    if (const DebuggerItem *existing = DebuggerItemManager::findByCommand(proxyScript))
        return existing->id();

    DebuggerItem item;
    item.setCommand(proxyScript);
    item.setEngineType(LldbDapEngineType);
    item.setUnexpandedDisplayName(Tr::tr("Harmony DAP Proxy"));
    item.setDetectionSource({ProjectExplorer::DetectionSource::FromSystem,
                             QStringLiteral("HarmonyConfiguration")});
    item.createId();
    return DebuggerItemManager::registerDebugger(item);
}

class HarmonyDebugWorkerFactory final : public RunWorkerFactory
{
public:
    HarmonyDebugWorkerFactory()
    {
        setId("HarmonyDebugWorkerFactory");
        setRecipeProducer([](RunControl *runControl) -> Group {
            if (!runControl->kit())
                return runControl->errorTask(Tr::tr("Harmony DAP debug: run configuration has no kit."));

            bool cppDebug = true;
            if (const auto *asp = runControl->aspectData<DebuggerRunConfigurationAspect>())
                cppDebug = asp->useCppDebugger;

            if (!cppDebug) {
                return runControl->errorTask(
                    Tr::tr("Harmony DAP debug requires C++ debugger enabled in run configuration."));
            }

            DebuggerRunParameters rp = DebuggerRunParameters::fromRunControl(runControl);
            if (rp.cppEngineType() != LldbDapEngineType) {
                return runControl->errorTask(
                    Tr::tr("Harmony debug was migrated to DAP-only.\n"
                           "Please select an LLDB DAP debugger in Projects > Kits > Debugger."));
            }

            const FilePath proxy = ensureHarmonyDapProxyScript();
            if (!proxy.isExecutableFile()) {
                return runControl->errorTask(
                    Tr::tr("Harmony DAP debug: failed to prepare DAP proxy script."));
            }
            const QVariant proxyDebuggerId = ensureHarmonyProxyDebuggerItem(proxy);
            if (!proxyDebuggerId.isValid()) {
                return runControl->errorTask(
                    Tr::tr("Harmony DAP debug: failed to register proxy debugger item."));
            }
            if (runControl->kit())
                DebuggerKitAspect::setDebugger(runControl->kit(), proxyDebuggerId);

            qputenv("HARMONY_DEVECO_TOOLS", HarmonyConfig::devecoToolsLocation().toFSPathString().toUtf8());
            rp = DebuggerRunParameters::fromRunControl(runControl);
            const BuildConfiguration *bc = runControl->buildConfiguration();
            const QString serial = bc ? harmonyEffectiveDeviceSerial(bc) : QString{};
            const QString bundle = bc ? harmonyDebuggerBundleName(bc) : QString{};
            QString remoteAddr = runControl->property("HarmonyRemoteAddress").toString();
            QString remoteAddrSimple = runControl->property("HarmonyRemoteAddressSimple").toString();
            if (remoteAddr.isEmpty() || containsHarmonyPlaceholder(remoteAddr)) {
                const QString sockName = QStringLiteral("platform-harmony.sock");
                remoteAddr = QStringLiteral("unix-abstract-connect://%1/%2/%3")
                                 .arg(serial, bundle, sockName);
                remoteAddrSimple = QStringLiteral("unix-abstract-connect://%1").arg(sockName);
                runControl->setProperty("HarmonyRemoteAddress", remoteAddr);
                runControl->setProperty("HarmonyRemoteAddressSimple", remoteAddrSimple);
            }
            if (containsHarmonyPlaceholder(remoteAddr) || containsHarmonyPlaceholder(serial)
                || containsHarmonyPlaceholder(bundle) || serial.isEmpty() || bundle.isEmpty()) {
                return runControl->errorTask(
                    Tr::tr("Harmony DAP debug: invalid remote attach context.\n"
                           "serial='%1'\n"
                           "bundle='%2'\n"
                           "remoteAddress='%3'")
                        .arg(serial, bundle, remoteAddr));
            }
            const QString ohproDir = bc ? harmonyBuildDirectory(bc).toFSPathString() : QString{};
            rp.modifyDebuggerEnvironment(
                {EnvironmentItem(QStringLiteral("HARMONY_DEVECO_TOOLS"),
                                 HarmonyConfig::devecoToolsLocation().toFSPathString()),
                 EnvironmentItem(QStringLiteral("HARMONY_REMOTE_ADDRESS"), remoteAddr),
                 EnvironmentItem(QStringLiteral("HARMONY_REMOTE_ADDRESS_SIMPLE"), remoteAddrSimple),
                 EnvironmentItem(QStringLiteral("HARMONY_REMOTE_PLATFORM"), QStringLiteral("remote-ohos")),
                 EnvironmentItem(QStringLiteral("HARMONY_DEVICE_SERIAL"), serial),
                 EnvironmentItem(QStringLiteral("HARMONY_BUNDLE_NAME"), bundle),
                 EnvironmentItem(QStringLiteral("HARMONY_OHPRO_DIR"), ohproDir),
                 EnvironmentItem(QStringLiteral("HDC_UTID"), serial)});
            runControl->postMessage(
                Tr::tr("Harmony DAP env: serial='%1' bundle='%2' remoteAddress='%3' remoteAddressSimple='%4' HDC_UTID='%5'")
                    .arg(serial, bundle, remoteAddr, remoteAddrSimple, serial),
                NormalMessageFormat);
            const FilePath adapter = rp.debugger().command.executable();
            if (!debuggerLooksLikeDapAdapter(adapter)) {
                return runControl->errorTask(
                    Tr::tr("Harmony DAP debug: debugger executable does not look like a DAP adapter:\n%1\n\n"
                           "Expected harmony-dap-proxy / lldb-dap / lldb-vscode.")
                        .arg(adapter.toUserOutput()));
            }

            fillHarmonyCppSymbolsAndSysroot(
                rp, runControl->buildConfiguration(), runControl->kit(), runControl->project(), runControl);

            // Harmony DAP should attach to the already-started app process, not launch host command line.
            rp.setStartMode(AttachToLocalProcess);
            if (runControl->attachPid().isValid())
                rp.setAttachPid(runControl->attachPid());

            runControl->postMessage(
                Tr::tr("Harmony DAP debug: using adapter %1").arg(adapter.toUserOutput()),
                NormalMessageFormat);

            const Storage<HarmonyDapPrepareContext> ctx;
            return Group{
                harmonyDapPrepRecipe(runControl, ctx),
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
    qCDebug(harmonyRunLog) << "Registering Harmony DAP debug worker factory.";
    static HarmonyDebugWorkerFactory theHarmonyDebugWorkerFactory;
}

} // namespace Ohos::Internal
