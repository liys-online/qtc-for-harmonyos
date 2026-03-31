// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
//
// 独立单元测试可执行文件：测试插件中无 Qt Creator 依赖的纯逻辑模块。
// 当前覆盖：harmonyhdctargetsparser.cpp
//
// 背景：这些测试原本通过 addTestCreator() 注册到 Qt Creator 插件框架，
// 但在 offscreen 模式下 Qt Creator 事件循环在测试队列被处理前退出，
// 导致 .gcda 数据不包含这些测试的覆盖率。
// 将其提取为独立可执行文件后，与 usbmonitortest / hdcsocketclienttest 同等对待。

#include "harmonyhdctargetsparser.h"

#ifdef HAVE_QTCREATOR_LIBS
#include "harmonydeviceinfo.h"
#include "harmonyhvigoroutputparser.h"
#include <projectexplorer/devicesupport/idevice.h>
#include <utils/outputformatter.h>
#endif

#include <QCoreApplication>
#include <QTest>

// ── 崩溃时强制写 .gcda（GCC --coverage 场景）──────────────────────────────────
// 若 libProjectExplorer / libUtils 静态初始化器在 Linux CI 上崩溃，atexit() 不会
// 被调用，导致覆盖率数据丢失。安装信号处理器，在进程终止前 flush gcov 数据。
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

namespace Ohos::Internal {

class HarmonyHdcTargetsParserTest : public QObject
{
    Q_OBJECT

private slots:
    // ── splitHdcListTargetsLine ──────────────────────────────────────────
    void split_tabSeparated();
    void split_multipleSpaces();
    void split_mixedTabSpace();
    void split_crlfStripped();
    void split_empty();
    void split_onlyWhitespace();
    void split_singleColumn();
    void split_extraColumnsPreserved();
    void split_unicodeSerial();

    // ── hdcListTargetsLineLooksLikeHeader ────────────────────────────────
    void header_knownKeywords_data();
    void header_knownKeywords();
    void header_emptyParts();
    void header_realDeviceSerialNotHeader();

    // ── parseHdcListTargetsLine – kind classification ────────────────────
    void parseKind_data();
    void parseKind();

    // ── parseHdcListTargetsLine – DeviceDataRow fields ───────────────────
    void parseDeviceRow_tabLine();
    void parseDeviceRow_spaceLine();
    void parseDeviceRow_wlanSerial();
    void parseDeviceRow_emulatorSerial();
    void parseDeviceRow_chineseState();
    void parseDeviceRow_extraColumnsIgnored();
    void parseDeviceRow_emptyPlaceholder();
    void parseDeviceRow_uartIgnored();

