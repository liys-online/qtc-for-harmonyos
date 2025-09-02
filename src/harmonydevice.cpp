#include "harmonydevice.h"
#include "harmonydevicemanager.h"
#include "ohosconstants.h"
#include "ohostr.h"
#include <projectexplorer/devicesupport/idevicewidget.h>
#include <projectexplorer/devicesupport/idevicefactory.h>
using namespace ProjectExplorer;
using namespace Utils;
namespace Ohos::Internal {

class HarmonyDeviceWidget : public IDeviceWidget
{
public:
    explicit HarmonyDeviceWidget(const IDevice::Ptr &device);

    void updateDeviceFromUi() final {}
};

HarmonyDeviceWidget::HarmonyDeviceWidget(const IDevice::Ptr &device)
    : IDeviceWidget(device)
{

}

static void updateDeviceState(const IDevice::ConstPtr &device)
{

}
HarmonyDevice::HarmonyDevice()
{
    setupId(IDevice::AutoDetected, Constants::HARMONY_DEVICE_ID);
    setType(Constants::HARMONY_DEVICE_TYPE);
    setDefaultDisplayName(Tr::tr("Run on HarmonyOS"));
    setDisplayType(Tr::tr("Harmony"));
    setMachineType(IDevice::Hardware);
    setOsType(OsType::OsTypeOtherUnix);
    setDeviceState(DeviceDisconnected);
    DeviceAction refreshAction;
    refreshAction.display = Tr::tr("Refresh");
    refreshAction.execute = [](const IDevice::Ptr &device) {
        updateDeviceState(device);
    };
    addDeviceAction(refreshAction);
}

IDevicePtr HarmonyDevice::create()
{
    return IDevice::Ptr(new HarmonyDevice);
}

IDeviceWidget *HarmonyDevice::createWidget()
{
    return new HarmonyDeviceWidget(shared_from_this());
}


// Factory

class HarmonyDeviceFactory final : public IDeviceFactory
{
public:
    HarmonyDeviceFactory()
        : IDeviceFactory(Constants::HARMONY_DEVICE_TYPE)
    {
        setDisplayName(Tr::tr("HarmonyOS Device"));
        setCombinedIcon(":/android/images/androiddevicesmall.png",
                        ":/android/images/androiddevice.png");
        setConstructionFunction(&HarmonyDevice::create);

        setCreator(HarmonyDevice::create);
    }
};

void setupHarmonyDevice()
{
    static HarmonyDeviceFactory theHarmonyDeviceFactory;
}

void setupHarmonyDeviceManager(const QObject *guard)
{

}

void setupDevicesWatcher()
{
    instance()->setupDevicesWatcher();
}

}

