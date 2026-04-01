#include "harmonyconfigurations.h"
#include <coreplugin/icore.h>
#include "ohosconstants.h"

#include <debugger/debuggeritem.h>
#include <debugger/debuggeritemmanager.h>
#include <debugger/debuggerkitaspect.h>
#include <qjsonobject.h>
#include <utils/persistentsettings.h>
#include <utils/algorithm.h>

#include <projectexplorer/kitaspect.h>
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
#include "harmonylogcategories.h"
#include "harmonyutils.h"
#include "hdcsocketclient.h"
#include <QDir>
#include <QJsonDocument>
#include <QVariant>
#include <QVersionNumber>

#include <algorithm>
#include <QSet>
#include <QStandardPaths>
#include <utils/environment.h>
#include <utils/hostosinfo.h>
#include <utils/qtcprocess.h>

#include <coreplugin/messagemanager.h> // writeFlashing (OHOS_ARCH kit 提示)

#include <ohprojectecreator/ohprojectecreator.h>

using namespace Utils;

using namespace ProjectExplorer;
using namespace QtSupport;
namespace Ohos::Internal {
using namespace Constants::Parameter;

static FilePath unixHostMakeProgramForCMake()
{
    const Environment sys = Environment::systemEnvironment();
    const QStringList names{QStringLiteral("make"), QStringLiteral("gmake")};
    for (const QString &name : names) {
        const FilePath found = sys.searchInPath(name);
        if (!found.isEmpty())
            return found;
    }
    const std::array fallbacks = {
        FilePath::fromString("/usr/bin/make"),
        FilePath::fromString("/bin/make"),
    };
    return findOr(fallbacks, FilePath{}, [](const auto &fp) { return fp.isExecutableFile(); });
}

static bool harmonySdkListContainsNormalized(const QStringList &list, const FilePath &sdkDir)
{
    const FilePath normalized = sdkDir.cleanPath();
    return std::any_of(list.cbegin(), list.cend(), [&normalized](const QString &entry) {
        return FilePath::fromUserInput(entry).cleanPath() == normalized;
    });
}

const Key changeTimeStamp("ChangeTimeStamp");
const QLatin1String ArmToolsDisplayName("armeabi-v7a");
const QLatin1String X86ToolsDisplayName("i686");
const QLatin1String AArch64ToolsDisplayName("arm64-v8a");
const QLatin1String X86_64ToolsDisplayName("x86_64");
const QLatin1String Unknown("unknown");

/** 供 CMake OHOS_ARCH 使用（与 ohos.toolchain.cmake 一致）；勿使用 displayName 的 "unknown"。 */
static QByteArray abiToOhosNdkArchString(const Abi &abi)
{
    if (!abi.isValid())
        return {};
    switch (abi.architecture()) {
    case Abi::ArmArchitecture:
        return abi.wordWidth() == 64 ? QByteArray(Constants::HARMONY_ABI_ARM64_V8A)
                                     : QByteArray(Constants::HARMONY_ABI_ARMEABI_V7A);
    case Abi::X86Architecture:
        return abi.wordWidth() == 64 ? QByteArray(Constants::HARMONY_ABI_X86_64)
                                     : QByteArray(Constants::HARMONY_ABI_X86);
    default:
        return {};
    }
}

/**
 * OpenHarmony SDK 的 ohos.toolchain.cmake 仅识别 arm64-v8a、armeabi-v7a、x86_64。
 * 旧 Kit / 手写参数 / 缓存里可能出现 unknown、x86 等非法值，在此统一纠正。
 */
static QByteArray normalizeOhosArchForSdkToolchain(QByteArray arch)
{
    arch = arch.trimmed();
    static const QList allowed{ // NOSONAR (cpp:S5041) - static const cannot use if-init-statement
        QByteArray(Constants::HARMONY_ABI_ARM64_V8A),
        QByteArray(Constants::HARMONY_ABI_ARMEABI_V7A),
        QByteArray(Constants::HARMONY_ABI_X86_64),
    };
    if (allowed.contains(arch))
        return arch;
    const QByteArray lower = arch.toLower();
    if (lower == QByteArrayLiteral("aarch64") || lower == QByteArrayLiteral("arm64"))
        return QByteArray(Constants::HARMONY_ABI_ARM64_V8A);
    if (lower == QByteArrayLiteral("arm") || lower == QByteArrayLiteral("armeabi")
        || lower == QByteArrayLiteral("armv7") || lower == QByteArrayLiteral("armv7-a"))
        return QByteArray(Constants::HARMONY_ABI_ARMEABI_V7A);
    if (arch == QByteArray(Constants::HARMONY_ABI_X86))
        return QByteArray(Constants::HARMONY_ABI_X86_64);
    if (lower == QByteArrayLiteral("i686") || lower == QByteArrayLiteral("x86"))
        return QByteArray(Constants::HARMONY_ABI_X86_64);
    return QByteArray(Constants::HARMONY_ABI_ARM64_V8A);
}

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
    const QString wireCmd = QStringLiteral("shell param get ") + property;
    const FilePath hdcPath = HarmonyConfig::hdcToolPath();
    const QStringList cliArgs{QStringLiteral("-t"), device, QStringLiteral("shell"), QStringLiteral("param"),
                              QStringLiteral("get"), property};
    const HdcShellSyncResult r = HdcSocketClient::runSyncWithCliFallback(
        device, wireCmd, hdcPath.toUserOutput(), cliArgs, 20000);
    if (r.isOk()) {
        qCDebug(harmonyConfigLog) << "getDeviceProperty ok for" << property;
        return r.standardOutput.trimmed();
    }
    qCWarning(harmonyConfigLog) << "getDeviceProperty failed" << int(r.code) << r.errorMessage;
    return {};
}