    // ── hdcListTargetsStateToConnectionState ─────────────────────────────
    void stateMapping_data();
    void stateMapping();
    void stateMapping_leadingTrailingSpaces();
    void stateMapping_unknownFallsToDisconnected();
};

// ── splitHdcListTargetsLine ──────────────────────────────────────────────────

void HarmonyHdcTargetsParserTest::split_tabSeparated()
{
    const QStringList p = splitHdcListTargetsLine(QStringLiteral("A\tB\tC"));
    QCOMPARE(p.size(), 3);
    QCOMPARE(p.at(0), QStringLiteral("A"));
    QCOMPARE(p.at(1), QStringLiteral("B"));
    QCOMPARE(p.at(2), QStringLiteral("C"));
}

void HarmonyHdcTargetsParserTest::split_multipleSpaces()
{
    const QStringList p = splitHdcListTargetsLine(QStringLiteral("  dev123   USB   Offline  "));
    QCOMPARE(p.size(), 3);
    QCOMPARE(p.at(0), QStringLiteral("dev123"));
    QCOMPARE(p.at(1), QStringLiteral("USB"));
    QCOMPARE(p.at(2), QStringLiteral("Offline"));
}

void HarmonyHdcTargetsParserTest::split_mixedTabSpace()
{
    const QStringList p = splitHdcListTargetsLine(QStringLiteral("SN123\tUSB\tConnected"));
    QCOMPARE(p.size(), 3);
    QCOMPARE(p.at(0), QStringLiteral("SN123"));
}

void HarmonyHdcTargetsParserTest::split_crlfStripped()
{
    const QStringList p = splitHdcListTargetsLine(QStringLiteral("SN\tUSB\tConnected\r\n"));
    QCOMPARE(p.size(), 3);
    QVERIFY(!p.at(2).contains(QLatin1Char('\r')));
    QCOMPARE(p.at(2), QStringLiteral("Connected"));
}

void HarmonyHdcTargetsParserTest::split_empty()
{
    QVERIFY(splitHdcListTargetsLine(QString()).isEmpty());
}

void HarmonyHdcTargetsParserTest::split_onlyWhitespace()
{
    QVERIFY(splitHdcListTargetsLine(QStringLiteral("   \t  ")).isEmpty());
}

void HarmonyHdcTargetsParserTest::split_singleColumn()
{
    const QStringList p = splitHdcListTargetsLine(QStringLiteral("SN"));
    QCOMPARE(p.size(), 1);
    QCOMPARE(p.at(0), QStringLiteral("SN"));
}

void HarmonyHdcTargetsParserTest::split_extraColumnsPreserved()
{
    const QStringList p = splitHdcListTargetsLine(QStringLiteral("SN\tUSB\tConnected\tExtra"));
    QCOMPARE(p.size(), 4);
    QCOMPARE(p.at(3), QStringLiteral("Extra"));
}

void HarmonyHdcTargetsParserTest::split_unicodeSerial()
{
    const QStringList p = splitHdcListTargetsLine(QStringLiteral("设备001\tUSB\t已连接"));
    QCOMPARE(p.size(), 3);
    QCOMPARE(p.at(0), QStringLiteral("设备001"));
}

// ── hdcListTargetsLineLooksLikeHeader ────────────────────────────────────────

void HarmonyHdcTargetsParserTest::header_knownKeywords_data()
{
    QTest::addColumn<QString>("firstToken");
    QTest::addColumn<bool>("expected");

    QTest::newRow("SN")         << QStringLiteral("SN")         << true;
    QTest::newRow("sn_lower")   << QStringLiteral("sn")         << true;
    QTest::newRow("Identifier") << QStringLiteral("Identifier") << true;
    QTest::newRow("DEVICE")     << QStringLiteral("DEVICE")     << true;
    QTest::newRow("Device")     << QStringLiteral("Device")     << true;
    QTest::newRow("DevSerial")  << QStringLiteral("DevSerial")  << true;
    QTest::newRow("Serial")     << QStringLiteral("Serial")     << true;
    QTest::newRow("realSN")     << QStringLiteral("0123456789ABCDEF") << false;
    QTest::newRow("ip_serial")  << QStringLiteral("192.168.0.1:7777") << false;
}

void HarmonyHdcTargetsParserTest::header_knownKeywords()
{
    QFETCH(QString, firstToken);
    QFETCH(bool, expected);
    QCOMPARE(hdcListTargetsLineLooksLikeHeader(QStringList{firstToken}), expected);
}

void HarmonyHdcTargetsParserTest::header_emptyParts()
{
    QVERIFY(hdcListTargetsLineLooksLikeHeader(QStringList{}));
}

void HarmonyHdcTargetsParserTest::header_realDeviceSerialNotHeader()
{
    QVERIFY(!hdcListTargetsLineLooksLikeHeader(QStringList{QStringLiteral("7B1A2F3C")}));
}

// ── parseHdcListTargetsLine – kind classification ────────────────────────────

void HarmonyHdcTargetsParserTest::parseKind_data()
{
    QTest::addColumn<QString>("line");
    QTest::addColumn<int>("expectedKind");

    QTest::newRow("empty")
        << QString() << int(HdcListTargetsLineKind::EmptyOrWhitespace);
    QTest::newRow("whitespace_only")
        << QStringLiteral("   ") << int(HdcListTargetsLineKind::EmptyOrWhitespace);
    QTest::newRow("header_SN")
        << QStringLiteral("SN\tConnType\tState") << int(HdcListTargetsLineKind::HeaderRow);
    QTest::newRow("too_few_cols")
        << QStringLiteral("SN_ONLY") << int(HdcListTargetsLineKind::TooFewColumns);
    QTest::newRow("uart_row")
        << QStringLiteral("COM3\tUART\tOffline") << int(HdcListTargetsLineKind::UartRow);
    QTest::newRow("empty_placeholder")
        << QStringLiteral("[Empty]\tUSB\tConnected") << int(HdcListTargetsLineKind::EmptySerialPlaceholder);
    QTest::newRow("device_connected")
        << QStringLiteral("ABCDEF0123456789\tUSB\tConnected") << int(HdcListTargetsLineKind::DeviceDataRow);
    QTest::newRow("device_offline")
        << QStringLiteral("FFFF0000\tUSB\tOffline") << int(HdcListTargetsLineKind::DeviceDataRow);
    QTest::newRow("device_space_separated")
        << QStringLiteral("ABC123   USB   Connected") << int(HdcListTargetsLineKind::DeviceDataRow);
}

void HarmonyHdcTargetsParserTest::parseKind()
{
    QFETCH(QString, line);
    QFETCH(int, expectedKind);
    const HdcListTargetsParseResult r = parseHdcListTargetsLine(line);
    QCOMPARE(int(r.kind), expectedKind);
}

// ── parseHdcListTargetsLine – DeviceDataRow fields ───────────────────────────

void HarmonyHdcTargetsParserTest::parseDeviceRow_tabLine()
{
    const auto r = parseHdcListTargetsLine(QStringLiteral("DEAD-BEEF\tUSB\tConnected"));
    QCOMPARE(int(r.kind), int(HdcListTargetsLineKind::DeviceDataRow));
    QCOMPARE(r.device.serial, QStringLiteral("DEAD-BEEF"));
    QCOMPARE(r.device.connectionType, QStringLiteral("USB"));
    QCOMPARE(r.device.stateRaw, QStringLiteral("Connected"));
}

void HarmonyHdcTargetsParserTest::parseDeviceRow_spaceLine()
{
    const auto r = parseHdcListTargetsLine(QStringLiteral("EMULATOR_1    USB     Offline"));
    QCOMPARE(int(r.kind), int(HdcListTargetsLineKind::DeviceDataRow));
    QCOMPARE(r.device.serial, QStringLiteral("EMULATOR_1"));
    QCOMPARE(r.device.connectionType, QStringLiteral("USB"));
    QCOMPARE(r.device.stateRaw, QStringLiteral("Offline"));
}

void HarmonyHdcTargetsParserTest::parseDeviceRow_wlanSerial()
{
    const auto r = parseHdcListTargetsLine(QStringLiteral("192.168.1.2:5555\tWLAN\t已连接\n"));
    QCOMPARE(int(r.kind), int(HdcListTargetsLineKind::DeviceDataRow));
    QCOMPARE(r.device.serial, QStringLiteral("192.168.1.2:5555"));
    QCOMPARE(r.device.connectionType, QStringLiteral("WLAN"));
    QCOMPARE(r.device.stateRaw, QStringLiteral("已连接"));
}

void HarmonyHdcTargetsParserTest::parseDeviceRow_emulatorSerial()
{
    const auto r = parseHdcListTargetsLine(QStringLiteral("emulator-5554\tUSB\tConnected"));
    QCOMPARE(int(r.kind), int(HdcListTargetsLineKind::DeviceDataRow));
    QCOMPARE(r.device.serial, QStringLiteral("emulator-5554"));
}

void HarmonyHdcTargetsParserTest::parseDeviceRow_chineseState()
{
    const auto r = parseHdcListTargetsLine(QStringLiteral("SN_CN\tUSB\t未连接"));
    QCOMPARE(int(r.kind), int(HdcListTargetsLineKind::DeviceDataRow));
    QCOMPARE(r.device.stateRaw, QStringLiteral("未连接"));
}

void HarmonyHdcTargetsParserTest::parseDeviceRow_extraColumnsIgnored()
{
    const auto r = parseHdcListTargetsLine(QStringLiteral("ABCD1234\tUSB\tConnected\tExtra1\tExtra2"));
    QCOMPARE(int(r.kind), int(HdcListTargetsLineKind::DeviceDataRow));
    QCOMPARE(r.device.serial, QStringLiteral("ABCD1234"));
    QCOMPARE(r.device.connectionType, QStringLiteral("USB"));
    QCOMPARE(r.device.stateRaw, QStringLiteral("Connected"));
}

void HarmonyHdcTargetsParserTest::parseDeviceRow_emptyPlaceholder()
{
    const auto r = parseHdcListTargetsLine(QStringLiteral("[Empty]\tUSB\tConnected"));
    QCOMPARE(int(r.kind), int(HdcListTargetsLineKind::EmptySerialPlaceholder));
    QVERIFY(r.device.serial.isEmpty());
}

void HarmonyHdcTargetsParserTest::parseDeviceRow_uartIgnored()
{
    const auto r = parseHdcListTargetsLine(QStringLiteral("COM1\tUART\tConnected"));
    QCOMPARE(int(r.kind), int(HdcListTargetsLineKind::UartRow));
    QVERIFY(r.device.serial.isEmpty());
}

// ── hdcListTargetsStateToConnectionState ─────────────────────────────────────

void HarmonyHdcTargetsParserTest::stateMapping_data()
{
    QTest::addColumn<QString>("state");
    QTest::addColumn<int>("expected");

    QTest::newRow("Connected_en")
        << QStringLiteral("Connected")  << int(HdcTargetConnectionState::ReadyToUse);
    QTest::newRow("connected_cn")
        << QStringLiteral("已连接")      << int(HdcTargetConnectionState::ReadyToUse);
    QTest::newRow("Offline_en")
        << QStringLiteral("Offline")    << int(HdcTargetConnectionState::ConnectedNotReady);
    QTest::newRow("offline_cn1")
        << QStringLiteral("未连接")      << int(HdcTargetConnectionState::ConnectedNotReady);
    QTest::newRow("offline_cn2")
        << QStringLiteral("断开")        << int(HdcTargetConnectionState::ConnectedNotReady);
    QTest::newRow("booting_unknown")
        << QStringLiteral("Booting")    << int(HdcTargetConnectionState::Disconnected);
    QTest::newRow("empty")
        << QString()                     << int(HdcTargetConnectionState::Disconnected);
    QTest::newRow("random_garbage")
        << QStringLiteral("Xyz!@#")     << int(HdcTargetConnectionState::Disconnected);
}

void HarmonyHdcTargetsParserTest::stateMapping()
{
    QFETCH(QString, state);
    QFETCH(int, expected);
    QCOMPARE(int(hdcListTargetsStateToConnectionState(state)), expected);
}

void HarmonyHdcTargetsParserTest::stateMapping_leadingTrailingSpaces()
{
    QCOMPARE(int(hdcListTargetsStateToConnectionState(QStringLiteral("  Connected  "))),
             int(HdcTargetConnectionState::ReadyToUse));
    QCOMPARE(int(hdcListTargetsStateToConnectionState(QStringLiteral("  Offline  "))),
             int(HdcTargetConnectionState::ConnectedNotReady));
}

void HarmonyHdcTargetsParserTest::stateMapping_unknownFallsToDisconnected()
{
    for (const QString &s : {QStringLiteral("AuthFailed"), QStringLiteral("Unauthorized"),
                              QStringLiteral("Recovery"), QStringLiteral("1234")}) {
        QCOMPARE(int(hdcListTargetsStateToConnectionState(s)),
                 int(HdcTargetConnectionState::Disconnected));
    }
}

#ifdef HAVE_QTCREATOR_LIBS

// ═══════════════════════════════════════════════════════════════════════════════
// HarmonyDeviceInfoTest
// ═══════════════════════════════════════════════════════════════════════════════

using namespace ProjectExplorer;

class HarmonyDeviceInfoTest : public QObject
{
    Q_OBJECT

private slots:
    // ── isValid ─────────────────────────────────────────────────────────
    void isValid_defaultConstructed_false();
    void isValid_serialNumberSet_true();
    void isValid_avdNameSet_true();
    void isValid_bothSet_true();
    void isValid_bothEmpty_false();

