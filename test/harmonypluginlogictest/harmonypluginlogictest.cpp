// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
//
// Standalone unit-test executable.
// Test classes are declared/implemented in *_test.h / *_test.cpp.
// The same files are also compiled into libHarmony.so via add_qtc_plugin TEST_SOURCES
// and registered through addTestCreator() in ohos.cpp for use with `-test Harmony`.
//
// #include of .cpp files below is intentional: lets the same code serve both
// the standalone executable and libHarmony.so as separate CMake targets.

#define WITH_TESTS  // standalone always enables all tests
#include "harmonyhdctargetsparser_test.cpp"
#ifdef HAVE_QTCREATOR_LIBS
#  include "harmonydeviceinfo_test.cpp"
#  include "harmonyhvigoroutputparser_test.cpp"
#endif

#include <QCoreApplication>
#include <QTest>

#if defined(__GNUC__) && !defined(__clang__)
#  include <csignal>
extern "C" void __gcov_dump() __attribute__((weak));
static void flushCoverageAndRethrow(int sig)
{
    if (&__gcov_dump)
        __gcov_dump();
    signal(sig, SIG_DFL);
    raise(sig);
}
#endif

int main(int argc, char *argv[])
{
#if defined(__GNUC__) && !defined(__clang__)
    if (&__gcov_dump) {
        signal(SIGSEGV, flushCoverageAndRethrow);
        signal(SIGABRT, flushCoverageAndRethrow);
        signal(SIGBUS,  flushCoverageAndRethrow);
    }
#endif
    QCoreApplication app(argc, argv);
    int status = 0;

    {
        Ohos::Internal::HarmonyHdcTargetsParserTest t;
        status |= QTest::qExec(&t, argc, argv);
    }
#ifdef HAVE_QTCREATOR_LIBS
    {
        Ohos::Internal::HarmonyDeviceInfoTest t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        Ohos::Internal::HarmonyHvigorOutputParserTest t;
        status |= QTest::qExec(&t, argc, argv);
    }
#endif
    return status;
}
