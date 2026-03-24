// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "harmonyhdctargetsparser.h"

#include <QRegularExpression>

namespace Ohos::Internal {

QStringList splitHdcListTargetsLine(const QString &line)
{
    QString s = line;
    s.remove(QLatin1Char('\r'));
    s = s.trimmed();
    if (s.isEmpty())
        return {};
    if (s.contains(QLatin1Char('\t')))
        return s.split(QLatin1Char('\t'), Qt::SkipEmptyParts);
    static const QRegularExpression wsSplit(QStringLiteral("\\s+"));
    return s.split(wsSplit, Qt::SkipEmptyParts);
}

bool hdcListTargetsLineLooksLikeHeader(const QStringList &parts)
{
    if (parts.isEmpty())
        return true;
    const QString a = parts.constFirst();
    const QStringList headers{QStringLiteral("SN"),       QStringLiteral("Identifier"),
                              QStringLiteral("DEVICE"),    QStringLiteral("Device"),
                              QStringLiteral("DevSerial"), QStringLiteral("Serial")};
    for (const QString &h : headers) {
        if (a.compare(h, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

HdcListTargetsParseResult parseHdcListTargetsLine(const QString &line)
{
    HdcListTargetsParseResult out;
    const QStringList parts = splitHdcListTargetsLine(line);
    if (parts.isEmpty()) {
        out.kind = HdcListTargetsLineKind::EmptyOrWhitespace;
        return out;
    }
    if (parts.size() < 3) {
        out.kind = HdcListTargetsLineKind::TooFewColumns;
        return out;
    }
    if (hdcListTargetsLineLooksLikeHeader(parts)) {
        out.kind = HdcListTargetsLineKind::HeaderRow;
        return out;
    }

    const QString serial = parts.constFirst();
    if (serial == QLatin1String("[Empty]") || serial.isEmpty()) {
        out.kind = HdcListTargetsLineKind::EmptySerialPlaceholder;
        return out;
    }
    if (parts.at(1) == QLatin1String("UART")) {
        out.kind = HdcListTargetsLineKind::UartRow;
        return out;
    }

    out.kind = HdcListTargetsLineKind::DeviceDataRow;
    out.device.serial = serial;
    out.device.connectionType = parts.at(1);
    out.device.stateRaw = parts.at(2);
    return out;
}

HdcTargetConnectionState hdcListTargetsStateToConnectionState(const QString &trimmedState)
{
    const QString t = trimmedState.trimmed();
    if (t == QLatin1String("Connected") || t == QStringLiteral("已连接"))
        return HdcTargetConnectionState::ReadyToUse;
    if (t == QLatin1String("Offline") || t == QStringLiteral("未连接") || t == QStringLiteral("断开"))
        return HdcTargetConnectionState::ConnectedNotReady;
    return HdcTargetConnectionState::Disconnected;
}

} // namespace Ohos::Internal