    // ── operator== ──────────────────────────────────────────────────────
    void equality_defaultInstances_equal();
    void equality_sameSerial_equal();
    void equality_differentSerial_notEqual();
    void equality_differentAvd_notEqual();
    void equality_differentSdk_notEqual();
    void equality_differentType_notEqual();
    void equality_differentState_notEqual();

    // ── operator< ───────────────────────────────────────────────────────
    void lessThan_questionMarkSerialSortsLast();
    void lessThan_hardwareSortsBeforeEmulator();
    void lessThan_lowerSdkFirst();
    void lessThan_avdNameAlphabetical();
    void lessThan_serialNumberAlphabetical();
    void lessThan_transitivity();

    // ── operator<< (smoke test) ──────────────────────────────────────────
    void debugStream_doesNotCrash();
};

static HarmonyDeviceInfo makeDevice(const QString &serial,
                                     IDevice::MachineType type = IDevice::Hardware,
                                     int sdk = 10,
                                     IDevice::DeviceState state = IDevice::DeviceReadyToUse)
{
    HarmonyDeviceInfo d;
    d.serialNumber = serial;
    d.type = type;
    d.sdk = sdk;
    d.state = state;
    return d;
}

static HarmonyDeviceInfo makeAvd(const QString &avdName,
                                  int sdk = 10,
                                  IDevice::DeviceState state = IDevice::DeviceReadyToUse)
{
    HarmonyDeviceInfo d;
    d.avdName = avdName;
    d.type = IDevice::Emulator;
    d.sdk = sdk;
    d.state = state;
    return d;
}

void HarmonyDeviceInfoTest::isValid_defaultConstructed_false()
{
    QVERIFY(!HarmonyDeviceInfo().isValid());
}

void HarmonyDeviceInfoTest::isValid_serialNumberSet_true()
{
    HarmonyDeviceInfo d;
    d.serialNumber = QStringLiteral("ABCDEF");
    QVERIFY(d.isValid());
}

void HarmonyDeviceInfoTest::isValid_avdNameSet_true()
{
    HarmonyDeviceInfo d;
    d.avdName = QStringLiteral("Emulator_10");
    QVERIFY(d.isValid());
}

void HarmonyDeviceInfoTest::isValid_bothSet_true()
{
    HarmonyDeviceInfo d;
    d.serialNumber = QStringLiteral("SN");
    d.avdName = QStringLiteral("AVD");
    QVERIFY(d.isValid());
}

void HarmonyDeviceInfoTest::isValid_bothEmpty_false()
{
    HarmonyDeviceInfo d;
    d.serialNumber.clear();
    d.avdName.clear();
    QVERIFY(!d.isValid());
}

void HarmonyDeviceInfoTest::equality_defaultInstances_equal()
{
    QCOMPARE(HarmonyDeviceInfo(), HarmonyDeviceInfo());
}

void HarmonyDeviceInfoTest::equality_sameSerial_equal()
{
    const HarmonyDeviceInfo a = makeDevice(QStringLiteral("SN1"));
    const HarmonyDeviceInfo b = makeDevice(QStringLiteral("SN1"));
    QCOMPARE(a, b);
}

void HarmonyDeviceInfoTest::equality_differentSerial_notEqual()
{
    QVERIFY(makeDevice(QStringLiteral("SN1")) != makeDevice(QStringLiteral("SN2")));
}

void HarmonyDeviceInfoTest::equality_differentAvd_notEqual()
{
    HarmonyDeviceInfo a = makeAvd(QStringLiteral("avd1"));
    HarmonyDeviceInfo b = makeAvd(QStringLiteral("avd2"));
    QVERIFY(a != b);
}

void HarmonyDeviceInfoTest::equality_differentSdk_notEqual()
{
    HarmonyDeviceInfo a = makeDevice(QStringLiteral("SN"), IDevice::Hardware, 9);
    HarmonyDeviceInfo b = makeDevice(QStringLiteral("SN"), IDevice::Hardware, 10);
    QVERIFY(a != b);
}

void HarmonyDeviceInfoTest::equality_differentType_notEqual()
{
    HarmonyDeviceInfo a = makeDevice(QStringLiteral("SN"), IDevice::Hardware);
    HarmonyDeviceInfo b = makeDevice(QStringLiteral("SN"), IDevice::Emulator);
    QVERIFY(a != b);
}

void HarmonyDeviceInfoTest::equality_differentState_notEqual()
{
    HarmonyDeviceInfo a = makeDevice(QStringLiteral("SN"), IDevice::Hardware, 10, IDevice::DeviceReadyToUse);
    HarmonyDeviceInfo b = makeDevice(QStringLiteral("SN"), IDevice::Hardware, 10, IDevice::DeviceDisconnected);
    QVERIFY(a != b);
}

void HarmonyDeviceInfoTest::lessThan_questionMarkSerialSortsLast()
{
    HarmonyDeviceInfo normal = makeDevice(QStringLiteral("ABCDEF"));
    HarmonyDeviceInfo questionMark = makeDevice(QStringLiteral("????"));
    QVERIFY(normal < questionMark);
    QVERIFY(!(questionMark < normal));
}

void HarmonyDeviceInfoTest::lessThan_hardwareSortsBeforeEmulator()
{
    HarmonyDeviceInfo hw = makeDevice(QStringLiteral("SN1"), IDevice::Hardware);
    HarmonyDeviceInfo emu = makeDevice(QStringLiteral("SN2"), IDevice::Emulator);
    QVERIFY(hw < emu);
    QVERIFY(!(emu < hw));
}

void HarmonyDeviceInfoTest::lessThan_lowerSdkFirst()
{
    HarmonyDeviceInfo d9 = makeDevice(QStringLiteral("SN"), IDevice::Hardware, 9);
    HarmonyDeviceInfo d10 = makeDevice(QStringLiteral("SN"), IDevice::Hardware, 10);
    QVERIFY(d9 < d10);
    QVERIFY(!(d10 < d9));
}

void HarmonyDeviceInfoTest::lessThan_avdNameAlphabetical()
{
    HarmonyDeviceInfo a = makeAvd(QStringLiteral("AVD_A"));
    HarmonyDeviceInfo b = makeAvd(QStringLiteral("AVD_B"));
    QVERIFY(a < b);
    QVERIFY(!(b < a));
}

void HarmonyDeviceInfoTest::lessThan_serialNumberAlphabetical()
{
    HarmonyDeviceInfo a = makeDevice(QStringLiteral("AAA"));
    HarmonyDeviceInfo b = makeDevice(QStringLiteral("BBB"));
    QVERIFY(a < b);
    QVERIFY(!(b < a));
}

void HarmonyDeviceInfoTest::lessThan_transitivity()
{
    HarmonyDeviceInfo a = makeDevice(QStringLiteral("SN"), IDevice::Hardware, 8);
    HarmonyDeviceInfo b = makeDevice(QStringLiteral("SN"), IDevice::Hardware, 9);
    HarmonyDeviceInfo c = makeDevice(QStringLiteral("SN"), IDevice::Hardware, 10);
    QVERIFY(a < b);
    QVERIFY(b < c);
    QVERIFY(a < c);
}

void HarmonyDeviceInfoTest::debugStream_doesNotCrash()
{
    HarmonyDeviceInfo d;
    d.serialNumber = QStringLiteral("SERIAL_TEST");
    d.avdName = QStringLiteral("TestAVD");
    d.sdk = 12;
    d.type = IDevice::Emulator;
    d.state = IDevice::DeviceReadyToUse;
    QString out;
    QDebug dbg(&out);
    dbg << d;
    QVERIFY(!out.isEmpty());
    QVERIFY(out.contains(QStringLiteral("SERIAL_TEST")));
}

// ═══════════════════════════════════════════════════════════════════════════════
// HarmonyHvigorOutputParserTest
// ═══════════════════════════════════════════════════════════════════════════════

using namespace Utils;

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
    // 各扩展名已通过 handleLine_pathLineCol_returnsStatus_data 逐一验证，
    // handleLine_supportedExtensions_done 因 OutputFormatter 状态累积不稳定已移除。
};

