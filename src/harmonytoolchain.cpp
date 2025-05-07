#include "harmonytoolchain.h"
#include "ohosconstants.h"
#include "ohostr.h"
#include "harmonyconfigurations.h"
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/toolchainmanager.h>
#include <projectexplorer/toolchainconfigwidget.h>
#include <QLoggingCategory>
using namespace Utils;
using namespace ProjectExplorer;
namespace Ohos::Internal {
static Q_LOGGING_CATEGORY(harmonyTCLog, "qtc.android.toolchainmanagement", QtWarningMsg);
using ClangTargetsType = QHash<QString, Abi>;
const ClangTargetsType &clangTargets()
{
    static const ClangTargetsType targets {
        // {"arm-linux-harmonyabi",
        //  Abi(Abi::ArmArchitecture, Abi::LinuxOS, Abi::GenericFlavor, Abi::ElfFormat, 32)},
        // {"i686-linux-harmony",
        //  Abi(Abi::X86Architecture, Abi::LinuxOS, Abi::GenericFlavor, Abi::ElfFormat, 32)},
        {"x86_64-linux-harmony",
         Abi(Abi::X86Architecture, Abi::LinuxOS, Abi::GenericFlavor, Abi::ElfFormat, 64)},
        {"aarch64-linux-harmony",
         Abi(Abi::ArmArchitecture, Abi::LinuxOS, Abi::GenericFlavor, Abi::ElfFormat, 64)}
    };
    return targets;
}

static Toolchain *findToolchain(const FilePath &compilerPath, Id lang, const QString &target,
                                const ToolchainList &alreadyKnown)
{
    Toolchain *tc = Utils::findOrDefault(alreadyKnown, [target, compilerPath, lang](Toolchain *tc) {
        return tc->typeId() == Constants::HARMONY_TOOLCHAIN_TYPEID
               && tc->language() == lang
               && tc->targetAbi() == clangTargets().value(target)
               && tc->compilerCommand() == compilerPath;
    });
    return tc;
}

HarmonyToolchain::HarmonyToolchain()
    : GccToolchain(Constants::HARMONY_TOOLCHAIN_TYPEID, Clang)
{
    setTypeDisplayName(Tr::tr("Harmony Clang"));

}

void HarmonyToolchain::addToEnvironment(Utils::Environment &env) const
{
    const FilePath sdkLocation = ndkLocation().parentDir();
    if (sdkLocation.exists())
    {
        env.set(Constants::OHOS_SDK_ENV_VAR, sdkLocation.toUserOutput());
    }
}

QStringList HarmonyToolchain::suggestedMkspecList() const
{
    return {"oh-clang++", "oh-clang"};
}

void HarmonyToolchain::setNdkLocation(const Utils::FilePath &ndkLocation)
{
    m_ndkLocation = ndkLocation;
}

FilePath HarmonyToolchain::ndkLocation() const
{
    return m_ndkLocation;
}

FilePath HarmonyToolchain::makeCommand(const Utils::Environment &environment) const
{
    const FilePath makePath = HarmonyConfig::makeLocation();
    if (makePath.isEmpty())
        return FilePath();
    FilePath makeExecutable = makePath / "bin" / "mingw32-make.exe";
    if (makeExecutable.isExecutableFile())
        return makeExecutable;
    return {};
}

ToolchainList autodetectToolchains(const ToolchainList &alreadyKnown)
{
    const QList<FilePath> ndkLocations = HarmonyConfig::ndkLocations();
    return autodetectToolchainsFromNdk(alreadyKnown, ndkLocations);
}

class HarmonyToolchainFactory final : public ToolchainFactory
{
public:
    HarmonyToolchainFactory()
    {
        setDisplayName(Tr::tr("Harmony Clang"));
        setSupportedToolchainType(Constants::HARMONY_TOOLCHAIN_TYPEID);
        setSupportedLanguages(
            {ProjectExplorer::Constants::C_LANGUAGE_ID,
             ProjectExplorer::Constants::CXX_LANGUAGE_ID});
        setToolchainConstructor([] { return new HarmonyToolchain; });
    }
private:
    std::unique_ptr<ToolchainConfigWidget> createConfigurationWidget(
        const ToolchainBundle &bundle) const final
    {
        return GccToolchain::createConfigurationWidget(bundle);
    }

