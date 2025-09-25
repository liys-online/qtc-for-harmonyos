#ifndef HARMONYDEVICEINFO_H
#define HARMONYDEVICEINFO_H

#include <projectexplorer/devicesupport/idevice.h>

#include <QString>
#include <QStringList>
#include <QMetaType>

namespace Ohos::Internal {

class HarmonyDeviceInfo
{
public:
    HarmonyDeviceInfo();
    bool isValid() const;
    QString serialNumber;
    QString avdName;
    QStringList cpuAbi;
    int sdk = -1;
    ProjectExplorer::IDevice::DeviceState state = ProjectExplorer::IDevice::DeviceDisconnected;
    ProjectExplorer::IDevice::MachineType type = ProjectExplorer::IDevice::Emulator;
    Utils::FilePath avdPath;

private:
    friend bool operator<(const HarmonyDeviceInfo &lhs, const HarmonyDeviceInfo &rhs);
    friend bool operator==(const HarmonyDeviceInfo &lhs, const HarmonyDeviceInfo &rhs) = default;
    friend bool operator!=(const HarmonyDeviceInfo &lhs, const HarmonyDeviceInfo &rhs) = default;
    friend QDebug &operator<<(QDebug &stream, const HarmonyDeviceInfo &device);
};

using HarmonyDeviceInfoList = QList<HarmonyDeviceInfo>;

} // namespace Ohos::Internal

Q_DECLARE_METATYPE(Ohos::Internal::HarmonyDeviceInfo)

#endif // HARMONYDEVICEINFO_H
