#include "harmonydevice.h"
#include "harmonydevicemanager.h"
#include "harmonylogcategories.h"
#include "harmonyutils.h"
#include "ohosconstants.h"
#include "ohostr.h"

#include <coreplugin/icore.h>

#include <projectexplorer/devicesupport/idevicewidget.h>
#include <projectexplorer/devicesupport/idevicefactory.h>

#include <QtTaskTree/QTaskTree>

#include <utils/commandline.h>
#include <utils/filepath.h>
#include <utils/portlist.h>
#include <utils/url.h>

#include <QMessageBox>
#include <QPushButton>
#include <QFormLayout>
#include <QTimer>

using namespace ProjectExplorer;
using namespace QtTaskTree;
using namespace Utils;

namespace Ohos::Internal {

class HarmonyDeviceWidget : public IDeviceWidget
{
public:
    explicit HarmonyDeviceWidget(const IDevice::Ptr &device);

    void updateDeviceFromUi() final { return; }
    static QString dialogTitle();
    static bool messageDialog(const QString &msg, QMessageBox::Icon icon);
    static bool criticalDialog(const QString &error);
    static bool infoDialog(const QString &msg);
    static bool questionDialog(const QString &question);
};

HarmonyDeviceWidget::HarmonyDeviceWidget(const IDevice::Ptr &device)
    : IDeviceWidget(device)
{
    const auto dev = std::static_pointer_cast<HarmonyDevice>(device);
    const auto formLayout = new QFormLayout(this);
    formLayout->setFormAlignment(Qt::AlignLeft);
    formLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(formLayout);
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    formLayout->addRow(Tr::tr("Device name:"), new QLabel(dev->displayName()));
    formLayout->addRow(Tr::tr("Device type:"), new QLabel(dev->deviceTypeName()));

    QPointer<QLabel> serialNumberLabel = new QLabel;
    formLayout->addRow(Tr::tr("Serial number:"), serialNumberLabel);

    const QString abis = dev->supportedAbis().join(", ");
    formLayout->addRow(Tr::tr("CPU architecture:"), new QLabel(abis));

    const auto osString = QString("%1 (SDK %2)").arg(dev->harmonyVersion()).arg(dev->sdkLevel());
    formLayout->addRow(Tr::tr("OS version:"), new QLabel(osString));

    if (dev->machineType() == IDevice::Hardware) {
        const QString authorizedStr = dev->deviceState() == IDevice::DeviceReadyToUse
                                          ? Tr::tr("Yes")
                                          : Tr::tr("No");
        formLayout->addRow(Tr::tr("Authorized:"), new QLabel(authorizedStr));
    }
    // const QString openGlStatus = dev->openGLStatus();
    // formLayout->addRow(Tr::tr("SD card size:"), new QLabel(dev->sdcardSize()));
    // formLayout->addRow(Tr::tr("OpenGL status:"), new QLabel(openGlStatus));

    // See QTCREATORBUG-31912 why this needs to be delayed.
    QTimer::singleShot(0, serialNumberLabel, [serialNumberLabel, dev] {
        const QString serialNumber = dev->serialNumber(); // This executes a blocking process.
        const QString printableSerialNumber = serialNumber.isEmpty() ? Tr::tr("Unknown")
                                                                     : serialNumber;
        if (serialNumberLabel)
            serialNumberLabel->setText(printableSerialNumber);
    });

    installMarkSettingsDirtyTriggerRecursively(this);
}

QString HarmonyDeviceWidget::dialogTitle()
{
    return Tr::tr("Harmony Device Manager");
}

bool HarmonyDeviceWidget::messageDialog(const QString &msg, QMessageBox::Icon icon)
{
    qCDebug(harmonyDeviceLog) << msg;
    QMessageBox box(Core::ICore::dialogParent());
    box.QDialog::setWindowTitle(dialogTitle());
    box.setText(msg);
    box.setIcon(icon);
    box.setWindowFlag(Qt::WindowTitleHint);
    return box.exec();
}

bool HarmonyDeviceWidget::criticalDialog(const QString &error)
{
    return messageDialog(error, QMessageBox::Critical);
}

bool HarmonyDeviceWidget::infoDialog(const QString &msg)
{
    return messageDialog(msg, QMessageBox::Information);
}

bool HarmonyDeviceWidget::questionDialog(const QString &question)
{
    QMessageBox box(Core::ICore::dialogParent());
    box.QDialog::setWindowTitle(dialogTitle());
    box.setText(question);
    box.setIcon(QMessageBox::Question);
    QPushButton *YesButton = box.addButton(QMessageBox::Yes);
    box.addButton(QMessageBox::No);
    box.setWindowFlag(Qt::WindowTitleHint);
    box.exec();

    if (box.clickedButton() == YesButton)
        return true;
    return false;
}

static void updateDeviceState(const IDevice::ConstPtr &device)
{
    /* ** hdc 无单设备查询 API：刷新即全量 list targets（与计划 P1-06 一致）。 */
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
    /*
    ** 若列表为空，则无法做出有效决策，说明某处出现了问题，但不应因此中断部署。
    */
    QTC_ASSERT(!abis.isEmpty(), return true);

    const QStringList ourAbis = supportedAbis();
    QTC_ASSERT(!ourAbis.isEmpty(), return false);

    for (const QString &abi : abis)
        if (ourAbis.contains(abi))
            return true; // it's enough if only one abi match is found

    /*
    ** 若未找到精确匹配，考虑 ABI 向后兼容性：
    ** arm64 通常可运行 {arm, armv7}，x86 可支持 {arm, armv7}，64 位设备可支持 32 位变体。
    */
    /* ** arm64 和 x86 可运行 armv7 和 arm */
    if ((abis.contains(Constants::HARMONY_ABI_ARMEABI) || abis.contains(Constants::HARMONY_ABI_ARMEABI_V7A))
        && (ourAbis.first() == Constants::HARMONY_ABI_ARM64_V8A
            || ourAbis.first() == Constants::HARMONY_ABI_X86))
        return true;
    /* ** x64 可运行 x86 */
    if (ourAbis.first() == Constants::HARMONY_ABI_X86_64 && abis.contains(Constants::HARMONY_ABI_X86))
        return true;

    return false;
}

bool HarmonyDevice::canHandleDeployments() const
{
    /* ** 若设备为硬件且已断开连接，则无法启动（与模拟器不同）。 */
    if (machineType() == Hardware && deviceState() == DeviceDisconnected)
        return false;
    return true;
}

QString HarmonyDevice::serialNumber() const
{
    const QString serialNumber = extraData(Constants::HarmonySerialNumber).toString();
    if (!serialNumber.isEmpty() || machineType() == Hardware)
        return serialNumber;
    return {};
}

int HarmonyDevice::sdkLevel() const
{
    return extraData(Constants::HarmonySdk).toInt();
}

QString HarmonyDevice::deviceTypeName() const
{
    if (machineType() == Emulator)
        return Tr::tr("Emulator device");
    return Tr::tr("Physical device");
}

QString HarmonyDevice::harmonyVersion() const
{
    return harmonyNameForApiLevel(sdkLevel());
}

IDevicePtr HarmonyDevice::create()
{
    return std::make_shared<HarmonyDevice>();
}

IDeviceWidget *HarmonyDevice::createWidget()
{
    return new HarmonyDeviceWidget(shared_from_this()); // NOSONAR (cpp:S5025) - Qt widget ownership transferred to caller via Qt object tree
}


/* ** Factory */

class HarmonyDeviceFactory final : public IDeviceFactory
{
public:
    HarmonyDeviceFactory()
        : IDeviceFactory(Constants::HARMONY_DEVICE_TYPE)
    {
        setDisplayName(Tr::tr("HarmonyOS Device"));
        /* ** 不引用 Android 插件资源；使用 ProjectExplorer 内置通用设备图标（本插件已依赖 PE）。 */
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
    Q_UNUSED(guard) // NOSONAR (cpp:S3623) - Qt macro requires no trailing semicolon rules don't apply here
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