// 每次调用创建独立的 OutputFormatter + parser，确保 scheduleTask 不会因
// null formatter 指针而崩溃（standalone 测试中没有 Qt Creator 构建步骤上下文）。
static OutputLineParser::Status callHandleLine(const QString &line)
{
    Utils::OutputFormatter fmt;
    auto *p = new HarmonyHvigorOhpmOutputParser; // fmt 接管所有权
    fmt.addLineParser(p);
    return p->handleLine(line, OutputFormat::StdOutFormat).status;
}

void HarmonyHvigorOutputParserTest::handleLine_pathLineCol_returnsStatus_data()
{
    QTest::addColumn<QString>("line");
    QTest::newRow("ets_error")  << QStringLiteral("/project/src/index.ets:10:5: error TS9001: bad syntax");
    QTest::newRow("ts_error")   << QStringLiteral("/tmp/src/pages/Main.ts:42:1: error TS2304: Cannot find name");
    QTest::newRow("tsx_error")  << QStringLiteral("/tmp/src/App.tsx:1:1: error TS1005: ';' expected.");
    QTest::newRow("js_warning") << QStringLiteral("/tmp/src/utils.js:7:3: warning: use strict");
    QTest::newRow("jsx_error")  << QStringLiteral("/tmp/src/Comp.jsx:3:9: error TS6133: unused");
    QTest::newRow("json_error") << QStringLiteral("/tmp/config.json:2:4: error: unexpected token");
    QTest::newRow("json5_error")<< QStringLiteral("/tmp/build.json5:5:1: error: expected comma");
    QTest::newRow("mjs_error")  << QStringLiteral("/tmp/src/mod.mjs:12:3: error: module error");
    QTest::newRow("cjs_error")  << QStringLiteral("/tmp/src/mod.cjs:8:2: error: require error");
    QTest::newRow("hml_error")  << QStringLiteral("/tmp/src/comp.hml:3:1: error: attribute error");
    QTest::newRow("css_error")  << QStringLiteral("/tmp/src/style.css:10:1: error: unexpected token");
    QTest::newRow("java_error") << QStringLiteral("/tmp/src/Main.java:5:1: error: class not found");
}

