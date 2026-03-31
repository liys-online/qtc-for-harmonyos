// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "harmonyhdctargetsparser_test.h"
#include "harmonyhdctargetsparser.h"

#include <QTest>

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
    // Tab 模式优先：只要含 tab 就用 tab 分割
    const QStringList p = splitHdcListTargetsLine(QStringLiteral("SN123\tUSB\tConnected"));
    QCOMPARE(p.size(), 3);
    QCOMPARE(p.at(0), QStringLiteral("SN123"));
}

void HarmonyHdcTargetsParserTest::split_crlfStripped()
{
    const QStringList p = splitHdcListTargetsLine(QStringLiteral("SN\tUSB\tConnected\r\n"));
    QCOMPARE(p.size(), 3);
    // \r 应被剥离，最后一列不含 \r
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
    // 含 CJK 字符的序列号应正常分割
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
    const QStringList parts{firstToken, QStringLiteral("USB"), QStringLiteral("Connected")};
    QCOMPARE(hdcListTargetsLineLooksLikeHeader(parts), expected);
}

void HarmonyHdcTargetsParserTest::header_emptyParts()
{
    QVERIFY(hdcListTargetsLineLooksLikeHeader({}));
}

void HarmonyHdcTargetsParserTest::header_realDeviceSerialNotHeader()
{
    QVERIFY(!hdcListTargetsLineLooksLikeHeader(
        {QStringLiteral("ABCDEF1234567890"), QStringLiteral("USB"), QStringLiteral("Connected")}));
}

// ── parseHdcListTargetsLine – kind ───────────────────────────────────────────

void HarmonyHdcTargetsParserTest::parseKind_data()
{
    QTest::addColumn<QString>("line");
    QTest::addColumn<int>("expectedKind");

    QTest::newRow("empty")
        << QString()
        << int(HdcListTargetsLineKind::EmptyOrWhitespace);
    QTest::newRow("whitespace_only")
        << QStringLiteral("   \t   ")
        << int(HdcListTargetsLineKind::EmptyOrWhitespace);
    QTest::newRow("header_sn")
        << QStringLiteral("SN\tUSB\tConnected")
        << int(HdcListTargetsLineKind::HeaderRow);
    QTest::newRow("header_identifier")
        << QStringLiteral("Identifier   Connection   Status")
        << int(HdcListTargetsLineKind::HeaderRow);
    QTest::newRow("too_few_cols_one")
        << QStringLiteral("only")
        << int(HdcListTargetsLineKind::TooFewColumns);
    QTest::newRow("too_few_cols_two")
        << QStringLiteral("only\ttwo")
        << int(HdcListTargetsLineKind::TooFewColumns);
    QTest::newRow("uart")
        << QStringLiteral("COM1\tUART\tConnected")
        << int(HdcListTargetsLineKind::UartRow);
    QTest::newRow("uart_spaces")
        << QStringLiteral("COM3   UART   Connected")
        << int(HdcListTargetsLineKind::UartRow);
    QTest::newRow("empty_placeholder_bracket")
        << QStringLiteral("[Empty]\tUSB\tConnected")
        << int(HdcListTargetsLineKind::EmptySerialPlaceholder);
    QTest::newRow("device_usb_connected")
        << QStringLiteral("0123456789ABCDEF\tUSB\tConnected")
        << int(HdcListTargetsLineKind::DeviceDataRow);
    QTest::newRow("device_wlan")
        << QStringLiteral("192.168.1.100:5555\tWLAN\t已连接")
        << int(HdcListTargetsLineKind::DeviceDataRow);
    QTest::newRow("device_offline")
        << QStringLiteral("FFFF0000\tUSB\tOffline")
        << int(HdcListTargetsLineKind::DeviceDataRow);
    QTest::newRow("device_space_separated")
        << QStringLiteral("ABC123   USB   Connected")
        << int(HdcListTargetsLineKind::DeviceDataRow);
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
    // 多余的列不影响前三列的解析；注意不能用 "SN" 作序列号，它是 header 关键词
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
    // device 字段不应有意义值
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
        << QStringLiteral("Connected")   << int(HdcTargetConnectionState::ReadyToUse);
    QTest::newRow("connected_cn")
        << QStringLiteral("已连接")       << int(HdcTargetConnectionState::ReadyToUse);
    QTest::newRow("Offline_en")
        << QStringLiteral("Offline")     << int(HdcTargetConnectionState::ConnectedNotReady);
    QTest::newRow("offline_cn1")
        << QStringLiteral("未连接")       << int(HdcTargetConnectionState::ConnectedNotReady);
    QTest::newRow("offline_cn2")
        << QStringLiteral("断开")         << int(HdcTargetConnectionState::ConnectedNotReady);
    QTest::newRow("booting_unknown")
        << QStringLiteral("Booting")     << int(HdcTargetConnectionState::Disconnected);
    QTest::newRow("empty")
        << QString()                      << int(HdcTargetConnectionState::Disconnected);
    QTest::newRow("random_garbage")
        << QStringLiteral("Xyz!@#")      << int(HdcTargetConnectionState::Disconnected);
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
    // 任意未列出字符串 → Disconnected（不崩溃）
    for (const QString &s : {QStringLiteral("AuthFailed"), QStringLiteral("Unauthorized"),
                              QStringLiteral("Recovery"), QStringLiteral("1234")}) {
        QCOMPARE(int(hdcListTargetsStateToConnectionState(s)),
                 int(HdcTargetConnectionState::Disconnected));
    }
}

QObject *createHarmonyHdcTargetsParserTest()
{
    return new HarmonyHdcTargetsParserTest;
}

} // namespace Ohos::Internal

#include "harmonyhdctargetsparser_test.moc"

