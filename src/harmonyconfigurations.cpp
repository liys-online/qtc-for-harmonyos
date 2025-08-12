#include "harmonyconfigurations.h"
#include <coreplugin/icore.h>
#include "ohosconstants.h"
#include <qjsonobject.h>
#include <utils/persistentsettings.h>

#include <projectexplorer/toolchainmanager.h>
#include <projectexplorer/devicesupport/devicekitaspects.h>
#include <projectexplorer/devicesupport/devicemanager.h>
#include <projectexplorer/kitmanager.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/toolchainkitaspect.h>
#include <cmakeprojectmanager/cmakekitaspect.h>
#include <projectexplorer/abi.h>

#include <qtsupport/qtversionfactory.h>
#include <qtsupport/qtversionmanager.h>
#include <qtsupport/qtkitaspect.h>

#include "harmonytoolchain.h"
#include "harmonyqtversion.h"
#include "harmonydevice.h"
#include <QJsonDocument>
#include <utils/qtcprocess.h>

#include <coreplugin/messagemanager.h>
using namespace Utils;

using namespace ProjectExplorer;
using namespace QtSupport;
namespace Ohos::Internal {
using namespace Constants::Parameter;
const Key changeTimeStamp("ChangeTimeStamp");
const QLatin1String ArmToolsDisplayName("armeabi-v7a");
const QLatin1String X86ToolsDisplayName("i686");
const QLatin1String AArch64ToolsDisplayName("arm64-v8a");
const QLatin1String X86_64ToolsDisplayName("x86_64");
const QLatin1String Unknown("unknown");
static FilePath sdkSettingsFileName()
{
    return Core::ICore::installerResourcePath("harmony.xml");
}
/**
 * @brief getDeviceProperty
 * 获取设备属性
 * @param device 设备
 * @param property 属性
 * @return QString 属性值
 */
static QString getDeviceProperty(const QString &device, const QString &property)
{
    Process hdcParam;
    const CommandLine command{HarmonyConfig::hdcToolPath(), {"-t", device, "shell", "param", "get", property}};
    qDebug() << "getDeviceProperty" << command.toUserOutput();
    hdcParam.setCommand(command);
    hdcParam.runBlocking();
    if (hdcParam.result() == ProcessResult::FinishedWithSuccess)
        return hdcParam.allOutput();
    return {};
}

namespace HarmonyConfig {
struct HarmonyConfigData{
    void load(const QtcSettings &settings);
    void save(QtcSettings &settings) const;
    /**
     * @brief m_makeLocation
     * make路径
     */
    FilePath m_makeLocation;
    /**
     * @brief m_sdkLocation
     * 默认SDK路径
     */
    FilePath m_defaultSdkLocation;
    /**
     * @brief m_devecoStudioPath
     * Deveco Studio路径
     */
    FilePath m_devecoStudioPath;
    /**
     * @brief m_qmakeList
     * qmake路径列表
     */
    QStringList m_qmakeList;
    /**
     * @brief m_sdkList
     * SDK路径列表
     */
    QStringList m_sdkList;
};
static HarmonyConfigData &config()
{
    static HarmonyConfigData theHarmonyConfig;
    return theHarmonyConfig;
}

void HarmonyConfigData::load(const QtcSettings &settings)
{
    m_makeLocation = FilePath::fromString(settings.value(Constants::MakeLocationKey).toString());
    m_devecoStudioPath = FilePath::fromString(settings.value(Constants::DevecoStudioLocationKey).toString());
    m_qmakeList = settings.value(Constants::QmakeLocationKey).toStringList();
    m_sdkList = settings.value(Constants::SDKLocationsKey).toStringList();
    m_defaultSdkLocation = FilePath::fromString(settings.value(Constants::DefaultSDKLocationKey).toString());
    // PersistentSettingsReader reader;

    // if (reader.load(sdkSettingsFileName())
    //     && settings.value(changeTimeStamp).toInt() != sdkSettingsFileName().lastModified().toMSecsSinceEpoch() / 1000)
    // {
    //     m_makeLocation = FilePath::fromString(reader.restoreValue(Constants::MakeLocationKey).toString());
    //     m_sdkLocation = FilePath::fromString(reader.restoreValue(Constants::SDKLocationKey).toString());
    //     m_devecoStudioPath = FilePath::fromString(reader.restoreValue(Constants::DevecoStudioLocationKey).toString());
    // }
    m_qmakeList.removeAll("");
    m_sdkList.removeAll("");


}

void HarmonyConfigData::save(QtcSettings &settings) const
{
    const FilePath sdkSettingsFile = sdkSettingsFileName();
    if (sdkSettingsFile.exists())
        settings.setValue(changeTimeStamp, sdkSettingsFile.lastModified().toMSecsSinceEpoch() / 1000);

    settings.setValue(Constants::MakeLocationKey, m_makeLocation.toFSPathString());
    settings.setValue(Constants::DevecoStudioLocationKey, m_devecoStudioPath.toFSPathString());
    settings.setValue(Constants::QmakeLocationKey, m_qmakeList);
    settings.setValue(Constants::SDKLocationsKey, m_sdkList);
    settings.setValue(Constants::DefaultSDKLocationKey, m_defaultSdkLocation.toFSPathString());
}

FilePath devecoStudioLocation()
{
    return config().m_devecoStudioPath;
}

void setDevecoStudioLocation(const Utils::FilePath &devecoStudioLocation)
{
    config().m_devecoStudioPath = devecoStudioLocation;
}

FilePath makeLocation()
{
    return config().m_makeLocation;
}

void setMakeLocation(const Utils::FilePath &makeLocation)
{
    config().m_makeLocation = makeLocation;
}

QList<FilePath> ndkLocations()
{
    QList<FilePath> ndkLocations;
    for (const QString &path : qAsConst(config().m_sdkList))
    {
        FilePath ndkPath = ndkLocation(FilePath::fromString(path));
        if (ndkPath.isReadableDir())
        {
            ndkLocations <<  ndkPath;
        }
    }
    return ndkLocations;
}

FilePath toolchainPathFromNdk(const Utils::FilePath &ndkLocation)
{
    const FilePath tcPath = ndkLocation / "llvm";
        return tcPath;
}

FilePath clangPathFromNdk(const Utils::FilePath &ndkLocation)
{
    const FilePath path = toolchainPathFromNdk(ndkLocation);
    if (path.isEmpty())
        return {};
    return path.pathAppended("bin/clang").withExecutableSuffix();
}

QLatin1String displayName(const ProjectExplorer::Abi &abi)
{
    switch (abi.architecture()) {
    case Abi::ArmArchitecture:
        if (abi.wordWidth() == 64)
            return AArch64ToolsDisplayName;
        return ArmToolsDisplayName;
    case Abi::X86Architecture:
        if (abi.wordWidth() == 64)
            return X86_64ToolsDisplayName;
        return X86ToolsDisplayName;
    default:
        return Unknown;
    }
}

Abi abi(const QLatin1String &arch)
{
    if (arch == ArmToolsDisplayName)
        return Abi(Abi::ArmArchitecture, Abi::LinuxOS, Abi::GenericFlavor, Abi::ElfFormat, 32);
    if (arch == X86ToolsDisplayName)
        return Abi(Abi::X86Architecture, Abi::LinuxOS, Abi::GenericFlavor, Abi::ElfFormat, 32);
    if (arch == AArch64ToolsDisplayName)
        return Abi(Abi::ArmArchitecture, Abi::LinuxOS, Abi::GenericFlavor, Abi::ElfFormat, 64);
    if (arch == X86_64ToolsDisplayName)
        return Abi(Abi::X86Architecture, Abi::LinuxOS, Abi::GenericFlavor, Abi::ElfFormat, 64);
    return {};
}

QPair<QVersionNumber, QVersionNumber> getVersion(const Utils::FilePath &releaseFile)
{
    // 打开文件
    QPair<QVersionNumber, QVersionNumber> versionPair(QVersionNumber(-1), QVersionNumber(-1));

    QString filePath = releaseFile.path();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file:" << filePath;
        return versionPair;
    }
    QByteArray rawData = file.readAll();
    file.close();
    QJsonDocument doc = QJsonDocument::fromJson(rawData);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "Invalid JSON data!";
        return versionPair;
    }
    QJsonObject rootObj = doc.object();
    QString versionStr = rootObj.value("version").toString();
    QString apiVersionStr = rootObj.value("apiVersion").toString();
    versionPair.first = QVersionNumber::fromString(apiVersionStr);
    versionPair.second = QVersionNumber::fromString(versionStr);
    return versionPair;
}

