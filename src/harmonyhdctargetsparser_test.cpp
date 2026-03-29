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
    void testSplit_tabSeparated();
    void testSplit_multiSpace();
    void testSplit_empty();

    void testHeaderDetection();

    void testParseLineKind_data();
    void testParseLineKind();

    void testParseDeviceRow_preservesColumns();

    void testStateToConnectionState_data();
    void testStateToConnectionState();
};

void HarmonyHdcTargetsParserTest::testSplit_tabSeparated()
{
    const QStringList p = splitHdcListTargetsLine(QStringLiteral("SN\tUSB\tConnected\r"));
    QCOMPARE(p.size(), 3);
    QCOMPARE(p.at(0), QStringLiteral("SN"));
    QCOMPARE(p.at(1), QStringLiteral("USB"));
    QCOMPARE(p.at(2), QStringLiteral("Connected"));
}

void HarmonyHdcTargetsParserTest::testSplit_multiSpace()
{
    const QStringList p = splitHdcListTargetsLine(QStringLiteral("  dev123   USB   Offline  "));
    QCOMPARE(p.size(), 3);
    QCOMPARE(p.at(0), QStringLiteral("dev123"));
    QCOMPARE(p.at(1), QStringLiteral("USB"));
    QCOMPARE(p.at(2), QStringLiteral("Offline"));
}

void HarmonyHdcTargetsParserTest::testSplit_empty()
{
    QVERIFY(splitHdcListTargetsLine(QString()).isEmpty());
    QVERIFY(splitHdcListTargetsLine(QStringLiteral("   \t  ")).isEmpty());
}

void HarmonyHdcTargetsParserTest::testHeaderDetection()
{
    QVERIFY(hdcListTargetsLineLooksLikeHeader({QStringLiteral("SN"), QStringLiteral("x"), QStringLiteral("y")}));
    QVERIFY(
        hdcListTargetsLineLooksLikeHeader({QStringLiteral("Identifier"), QStringLiteral("a"), QStringLiteral("b")}));
    QVERIFY(!hdcListTargetsLineLooksLikeHeader(
        {QStringLiteral("0123456789ABCDEF"), QStringLiteral("USB"), QStringLiteral("Connected")}));
}

void HarmonyHdcTargetsParserTest::testParseLineKind_data()
{
    QTest::addColumn<QString>("line");
    QTest::addColumn<int>("expectedKind");

    QTest::newRow("empty") << QString() << int(HdcListTargetsLineKind::EmptyOrWhitespace);
    QTest::newRow("whitespace") << QStringLiteral("  \t  ") << int(HdcListTargetsLineKind::EmptyOrWhitespace);
    QTest::newRow("header_sn") << QStringLiteral("SN\tUSB\tConnected")
                               << int(HdcListTargetsLineKind::HeaderRow);
    QTest::newRow("too_few") << QStringLiteral("only\tone") << int(HdcListTargetsLineKind::TooFewColumns);
    QTest::newRow("uart") << QStringLiteral("COM1\tUART\tConnected") << int(HdcListTargetsLineKind::UartRow);
    QTest::newRow("empty_placeholder")
        << QStringLiteral("[Empty]\tUSB\tConnected") << int(HdcListTargetsLineKind::EmptySerialPlaceholder);
    QTest::newRow("device_ok")
        << QStringLiteral("0123456789ABCDEF\tUSB\tConnected") << int(HdcListTargetsLineKind::DeviceDataRow);
}

void HarmonyHdcTargetsParserTest::testParseLineKind()
{
    QFETCH(QString, line);
    QFETCH(int, expectedKind);

    const HdcListTargetsParseResult r = parseHdcListTargetsLine(line);
    QCOMPARE(int(r.kind), expectedKind);
}

void HarmonyHdcTargetsParserTest::testParseDeviceRow_preservesColumns()
{
    /* ** Mock：常见 hdc 文本形态（制表符 / 多空格），不依赖本机 hdc。 */
    const QString tabLine = QStringLiteral("192.168.1.2:5555\tWLAN\t已连接\n");
    const HdcListTargetsParseResult a = parseHdcListTargetsLine(tabLine);
    QCOMPARE(int(a.kind), int(HdcListTargetsLineKind::DeviceDataRow));
    QCOMPARE(a.device.serial, QStringLiteral("192.168.1.2:5555"));
    QCOMPARE(a.device.connectionType, QStringLiteral("WLAN"));
    QCOMPARE(a.device.stateRaw, QStringLiteral("已连接"));

    const QString spaceLine = QStringLiteral("EMULATOR_1    USB     Offline");
    const HdcListTargetsParseResult b = parseHdcListTargetsLine(spaceLine);
    QCOMPARE(int(b.kind), int(HdcListTargetsLineKind::DeviceDataRow));
    QCOMPARE(b.device.serial, QStringLiteral("EMULATOR_1"));
    QCOMPARE(b.device.connectionType, QStringLiteral("USB"));
    QCOMPARE(b.device.stateRaw, QStringLiteral("Offline"));
}

void HarmonyHdcTargetsParserTest::testStateToConnectionState_data()
{
    QTest::addColumn<QString>("state");
    QTest::addColumn<int>("expected");

    QTest::newRow("connected_en") << QStringLiteral("Connected")
                                  << int(HdcTargetConnectionState::ReadyToUse);
    QTest::newRow("connected_cn") << QStringLiteral("已连接") << int(HdcTargetConnectionState::ReadyToUse);
    QTest::newRow("offline_en") << QStringLiteral("Offline")
                                << int(HdcTargetConnectionState::ConnectedNotReady);
    QTest::newRow("offline_cn1") << QStringLiteral("未连接") << int(HdcTargetConnectionState::ConnectedNotReady);
    QTest::newRow("offline_cn2") << QStringLiteral("断开") << int(HdcTargetConnectionState::ConnectedNotReady);
    QTest::newRow("unknown") << QStringLiteral("Booting") << int(HdcTargetConnectionState::Disconnected);
    QTest::newRow("spaces") << QStringLiteral("  Connected  ") << int(HdcTargetConnectionState::ReadyToUse);
}

void HarmonyHdcTargetsParserTest::testStateToConnectionState()
{
    QFETCH(QString, state);
    QFETCH(int, expected);

    QCOMPARE(int(hdcListTargetsStateToConnectionState(state)), expected);
}

QObject *createHarmonyHdcTargetsParserTest()
{
    return new HarmonyHdcTargetsParserTest;
}

} // namespace Ohos::Internal

#include "harmonyhdctargetsparser_test.moc"
