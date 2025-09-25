#include "harmonydeviceinfo.h"

using namespace ProjectExplorer;

namespace Ohos::Internal {

HarmonyDeviceInfo::HarmonyDeviceInfo() {}

bool HarmonyDeviceInfo::isValid() const
{
    return !serialNumber.isEmpty() || !avdName.isEmpty();
}

bool operator<(const HarmonyDeviceInfo &lhs, const HarmonyDeviceInfo &rhs)
{
    if (lhs.serialNumber.contains("????") != rhs.serialNumber.contains("????"))
        return !lhs.serialNumber.contains("????");
    if (lhs.type != rhs.type)
        return lhs.type == IDevice::Hardware;
    if (lhs.sdk != rhs.sdk)
        return lhs.sdk < rhs.sdk;
    if (lhs.avdName != rhs.avdName)
        return lhs.avdName < rhs.avdName;

    return lhs.serialNumber < rhs.serialNumber;
}

QDebug &operator<<(QDebug &stream, const HarmonyDeviceInfo &device)
{
    stream.nospace()
    << "Type:" << (device.type == IDevice::Emulator ? "Emulator" : "Device")
    << ", ABI:" << device.cpuAbi << ", Serial:" << device.serialNumber
    << ", Name:" << device.avdName << ", API:" << device.sdk
    << ", Authorised:" << (device.state == IDevice::DeviceReadyToUse);
    return stream;
}

}