FilePath hdcToolPath()
{
    return config().m_defaultSdkLocation.pathAppended("default/openharmony/toolchains/hdc").withExecutableSuffix();
}

int getSDKVersion(const QString &device)
{
    const QString tmp = getDeviceProperty(device, Ohos::apiversion);
    return tmp.isEmpty() ? -1 : tmp.trimmed().toInt();
}

QString getProductModel(const QString &device)
{
    const QString tmp = getDeviceProperty(device, Product::model);
    return tmp.isEmpty() ? QString() : tmp.trimmed();
}

QString getDeviceName(const QString &device)
{
    const QString tmp = getDeviceProperty(device, Product::name);
    if (tmp.contains("[Fail][E001005]") || tmp.contains("[Fail]Device not founded or connected")) {
        return QString();
    }
    return tmp.isEmpty() ? QString() : tmp.trimmed();
}

QString getAbis(const QString &device)
{
    const QString tmp = getDeviceProperty(device, Product::Cpu::abilist);
    return tmp.isEmpty() ? QString() : tmp.trimmed();
}

QStringList &getSdkList()
{
    return config().m_sdkList;
}

void addSdk(const QString &sdk)
{
    if (!config().m_sdkList.contains(sdk))
        config().m_sdkList.append(sdk);
}

void removeSdkList(const QString &sdk)
{
    config().m_sdkList.removeAll(sdk);
}

