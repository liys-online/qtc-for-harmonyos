// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
#pragma once

#ifdef WITH_TESTS

#include <QObject>

namespace Ohos::Internal {

class HarmonyHvigorOutputParserTest : public QObject
{
    Q_OBJECT

private slots:
    void handleLine_pathLineCol_returnsStatus_data();
    void handleLine_pathLineCol_returnsStatus();
    void handleLine_pathLineNoCol_returnsStatus_data();
    void handleLine_pathLineNoCol_returnsStatus();
    void handleLine_pathParenLineCol_returnsStatus();
    void handleLine_atFile_returnsStatus();
    void handleLine_npmErrPath_returnsStatus();
    void handleLine_hvigorError_returnsStatus_data();
    void handleLine_hvigorError_returnsStatus();
    void handleLine_ohpmError_returnsStatus();
    void handleLine_emptyLine_notHandled();
    void handleLine_whitespaceOnly_notHandled();
    void handleLine_plainMessage_notHandled();
    void handleLine_partialMatch_notHandled();
    void handleLine_unknownExtension_notHandled();
    void handleLine_headerLine_notHandled();
};

} // namespace Ohos::Internal

#endif // WITH_TESTS
