// Copyright (C) 2026 Li-Yaosong.
// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#ifndef HARMONYDEBUGSUPPORT_H
#define HARMONYDEBUGSUPPORT_H

namespace Ohos::Internal {

/**
 * Register the HarmonyDebugWorkerFactory for DEBUG_RUN_MODE.
 * Must be called from ohos.cpp after setupHarmonyRunWorker().
 */
void setupHarmonyDebugWorker();

} // namespace Ohos::Internal

#endif // HARMONYDEBUGSUPPORT_H
