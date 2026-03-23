// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <projectexplorer/ioutputparser.h>

namespace Ohos::Internal {

/*!
    Parses hvigor / ohpm / ArkTS-style compiler lines into Issues (with file links).

    Typical formats:
    \list
    \li \c path.ets:10:5: error message
    \li \c path.ts(10,5): error message
    \li \c At file: path/to/File.ets
    \endlist
*/
class HarmonyHvigorOhpmOutputParser final : public ProjectExplorer::OutputTaskParser
{
public:
    HarmonyHvigorOhpmOutputParser();

    Utils::OutputLineParser::Result handleLine(const QString &line,
                                               Utils::OutputFormat type) override;

private:
    static bool looksLikeWarning(const QString &message);
};

} // namespace Ohos::Internal