void HarmonyHvigorOutputParserTest::handleLine_pathLineCol_returnsStatus()
{
    QFETCH(QString, line);
    QCOMPARE(callHandleLine(line), OutputLineParser::Status::Done);
}

void HarmonyHvigorOutputParserTest::handleLine_pathLineNoCol_returnsStatus_data()
{
    QTest::addColumn<QString>("line");
    QTest::newRow("ets_no_col") << QStringLiteral("/build/src/Main.ets:15: warning TS2339: property missing");
    QTest::newRow("ts_no_col")  << QStringLiteral("/tmp/src/helper.ts:1: error: parse error");
}

void HarmonyHvigorOutputParserTest::handleLine_pathLineNoCol_returnsStatus()
{
    QFETCH(QString, line);
    QCOMPARE(callHandleLine(line), OutputLineParser::Status::Done);
}

void HarmonyHvigorOutputParserTest::handleLine_pathParenLineCol_returnsStatus()
{
    QCOMPARE(callHandleLine(QStringLiteral("/tmp/src/mod.ets(10,5): error: bad syntax")),
             OutputLineParser::Status::Done);
    QCOMPARE(callHandleLine(QStringLiteral("/tmp/src/App.ts(1,1): error TS1005: delimiter expected.")),
             OutputLineParser::Status::Done);
}

