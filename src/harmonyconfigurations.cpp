#include "harmonyconfigurations.h"
#include <coreplugin/icore.h>
#include "ohosconstants.h"
#include <qjsonobject.h>
#include <utils/persistentsettings.h>
#include <projectexplorer/toolchainmanager.h>
#include <projectexplorer/devicesupport/devicemanager.h>
#include <qtsupport/qtversionfactory.h>
#include <qtsupport/qtversionmanager.h>
#include "harmonytoolchain.h"
#include "harmonyqtversion.h"
#include "harmonydevice.h"
#include <QJsonDocument>
#include <utils/qtcprocess.h>
using namespace Utils;

using namespace ProjectExplorer;
using namespace QtSupport;
namespace Ohos::Internal {
using namespace Constants::Parameter;
const Key changeTimeStamp("ChangeTimeStamp");
const QLatin1String ArmToolsDisplayName("arm");
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
        FilePath ndkPath = FilePath::fromString(path) / "default" / "openharmony" / "native";
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
    return sdkLocation / "default" / "openharmony" / "native";
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
    // updateAutomaticKitList();
    // removeOldToolchains();
    emit m_instance->updated();
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

void setupHarmonyConfigurations()
{
    qDebug() << "setupHarmonyConfigurations";
    static HarmonyConfigurations harmonyConfigurations;
}

} // namespace Ohos::Internal
