#ifndef HARMONYDEVICE_H
#define HARMONYDEVICE_H

#include "harmonydeviceinfo.h"

#include <projectexplorer/devicesupport/idevice.h>
namespace Ohos::Internal {
class HarmonyDevice : public ProjectExplorer::IDevice
{
public:
    HarmonyDevice();

    static ProjectExplorer::IDevicePtr create();
    static HarmonyDeviceInfo harmonyDeviceInfoFromDevice(const ConstPtr &device);

    QStringList supportedAbis() const;
    bool canSupportAbis(const QStringList &abis) const;

    bool canHandleDeployments() const;
    ProjectExplorer::IDeviceWidget *createWidget() override;
};
/**
 * @brief setupDevicesWatcher
 * 设置设备监视器
 */
void setupDevicesWatcher();

void setupHarmonyDevice();
void setupHarmonyDeviceManager(const QObject *guard);
} // namespace Ohos::Internal

#endif // HARMONYDEVICE_H