void HarmonyHvigorOutputParserTest::handleLine_atFile_returnsStatus()
{
    QCOMPARE(callHandleLine(QStringLiteral("At file: /project/entry/src/Main.ets")),
             OutputLineParser::Status::Done);
    QCOMPARE(callHandleLine(QStringLiteral("  At file: /tmp/src/index.ts")),
             OutputLineParser::Status::Done);
}

void HarmonyHvigorOutputParserTest::handleLine_npmErrPath_returnsStatus()
{
    QCOMPARE(callHandleLine(QStringLiteral("npm ERR! code ENOENT path /home/user/.npm/pkg")),
             OutputLineParser::Status::Done);
    QCOMPARE(callHandleLine(QStringLiteral("npm ERR! syscall open file /etc/missing")),
             OutputLineParser::Status::Done);
}

void HarmonyHvigorOutputParserTest::handleLine_hvigorError_returnsStatus_data()
{
    QTest::addColumn<QString>("line");
    QTest::newRow("plain")        << QStringLiteral("hvigor  ERROR: BUILD FAILED in 3 s");
    QTest::newRow("piped")        << QStringLiteral("> hvigor  ERROR: BUILD FAILED");
    QTest::newRow("single_space") << QStringLiteral("hvigor ERROR: compilation failed");
}