namespace {
FilePath javaHomeIfValid(const FilePath &home)
{
    if (home.isEmpty())
        return {};
    const FilePath javaExe = (home / "bin" / "java").withExecutableSuffix();
    return javaExe.isExecutableFile() ? home : FilePath{};
}

#ifdef Q_OS_MACOS
/** 使用 Apple 官方工具自动解析本机已安装的 JDK（无需用户配置 JAVA_HOME）。 */
FilePath macOsJavaHomeFromJavaHomeTool()
{
    static FilePath cache;
    static bool done = false;
    if (done)
        return cache;
    done = true;

    const FilePath tool = FilePath::fromString("/usr/libexec/java_home");
    if (!tool.isExecutableFile())
        return cache;
    const QList<QStringList> argVariants = {
        {QStringLiteral("-v"), QStringLiteral("17+")},
        {QStringLiteral("-v"), QStringLiteral("17")},
        {QStringLiteral("-v"), QStringLiteral("11+")},
        QStringList{},
    };
    for (const QStringList &args : argVariants) {
        Process proc;
        proc.setCommand({tool, args});
        proc.runBlocking();
        if (proc.result() != ProcessResult::FinishedWithSuccess)
            continue;
        const FilePath home = FilePath::fromUserInput(proc.cleanedStdOut().trimmed()).cleanPath();
        if (const FilePath v = javaHomeIfValid(home); !v.isEmpty()) {
            cache = v;
            return cache;
        }
    }
    return cache;
}
#endif

/** PATH 中的 java 解析到 JDK 根目录（resolveSymlinks 后取 bin 的父目录）。 */
FilePath javaHomeFromPathJava()
{
    static FilePath cache;
    static bool done = false;
    if (done)
        return cache;
    done = true;

    const FilePath javaExe = Environment::systemEnvironment().searchInPath("java");
    if (!javaExe.isExecutableFile())
        return cache;
    const FilePath resolved = javaExe.resolveSymlinks();
    FilePath home = resolved.parentDir();
    if (home.fileName() == QStringLiteral("bin"))
        home = home.parentDir();
    cache = javaHomeIfValid(home);
    return cache;
}
} // namespace

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
    FilePath m_ohosSdkRoot;
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
    QString m_ohpmRegistryUrl;
    bool m_ohpmStrictSsl = true;
    QStringList m_ohModuleDeviceTypes;
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
    m_ohosSdkRoot = FilePath::fromString(settings.value(Constants::OhosSdkRootKey).toString());
    m_ohpmRegistryUrl = settings.value(Constants::OhpmRegistryUrlKey).toString();
    m_ohpmStrictSsl = settings.contains(Constants::OhpmStrictSslKey)
                          ? settings.value(Constants::OhpmStrictSslKey).toBool()
                          : true;
    if (settings.contains(Constants::OhModuleDeviceTypesKey))
        m_ohModuleDeviceTypes = settings.value(Constants::OhModuleDeviceTypesKey).toStringList();
    else
        m_ohModuleDeviceTypes.clear();
    m_qmakeList.removeAll("");
    m_sdkList.removeAll("");
}

void HarmonyConfigData::save(QtcSettings &settings) const
{
    if (const FilePath sdkSettingsFile = sdkSettingsFileName(); sdkSettingsFile.exists())
        settings.setValue(changeTimeStamp, sdkSettingsFile.lastModified().toMSecsSinceEpoch() / 1000);

    settings.setValue(Constants::MakeLocationKey, m_makeLocation.toFSPathString());
    settings.setValue(Constants::DevecoStudioLocationKey, m_devecoStudioPath.toFSPathString());
    settings.setValue(Constants::QmakeLocationKey, m_qmakeList);
    settings.setValue(Constants::SDKLocationsKey, m_sdkList);
    settings.setValue(Constants::DefaultSDKLocationKey, m_defaultSdkLocation.toFSPathString());
    settings.setValue(Constants::OhosSdkRootKey, m_ohosSdkRoot.toFSPathString());
    settings.setValue(Constants::OhpmRegistryUrlKey, m_ohpmRegistryUrl);
    settings.setValue(Constants::OhpmStrictSslKey, m_ohpmStrictSsl);
    settings.setValue(Constants::OhModuleDeviceTypesKey, m_ohModuleDeviceTypes);
}

FilePath devecoStudioLocation()
{
    return devecoStudioLocation(config().m_devecoStudioPath); // normalize (e.g. resolve .app to Contents on macOS)
}

FilePath devecoStudioLocation(const Utils::FilePath &root)
{
    FilePath ret = root.cleanPath();
    if (ret.isEmpty()) {
        return {};
    }
#ifdef Q_OS_MACOS
    if (ret.path().endsWith(QLatin1String(".app"), Qt::CaseInsensitive)) {
        return ret / "Contents";
    }
#endif
    return ret;
}

bool isValidDevecoStudioRoot(const FilePath &root)
{
    const FilePath stored = root.cleanPath();
    if (stored.isEmpty() || !stored.isReadableDir())
        return false;
    const FilePath layout = devecoStudioLocation(stored);

    if (!layout.isReadableDir())
        return false;
    if ((layout / "tools").isReadableDir())
        return true;
    return devecostudioExeLocation(layout).isExecutableFile();
}

