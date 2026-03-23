// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "harmonyhdcforward.h"

#include "harmonyconfigurations.h"
#include "harmonylogcategories.h"
#include "harmonyutils.h"
#include "ohostr.h"

#include <utils/commandline.h>
#include <utils/fileutils.h>
#include <utils/qtcprocess.h>

#include <chrono>

using namespace Utils;

namespace Ohos::Internal {

static HarmonyHdcFportResult runHdcFport(const QString &deviceSerial, const QStringList &fportSubArgs,
                                         int timeoutSeconds = 30)
{
    HarmonyHdcFportResult r;
    const FilePath hdc = HarmonyConfig::hdcToolPath();
    if (!hdc.isExecutableFile()) {
        r.errorMessage = Tr::tr("The hdc tool is missing or not executable.");
        qCWarning(harmonyDeviceLog) << "hdc fport: hdc not executable" << hdc;
        return r;
    }

    QStringList args;
    if (!deviceSerial.trimmed().isEmpty())
        args += hdcSelector(deviceSerial);
    args += fportSubArgs;

    const CommandLine cmd{hdc, args};
    r.commandLineForLog = cmd.toUserOutput();

    Process process;
    process.setCommand(cmd);
    process.setWorkingDirectory(hdc.parentDir());
    process.runBlocking(std::chrono::seconds(timeoutSeconds));
    r.combinedOutput = process.allOutput();
    r.success = process.result() == ProcessResult::FinishedWithSuccess;
    if (!r.success) {
        r.errorMessage = Tr::tr("Command failed: %1").arg(process.exitMessage());
        qCWarning(harmonyDeviceLog) << "hdc fport failed:" << r.commandLineForLog << r.combinedOutput;
    } else {
        qCDebug(harmonyDeviceLog) << "hdc fport ok:" << r.commandLineForLog;
    }
    return r;
}

HarmonyHdcFportResult hdcFportForwardTcp(const QString &deviceSerial,
                                           const quint16 deviceTcpPort,
                                           const quint16 hostTcpPort)
{
    const QStringList sub{QStringLiteral("fport"),
                          QStringLiteral("tcp:%1").arg(deviceTcpPort),
                          QStringLiteral("tcp:%1").arg(hostTcpPort)};
    return runHdcFport(deviceSerial, sub);
}

HarmonyHdcFportResult hdcFportRemoveTcpForward(const QString &deviceSerial, const quint16 hostTcpPort)
{
    const QStringList sub{QStringLiteral("fport"), QStringLiteral("rm"),
                          QStringLiteral("tcp:%1").arg(hostTcpPort)};
    return runHdcFport(deviceSerial, sub);
}

HarmonyHdcFportResult hdcFportList(const QString &deviceSerial)
{
    return runHdcFport(deviceSerial, {QStringLiteral("fport"), QStringLiteral("ls")});
}

} // namespace Ohos::Internal