    FilePath correspondingCompilerCommand(const FilePath &srcPath, Id targetLang) const override
    {
        return GccToolchain::correspondingCompilerCommand(srcPath, targetLang, "clang", "clang++");
    }
};

static FilePath clangPlusPlusPath(const FilePath &clangPath)
{
    return clangPath.parentDir().pathAppended(clangPath.baseName() + "++").withExecutableSuffix();
}

ToolchainList autodetectToolchainsFromNdk(const ToolchainList &alreadyKnown,
                                          const QList<FilePath> &ndkLocations,
                                          const bool isCustom)
{
    QList<Toolchain *> newToolchains;

    const Id LanguageIds[] {
        ProjectExplorer::Constants::CXX_LANGUAGE_ID,
        ProjectExplorer::Constants::C_LANGUAGE_ID
    };

    for (const FilePath &ndkLocation : ndkLocations)
    {
        const FilePath clangPath = HarmonyConfig::clangPathFromNdk(ndkLocation);
        if (!clangPath.exists())
        {
            qCDebug(harmonyTCLog) << "Clang toolchains detection fails. Can not find Clang"
                                  << clangPath;
        }
        for (const Id &lang : LanguageIds)
        {
            FilePath compilerCommand = clangPath;
            if (lang == ProjectExplorer::Constants::CXX_LANGUAGE_ID)
                compilerCommand = clangPlusPlusPath(clangPath);

            if (!compilerCommand.exists())
            {
                qCDebug(harmonyTCLog)
                << "Skipping Clang toolchain. Can not find compiler" << compilerCommand;
                continue;
            }

            auto targetItr = clangTargets().constBegin();
            while (targetItr != clangTargets().constEnd())
            {
                const Abi &abi = targetItr.value();
                const QString target = targetItr.key();
                Toolchain *tc = findToolchain(compilerCommand, lang, target, alreadyKnown);

                const QString customStr = isCustom ? "Custom " : QString();
                QPair<QVersionNumber, QVersionNumber> versionPair = HarmonyConfig::getVersion(HarmonyConfig::releaseFile(ndkLocation));
                const QString displayName(customStr + QString("Harmony Clang (%1, %2, API %3 NDK %4)")
                                                          .arg(ToolchainManager::displayNameOfLanguageId(lang),
                                                               HarmonyConfig::displayName(abi),
                                                               versionPair.first.toString(),
                                                               versionPair.second.toString()));
                if(tc)
                {
                    if (tc->displayName() != displayName)
                    {
                        tc->setDisplayName(displayName);
                    }
                }
                else
                {
                    qCDebug(harmonyTCLog) << "New Clang toolchain found" << abi.toString() << lang
                                          << "for NDK" << ndkLocation;
                    auto atc = new HarmonyToolchain();
                    atc->setNdkLocation(ndkLocation);
                    atc->setOriginalTargetTriple(target);
                    atc->setLanguage(lang);
                    atc->setTargetAbi(clangTargets().value(target));
                    atc->setPlatformCodeGenFlags({"-target", target});
                    atc->setPlatformLinkerFlags({"-target", target});
                    atc->setDisplayName(displayName);
                    tc = atc;
                    newToolchains << tc;
                }
                if (auto gccTc = dynamic_cast<GccToolchain*>(tc))
                    gccTc->resetToolchain(compilerCommand);

                tc->setDetection(Toolchain::ManualDetection);
                ++targetItr;
            }
        }
    }

    return newToolchains;
}

void setupHarmonyToolchain()
{
    static HarmonyToolchainFactory harmonyToolchainFactory;
}

} // namespace Ohos