bool tryAutoDetectDevecoStudio()
{
    if (!devecoStudioLocation().isEmpty())
        return false;

    if (const QString fromEnv
            = qtcEnvironmentVariable(QString::fromLatin1(Constants::DEVECO_STUDIO_HOME_ENV_VAR));
        !fromEnv.isEmpty()) {
        const FilePath fp = FilePath::fromUserInput(fromEnv).cleanPath();
        if (isValidDevecoStudioRoot(fp)) {
            setDevecoStudioLocation(fp);
            return true;
        }
    }
#ifdef Q_OS_MACOS
    QSet<QString> seen;
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QStringList appRoots;
    appRoots << QStringLiteral("/Applications") << (home + QStringLiteral("/Applications"));
    appRoots << QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);

    for (const QString &appsRoot : std::as_const(appRoots)) {
        const QDir rootDir(appsRoot);
        if (!rootDir.exists())
            continue;
        const QStringList apps = rootDir.entryList({QStringLiteral("*.app")},
                                                   QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &app : std::as_const(apps)) {
            if (!app.contains(QStringLiteral("DevEco"), Qt::CaseInsensitive))
                continue;
            const QString bundlePath = appsRoot + QLatin1Char('/') + app;
            if (seen.contains(bundlePath))
                continue;
            seen.insert(bundlePath);
            const FilePath fp = FilePath::fromUserInput(bundlePath).cleanPath();
            if (isValidDevecoStudioRoot(fp)) {
                setDevecoStudioLocation(fp);
                return true;
            }
        }
    }
#endif
    return false;
}

void setDevecoStudioLocation(const Utils::FilePath &devecoStudioLocation)
{
    config().m_devecoStudioPath = devecoStudioLocation.cleanPath();
    HarmonyConfigurations::persistSettings();
}

FilePath makeLocation()
{
    return config().m_makeLocation;
}

void setMakeLocation(const Utils::FilePath &makeLocation)
{
    config().m_makeLocation = makeLocation;
    HarmonyConfigurations::persistSettings();
}

QList<FilePath> ndkLocations()
{
    QList<FilePath> ndkLocations;
    for (const QString &path : qAsConst(config().m_sdkList))
    {
        FilePath ndkPath = ndkLocation(FilePath::fromUserInput(path).cleanPath());
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
    /* ** 打开文件 */
    QPair<QVersionNumber, QVersionNumber> versionPair(QVersionNumber(-1), QVersionNumber(-1));

    QString filePath = releaseFile.path();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(harmonyConfigLog) << "Failed to open file:" << filePath;
        return versionPair;
    }
    QByteArray rawData = file.readAll();
    file.close();
    QJsonDocument doc = QJsonDocument::fromJson(rawData);
    if (doc.isNull() || !doc.isObject()) {
        qCWarning(harmonyConfigLog) << "Invalid JSON data!";
        return versionPair;
    }
    QJsonObject rootObj = doc.object();
    QString versionStr = rootObj.value("version").toString();
    QString apiVersionStr = rootObj.value("apiVersion").toString();
    versionPair.first = QVersionNumber::fromString(apiVersionStr);
    versionPair.second = QVersionNumber::fromString(versionStr);
    return versionPair;
}

static void appendHarmonyHdcToolchainBases(QVector<FilePath> &out, const FilePath &sdkRoot)
{
    if (sdkRoot.isEmpty() || !sdkRoot.isReadableDir())
        return;
    const auto appendPair = [&](const FilePath &root) {
        if (root.isEmpty() || !root.isReadableDir())
            return;
        out.append(root / "toolchains");
        out.append(root / "default" / "openharmony" / "toolchains");
    };
    appendPair(sdkRoot);
    /* ** SDK 根下多 API 目录（如 …/sdk/18/toolchains/hdc） */
    const FilePaths children = sdkRoot.dirEntries(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const FilePath &ch : children) {
        if (ch.fileName() == QStringLiteral(".temp"))
            continue;
        if (const QString n = ch.fileName(); !n.isEmpty() && n.at(0) == QLatin1Char('.'))
            continue;
        appendPair(ch);
    }
}

FilePath hdcToolPath()
{
    QVector<FilePath> bases;
    QSet<QString> seen;

    const auto addRoots = [&](const FilePath &sdkRoot) {
        QVector<FilePath> chunk;
        appendHarmonyHdcToolchainBases(chunk, sdkRoot.cleanPath());
        for (const FilePath &b : std::as_const(chunk)) {
            const QString k = b.toUserOutput();
            if (k.isEmpty() || seen.contains(k))
                continue;
            seen.insert(k);
            bases.append(b);
        }
    };

    addRoots(defaultSdk());
    for (const QString &s : getSdkList())
        addRoots(FilePath::fromUserInput(s).cleanPath());
    addRoots(effectiveOhosSdkRoot());

    for (const FilePath &path : std::as_const(bases)) {
        const FilePath hdcPath = path.pathAppended("hdc").withExecutableSuffix();
        if (hdcPath.isExecutableFile())
            return hdcPath;
    }

    const FilePath fallbackRoot = !defaultSdk().isEmpty() ? defaultSdk().cleanPath() : effectiveOhosSdkRoot();
    return (fallbackRoot / "toolchains" / "hdc").withExecutableSuffix();
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
    if (tmp.contains("[Fail]") || tmp.contains("[Fail]Device not founded or connected")) {
        return QString();
    }
    return tmp.isEmpty() ? QString() : tmp.trimmed();
}

QStringList getAbis(const QString &device)
{
    const QString tmp = getDeviceProperty(device, Product::Cpu::abilist);
    return tmp.trimmed().split(QRegularExpression(QStringLiteral("[,\\s]+")),
                 Qt::SkipEmptyParts);
}

QStringList &getSdkList()
{
    return config().m_sdkList;
}

void addSdk(const QString &sdk)
{
    if (!config().m_sdkList.contains(sdk)) {
        config().m_sdkList.append(sdk);
        HarmonyConfigurations::persistSettings();
    }
}

void removeSdkList(const QString &sdk)
{
    config().m_sdkList.removeAll(sdk);
    HarmonyConfigurations::persistSettings();
}