bool isValidSdk(const QString &sdkLocation)
{
    FilePath ndkPath = ndkLocation(FilePath::fromString(sdkLocation));
    if (ndkPath.isReadableDir())
    {
        if (releaseFile(ndkPath).isReadableFile())
        {
            return true;
        }
        return false;
    }
    return false;
}

FilePath releaseFile(const Utils::FilePath &ndkLocation)
{
    return ndkLocation / "oh-uni-package.json";
}

FilePath ndkLocation(const Utils::FilePath &sdkLocation)
{
    const QVector<FilePath> candidatePaths = {
        sdkLocation / "native",
        sdkLocation / "default" / "openharmony" / "native"
    };

    for (const FilePath &path : candidatePaths) {
        if (releaseFile(path).isReadableFile())
            return path;
    }

    return candidatePaths.first();
}

FilePath defaultSdk()
{
    return config().m_defaultSdkLocation;
}

void setdefaultSdk(const Utils::FilePath &sdkLocation)
{
    config().m_defaultSdkLocation = sdkLocation;
}

void addQmake(const QString &qmake)
{
    if (!config().m_qmakeList.contains(qmake))
    {
        config().m_qmakeList.append(qmake);
    }
}

void removeQmake(const QString &qmake)
{
    config().m_qmakeList.removeAll(qmake);
}

QStringList &getQmakeList()
{
    return config().m_qmakeList;
}

FilePath toolchainFilePath(const Utils::FilePath &ndkLocation)
{
    FilePath toolchainFile = ndkLocation / "build" / "cmake" / Constants::OHOS_TOOLCHAIN_FILE;
    return toolchainFile;
}

} // namespace HarmonyConfig


const char SettingsGroup[] = "HarmonyConfigurations";
HarmonyConfigurations *m_instance = nullptr;
HarmonyConfigurations::HarmonyConfigurations(QObject *parent)
    : QObject{parent}
{
    load();
    connect(DeviceManager::instance(), &DeviceManager::devicesLoaded,
        this, &HarmonyConfigurations::updateHarmonyDevice);
    m_instance = this;
}

void HarmonyConfigurations::load()
{
    QtcSettings *settings = Core::ICore::settings();
    settings->beginGroup(SettingsGroup);
    HarmonyConfig::config().load(*settings);
    settings->endGroup();
}