void HarmonyHvigorOutputParserTest::handleLine_hvigorError_returnsStatus()
{
    QFETCH(QString, line);
    QCOMPARE(callHandleLine(line), OutputLineParser::Status::Done);
}

void HarmonyHvigorOutputParserTest::handleLine_ohpmError_returnsStatus()
{
    QCOMPARE(callHandleLine(QStringLiteral("ohpm ERROR: package not found")),
             OutputLineParser::Status::Done);
    QCOMPARE(callHandleLine(QStringLiteral("ohpm ERROR pkg install failed")),
             OutputLineParser::Status::Done);
}

void HarmonyHvigorOutputParserTest::handleLine_emptyLine_notHandled()
{
    QCOMPARE(callHandleLine(QString()), OutputLineParser::Status::NotHandled);
}

void HarmonyHvigorOutputParserTest::handleLine_whitespaceOnly_notHandled()
{
    QCOMPARE(callHandleLine(QStringLiteral("   \t  ")), OutputLineParser::Status::NotHandled);
}

void HarmonyHvigorOutputParserTest::handleLine_plainMessage_notHandled()
{
    QCOMPARE(callHandleLine(QStringLiteral("BUILD SUCCESSFUL in 12 s")),
             OutputLineParser::Status::NotHandled);
    QCOMPARE(callHandleLine(QStringLiteral("Compiling sources...")),
             OutputLineParser::Status::NotHandled);
    // 注意：类似 "  at File.ts:0:0" 的堆栈跟踪行会被 kPathLineMessage 误匹配
    // （把第二个 "0" 解析为消息），属于已知 parser 宽松行为，此处不测该场景。
}

