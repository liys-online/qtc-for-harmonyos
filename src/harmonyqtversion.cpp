#include "harmonyqtversion.h"
#include "harmonyconfigurations.h"
#include "ohosconstants.h"
#include <utils/filepath.h>
#include <utils/environment.h>
#include "ohostr.h"

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>
using namespace Utils;
namespace Ohos::Internal {
HarmonyQtVersion::HarmonyQtVersion() {}

bool HarmonyQtVersion::supportsMultipleQtAbis() const
{
    return qtAbis().size() > 1;
}

void HarmonyQtVersion::addToBuildEnvironment(const ProjectExplorer::Kit *k, Utils::Environment &env) const
{
    QtVersion::addToBuildEnvironment(k, env);
    const QStringList sdkList = HarmonyConfig::getSdkList();
    if (!sdkList.isEmpty())
    {
        for(const QString &sdk : sdkList)
        {
            FilePath releaseFile = HarmonyConfig::releaseFile(
                HarmonyConfig::ndkLocation(FilePath::fromUserInput(sdk).cleanPath()));
            if(HarmonyConfig::getVersion(releaseFile).first == supportOhVersion())
            {
                env.set(Constants::OHOS_SDK_ENV_VAR, sdk);
            }
        }
    }
    // 自动注入 JAVA_HOME（DevEco JBR → JAVA_HOME 环境变量 → macOS java_home → PATH），
    // 使 CMake/hvigor/终端构建无需用户手工配置。
    const FilePath javaHome = HarmonyConfig::javaLocation();
    const FilePath javaExe = (javaHome / "bin" / "java").withExecutableSuffix();
    if (!javaHome.isEmpty() && javaExe.isExecutableFile()) {
        env.set(QStringLiteral("JAVA_HOME"), javaHome.toFSPathString());
        env.appendOrSetPath({javaHome / "bin"});
    }
}

QSet<Id> HarmonyQtVersion::targetDeviceTypes() const
{
    return {Id(Constants::HARMONY_DEVICE_TYPE)};
}

QString HarmonyQtVersion::description() const
{
    return Tr::tr("HarmonyOS");
}

QString HarmonyQtVersion::defaultUnexpandedDisplayName() const
{
    return Tr::tr("Qt %{Qt:Version} for HarmonyOS");
}

QVersionNumber HarmonyQtVersion::supportOhVersion() const
{
    if (FilePath qconfigHeader = headerPath().pathAppended(Constants::Q_CONFIG_H); qconfigHeader.exists())
    {
        if (QFile qconfigFile(qconfigHeader.toFSPathString()); qconfigFile.open(QIODevice::ReadOnly))
        {
            QTextStream in(&qconfigFile);
            while (!in.atEnd())
            {
                if (const QString line = in.readLine(); line.contains(Constants::OHOS_SDK_VERSION))
                    return QVersionNumber::fromString(line.simplified().split(' ').last());
            }
            qconfigFile.close();
        }
    }
    return QVersionNumber();
}

static QString toOhosAbi(ProjectExplorer::Abi abi)
{
    if (abi.architecture() == ProjectExplorer::Abi::Architecture::ArmArchitecture) {
        if (abi.wordWidth() == 32)
            return Constants::HARMONY_ABI_ARMEABI_V7A;
        if (abi.wordWidth() == 64)
            return Constants::HARMONY_ABI_ARM64_V8A;
    } else if (abi.architecture() == ProjectExplorer::Abi::Architecture::X86Architecture) {
        if (abi.wordWidth() == 32)
            return Constants::HARMONY_ABI_X86;
        if (abi.wordWidth() == 64)
            return Constants::HARMONY_ABI_X86_64;
    }
    return {};
}
const QStringList HarmonyQtVersion::ohosAbis() const
{
    auto abis = detectQtAbis();
    QStringList result;
    for(const auto &abi : std::as_const(abis))
    {
        result << toOhosAbi(abi);
    }
    return result;
}

ProjectExplorer::Abi HarmonyQtVersion::targetAbi() const
{
    const FilePath qdevicepri = mkspecsPath().pathAppended(Constants::Q_DEVICE_PRI);
    if (!qdevicepri.exists())
        return {};

    QFile qconfigFile(qdevicepri.toFSPathString());
    if (!qconfigFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    // 旧实现：任意「包含 OHOS_ARCH 子串」的行都取最后一个词。会误匹配注释，例如
    // 「# OHOS_ARCH unknown」→ 得到 unknown → abi() 失败 → 无效 Abi；若别处再把
    // displayName/错误字符串写进 CMake，就会出现 ohos.toolchain.cmake 的 unrecognized unknown。
    static const QRegularExpression assignRe(
        QStringLiteral(R"(OHOS_ARCH\s*[=:]\s*(\S+))"),
        QRegularExpression::CaseInsensitiveOption);

    QTextStream in(&qconfigFile);
    while (!in.atEnd()) {
        const QString trimmed = in.readLine().trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
            continue;
        if (!trimmed.contains(QLatin1String(Constants::OHOS_ARCH), Qt::CaseInsensitive))
            continue;

        const QRegularExpressionMatch m = assignRe.match(trimmed);
        if (m.hasMatch()) {
            const QByteArray token = m.captured(1).toLatin1();
            qconfigFile.close();
            return HarmonyConfig::abi(QLatin1String(token.constData(), token.size()));
        }

        // 无等号：「OHOS_ARCH arm64-v8a」；要求首词即为 OHOS_ARCH，避免匹配 DEFINES 等行。
        const QStringList parts = trimmed.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() >= 2
            && parts.constFirst().compare(QString::fromLatin1(Constants::OHOS_ARCH),
                                          Qt::CaseInsensitive) == 0) {
            const QByteArray token = parts.constLast().toLatin1();
            qconfigFile.close();
            return HarmonyConfig::abi(QLatin1String(token.constData(), token.size()));
        }
    }
    qconfigFile.close();
    return {};
}

// Factory

class HarmonyQtVersionFactory : public QtSupport::QtVersionFactory
{
public:
    HarmonyQtVersionFactory()
    {
        setQtVersionCreator([] { return new HarmonyQtVersion; });
        setSupportedType(Constants::HARMONY_QT_TYPE);
        setPriority(90);

        setRestrictionChecker([](const SetupData &setup) {
            return !setup.config.contains("openharmony-no-sdk")
            && (setup.config.contains("openharmony")
               || setup.platforms.contains("openharmony"));
        });
    }
};

void setupHarmonyQtVersion()
{
    static HarmonyQtVersionFactory factory;
}

} // Ohos::Internal