void HarmonyConfigurations::save()
{
    QtcSettings *settings = Core::ICore::settings();
    settings->beginGroup(SettingsGroup);
    HarmonyConfig::config().save(*settings);
    settings->endGroup();
}

void HarmonyConfigurations::updateHarmonyDevice()
{
    // Remove any dummy Harmony device, because it won't be usable.
    DeviceManager *const devMgr = DeviceManager::instance();
    IDevice::ConstPtr dev = devMgr->find(Constants::HARMONY_DEVICE_ID);
    if (dev)
        devMgr->removeDevice(dev->id());
    /*
     * Add a deault Harmony device
     */
    HarmonyDevice *deaultDevice = new HarmonyDevice();
    deaultDevice->setupId(IDevice::AutoDetected, Constants::HARMONY_DEVICE_ID);
    deaultDevice->setMachineType(IDevice::Emulator);
    deaultDevice->setDeviceState(IDevice::DeviceStateUnknown);
    devMgr->addDevice(IDevicePtr(deaultDevice));
    setupDevicesWatcher();
}

HarmonyConfigurations *HarmonyConfigurations::instance()
{
    return m_instance;
}

void HarmonyConfigurations::applyConfig()
{
    emit m_instance->aboutToUpdate();
    m_instance->save();
    updateHarmonyDevice();
    registerNewToolchains();
    registerQtVersions();
    updateAutomaticKitList();
    removeOldToolchains();
    // emit m_instance->updated();
}

void HarmonyConfigurations::registerNewToolchains()
{
    const Toolchains existingHarmonyToolchains
        = ToolchainManager::toolchains(Utils::equal(&Toolchain::typeId, Id(Constants::HARMONY_TOOLCHAIN_TYPEID)));
    ToolchainManager::registerToolchains(autodetectToolchains(existingHarmonyToolchains));
    // registerCustomToolchainsAndDebuggers();
}
bool hasExistingVersion(const QtVersion *qtVersion) {
    const QtVersions installedVersions = QtVersionManager::versions(
        [qtVersion](const QtVersion *v) {
            return v->qmakeFilePath() == qtVersion->qmakeFilePath();
        });
    return !installedVersions.isEmpty();
}

// 匹配Kit与工具链Bundle的辅助函数
static bool matchKit(const ToolchainBundle &bundle, const Kit &kit)
{
    using namespace ProjectExplorer::Constants;
    for (const Id lang : {Id(C_LANGUAGE_ID), Id(CXX_LANGUAGE_ID)}) {
        const Toolchain * const tc = ToolchainKitAspect::toolchain(&kit, lang);
        if (!tc || tc->typeId() != Constants::HARMONY_TOOLCHAIN_TYPEID
            || tc->targetAbi() != bundle.targetAbi()) {
            return false;
        }
    }
    return true;
}

void HarmonyConfigurations::registerQtVersions()
{
    const auto installedVersions = QtVersionManager::versions([](auto version) {
        return version->type() == Constants::HARMONY_QT_TYPE;
    });
    for (auto *version : installedVersions)
    {
        if (!version->qmakeFilePath().exists() ||
            !HarmonyConfig::getQmakeList().contains(version->qmakeFilePath().toFSPathString()))
        {
            QtVersionManager::instance()->removeVersion(version);
        }
    }
    for(const auto &qmake : std::as_const(HarmonyConfig::getQmakeList()))
    {
        const auto qmakePath = FilePath::fromString(qmake);
        if (qmakePath.isExecutableFile())
        {
            auto *qtVersion = QtVersionFactory::createQtVersionFromQMakePath(qmakePath, true);
            if (auto *harmonyQtVersion = dynamic_cast<HarmonyQtVersion *>(qtVersion))
            {
                QString displayName = harmonyQtVersion->defaultUnexpandedDisplayName()
                                      + harmonyQtVersion->supportOhVersion().toString();
                harmonyQtVersion->setUnexpandedDisplayName(displayName);
                if(QtVersionManager::instance()->isLoaded() && !hasExistingVersion(harmonyQtVersion))
                {
                    QtVersionManager::instance()->addVersion(harmonyQtVersion);
                }
            }
        }
    }
}

