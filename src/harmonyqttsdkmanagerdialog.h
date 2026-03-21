// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
#pragma once

class QWidget;

namespace Ohos::Internal {

/**
 * Qt for OpenHarmony SDK release browser + download: binary catalog JSON
 * (\c qt-for-openharmony.binary-catalog v1) from a configurable HTTPS URL.
 */
void executeHarmonyQtOhSdkManagerDialog(QWidget *parent = nullptr);

} // namespace Ohos::Internal
