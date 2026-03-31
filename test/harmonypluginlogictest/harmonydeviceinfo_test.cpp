// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#ifdef WITH_TESTS

#include "harmonydeviceinfo_test.h"
#include "harmonydeviceinfo.h"

#include <projectexplorer/devicesupport/idevice.h>
#include <QTest>

using namespace ProjectExplorer;

namespace Ohos::Internal {

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
    QCOMPARE(makeDevice(QStringLiteral("SN1")), makeDevice(QStringLiteral("SN1")));
}

void HarmonyDeviceInfoTest::equality_differentSerial_notEqual()
{
    QVERIFY(makeDevice(QStringLiteral("SN1")) != makeDevice(QStringLiteral("SN2")));
}

void HarmonyDeviceInfoTest::equality_differentAvd_notEqual()
{
    QVERIFY(makeAvd(QStringLiteral("avd1")) != makeAvd(QStringLiteral("avd2")));
}

void HarmonyDeviceInfoTest::equality_differentSdk_notEqual()
{
    QVERIFY(makeDevice(QStringLiteral("SN"), IDevice::Hardware, 9)
            != makeDevice(QStringLiteral("SN"), IDevice::Hardware, 10));
}

void HarmonyDeviceInfoTest::equality_differentType_notEqual()
{
    QVERIFY(makeDevice(QStringLiteral("SN"), IDevice::Hardware)
            != makeDevice(QStringLiteral("SN"), IDevice::Emulator));
}

void HarmonyDeviceInfoTest::equality_differentState_notEqual()
{
    QVERIFY(makeDevice(QStringLiteral("SN"), IDevice::Hardware, 10, IDevice::DeviceReadyToUse)
            != makeDevice(QStringLiteral("SN"), IDevice::Hardware, 10, IDevice::DeviceDisconnected));
}

void HarmonyDeviceInfoTest::lessThan_questionMarkSerialSortsLast()
{
    const HarmonyDeviceInfo normal = makeDevice(QStringLiteral("ABCDEF"));
    const HarmonyDeviceInfo qmark  = makeDevice(QStringLiteral("????"));
    QVERIFY(normal < qmark);
    QVERIFY(!(qmark < normal));
}

void HarmonyDeviceInfoTest::lessThan_hardwareSortsBeforeEmulator()
{
    const HarmonyDeviceInfo hw  = makeDevice(QStringLiteral("SN1"), IDevice::Hardware);
    const HarmonyDeviceInfo emu = makeDevice(QStringLiteral("SN2"), IDevice::Emulator);
    QVERIFY(hw < emu);
    QVERIFY(!(emu < hw));
}

void HarmonyDeviceInfoTest::lessThan_lowerSdkFirst()
{
    QVERIFY(makeDevice(QStringLiteral("SN"), IDevice::Hardware, 9)
            < makeDevice(QStringLiteral("SN"), IDevice::Hardware, 10));
}

void HarmonyDeviceInfoTest::lessThan_avdNameAlphabetical()
{
    QVERIFY(makeAvd(QStringLiteral("AVD_A")) < makeAvd(QStringLiteral("AVD_B")));
}

void HarmonyDeviceInfoTest::lessThan_serialNumberAlphabetical()
{
    QVERIFY(makeDevice(QStringLiteral("AAA")) < makeDevice(QStringLiteral("BBB")));
}

void HarmonyDeviceInfoTest::lessThan_transitivity()
{
    const auto a = makeDevice(QStringLiteral("SN"), IDevice::Hardware, 8);
    const auto b = makeDevice(QStringLiteral("SN"), IDevice::Hardware, 9);
    const auto c = makeDevice(QStringLiteral("SN"), IDevice::Hardware, 10);
    QVERIFY(a < b);
    QVERIFY(b < c);
    QVERIFY(a < c);
}

void HarmonyDeviceInfoTest::debugStream_doesNotCrash()
{
    HarmonyDeviceInfo d;
    d.serialNumber = QStringLiteral("SERIAL_TEST");
    d.avdName      = QStringLiteral("TestAVD");
    d.sdk          = 12;
    d.type         = IDevice::Emulator;
    d.state        = IDevice::DeviceReadyToUse;
    QString out;
    QDebug dbg(&out);
    dbg << d;
    QVERIFY(!out.isEmpty());
    QVERIFY(out.contains(QStringLiteral("SERIAL_TEST")));
}

} // namespace Ohos::Internal

#include "harmonydeviceinfo_test.moc"

#endif // WITH_TESTS