void HarmonyConfigurations::removeOldToolchains()
{
    const auto invalidAndroidTcs = ToolchainManager::toolchains([](const Toolchain *tc) {
        return tc->id() == Constants::HARMONY_TOOLCHAIN_TYPEID && !tc->isValid();
    });
    ToolchainManager::deregisterToolchains(invalidAndroidTcs);
}

void HarmonyConfigurations::updateAutomaticKitList()
{
    // 更新现有Harmony Kit的NDK和SDK设置
    for (Kit *k : KitManager::kits()) {
        if (RunDeviceTypeKitAspect::deviceTypeId(k) == Constants::HARMONY_DEVICE_TYPE) {
            if (k->value(Constants::HARMONY_KIT_NDK).isNull() || k->value(Constants::HARMONY_KIT_SDK).isNull()) {
                if (QtVersion *qt = QtKitAspect::qtVersion(k)) {
                    // 设置NDK和SDK路径
                    const FilePath ndkPath = HarmonyConfig::ndkLocation(HarmonyConfig::defaultSdk());
                    k->setValueSilently(Constants::HARMONY_KIT_NDK, ndkPath.toSettings());
                    k->setValueSilently(Constants::HARMONY_KIT_SDK, HarmonyConfig::defaultSdk().toSettings());
                }
            }
        }
    }

    // 获取现有的Harmony Kit列表
    const QList<Kit *> existingKits = Utils::filtered(KitManager::kits(), [](Kit *k) {
        Id deviceTypeId = RunDeviceTypeKitAspect::deviceTypeId(k);
        return k->isAutoDetected() && !k->isSdkProvided()
               && deviceTypeId == Constants::HARMONY_DEVICE_TYPE;
    });

    // 按架构分组Qt版本
    QHash<Abi, QList<const QtVersion *>> qtVersionsForArch;
    const QtVersions qtVersions = QtVersionManager::versions([](const QtVersion *v) {
        return v->type() == Constants::HARMONY_QT_TYPE;
    });
    
    for (const QtVersion *qtVersion : qtVersions) {
        const Abis qtAbis = qtVersion->qtAbis();
        if (qtAbis.empty())
            continue;
        qtVersionsForArch[qtAbis.first()].append(qtVersion);
    }

    // 获取有效的工具链Bundle
    const QList<ToolchainBundle> bundles = Utils::filtered(
        ToolchainBundle::collectBundles(
            ToolchainManager::toolchains([](const Toolchain *tc) {
                return /*tc->isAutoDetected() &&*/ tc->typeId() == Constants::HARMONY_TOOLCHAIN_TYPEID;
            }),
            ToolchainBundle::HandleMissing::CreateAndRegister),
        [](const ToolchainBundle &b) { return b.isCompletelyValid(); });
    Core::MessageManager::writeSilently(
        tr("Found %1 Harmony toolchain bundles").arg(bundles.size()));
    QList<Kit *> unhandledKits = existingKits;

    // 为每个工具链Bundle和Qt版本组合创建Kit
    for (const ToolchainBundle &bundle : bundles) {
        const auto &versions = qtVersionsForArch.value(bundle.targetAbi());
        for (const QtVersion *qt : versions) {
            // 检查工具链的NDK位置是否与Qt版本匹配
            const auto tcApiVersion = bundle.get(&HarmonyToolchain::apiVersion);
            const auto expectedNdkPath = bundle.get(&HarmonyToolchain::ndkLocation);
            Core::MessageManager::writeSilently(
                tr("Processing Qt %1 for HarmonyOS %2 with NDK %3")
                    .arg(qt->displayName(), bundle.targetAbi().toString(), expectedNdkPath.toUserOutput()));
            auto ohQt = static_cast<const HarmonyQtVersion *>(qt);
            if(tcApiVersion.isNull() || ohQt->supportOhVersion() != tcApiVersion) {
                Core::MessageManager::writeSilently(
                    tr("Skipping Qt %1 for HarmonyOS %2: unsupported API version %3")
                        .arg(ohQt->displayName(), ohQt->supportOhVersion().toString(), tcApiVersion.toString()));
                continue;
            }
            // 查找是否已存在匹配的Kit
            Kit *existingKit = Utils::findOrDefault(existingKits, [&](const Kit *k) {
                if (ohQt != QtKitAspect::qtVersion(k))
                    return false;
                return matchKit(bundle, *k);
            });

            // Kit初始化函数
            const auto initializeKit = [&bundle, expectedNdkPath, ohQt](Kit *k) {

                using namespace CMakeProjectManager;
                auto cmakeConfig = CMakeConfigurationKitAspect::defaultConfiguration(k);
                cmakeConfig.append(CMakeConfigItem("CMAKE_TOOLCHAIN_FILE", CMakeConfigItem::FILEPATH,
                                            HarmonyConfig::toolchainFilePath(expectedNdkPath).toUserOutput().toUtf8()));
                cmakeConfig.append(CMakeConfigItem("CMAKE_FIND_ROOT_PATH", CMakeConfigItem::PATH,
                                            "%{Qt:QT_INSTALL_PREFIX}"));
                cmakeConfig.append(CMakeConfigItem("CMAKE_C_COMPILER_EXTERNAL_TOOLCHAIN", CMakeConfigItem::PATH, ""));
                cmakeConfig.append(CMakeConfigItem("CMAKE_CXX_COMPILER_EXTERNAL_TOOLCHAIN", CMakeConfigItem::PATH, ""));
                cmakeConfig.append(CMakeConfigItem("OHOS_STL", CMakeConfigItem::STRING, "c++_shared"));
                cmakeConfig.append(CMakeConfigItem("OHOS_ARCH", CMakeConfigItem::STRING,
                                            QByteArray(HarmonyConfig::displayName(ohQt->targetAbi()))));
                cmakeConfig.append(CMakeConfigItem("OHOS_PLATFORM", CMakeConfigItem::STRING,"OHOS"));


                k->setAutoDetected(true);
                k->setAutoDetectionSource("HarmonyConfiguration");
                RunDeviceTypeKitAspect::setDeviceTypeId(k, Constants::HARMONY_DEVICE_TYPE);
                ToolchainKitAspect::setBundle(k, bundle);
                QtKitAspect::setQtVersion(k, ohQt);

                // 设置构建设备为默认桌面设备
                BuildDeviceKitAspect::setDeviceId(k, DeviceManager::defaultDesktopDevice()->id());

                CMakeConfigurationKitAspect::setConfiguration(k, cmakeConfig);
                
                // 设置粘性属性，防止用户意外修改关键设置
                k->setSticky(QtKitAspect::id(), true);
                k->setSticky(RunDeviceTypeKitAspect::id(), true);
                k->setSticky(ToolchainKitAspect::id(), true);

                // 设置显示名称
                QString versionStr = QLatin1String("Qt %{Qt:Version}");
                if (!ohQt->isAutodetected())
                    versionStr = QString("%1").arg(ohQt->displayName());
                
                k->setUnexpandedDisplayName(QObject::tr("HarmonyOS%1 %2 Clang %3")
                                                .arg(ohQt->supportOhVersion().toString(),
                                                     versionStr,
                                                     HarmonyConfig::displayName(ohQt->targetAbi())));

                // 设置NDK和SDK路径
                k->setValueSilently(Constants::HARMONY_KIT_NDK, expectedNdkPath.toSettings());
                k->setValueSilently(Constants::HARMONY_KIT_SDK, ohQt->qmakeFilePath().toSettings());
            };

            if (existingKit) {
                // 更新现有Kit
                initializeKit(existingKit);
                unhandledKits.removeOne(existingKit);
            } else {
                // 注册新Kit
                KitManager::registerKit(initializeKit);
            }
        }
    }

    // 清理不再使用的Kit
    KitManager::deregisterKits(unhandledKits);
}

void setupHarmonyConfigurations()
{
    Core::MessageManager::writeSilently(
        QObject::tr("Setting up Harmony configurations..."));
    static HarmonyConfigurations harmonyConfigurations;
}

} // namespace Ohos::Internal
