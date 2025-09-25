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

HarmonyDeviceInfo HarmonyDevice::harmonyDeviceInfoFromDevice(const ConstPtr &device)
{
    QTC_ASSERT(device, return {});
    HarmonyDeviceInfo info;
    info.state = device->deviceState();
    info.serialNumber = device->extraData(Constants::HarmonySerialNumber).toString();
    info.cpuAbi = device->extraData(Constants::HarmonyCpuAbi).toStringList();
    info.sdk = device->extraData(Constants::HarmonySdk).toInt();
    info.type = device->machineType();
    return info;
}

QStringList HarmonyDevice::supportedAbis() const
{
    return extraData(Constants::HarmonyCpuAbi).toStringList();
}

bool HarmonyDevice::canSupportAbis(const QStringList &abis) const
{
    // If the list is empty, no valid decision can be made, this means something is wrong
    // somewhere, but let's not stop deployment.
    QTC_ASSERT(!abis.isEmpty(), return true);

    const QStringList ourAbis = supportedAbis();
    QTC_ASSERT(!ourAbis.isEmpty(), return false);

    for (const QString &abi : abis)
        if (ourAbis.contains(abi))
            return true; // it's enough if only one abi match is found

    // If no exact match is found, let's take ABI backward compatibility into account
    // arm64 usually can run {arm, armv7}, x86 can support {arm, armv7}, and 64-bit devices
    // can support their 32-bit variants.
    const bool isTheirsArm = abis.contains(Constants::HARMONY_ABI_ARMEABI)
                             || abis.contains(Constants::HARMONY_ABI_ARMEABI_V7A);
    // The primary ABI at the first index
    const bool oursSupportsArm = ourAbis.first() == Constants::HARMONY_ABI_ARM64_V8A
                                 || ourAbis.first() == Constants::HARMONY_ABI_X86;
    // arm64 and x86 can run armv7 and arm
    if (isTheirsArm && oursSupportsArm)
        return true;
    // x64 can run x86
    if (ourAbis.first() == Constants::HARMONY_ABI_X86_64 && abis.contains(Constants::HARMONY_ABI_X86))
        return true;

    return false;
}

bool HarmonyDevice::canHandleDeployments() const
{
    // If hardware and disconned, it wouldn't be possilbe to start it, unlike an emulator
    if (machineType() == Hardware && deviceState() == DeviceDisconnected)
        return false;
    return true;
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

