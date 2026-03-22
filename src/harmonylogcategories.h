// Copyright (C) 2026 The Qt Company / Harmony Qt Creator plugin contributors.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QLoggingCategory>

// 统一 Harmony 插件 QLoggingCategory（P1-01）。开关示例：
//   QT_LOGGING_RULES="qtc.harmony.config.debug=true;qtc.harmony.device.debug=true"
//
// qtc.harmony.config — Kit/工具链同步、HarmonyConfigurations
// qtc.harmony.device — hdc 设备枚举、设备管理器
// qtc.harmony.build — Build HAP / hvigor
// qtc.harmony.deploy — 部署与 HAP 解析
// qtc.harmony.run — 运行控制（预留，供后续 qCDebug）
// qtc.harmony.plugin — 插件加载与工程钩子
// qtc.harmony.toolchain — Clang 工具链与代码模型
//
// lib/usbmonitor/usbmonitor.cpp 内单独定义 qtc.harmony.device.usbmonitor（单编译单元）。
Q_DECLARE_LOGGING_CATEGORY(harmonyConfigLog)
Q_DECLARE_LOGGING_CATEGORY(harmonyDeviceLog)
Q_DECLARE_LOGGING_CATEGORY(harmonyBuildLog)
Q_DECLARE_LOGGING_CATEGORY(harmonyDeployLog)
Q_DECLARE_LOGGING_CATEGORY(harmonyRunLog)
Q_DECLARE_LOGGING_CATEGORY(harmonyPluginLog)
Q_DECLARE_LOGGING_CATEGORY(harmonyToolchainLog)
