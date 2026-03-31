// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
#pragma once

#ifdef WITH_TESTS

#include <QObject>

namespace Ohos::Internal {

class HarmonyDeviceInfoTest : public QObject
{
    Q_OBJECT

private slots:
    void isValid_defaultConstructed_false();
    void isValid_serialNumberSet_true();
    void isValid_avdNameSet_true();
    void isValid_bothSet_true();
    void isValid_bothEmpty_false();

    void equality_defaultInstances_equal();
    void equality_sameSerial_equal();
    void equality_differentSerial_notEqual();
    void equality_differentAvd_notEqual();
    void equality_differentSdk_notEqual();
    void equality_differentType_notEqual();
    void equality_differentState_notEqual();

    void lessThan_questionMarkSerialSortsLast();
    void lessThan_hardwareSortsBeforeEmulator();
    void lessThan_lowerSdkFirst();
    void lessThan_avdNameAlphabetical();
    void lessThan_serialNumberAlphabetical();
    void lessThan_transitivity();

    void debugStream_doesNotCrash();
};

} // namespace Ohos::Internal

#endif // WITH_TESTS
