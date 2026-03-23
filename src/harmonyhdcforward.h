// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QtGlobal>

#include <QString>

namespace Ohos::Internal {

/**
 * @file harmonyhdcforward.h
 * @brief hdc **fport** TCP 转发封装（DEBUG-TASKS 阶段 2 / PRIORITY-PLAN P2-07）。
 *
 * **说明**：当前官方零售机无 root，调试插件**不**自动做 §7.1 + fport 编排；本 API 供**手工**脚本、
 * 工程机或其它工具链使用。与官方 **§7.1** 文档一致：设备上 `lldb-server --listen "*:<devicePort>"`，本机 LLDB
 * `platform connect connect://localhost:<hostPort>` 前，调用
 * `hdcFportForwardTcp(serial, devicePort, hostPort)`。
 *
 * **参数顺序**以 OpenHarmony / 社区文档常见写法为准：`tcp:<设备端口> tcp:<本机端口>`。
 * 若你本机 `hdc fport -h` 与下文不一致，请以 **当前 hdc 版本** 为准并反馈修订文档。
 */

struct HarmonyHdcFportResult {
    bool success = false;
    /** 用于日志/诊断的完整命令行（已脱敏路径风格与 Creator 一致） */
    QString commandLineForLog;
    /** hdc 合并输出（stdout+stderr，便于排查） */
    QString combinedOutput;
    /** 失败时的简短说明；成功时通常为空 */
    QString errorMessage;
};

/**
 * 建立 TCP 转发：设备 `deviceTcpPort` → 本机 `hostTcpPort`。
 * @param deviceSerial 非空时附加 `-t`；空则使用 hdc 当前默认设备。
 */
HarmonyHdcFportResult hdcFportForwardTcp(const QString &deviceSerial,
                                         quint16 deviceTcpPort,
                                         quint16 hostTcpPort);

/**
 * 移除转发规则。当前实现使用 `hdc fport rm tcp:<hostTcpPort>`（与常见示例一致）；
 * 若失败可结合 `hdcFportList` 输出手工 `hdc fport rm …` / `hdc fport clear`（慎用 clear）。
 */
HarmonyHdcFportResult hdcFportRemoveTcpForward(const QString &deviceSerial, quint16 hostTcpPort);

/** 列出当前 fport 规则（`hdc fport ls`）。 */
HarmonyHdcFportResult hdcFportList(const QString &deviceSerial);

} // namespace Ohos::Internal
