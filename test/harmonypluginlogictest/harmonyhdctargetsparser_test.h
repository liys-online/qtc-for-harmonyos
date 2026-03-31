// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
#pragma once

#ifdef WITH_TESTS

#include <QObject>

namespace Ohos::Internal {

class HarmonyHdcTargetsParserTest : public QObject
{
    Q_OBJECT

private slots:
    void split_tabSeparated();
    void split_multipleSpaces();
    void split_mixedTabSpace();
    void split_crlfStripped();
    void split_empty();
    void split_onlyWhitespace();
    void split_singleColumn();
    void split_extraColumnsPreserved();
    void split_unicodeSerial();

    void header_knownKeywords_data();
    void header_knownKeywords();
    void header_emptyParts();
    void header_realDeviceSerialNotHeader();

    void parseKind_data();
    void parseKind();

    void parseDeviceRow_tabLine();
    void parseDeviceRow_spaceLine();
    void parseDeviceRow_wlanSerial();
    void parseDeviceRow_emulatorSerial();
    void parseDeviceRow_chineseState();
    void parseDeviceRow_extraColumnsIgnored();
    void parseDeviceRow_emptyPlaceholder();
    void parseDeviceRow_uartIgnored();

    void stateMapping_data();
    void stateMapping();
    void stateMapping_leadingTrailingSpaces();
    void stateMapping_unknownFallsToDisconnected();
};

} // namespace Ohos::Internal

#endif // WITH_TESTS
