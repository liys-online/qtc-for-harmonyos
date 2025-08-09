#include "harmonytoolchain.h"
#include "ohosconstants.h"
#include "ohostr.h"
#include "harmonyconfigurations.h"
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/toolchainmanager.h>
#include <projectexplorer/toolchainconfigwidget.h>
#include <coreplugin/messagemanager.h>
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

bool HarmonyToolchain::isValid() const
{
    // 确保NDK位置被正确推断（如果还没有设置的话）
    ndkLocation(); // 这会触发自动推断逻辑
    
    const bool isChildofNdk = compilerCommand().isChildOf(m_ndkLocation);
    const bool hasValidNdk = !m_ndkLocation.isEmpty() && m_ndkLocation.exists();
    
    return Toolchain::isValid()
           && typeId() == Constants::HARMONY_TOOLCHAIN_TYPEID
           && targetAbi().isValid() 
           && isChildofNdk 
           && hasValidNdk
           && !originalTargetTriple().isEmpty();
}

void HarmonyToolchain::setNdkLocation(const Utils::FilePath &ndkLocation)
{
    m_ndkLocation = ndkLocation;
}

QVersionNumber HarmonyToolchain::apiVersion() const
{
    if (m_apiVersion.isNull()) {
        m_apiVersion = HarmonyConfig::getVersion(HarmonyConfig::releaseFile(m_ndkLocation)).first;
    }
    return m_apiVersion;
}

FilePath HarmonyToolchain::ndkLocation() const
{
    // 如果NDK位置为空，尝试从编译器路径推断
    if (m_ndkLocation.isEmpty()) {
        const QString compilerPath = compilerCommand().toFSPathString();
        
        // HarmonyOS NDK的典型路径结构：.../native/llvm/bin/clang
        QStringList ndkParts = compilerPath.split("/native/llvm/");
        if (ndkParts.size() > 1) {
            QString inferredNdkLocation = ndkParts.first() + "/native";
            m_ndkLocation = FilePath::fromString(inferredNdkLocation);
        } else {
            // 备用方案：尝试其他可能的路径模式
            ndkParts = compilerPath.split("/llvm/");
            if (ndkParts.size() > 1) {
                // 检查是否在 native 目录下
                QString basePath = ndkParts.first();
                if (basePath.endsWith("/native")) {
                    m_ndkLocation = FilePath::fromString(basePath);
                }
            }
        }
    }
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