bool isValidSdk(const FilePath &sdkLocation)
{
    const FilePath ndkPath = ndkLocation(sdkLocation);
    if (!ndkPath.isReadableDir())
        return false;
    return isValidReleaseFile(ndkPath);
}

bool isValidSdk(const QString &sdkLocation)
{
    if (sdkLocation.isEmpty())
        return false;
    return isValidSdk(FilePath::fromUserInput(sdkLocation).cleanPath());
}

FilePath releaseFile(const Utils::FilePath &ndkLocation)
{
    return ndkLocation / "oh-uni-package.json";
}

bool isValidReleaseFile(const Utils::FilePath &ndkLocation)
{
    return releaseFile(ndkLocation).isReadableFile();
}

FilePath ndkLocation(const Utils::FilePath &sdkLocation)
{
    const FilePath root = sdkLocation.cleanPath();
    const QVector candidatePaths = {
        root / "native",
        root / "default" / "openharmony" / "native",
        /* ** 部分 DevEco / 华为侧 SDK 布局：版本目录下直接为 openharmony/native */
        root / "openharmony" / "native",
    };
    return findOr(candidatePaths, candidatePaths.at(0), isValidReleaseFile);
}

FilePath defaultSdk()
{
    return config().m_defaultSdkLocation;
}

void setdefaultSdk(const Utils::FilePath &sdkLocation)
{
    config().m_defaultSdkLocation = sdkLocation;
    HarmonyConfigurations::persistSettings();
}

FilePath ohosSdkRoot()
{
    return config().m_ohosSdkRoot;
}

void setOhosSdkRoot(const FilePath &path)
{
    config().m_ohosSdkRoot = path;
    HarmonyConfigurations::persistSettings();
}

QString effectiveQtOhBinaryCatalogUrl()
{
    const QString fromEnv = qtcEnvironmentVariable(QStringLiteral("QT_OH_BINARY_CATALOG_URL")).trimmed();
    if (!fromEnv.isEmpty())
        return fromEnv;
    return QString::fromLatin1(Constants::QtOhBinaryCatalogDefaultGitcodeUrl);
}

