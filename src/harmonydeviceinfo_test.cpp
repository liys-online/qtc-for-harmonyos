// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "harmonydeviceinfo_test.h"
#include "harmonydeviceinfo.h"

#include <projectexplorer/devicesupport/idevice.h>

#include <QDebug>
#include <QTest>

using namespace ProjectExplorer;

namespace Ohos::Internal {

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

// ── helpers ─────────────────────────────────────────────────────────────────

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

// ── isValid ──────────────────────────────────────────────────────────────────

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

// ── operator== ──────────────────────────────────────────────────────────────

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

// ── operator< ───────────────────────────────────────────────────────────────

void HarmonyDeviceInfoTest::lessThan_questionMarkSerialSortsLast()
{
    // "????" 序列号排在正常设备之后
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
    // a < b < c → a < c
    HarmonyDeviceInfo a = makeDevice(QStringLiteral("SN"), IDevice::Hardware, 8);
    HarmonyDeviceInfo b = makeDevice(QStringLiteral("SN"), IDevice::Hardware, 9);
    HarmonyDeviceInfo c = makeDevice(QStringLiteral("SN"), IDevice::Hardware, 10);

    QVERIFY(a < b);
    QVERIFY(b < c);
    QVERIFY(a < c);
}

// ── operator<< ───────────────────────────────────────────────────────────────

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

QObject *createHarmonyDeviceInfoTest()
{
    return new HarmonyDeviceInfoTest;
}

} // namespace Ohos::Internal

#include "harmonydeviceinfo_test.moc"