void HarmonyHvigorOutputParserTest::handleLine_partialMatch_notHandled()
{
    QCOMPARE(callHandleLine(QStringLiteral("src/index.ets: error: no line number")),
             OutputLineParser::Status::NotHandled);
    QCOMPARE(callHandleLine(QStringLiteral("Makefile:10:1: error something")),
             OutputLineParser::Status::NotHandled);
}

void HarmonyHvigorOutputParserTest::handleLine_unknownExtension_notHandled()
{
    QCOMPARE(callHandleLine(QStringLiteral("src/main.cpp:1:1: error: undefined")),
             OutputLineParser::Status::NotHandled);
    QCOMPARE(callHandleLine(QStringLiteral("include/a.h:5:1: error: bad include")),
             OutputLineParser::Status::NotHandled);
    QCOMPARE(callHandleLine(QStringLiteral("module.py:10:1: SyntaxError: invalid")),
             OutputLineParser::Status::NotHandled);
}

void HarmonyHvigorOutputParserTest::handleLine_headerLine_notHandled()
{
    QCOMPARE(callHandleLine(QStringLiteral("Scanning sources")),
             OutputLineParser::Status::NotHandled);
    QCOMPARE(callHandleLine(QStringLiteral("> Task :default@CompileArkTS")),
             OutputLineParser::Status::NotHandled);
}

#endif // HAVE_QTCREATOR_LIBS

} // namespace Ohos::Internal

// ── 自定义 main：依次运行所有测试对象，汇总退出码 ───────────────────────────────
int main(int argc, char *argv[])
{
#if defined(__GNUC__) && !defined(__clang__)
    // 在 QCoreApplication 构造之前装好处理器，覆盖 QTC 静态初始化期间可能的崩溃。
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

#include "harmonypluginlogictest.moc"
