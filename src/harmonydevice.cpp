#include "harmonydevice.h"
#include "harmonydevicemanager.h"
#include "harmonylogcategories.h"
#include "ohosconstants.h"
#include "ohostr.h"

#include <projectexplorer/devicesupport/idevicewidget.h>
#include <projectexplorer/devicesupport/idevicefactory.h>

#include <QtTaskTree/QTaskTree>

#include <utils/commandline.h>
#include <utils/filepath.h>
#include <utils/portlist.h>
#include <utils/url.h>

using namespace ProjectExplorer;
using namespace QtTaskTree;
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
    // hdc 无单设备查询 API：刷新即全量 list targets（与计划 P1-06 一致）。
    qCDebug(harmonyDeviceLog) << "Refresh requested for device" << (device ? device->id().toString() : QString());
    instance()->queryDevice();
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
    // Local TCP ports for RunControl debug/QML channel allocation. Default Harmony C++ debug uses §7.2
    // (abstract socket), not fport; ports remain for legacy env / other workers or future use.
    setFreePorts(PortList::fromString(QStringLiteral("45450-45550")));
    addDeviceAction(DeviceAction(
        Tr::tr("Refresh"),
        [](const IDevice::Ptr &device) { updateDeviceState(device); }));
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
    // If hardware and disconnected, it would not be possible to start it, unlike an emulator.
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
        // 不引用 Android 插件资源；使用 ProjectExplorer 内置通用设备图标（本插件已依赖 PE）。
        setCombinedIcon(":/projectexplorer/images/desktopdevice.png",
                        ":/projectexplorer/images/desktopdevice@2x.png");
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
    Q_UNUSED(guard);
}

void setupDevicesWatcher()
{
    instance()->setupDevicesWatcher();
}

ExecutableItem HarmonyDevice::portsGatheringRecipe(const Storage<PortsOutputData> &output) const
{
    const Storage<PortsInputData> input;
    const auto onSetup = [this, input] {
        const CommandLine cmd{FilePath::fromUserInput(QStringLiteral("netstat")),
                              QStringList{QStringLiteral("-a"), QStringLiteral("-n")}};
        *input = {freePorts(), cmd};
    };
    return Group{input, onGroupSetup(onSetup), portsFromProcessRecipe(input, output)};
}

QUrl HarmonyDevice::toolControlChannel(const ControlChannelHint &) const
{
    QUrl url;
    url.setScheme(Utils::urlTcpScheme());
    url.setHost(QStringLiteral("localhost"));
    return url;
}

}

