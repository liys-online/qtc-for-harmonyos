// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
#pragma once

#include <utils/filepath.h>

namespace Ohos::Internal {

/**
 * Uses system `tar` (macOS / Linux / Windows 10+). Supports .zip, .tar, .tar.gz, .tgz, .tar.bz2, .tbz2.
 * After the outer archive is unpacked, supported nested archives are unpacked too: files in
 * @p destDir and files in immediate subfolders (e.g. wrapper dir + inner .zip), largest first,
 * each into its parent directory, then the inner file is removed (up to a bounded number of rounds).
 */
bool extractHarmonySdkArchive(const Utils::FilePath &archive,
                              const Utils::FilePath &destDir,
                              QString *errorMessage = nullptr);

/**
 * Search under @p searchRoot (recursive, bounded depth) for an OpenHarmony SDK root
 * (same rules as HarmonyConfig::isValidSdk).
 */
Utils::FilePath findOhosSdkRootUnder(const Utils::FilePath &searchRoot, int maxDepth = 8);

/**
 * Search under @p searchRoot (recursive, bounded depth) for \c bin/qmake (Qt for OpenHarmony prefix).
 */
Utils::FilePath findQtOhQmakeUnder(const Utils::FilePath &searchRoot, int maxDepth = 8);

} // namespace Ohos::Internal
