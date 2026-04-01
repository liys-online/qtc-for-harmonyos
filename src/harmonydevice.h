#ifndef HARMONYDEVICE_H
#define HARMONYDEVICE_H

#include "harmonydeviceinfo.h"

#include <projectexplorer/devicesupport/idevice.h>

#include <QtTaskTree/QTaskTree>

#include <QUrl>

#include <utils/portlist.h>

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

    QString serialNumber() const;
    int sdkLevel() const;

    QString deviceTypeName() const;
    QString harmonyVersion() const;
    ProjectExplorer::IDeviceWidget *createWidget() override;

    QtTaskTree::ExecutableItem portsGatheringRecipe(
        const QtTaskTree::Storage<Utils::PortsOutputData> &output) const override;
    QUrl toolControlChannel(const ControlChannelHint &) const override;
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