FilePath defaultOhosSdkPath()
{
    QString fromEnv = qtcEnvironmentVariable(QString::fromLatin1(Constants::OHOS_SDK_HOME_ENV_VAR));
    if (fromEnv.isEmpty())
        fromEnv = qtcEnvironmentVariable(QString::fromLatin1(Constants::OHOS_SDK_ENV_VAR));
    if (!fromEnv.isEmpty())
        return FilePath::fromUserInput(fromEnv).cleanPath();

    if (HostOsInfo::isMacHost()) {
        return FilePath::fromString(
            QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/Library/OpenHarmony/sdk");
    }
    if (HostOsInfo::isWindowsHost()) {
        return FilePath::fromString(
            QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
            + "/OpenHarmony/Sdk");
    }
    return FilePath::fromString(
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/OpenHarmony/sdk");
}

FilePath effectiveOhosSdkRoot()
{
    if (const FilePath stored = ohosSdkRoot().cleanPath(); !stored.isEmpty())
        return stored;
    return defaultOhosSdkPath().cleanPath();
}

int registerDownloadedSdksUnder(const FilePath &sdkRoot)
{
    const FilePath root = sdkRoot.cleanPath();
    if (root.isEmpty() || !root.isReadableDir())
        return 0;

    int added = 0;
    const QStringList &list = getSdkList();

    if (isValidSdk(root) && !harmonySdkListContainsNormalized(list, root)) {
        addSdk(root.toUserOutput());
        ++added;
    }

    const FilePaths children = root.dirEntries(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const FilePath &child : children) {
        if (child.fileName() == QStringLiteral(".temp"))
            continue;
        if (const QString name = child.fileName(); !name.isEmpty() && name.at(0) == QLatin1Char('.'))
            continue;
        const FilePath cleaned = child.cleanPath();
        if (!isValidSdk(cleaned))
            continue;
        if (harmonySdkListContainsNormalized(list, cleaned))
            continue;
        addSdk(cleaned.toUserOutput());
        ++added;
    }
    return added;
}

void addQmake(const QString &qmake)
{
    if (!config().m_qmakeList.contains(qmake))
    {
        config().m_qmakeList.append(qmake);
    }
    HarmonyConfigurations::persistSettings();
    HarmonyConfigurations::syncToolchainsQtAndKits();
}

void removeQmake(const QString &qmake)
{
    config().m_qmakeList.removeAll(qmake);
    HarmonyConfigurations::persistSettings();
    HarmonyConfigurations::syncToolchainsQtAndKits();
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

FilePath devecoToolsLocation()
{
    const FilePath stored = devecoStudioLocation();
    if (stored.isEmpty())
        return {};
    return stored / "tools";
}

FilePath devecoSdkLocation()
{
    const FilePath stored = devecoStudioLocation();
    if (stored.isEmpty())
        return {};
    return stored / "sdk";
}

FilePath hvigorwJsLocation()
{
    return devecoToolsLocation() / "hvigor" / "bin" / "hvigorw.js";
}

namespace {
FilePath harmonyBundledNodeUnderTools(const FilePath &toolsDir)
{
    if (toolsDir.isEmpty() || !toolsDir.isReadableDir())
        return {};
    const std::array candidates = {
        /* ** 旧布局（部分 DevEco / Windows） */
        toolsDir / "node" / "node",
        /* ** 常见：macOS / 新版 DevEco 为 tools/node/bin/node */
        toolsDir / "node" / "bin" / "node",
    };
    for (const FilePath &p : candidates) {
        const FilePath exe = p.withExecutableSuffix();
        if (exe.isExecutableFile())
            return exe;
    }
    return {};
}
} // namespace

FilePath nodeLocation()
{
    const FilePath tools = devecoToolsLocation();
    if (const FilePath bundled = harmonyBundledNodeUnderTools(tools); !bundled.isEmpty())
        return bundled;

    /* ** 未随 DevEco 安装或路径变更时，使用系统 PATH 中的 node（brew / 官网安装均可跑 hvigor） */
    if (const FilePath fromPath = Environment::systemEnvironment().searchInPath("node"); fromPath.isExecutableFile())
        return fromPath;

    /* ** 供诊断：仍返回旧版默认路径（可能不存在） */
    if (!tools.isEmpty())
        return (tools / "node" / "node").withExecutableSuffix();
    return {};
}

FilePath ohpmJsLocation()
{
    return devecoToolsLocation() / "ohpm" / "bin" / "pm-cli.js";
}

QString defaultOhpmRegistryUrl()
{
    return QStringLiteral("https://ohpm.openharmony.cn/ohpm");
}

QString ohpmRegistryUrl()
{
    return config().m_ohpmRegistryUrl;
}

QString effectiveOhpmRegistryUrl()
{
    const QString s = config().m_ohpmRegistryUrl.trimmed();
    return s.isEmpty() ? defaultOhpmRegistryUrl() : s;
}

void setOhpmRegistryUrl(const QString &url)
{
    config().m_ohpmRegistryUrl = url.trimmed();
    HarmonyConfigurations::persistSettings();
}

bool ohpmStrictSsl()
{
    return config().m_ohpmStrictSsl;
}

void setOhpmStrictSsl(bool on)
{
    config().m_ohpmStrictSsl = on;
    HarmonyConfigurations::persistSettings();
}

namespace {
QStringList cleanedOhModuleDeviceTypes(const QStringList &in)
{
    QStringList out;
    for (const QString &s : in) {
        const QString t = s.trimmed();
        if (!t.isEmpty())
            out.append(t);
    }
    return out;
}
} // namespace

QStringList ohModuleDeviceTypesUserList()
{
    return config().m_ohModuleDeviceTypes;
}

QStringList ohModuleDeviceTypes()
{
    if (config().m_ohModuleDeviceTypes.isEmpty())
        return {QStringLiteral("2in1")};
    return config().m_ohModuleDeviceTypes;
}

void setOhModuleDeviceTypes(const QStringList &types)
{
    config().m_ohModuleDeviceTypes = cleanedOhModuleDeviceTypes(types);
    HarmonyConfigurations::persistSettings();
    HarmonyConfigurations::syncToolchainsQtAndKits();
}

FilePath javaLocation()
{
    const FilePath stored = devecoStudioLocation();
    if (!stored.isEmpty()) {
        const FilePath jbrRoot = stored / "jbr";
#ifdef Q_OS_MACOS
        /* ** JetBrains JBR on macOS: …/jbr/Contents/Home/bin/java（勿把 JAVA_HOME 指到 jbr 根目录） */
        if (const FilePath macHome = javaHomeIfValid(jbrRoot / "Contents" / "Home"); !macHome.isEmpty())
            return macHome;
#endif
        if (const FilePath flat = javaHomeIfValid(jbrRoot); !flat.isEmpty())
            return flat;
    }

    const QString fromEnv = qtcEnvironmentVariable(QStringLiteral("JAVA_HOME"));
    if (!fromEnv.isEmpty()) {
        if (const FilePath h = javaHomeIfValid(FilePath::fromUserInput(fromEnv).cleanPath());
            !h.isEmpty())
            return h;
    }

#ifdef Q_OS_MACOS
    if (const FilePath mac = macOsJavaHomeFromJavaHomeTool(); !mac.isEmpty())
        return mac;
#endif
    if (const FilePath fromPath = javaHomeFromPathJava(); !fromPath.isEmpty())
        return fromPath;

    if (!stored.isEmpty())
        return stored / "jbr";
    return {};
}

QPair<int, QVersionNumber> devecoStudioVersion()
{
    const FilePath stored = devecoStudioLocation();
    const FilePath sdkPkg = stored.isEmpty()
                                ? FilePath{}
                                : stored / "sdk" / "default" / "sdk-pkg.json";
    QString sdkPkgJson = sdkPkg.toUserOutput();
    if (!QFile::exists(sdkPkgJson))
        return { -1, QVersionNumber() };
    QFile file(sdkPkgJson);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return { -1, QVersionNumber() };
    QByteArray rawData = file.readAll();
    file.close();
    QJsonDocument doc = QJsonDocument::fromJson(rawData);
    if (doc.isNull() || !doc.isObject()) {
        qCWarning(harmonyConfigLog) << "Invalid JSON data!";
        return { -1, QVersionNumber() };
    }
    QJsonObject rootObj = doc.object();
    QJsonObject dataObj = rootObj.value("data").toObject();
    QString versionStr = dataObj.value("apiVersion").toString();
    QString platformVersionStr = dataObj.value("platformVersion").toString();
    return { versionStr.toInt(), QVersionNumber::fromString(platformVersionStr) };
}

FilePath devecostudioExeLocation()
{
    return devecostudioExeLocation(devecoStudioLocation());
}

FilePath devecostudioExeLocation(const Utils::FilePath &devecoLocation)
{
    if (devecoLocation.isEmpty())
        return {};
#ifdef Q_OS_MACOS
    const FilePath classic = FilePath(devecoLocation / "MacOS" / "devecostudio");
#else
    const FilePath classic = FilePath(devecoLocation / "bin" / "devecostudio64");
#endif
    return classic.withExecutableSuffix();
}

QStringList apiLevelNamesFor(const QList<int> &apiLevels)
{
    return Utils::transform(apiLevels, HarmonyConfig::apiLevelNameFor);
}

QString apiLevelNameFor(const int apiLevel)
{
    if (apiLevel > 0) {
        QString apiLevelName = OhProjecteCreator::versionForApiLevel(apiLevel);
        return apiLevelName.append(QString("(%1)").arg(apiLevel));
    }
    return QString();
}

} // namespace HarmonyConfig


const char SettingsGroup[] = "HarmonyConfigurations";
HarmonyConfigurations *m_instance = nullptr; // NOSONAR (cpp:S1169) - reassigned during initialization
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

    bool needSave = false;
    /* ** macOS/Linux：make 来自 PATH（与 GCC 工具链思路相同）；丢弃已存的 MinGW 式路径。 */
    if (!HostOsInfo::isWindowsHost() && !HarmonyConfig::makeLocation().isEmpty()) {
        HarmonyConfig::setMakeLocation({});
        needSave = true;
    }
    if (HarmonyConfig::tryAutoDetectDevecoStudio())
        needSave = true;
    if (needSave)
        save();
}

void HarmonyConfigurations::save() const
{
    QtcSettings *settings = Core::ICore::settings();
    settings->beginGroup(SettingsGroup);
    HarmonyConfig::config().save(*settings);
    settings->endGroup();
}

void HarmonyConfigurations::persistSettings()
{
    if (m_instance)
        m_instance->save();
}

void HarmonyConfigurations::updateHarmonyDevice()
{
    /* ** 移除虚拟 Harmony 设备（无法使用）。 */
    if (const IDevice::ConstPtr dev = DeviceManager::find(Constants::HARMONY_DEVICE_ID)) {
        DeviceManager::removeDevice(dev->id());
    }
    setupDevicesWatcher();
}

HarmonyConfigurations *HarmonyConfigurations::instance()
{
    return m_instance;
}

void HarmonyConfigurations::syncToolchainsQtAndKits()
{
    registerNewToolchains();
    registerQtVersions();
    updateAutomaticKitList();
    removeOldToolchains();
}

void HarmonyConfigurations::applyConfig()
{
    emit m_instance->aboutToUpdate();
    m_instance->save();
    updateHarmonyDevice();
    syncToolchainsQtAndKits();
    emit m_instance->updated();
}

void HarmonyConfigurations::registerNewToolchains()
{
    const Toolchains existingHarmonyToolchains
        = ToolchainManager::toolchains(Utils::equal(&Toolchain::typeId, Id(Constants::HARMONY_TOOLCHAIN_TYPEID)));
    ToolchainManager::registerToolchains(autodetectToolchains(existingHarmonyToolchains));
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
            auto *qtVersion = QtVersionFactory::createQtVersionFromQMakePath(
                qmakePath, DetectionSource(DetectionSource::Manual));
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
    const auto invalidHarmonyTcs = ToolchainManager::toolchains([](const Toolchain *tc) {
        return tc->id() == Constants::HARMONY_TOOLCHAIN_TYPEID && !tc->isValid();
    });
    ToolchainManager::deregisterToolchains(invalidHarmonyTcs);
}

/*
** 查找 OHOS LLDB 主机二进制文件。
** 搜索顺序：(1) DevEco Studio 捆绑 SDK，(2) 给定的 NDK 路径，(3) 所有已注册 SDK。
*/
static FilePath ohosHostLldb(const FilePath &ndkPath)
{
    /* ** (1) DevEco Studio: <DevEco.app>/Contents/sdk/default/openharmony/native/llvm/bin/lldb */
    const FilePath tools = HarmonyConfig::devecoToolsLocation();
    if (!tools.isEmpty()) {
        const FilePath candidate = tools.parentDir()
                                   / "sdk" / "default" / "openharmony" / "native" / "llvm" / "bin" / "lldb";
        if (candidate.isExecutableFile())
            return candidate;
    }
    /* ** (2) NDK 路径：<ndk>/llvm/bin/lldb */
    if (!ndkPath.isEmpty()) {
        const FilePath candidate = ndkPath / "llvm" / "bin" / "lldb";
        if (candidate.isExecutableFile())
            return candidate;
    }
    /* ** (3) 所有已注册 SDK */
    for (const QString &s : HarmonyConfig::getSdkList()) {
        const FilePath ndk = HarmonyConfig::ndkLocation(FilePath::fromUserInput(s));
        if (!ndk.isEmpty()) {
            const FilePath candidate = ndk / "llvm" / "bin" / "lldb";
            if (candidate.isExecutableFile())
                return candidate;
        }
    }
    return {};
}

/*
** 将 OHOS LLDB 二进制文件注册为 DebuggerItem（如未注册）
** 并返回其 ID。找不到二进制文件时返回空 QVariant。
*/
static QVariant findOrRegisterOhosDebugger(const FilePath &ndkPath)
{
    const FilePath lldb = ohosHostLldb(ndkPath);
    if (lldb.isEmpty())
        return {};

    /* ** 若命令匹配则返回现有注册 */
    using namespace Debugger;
    const DebuggerItem *existing = DebuggerItemManager::findByCommand(lldb);
    if (existing && existing->engineType() == LldbEngineType)
        return existing->id();

    /* ** 注册新条目 */
    DebuggerItem item;
    item.setCommand(lldb);
    item.setEngineType(LldbEngineType);
    item.setUnexpandedDisplayName(
        QObject::tr("OHOS LLDB (%1)").arg(lldb.parentDir().parentDir().parentDir().parentDir().parentDir().fileName()));
    item.setDetectionSource(DetectionSource::FromSystem);
    item.reinitializeFromFile();
    return DebuggerItemManager::registerDebugger(item);
}

void HarmonyConfigurations::updateAutomaticKitList()
{
    /* ** 更新现有 Harmony Kit 的 NDK 和 SDK 设置 */
    for (Kit *k : KitManager::kits()) {
        if (RunDeviceTypeKitAspect::deviceTypeId(k) == Constants::HARMONY_DEVICE_TYPE) {
            if (k->value(Constants::HARMONY_KIT_NDK).isNull() || k->value(Constants::HARMONY_KIT_SDK).isNull()) {
                if (QtKitAspect::qtVersion(k)) {
                    /* ** 设置 NDK 和 SDK 路径 */
                    const FilePath ndkPath = HarmonyConfig::ndkLocation(HarmonyConfig::defaultSdk());
                    k->setValueSilently(Constants::HARMONY_KIT_NDK, ndkPath.toSettings());
                    k->setValueSilently(Constants::HARMONY_KIT_SDK, HarmonyConfig::defaultSdk().toSettings());
                }
            }
        }
    }

    /* ** 获取现有的 Harmony Kit 列表 */
    const QList<Kit *> existingKits = Utils::filtered(KitManager::kits(), [](Kit *k) {
        Id deviceTypeId = RunDeviceTypeKitAspect::deviceTypeId(k);
        return k->detectionSource().isAutoDetected() && !k->detectionSource().isSdkProvided()
               && deviceTypeId == Constants::HARMONY_DEVICE_TYPE;
    });

    /* ** 按架构分组 Qt 版本 */
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

    /* ** 获取有效的工具链 Bundle */
    const QList<ToolchainBundle> bundles = Utils::filtered(
        ToolchainBundle::collectBundles(
            ToolchainManager::toolchains([](const Toolchain *tc) {
                return /*tc->isAutoDetected() &&*/ tc->typeId() == Constants::HARMONY_TOOLCHAIN_TYPEID;
            }),
            ToolchainBundle::HandleMissing::CreateAndRegister),
        [](const ToolchainBundle &b) { return b.isCompletelyValid(); });
    qCDebug(harmonyConfigLog) << tr("Found %1 Harmony toolchain bundles").arg(bundles.size());
    QList<Kit *> unhandledKits = existingKits;

    /* ** 为每个工具链 Bundle 和 Qt 版本组合创建 Kit */
    for (const ToolchainBundle &bundle : bundles) {
        const auto &versions = qtVersionsForArch.value(bundle.targetAbi());
        for (const QtVersion *qt : versions) {
            /* ** 检查工具链的 NDK 位置是否与 Qt 版本匹配 */
            const auto tcApiVersion = bundle.get(&HarmonyToolchain::apiVersion);
            const auto expectedNdkPath = bundle.get(&HarmonyToolchain::ndkLocation);
            qCDebug(harmonyConfigLog)
                << tr("Processing Qt %1 for HarmonyOS %2 with NDK %3")
                       .arg(qt->displayName(), bundle.targetAbi().toString(), expectedNdkPath.toUserOutput());
            auto ohQt = static_cast<const HarmonyQtVersion *>(qt);
            if(tcApiVersion.isNull() || ohQt->supportOhVersion() != tcApiVersion) {
                qCDebug(harmonyConfigLog)
                    << tr("Skipping Qt %1 for HarmonyOS %2: unsupported API version %3")
                           .arg(ohQt->displayName(), ohQt->supportOhVersion().toString(), tcApiVersion.toString());
                continue;
            }

            /*
             * 查找是否已存在匹配的Kit
             * 目前暂时只支持一个Harmony Qt版本对应一个Kit
             */
            Kit *existingKit = Utils::findOrDefault(existingKits, [qt](const Kit *k) {
                return (qt == QtKitAspect::qtVersion(k));
            });

            /* ** Kit 初始化函数 */
            const auto initializeKit = [&bundle, expectedNdkPath, ohQt](Kit *k) {

                using namespace CMakeProjectManager;
                auto cmakeConfig = CMakeConfigurationKitAspect::defaultConfiguration(k);
                /* ** CMakeConfig 为 QMap：用 insert，无 append（Qt Creator 19+） */
                cmakeConfig.insert(CMakeConfigItem("CMAKE_TOOLCHAIN_FILE", CMakeConfigItem::FILEPATH,
                                                   HarmonyConfig::toolchainFilePath(expectedNdkPath)
                                                       .toUserOutput()
                                                       .toUtf8()));
                cmakeConfig.insert(
                    CMakeConfigItem("CMAKE_FIND_ROOT_PATH", CMakeConfigItem::PATH,
                                    QByteArrayLiteral("%{Qt:QT_INSTALL_PREFIX}")));
                cmakeConfig.insert(
                    CMakeConfigItem("OHOS_STL", CMakeConfigItem::STRING, QByteArrayLiteral("c++_shared")));
                /*
                ** targetAbi() 若未从 qdevice.pri 解析到，会得到无效 Abi → displayName 为 "unknown"，
                ** ohos.toolchain.cmake 会报 unrecognized unknown。优先用 Qt 版本身 ABI，否则用工具链 Bundle。
                */
                const QByteArray rawOhosArch = [&] {
                    QByteArray a = abiToOhosNdkArchString(ohQt->targetAbi());
                    if (a.isEmpty())
                        a = abiToOhosNdkArchString(bundle.targetAbi());
                    return a;
                }();
                QByteArray ohosArch = normalizeOhosArchForSdkToolchain(rawOhosArch);
                if (rawOhosArch.isEmpty()) {
                    Core::MessageManager::writeFlashing(
                        QObject::tr("Harmony Kit: Could not detect OHOS_ARCH from Qt or toolchain; "
                                    "defaulting CMake variable to %1. If configure still fails, add "
                                    "OHOS_ARCH in Kit CMake configuration or fix Qt mkspecs qdevice.pri.")
                            .arg(QString::fromLatin1(ohosArch)));
                } else if (rawOhosArch != ohosArch) {
                    Core::MessageManager::writeFlashing(
                        QObject::tr("Harmony Kit: OHOS_ARCH value %1 is not valid for OpenHarmony "
                                    "ohos.toolchain.cmake (allowed: arm64-v8a, armeabi-v7a, x86_64). "
                                    "Using %2 instead. Remove conflicting -DOHOS_ARCH from the kit's "
                                    "additional CMake arguments if configure still fails.")
                            .arg(QString::fromLatin1(rawOhosArch), QString::fromLatin1(ohosArch)));
                }
                cmakeConfig.insert(
                    CMakeConfigItem("OHOS_ARCH", CMakeConfigItem::STRING, ohosArch));
                cmakeConfig.insert(
                    CMakeConfigItem("OHOS_PLATFORM", CMakeConfigItem::STRING, QByteArrayLiteral("OHOS")));

                /* ** Unix Makefiles 生成器需要本机 make：PATH 优先（Homebrew 等），再试常见路径。 */
                if (HostOsInfo::isAnyUnixHost()) {
                    const FilePath unixMake = unixHostMakeProgramForCMake();
                    if (!unixMake.isEmpty()) {
                        cmakeConfig.insert(CMakeConfigItem("CMAKE_MAKE_PROGRAM", CMakeConfigItem::FILEPATH,
                                                           unixMake.toUserOutput().toUtf8()));
                    }
                } else if (HostOsInfo::isWindowsHost()) {
                    const FilePath root = HarmonyConfig::makeLocation();
                    if (!root.isEmpty()) {
                        const FilePath mingwMake = root.pathAppended("bin/mingw32-make.exe");
                        if (mingwMake.isExecutableFile()) {
                            cmakeConfig.insert(CMakeConfigItem("CMAKE_MAKE_PROGRAM", CMakeConfigItem::FILEPATH,
                                                               mingwMake.toUserOutput().toUtf8()));
                        }
                    }
                }


                k->setDetectionSource(DetectionSource(
                    DetectionSource::FromSystem, QStringLiteral("HarmonyConfiguration")));
                RunDeviceTypeKitAspect::setDeviceTypeId(k, Constants::HARMONY_DEVICE_TYPE);
                ToolchainKitAspect::setBundle(k, bundle);
                QtKitAspect::setQtVersion(k, ohQt);

                /* ** 设置构建设备为默认桌面设备 */
                BuildDeviceKitAspect::setDeviceId(k, DeviceManager::defaultDesktopDevice()->id());

                CMakeConfigurationKitAspect::setConfiguration(k, cmakeConfig);

                /* ** Windows 上：将 Harmony Qt Kit 适配为使用 MinGW Makefiles（如可能）。 */
                if (HostOsInfo::isWindowsHost()) {
                    using namespace CMakeProjectManager;
                    /* ** 强制生成器为 MinGW Makefiles，使 CMake 使用 mingw32-make */
                    CMakeGeneratorKitAspect::setGenerator(k, QStringLiteral("MinGW Makefiles"));

                    /* ** 若已配置 makeLocation（MinGW 根目录），确保 CMAKE_MAKE_PROGRAM 指向 mingw32-make.exe */
                    const FilePath root = HarmonyConfig::makeLocation();
                    if (!root.isEmpty()) {
                        const FilePath mingwMake = root.pathAppended("bin/mingw32-make.exe");
                        if (mingwMake.isExecutableFile()) {
                            cmakeConfig.insert(CMakeConfigItem("CMAKE_MAKE_PROGRAM", CMakeConfigItem::FILEPATH,
                                                               mingwMake.toUserOutput().toUtf8()));
                            CMakeConfigurationKitAspect::setConfiguration(k, cmakeConfig);
                        }
                    }
                }
                
                /* ** 设置粘性属性，防止用户意外修改关键设置 */
                k->setSticky(QtKitAspect::id(), true);
                k->setSticky(RunDeviceTypeKitAspect::id(), true);
                k->setSticky(ToolchainKitAspect::id(), true);

                /* ** 注册并设置 OHOS LLDB 调试器 */
                const QVariant debuggerId = findOrRegisterOhosDebugger(expectedNdkPath);
                if (!debuggerId.isNull())
                    Debugger::DebuggerKitAspect::setDebugger(k, debuggerId);

                /* ** 设置显示名称 */
                QString versionStr = QLatin1String("Qt %{Qt:Version}");
                if (!ohQt->detectionSource().isAutoDetected())
                    versionStr = QString("%1").arg(ohQt->displayName());
                
                k->setUnexpandedDisplayName(QObject::tr("HarmonyOS%1 %2 Clang %3")
                                                .arg(ohQt->supportOhVersion().toString(),
                                                     versionStr,
                                                     HarmonyConfig::displayName(ohQt->targetAbi())));

                /* ** 设置 NDK 和 SDK 路径 */
                k->setValueSilently(Constants::HARMONY_KIT_NDK, expectedNdkPath.toSettings());
                k->setValueSilently(Constants::HARMONY_KIT_SDK, ohQt->qmakeFilePath().toSettings());
                k->setValueSilently(Constants::HARMONY_KIT_MODULE_DEVICE_TYPES,
                                    QVariant::fromValue(HarmonyConfig::ohModuleDeviceTypes()));
            };

            if (existingKit) {
                /* ** 更新现有 Kit */
                initializeKit(existingKit);
                unhandledKits.removeOne(existingKit);
            } else {
                /* ** 注册新 Kit */
                KitManager::registerKit(initializeKit);
            }
        }
    }

    /* ** 清理不再使用的 Kit */
    KitManager::deregisterKits(unhandledKits);
}

void setupHarmonyConfigurations()
{
    qCDebug(harmonyConfigLog) << QObject::tr("Setting up Harmony configurations...");
    static HarmonyConfigurations harmonyConfigurations;
}

} // namespace Ohos::Internal
