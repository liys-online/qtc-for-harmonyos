#ifndef HARMONYDEVICE_H
#define HARMONYDEVICE_H

#include <projectexplorer/devicesupport/idevice.h>
namespace Ohos::Internal {
class HarmonyDevice : public ProjectExplorer::IDevice
{
public:
    HarmonyDevice();

    static IDevice::Ptr create();

    // IDevice interface
public:
    ProjectExplorer::IDeviceWidget *createWidget() override;
};
void setupDevicesWatcher();

void setupHarmonyDevice();
void setupHarmonyDeviceManager(QObject *guard);
} // namespace Ohos::Internal

#endif // HARMONYDEVICE_H
