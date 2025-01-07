#include "harmonyconfigurations.h"
#include <coreplugin/icore.h>
#include "ohosconstants.h"
#include <qjsonobject.h>
#include <utils/persistentsettings.h>
#include <projectexplorer/toolchainmanager.h>
#include <qtsupport/qtversionfactory.h>
#include <qtsupport/qtversionmanager.h>
#include "harmonytoolchain.h"
#include "harmonyqtversion.h"
#include <QJsonDocument>
using namespace Utils;

using namespace ProjectExplorer;
using namespace QtSupport;
namespace Ohos::Internal {
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
namespace HarmonyConfig {
struct HarmonyConfigData{
    void load(const QtcSettings &settings);
    void save(QtcSettings &settings) const;
    FilePath m_makeLocation;
    FilePath m_sdkLocation;
    FilePath m_ndkLocation;
    FilePath m_devecoStudioPath;
    FilePath m_qmakeLocation;

    QVersionNumber m_sdkVersion;
    QVersionNumber m_ndkVersion;
};
static HarmonyConfigData &config()
{
    static HarmonyConfigData theHarmonyConfig;
    return theHarmonyConfig;
}

void HarmonyConfigData::load(const QtcSettings &settings)
{
    m_makeLocation = FilePath::fromString(settings.value(Constants::MakeLocationKey).toString());
    m_sdkLocation = FilePath::fromString(settings.value(Constants::SDKLocationKey).toString());
    m_ndkLocation = FilePath::fromString(settings.value(Constants::NDKLocationKey).toString());
    m_devecoStudioPath = FilePath::fromString(settings.value(Constants::DevecoStudioLocationKey).toString());
    m_qmakeLocation = FilePath::fromString(settings.value(Constants::QmakeLocationKey).toString());
    m_sdkVersion = QVersionNumber::fromString(settings.value(Constants::SDKVersionKey).toString());
    m_ndkVersion = QVersionNumber::fromString(settings.value(Constants::NDKVersionKey).toString());
    // PersistentSettingsReader reader;

    // if (reader.load(sdkSettingsFileName())
    //     && settings.value(changeTimeStamp).toInt() != sdkSettingsFileName().lastModified().toMSecsSinceEpoch() / 1000)
    // {
    //     m_makeLocation = FilePath::fromString(reader.restoreValue(Constants::MakeLocationKey).toString());
    //     m_sdkLocation = FilePath::fromString(reader.restoreValue(Constants::SDKLocationKey).toString());
    //     m_devecoStudioPath = FilePath::fromString(reader.restoreValue(Constants::DevecoStudioLocationKey).toString());
    // }


}

void HarmonyConfigData::save(QtcSettings &settings) const
{
    const FilePath sdkSettingsFile = sdkSettingsFileName();
    if (sdkSettingsFile.exists())
        settings.setValue(changeTimeStamp, sdkSettingsFile.lastModified().toMSecsSinceEpoch() / 1000);

    settings.setValue(Constants::MakeLocationKey, m_makeLocation.toString());
    settings.setValue(Constants::SDKLocationKey, m_sdkLocation.toString());
    settings.setValue(Constants::NDKLocationKey, m_ndkLocation.toString());
    settings.setValue(Constants::DevecoStudioLocationKey, m_devecoStudioPath.toString());
    settings.setValue(Constants::QmakeLocationKey, m_qmakeLocation.toString());
    settings.setValue(Constants::SDKVersionKey, m_sdkVersion.toString());
    settings.setValue(Constants::NDKVersionKey, m_ndkVersion.toString());

}

FilePath sdkLocation()
{
    return config().m_sdkLocation;
}

void setSdkLocation(const Utils::FilePath &sdkLocation)
{
    config().m_sdkLocation = sdkLocation;
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

FilePath ndkLocation()
{
    return config().m_ndkLocation;
}

void setNdkLocation(const Utils::FilePath &ndkLocation)
{
    config().m_ndkLocation = ndkLocation;
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

QVersionNumber ndkVersion()
{
    return config().m_ndkVersion;
}

QVersionNumber sdkVersion()
{
    return config().m_sdkVersion;
}

bool setVersion(const Utils::FilePath &releaseFile)
{
    // 打开文件
    QString filePath = releaseFile.path();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file:" << filePath;
        return false;
    }
    QByteArray rawData = file.readAll();
    file.close();
    QJsonDocument doc = QJsonDocument::fromJson(rawData);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "Invalid JSON data!";
        return false;
    }
    QJsonObject rootObj = doc.object();
    QString versionStr = rootObj.value("version").toString();
    QString apiVersionStr = rootObj.value("apiVersion").toString();
    config().m_ndkVersion = QVersionNumber::fromString(versionStr);
    config().m_sdkVersion = QVersionNumber::fromString(apiVersionStr);
    return true;
}

void setQmakeLocation(const Utils::FilePath &qmakeLocation)
{
    config().m_qmakeLocation = qmakeLocation;
}

FilePath qmakeLocation()
{
    return config().m_qmakeLocation;
}

} // namespace HarmonyConfig

const char SettingsGroup[] = "HarmonyConfigurations";
HarmonyConfigurations *m_instance = nullptr;
HarmonyConfigurations::HarmonyConfigurations(QObject *parent)
    : QObject{parent}
{
    load();
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

HarmonyConfigurations *HarmonyConfigurations::instance()
{
    return m_instance;
}

void HarmonyConfigurations::applyConfig()
{
    emit m_instance->aboutToUpdate();
    m_instance->save();
    // updateHarmonyDevice();
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

void HarmonyConfigurations::registerQtVersions()
{
    const FilePath qmakePath = HarmonyConfig::qmakeLocation();
    if (qmakePath.isExecutableFile())
    {
        // 创建 Qt 版本
        QtVersion * qtVersion = QtVersionFactory::createQtVersionFromQMakePath(qmakePath, true);
        HarmonyQtVersion * version = dynamic_cast<HarmonyQtVersion *>(qtVersion);
        version->setUnexpandedDisplayName(version->defaultUnexpandedDisplayName());
        const QtVersions installedVersions = QtVersionManager::versions([](const QtVersion *v) {
            return v->type() == Constants::HARMONY_QT_TYPE;
        });
        for(QtVersion *v: installedVersions)
        {
            if(!v->qmakeFilePath().exists())
            {
                QtVersionManager::instance()->removeVersion(v);
                continue;
            }
            if(v->qmakeFilePath() == version->qmakeFilePath()
                && v->displayName() == version->displayName())
            {
                return;
            }
            else if (v->qmakeFilePath() == version->qmakeFilePath()
                && v->displayName() != version->displayName())
            {
                QtVersionManager::instance()->removeVersion(v);
            }
        }
        if(QtVersionManager::instance()->isLoaded())
        {
            QtVersionManager::instance()->addVersion(version);
        }
    }
}

void setupHarmonyConfigurations()
{
    qDebug() << "setupHarmonyConfigurations";
    static HarmonyConfigurations harmonyConfigurations;
}

} // namespace Ohos::Internal
