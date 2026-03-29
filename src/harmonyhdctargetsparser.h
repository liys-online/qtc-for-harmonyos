// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QString>
#include <QStringList>

namespace Ohos::Internal {

/*!
    \brief 一行 \c hdc list targets（含 \c -v）stdout 的解析结果分类。

    用于在 hdc 升级导致列对齐或表头文案变化时，集中调整策略并配套单测 mock 样例。
*/
enum class HdcListTargetsLineKind {
    EmptyOrWhitespace,
    HeaderRow,
    TooFewColumns,
    UartRow,
    EmptySerialPlaceholder,
    DeviceDataRow,
};

/*!
    \brief 由「序列号 / 连接类型 / 状态」三列（及以后列，当前仅消费前三列）得到的结构化行。
*/
struct HdcListTargetsDeviceRow {
    QString serial;
    QString connectionType;
    QString stateRaw;
};

struct HdcListTargetsParseResult {
    HdcListTargetsLineKind kind = HdcListTargetsLineKind::EmptyOrWhitespace;
    HdcListTargetsDeviceRow device;
};

enum class HdcTargetConnectionState {
    ReadyToUse,
    ConnectedNotReady,
    Disconnected,
};

/* ** 无 I/O、无 HarmonyConfig；供设备管理与单测共用。 */
HdcListTargetsParseResult parseHdcListTargetsLine(const QString &line);
HdcTargetConnectionState hdcListTargetsStateToConnectionState(const QString &trimmedState);

/* ** 供单测与诊断：与 \c parseHdcListTargetsLine 内部分词策略一致。 */
QStringList splitHdcListTargetsLine(const QString &line);
bool hdcListTargetsLineLooksLikeHeader(const QStringList &parts);

} // namespace Ohos::Internal
