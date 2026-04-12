#include "harmonytoolchain.h"
#include "harmonylogcategories.h"
#include "ohosconstants.h"
#include "ohostr.h"
#include "harmonyconfigurations.h"
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/kitaspect.h>
#include <projectexplorer/toolchainmanager.h>
#include <projectexplorer/toolchainconfigwidget.h>
#include <projectexplorer/clangparser.h>

#include <utils/hostosinfo.h>
#include <utils/qtcprocess.h>

#include <QBuffer>

using namespace Utils;
using namespace ProjectExplorer;
namespace Ohos::Internal {
using ClangTargetsType = QHash<QString, Abi>;

class WarningFlagAdder
{
public:
    WarningFlagAdder(const QString &flag, WarningFlags &flags) :
        m_flags(flags)
    {
        if (!flag.startsWith("-W")) {
            m_triggered = true;
            return;
        }

        m_doesEnable = !flag.startsWith("-Wno-");
        if (m_doesEnable)
            m_flagUtf8 = flag.mid(2).toUtf8();
        else
            m_flagUtf8 = flag.mid(5).toUtf8();
    }

    void operator()(const char name[], WarningFlags flagsSet)
    {
        if (m_triggered)
            return;
        if (0 == strcmp(m_flagUtf8.data(), name)) {
            m_triggered = true;
            if (m_doesEnable)
                m_flags |= flagsSet;
            else
                m_flags &= ~flagsSet;
        }
    }

    bool triggered() const
    {
        return m_triggered;
    }

private:
    QByteArray m_flagUtf8;
    WarningFlags &m_flags;
    bool m_doesEnable = false;
    bool m_triggered = false;
};

const ClangTargetsType &clangTargets()
{
    static const ClangTargetsType targets {
        /* ** {"armv7-linux-ohos",
        **  Abi(Abi::ArmArchitecture, Abi::LinuxOS, Abi::GenericFlavor, Abi::ElfFormat, 32)}, */
        {"x86_64-linux-ohos",
         Abi(Abi::X86Architecture, Abi::LinuxOS, Abi::GenericFlavor, Abi::ElfFormat, 64)},
        {"aarch64-linux-ohos",
         Abi(Abi::ArmArchitecture, Abi::LinuxOS, Abi::GenericFlavor, Abi::ElfFormat, 64)}
    };
    return targets;
}

static const QStringList languageOption(Id languageId)
{
    if (languageId == ProjectExplorer::Constants::C_LANGUAGE_ID)
        return {"-x", "c"};
    return {"-x", "c++"};
}

// const QStringList gccPredefinedMacrosOptions(Id languageId)
// {
//     return languageOption(languageId) + QStringList({"-E", "-dM"});
// }

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

const char compilerPlatformCodeGenFlagsKeyC[] = "ProjectExplorer.HarmonyToolChain.PlatformCodeGenFlags";
const char compilerPlatformLinkerFlagsKeyC[] = "ProjectExplorer.HarmonyToolChain.PlatformLinkerFlags";
const char targetAbiKeyC[] = "ProjectExplorer.HarmonyToolChain.TargetAbi";
const char originalTargetTripleKeyC[] = "ProjectExplorer.HarmonyToolChain.OriginalTargetTriple";
const char supportedAbisKeyC[] = "ProjectExplorer.HarmonyToolChain.SupportedAbis";
const char parentToolchainIdKeyC[] = "ProjectExplorer.ClangToolChain.ParentToolChainId";
const char priorityKeyC[] = "ProjectExplorer.ClangToolChain.Priority";
const char binaryRegexp[] = "(?:^|-|\\b)(?:gcc|g\\+\\+|clang(?:\\+\\+)?)(?:-([\\d.]+))?$";

static Result<QString> runGcc(
    const FilePath &gcc, const QStringList &arguments, const Environment &env)
{
    if (!gcc.isExecutableFile())
        return make_unexpected(QString("Compiler '%1' not found.").arg(gcc.toUserOutput()));

    Process cpp;
    Environment environment(env);
    environment.setupEnglishOutput();

    cpp.setEnvironment(environment);
    cpp.setCommand({gcc, arguments});
    cpp.runBlocking();
    if (cpp.result() != ProcessResult::FinishedWithSuccess || cpp.exitCode() != 0) {
        return make_unexpected(
            QString("Compiler feature detection failure.\n%1").arg(cpp.verboseExitMessage()));
    }

    return cpp.allOutput().trimmed();
}

static Result<ProjectExplorer::Macros> gccPredefinedMacros(
    const FilePath &gcc, const QStringList &args, const Environment &env)
{
    QStringList arguments = args;
    arguments << "-";

    Result<QString> result = runGcc(gcc, arguments, env);
    if (!result)
        return make_unexpected(result.error());

    ProjectExplorer::Macros predefinedMacros = Macro::toMacros(result->toUtf8());
    /* ** 健康性检查：防止得到的是错误消息而非真实输出 */
    QTC_CHECK(predefinedMacros.isEmpty()
              || predefinedMacros.front().type == ProjectExplorer::MacroType::Define);
    if (HostOsInfo::isMacHost()) {
        /* ** 关闭表示 Apple blocks 支持的标志 */
        const ProjectExplorer::Macro blocksDefine("__BLOCKS__", "1");
        const ProjectExplorer::Macro blocksUndefine("__BLOCKS__", ProjectExplorer::MacroType::Undefine);
        const int idx = predefinedMacros.indexOf(blocksDefine);
        if (idx != -1)
            predefinedMacros[idx] = blocksUndefine;

        /* ** 将 Apple GC 扩展中使用的 __strong 和 __weak 定义为空 */
        predefinedMacros.append({"__strong"});
        predefinedMacros.append({"__weak"});
    }
    return predefinedMacros;
}

static void appendHeaderPath(const FilePath &gcc,
                             QByteArray line,
                             HeaderPathType kind,
                             HeaderPaths &paths)
{
    line = line.trimmed();
    const int index = line.indexOf(" (framework directory)");
    if (index != -1) {
        line.truncate(index);
        kind = HeaderPathType::Framework;
    }
    const FilePath headerPath = gcc.withNewPath(QString::fromUtf8(line)).canonicalPath();
    if (!headerPath.isEmpty())
        paths.append({headerPath, kind});
}

static HeaderPaths gccHeaderPaths(const FilePath &gcc,
                                  const QStringList &arguments,
                                  const Environment &env)
{
    Result<QString> result = runGcc(gcc, arguments, env);
    QTC_ASSERT_RESULT(result, return {});

    HeaderPaths builtInHeaderPaths;
    QByteArray line;
    QByteArray data = result->toUtf8();
    QBuffer cpp(&data);
    cpp.open(QIODevice::ReadOnly);
    while (cpp.canReadLine()) {
        line = cpp.readLine();
        if (line.startsWith("#include"))
            break;
    }

    if (!line.isEmpty() && line.startsWith("#include")) {
        auto kind = HeaderPathType::User;
        while (cpp.canReadLine()) {
            line = cpp.readLine();
            if (line.startsWith("#include")) {
                kind = HeaderPathType::BuiltIn;
            } else if (!line.isEmpty() && QChar(line.at(0)).isSpace()) {
                appendHeaderPath(gcc, line, kind, builtInHeaderPaths);
            } else if (line.startsWith("End of search list.")) {
                break;
            } else {
                qWarning("%s: Ignoring line: %s", __FUNCTION__, line.constData());
            }
        }
    }
    return builtInHeaderPaths;
}

HarmonyToolchain::HarmonyToolchain()
    : Toolchain(Constants::HARMONY_TOOLCHAIN_TYPEID)
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

void HarmonyToolchain::toMap(Utils::Store &data) const
{
    Toolchain::toMap(data);
    data.insert(compilerPlatformCodeGenFlagsKeyC, m_platformCodeGenFlags);
    data.insert(compilerPlatformLinkerFlagsKeyC, m_platformLinkerFlags);
    data.insert(originalTargetTripleKeyC, m_originalTargetTriple);
    data.insert(supportedAbisKeyC, Utils::transform<QStringList>(m_supportedAbis, &Abi::toString));

    data.insert(parentToolchainIdKeyC, m_parentToolchainId);
    data.insert(priorityKeyC, m_priority);
}

void HarmonyToolchain::fromMap(const Utils::Store &data)
{
    Toolchain::fromMap(data);
    if (hasError())
        return;

    m_platformCodeGenFlags = data.value(compilerPlatformCodeGenFlagsKeyC).toStringList();
    m_platformLinkerFlags = data.value(compilerPlatformLinkerFlagsKeyC).toStringList();
    m_originalTargetTriple = data.value(originalTargetTripleKeyC).toString();
    /* ** clang 19 不再接受 "--target=unknown-qnx-gnu"
    **   error: unknown target triple 'unknown-qnx-unknown-gnu' */
    if (m_originalTargetTriple == "unknown-qnx-gnu")
        m_originalTargetTriple.clear();
    const QStringList abiList = data.value(supportedAbisKeyC).toStringList();
    m_supportedAbis.clear();
    for (const QString &a : abiList)
        m_supportedAbis.append(Abi::fromString(a));

    const QString targetAbiString = data.value(targetAbiKeyC).toString();
    if (targetAbiString.isEmpty())
        resetToolchain(compilerCommand());

    m_parentToolchainId = data.value(parentToolchainIdKeyC).toByteArray();
    m_priority = data.value(priorityKeyC, PriorityNormal).toInt();
    syncAutodetectedWithParentToolchains();
}

void HarmonyToolchain::resetToolchain(const FilePath &path)
{
    bool resetDisplayName = (displayName() == defaultDisplayName());

    setCompilerCommand(path);

    const Abi currentAbi = targetAbi();
    const DetectedAbisResult detectedAbis = detectSupportedAbis();
    m_supportedAbis = detectedAbis.supportedAbis;
    m_originalTargetTriple = detectedAbis.originalTargetTriple;
    m_installDir.clear();

    if (m_supportedAbis.isEmpty())
        setTargetAbiNoSignal(Abi());
    else if (!m_supportedAbis.contains(currentAbi))
        setTargetAbiNoSignal(m_supportedAbis.at(0));

    if (resetDisplayName)
        setDisplayName(defaultDisplayName()); // calls toolChainUpdated()!
    else
        toolChainUpdated();
}

bool HarmonyToolchain::isValid() const
{
    /* ** 确保 NDK 位置被正确推断（如果还没有设置的话） */
    ndkLocation(); /* ** 这会触发自动推断逻辑 */
    
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

void HarmonyToolchain::setPlatformCodeGenFlags(const QStringList &flags)
{
    if (flags != m_platformCodeGenFlags) {
        m_platformCodeGenFlags = flags;
        toolChainUpdated();
    }
}

QStringList HarmonyToolchain::platformCodeGenFlags() const
{
    return m_platformCodeGenFlags;
}

void HarmonyToolchain::setPlatformLinkerFlags(const QStringList &flags)
{
    if (flags != m_platformLinkerFlags) {
        m_platformLinkerFlags = flags;
        toolChainUpdated();
    }
}

/*
** 用于 查询操作（如 -dM）
*/
static QStringList filteredFlags(const QStringList &allFlags, bool considerSysroot)
{
    QStringList filtered;
    for (int i = 0; i < allFlags.size(); ++i) {
        const QString &a = allFlags.at(i);
        if (a.startsWith("--gcc-toolchain=")) {
            filtered << a;
        } else if (a == "-arch") {
            if (++i < allFlags.length() && !filtered.contains(a))
                filtered << a << allFlags.at(i);
        }  else if (a == "-Xclang") {
            filtered << a;
            continue;
        } else if ((considerSysroot && (a == "--sysroot" || a == "-isysroot"))
                   || a == "-D" || a == "-U"
                   || a == "-gcc-toolchain" || a == "-target" || a == "-mllvm" || a == "-isystem") {
            if (++i < allFlags.length())
                filtered << a << allFlags.at(i);
        } else if (a.startsWith("-m")
                   || (a.startsWith("-f") && !a.startsWith("-fcolor") && !a.startsWith("-fno-color"))
                   || a.startsWith("-O")
                   || a.startsWith("-std=") || a.startsWith("-stdlib=")
                   || a.startsWith("-specs=") || a == "-ansi" || a == "-undef"
                   || a.startsWith("-D") || a.startsWith("-U")
                   || a.startsWith("-stdlib=") || a.startsWith("-B")
                   || a.startsWith("--target=")
                   || (a.startsWith("-isystem") && a.length() > 8)
                   || a == "-Wno-deprecated"
                   || a == "-nostdinc" || a == "-nostdinc++") {
            filtered << a;
        }
        if (!filtered.isEmpty() && filtered.last() == "-Xclang")
            filtered.removeLast();
    }
    return filtered;
}

static bool isNetworkCompiler(const QString &dirPath)
{
    return dirPath.contains("icecc") || dirPath.contains("distcc");
}

static FilePath findLocalCompiler(const FilePath &compilerPath, const Environment &env)
{
    /* ** 如果使用了 icecc/distcc 等网络编译器，查找真实编译器；ccache 已是本地的，不需处理。 */

    /* ** 获取编译器路径，忽略对 icecc/distcc 的直接调用（我们无法处理那种情况）。 */
    if (!isNetworkCompiler(compilerPath.parentDir().path()))
        return compilerPath;

    /* ** 过滤掉网络编译器 */
    const FilePaths pathComponents = Utils::filtered(env.path(), [] (const FilePath &dirPath) {
        return !isNetworkCompiler(dirPath.path());
    });

    /* ** 该操作实际搜索 PATH 两次（分别通过 pathComponents 和 PATH 本身）；
    ** searchInPath 会过滤重复项，因此不会有副作用。 */
    const FilePath path = env.searchInPath(compilerPath.fileName(), pathComponents);

    return path.isEmpty() ? compilerPath : path;
}

void HarmonyToolchain::setOptionsReinterpreter(const OptionsReinterpreter &optionsReinterpreter)
{
    m_optionsReinterpreter = optionsReinterpreter;
}

static QStringList gccPrepareArguments(const QStringList &flags,
                                       const FilePath &sysRoot,
                                       const QStringList &platformCodeGenFlags,
                                       Id languageId)
{
    QStringList arguments;
    const bool hasKitSysroot = !sysRoot.isEmpty();
    if (hasKitSysroot)
        arguments.append(QString("--sysroot=%1").arg(sysRoot.nativePath()));

    QStringList allFlags;
    allFlags << platformCodeGenFlags << flags;
    arguments += filteredFlags(allFlags, !hasKitSysroot);
    arguments << languageOption(languageId) << "-E" << "-v" << "-";

    return arguments;
}

void HarmonyToolchain::initExtraHeaderPathsFunction(ExtraHeaderPathsFunction &&extraHeaderPathsFunction) const
{
    m_extraHeaderPathsFunction = std::move(extraHeaderPathsFunction);
}

void HarmonyToolchain::setOriginalTargetTriple(const QString &targetTriple)
{
    if (targetTriple != m_originalTargetTriple) {
        m_originalTargetTriple = targetTriple;
        toolChainUpdated();
    }
}

QString HarmonyToolchain::originalTargetTriple() const
{
    return m_originalTargetTriple;
}

QList<OutputLineParser *> HarmonyToolchain::createOutputParsers() const
{
    return ClangParser::clangParserSuite();
}

template<typename ReinterpretOptions, typename ExtraHeaderPathsFunction>
static HeaderPaths builtInHeaderPaths(const Environment &env,
                                      const FilePath &compilerCommand,
                                      const QStringList &platformCodeGenFlags,
                                      ReinterpretOptions reinterpretOptions,
                                      GccToolchain::HeaderPathsCache headerCache,
                                      Id languageId,
                                      ExtraHeaderPathsFunction extraHeaderPathsFunction,
                                      const QStringList &flags,
                                      const FilePath &sysRoot,
                                      const QString &originalTargetTriple)
{
    QStringList arguments = gccPrepareArguments(flags,
                                                sysRoot,
                                                platformCodeGenFlags,
                                                languageId);
    arguments = reinterpretOptions(arguments);

    /* ** 仅限 clang 情况 */
    if (!originalTargetTriple.isEmpty())
        arguments << "-target" << originalTargetTriple;

    const std::optional<HeaderPaths> cachedPaths = headerCache->check({env, arguments});
    if (cachedPaths)
        return *cachedPaths;

    HeaderPaths paths = gccHeaderPaths(findLocalCompiler(compilerCommand, env),
                                       arguments,
                                       env);
    extraHeaderPathsFunction(paths);
    headerCache->insert({env, arguments}, paths);

    qCDebug(harmonyToolchainLog) << "Reporting header paths to code model:";
    for (const HeaderPath &hp : std::as_const(paths)) {
        qCDebug(harmonyToolchainLog) << compilerCommand.toUserOutput()
        << (languageId == ProjectExplorer::Constants::CXX_LANGUAGE_ID ? ": C++ [" : ": C [")
        << arguments.join(", ") << "]" << hp.path;
    }

    return paths;
}

Toolchain::BuiltInHeaderPathsRunner HarmonyToolchain::createBuiltInHeaderPathsRunner(
    const Utils::Environment &env) const
{
    /* ** 使用干净环境会破坏 ccache/distcc 等工具。 */
    Environment fullEnv = env;
    addToEnvironment(fullEnv);

        /* ** 此 runner 必须是线程安全的！ */
        return [fullEnv,
                compilerCommand = compilerCommand(),
                platformCodeGenFlags = m_platformCodeGenFlags,
                reinterpretOptions = m_optionsReinterpreter,
                headerCache = headerPathsCache(),
                languageId = language(),
                extraHeaderPathsFunction = m_extraHeaderPathsFunction](const QStringList &flags,
                                                                       const Utils::FilePath &sysRoot,
                                                                       const QString &target) {
            return builtInHeaderPaths(fullEnv,
                                      compilerCommand,
                                      platformCodeGenFlags,
                                      reinterpretOptions,
                                      headerCache,
                                      languageId,
                                      extraHeaderPathsFunction,
                                      flags,
                                      sysRoot,
                                      target);
        };
}
const QStringList gccPredefinedMacrosOptions(Id languageId)
{
    return languageOption(languageId) + QStringList({"-E", "-dM"});
}
Toolchain::MacroInspectionRunner HarmonyToolchain::createMacroInspectionRunner() const
{
    /* ** 使用干净环境会破坏 ccache/distcc 等工具。 */
    Environment env = compilerCommand().deviceEnvironment();
    addToEnvironment(env);
    const QStringList platformCodeGenFlags = m_platformCodeGenFlags;
    OptionsReinterpreter reinterpretOptions = m_optionsReinterpreter;
    QTC_CHECK(reinterpretOptions);
    MacrosCache macroCache = predefinedMacrosCache();
    Id lang = language();

    /*
     * Asks compiler for set of predefined macros
     * flags are the compiler flags collected from project settings
     * returns the list of defines, one per line, e.g. "#define __GXX_WEAK__ 1"
     * Note: changing compiler flags sometimes changes macros set, e.g. -fopenmp
     * adds _OPENMP macro, for full list of macro search by word "when" on this page:
     * http://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html
     *
     * This runner must be thread-safe!
     */
    using namespace ProjectExplorer::Internal;
    return [this, env, platformCodeGenFlags, reinterpretOptions, macroCache, lang] (const auto &flags) {
        QStringList allFlags = platformCodeGenFlags + flags;  // add only cxxflags is empty?
        QStringList arguments = gccPredefinedMacrosOptions(lang) + filteredFlags(allFlags, true);
        arguments = reinterpretOptions(arguments);
        const std::optional<MacroInspectionReport> cachedMacros = macroCache->check(arguments);
        if (cachedMacros)
            return *cachedMacros;

        const Result<Macros> macroResult
            = gccPredefinedMacros(findLocalCompiler(compilerCommand(), env), arguments, env);
        QTC_CHECK_RESULT(macroResult);

        const auto macros = macroResult.value_or(Macros{});

        const auto report = MacroInspectionReport{macros, languageVersion(lang, macros)};
        macroCache->insert(arguments, report);

        qCDebug(harmonyToolchainLog) << "MacroInspectionReport for code model:";
        qCDebug(harmonyToolchainLog) << "Language version:" << static_cast<int>(report.languageVersion);
        for (const Macro &m : macros) {
            qCDebug(harmonyToolchainLog) << compilerCommand().toUserOutput()
            << (lang == ProjectExplorer::Constants::CXX_LANGUAGE_ID ? ": C++ [" : ": C [")
            << arguments.join(", ") << "]"
            << QString::fromUtf8(m.toByteArray());
        }

        return report;
    };
}

WarningFlags HarmonyToolchain::warningFlags(const QStringList &cflags) const
{
    /* ** 基于 'LC_ALL="en" gcc -Q --help=warnings | grep enabled' */
    WarningFlags flags(WarningFlags::Deprecated | WarningFlags::IgnoredQualifiers
                       | WarningFlags::SignedComparison | WarningFlags::UninitializedVars);
    WarningFlags groupWall(WarningFlags::All | WarningFlags::UnknownPragma | WarningFlags::UnusedFunctions
                           | WarningFlags::UnusedLocals | WarningFlags::UnusedResult | WarningFlags::UnusedValue
                           | WarningFlags::SignedComparison | WarningFlags::UninitializedVars);
    WarningFlags groupWextra(WarningFlags::Extra | WarningFlags::IgnoredQualifiers | WarningFlags::UnusedParams);

    for (int end = cflags.size(), i = 0; i != end; ++i) {
        const QString &flag = cflags[i];
        if (flag == "--all-warnings")
            flags |= groupWall;
        else if (flag == "--extra-warnings")
            flags |= groupWextra;

        WarningFlagAdder add(flag, flags);
        if (add.triggered())
            continue;

        /* ** clang 同样支持 */
        add("error", WarningFlags::AsErrors);
        add("all", groupWall);
        add("extra", groupWextra);
        add("deprecated", WarningFlags::Deprecated);
        add("effc++", WarningFlags::EffectiveCxx);
        add("ignored-qualifiers", WarningFlags::IgnoredQualifiers);
        add("non-virtual-dtor", WarningFlags::NonVirtualDestructor);
        add("overloaded-virtual", WarningFlags::OverloadedVirtual);
        add("shadow", WarningFlags::HiddenLocals);
        add("sign-compare", WarningFlags::SignedComparison);
        add("unknown-pragmas", WarningFlags::UnknownPragma);
        add("unused", WarningFlags::UnusedFunctions | WarningFlags::UnusedLocals | WarningFlags::UnusedParams
                          | WarningFlags::UnusedResult | WarningFlags::UnusedValue);
        add("unused-function", WarningFlags::UnusedFunctions);
        add("unused-variable", WarningFlags::UnusedLocals);
        add("unused-parameter", WarningFlags::UnusedParams);
        add("unused-result", WarningFlags::UnusedResult);
        add("unused-value", WarningFlags::UnusedValue);
        add("uninitialized", WarningFlags::UninitializedVars);
    }
    for (int end = cflags.size(), i = 0; i != end; ++i) {
        const QString &flag = cflags[i];
        if (flag == "-Wdocumentation")
            flags |= WarningFlags::Documentation;
        if (flag == "-Wno-documentation")
            flags &= ~WarningFlags::Documentation;
    }

    return flags;
}

LanguageExtensions HarmonyToolchain::defaultLanguageExtensions() const
{
    return LanguageExtension::Gnu;
}

LanguageExtensions HarmonyToolchain::languageExtensions(const QStringList &cxxflags) const
{
    LanguageExtensions extensions = defaultLanguageExtensions();

    const QStringList allCxxflags = m_platformCodeGenFlags + cxxflags; // add only cxxflags is empty?
    for (const QString &flag : allCxxflags) {
        if (flag.startsWith("-std=")) {
            const QByteArray std = flag.mid(5).toLatin1();
            if (std.startsWith("gnu"))
                extensions |= LanguageExtension::Gnu;
            else
                extensions &= ~LanguageExtensions(LanguageExtension::Gnu);
        } else if (flag == "-fopenmp") {
            extensions |= LanguageExtension::OpenMP;
        } else if (flag == "-fms-extensions") {
            extensions |= LanguageExtension::Microsoft;
        }
    }

    /* ** Clang 支持 -fborland-extensions */
    if (cxxflags.contains("-fborland-extensions"))
        extensions |= LanguageExtension::Borland;

    return extensions;
}

QString HarmonyToolchain::defaultDisplayName() const
{
    auto versionPair = HarmonyConfig::getVersion(HarmonyConfig::releaseFile(ndkLocation()));
    return Tr::tr("Harmony Clang (%1, API %2 %3)")
            .arg(HarmonyConfig::displayName(targetAbi()),
                 versionPair.first.toString(),
                 versionPair.second.toString());
}

static const Toolchains mingwToolchains()
{
    return ToolchainManager::toolchains([](const Toolchain *tc) -> bool {
        return tc->typeId() == ProjectExplorer::Constants::MINGW_TOOLCHAIN_TYPEID;
    });
}

static const GccToolchain *mingwToolchainFromId(const QByteArray &id)
{
    if (id.isEmpty())
        return nullptr;

    for (const Toolchain *tc : mingwToolchains()) {
        if (tc->id() == id)
            return static_cast<const GccToolchain *>(tc);
    }

    return nullptr;
}

void HarmonyToolchain::syncAutodetectedWithParentToolchains()
{
    /* ** 匹配 GccToolchain::syncAutodetectedWithParentToolchains（仅限 Windows + Clang 系列）。
    ** 之前错误检查了 CLANG_TOOLCHAIN_TYPEID，导致 Harmony 从未运行此逻辑。 */
    if (!HostOsInfo::isWindowsHost() || typeId() != Id(Constants::HARMONY_TOOLCHAIN_TYPEID)
        || !detectionSource().isAutoDetected()) {
        return;
    }

    QObject::disconnect(m_thisToolchainRemovedConnection);
    QObject::disconnect(m_mingwToolchainAddedConnection);

    if (!ToolchainManager::isLoaded()) {
        connect(ToolchainManager::instance(), &ToolchainManager::toolchainsLoaded, this,
                [id = id()] {
                    if (Toolchain * const tc = ToolchainManager::findToolchain(id)) {
                        if (tc->typeId() == Id(Constants::HARMONY_TOOLCHAIN_TYPEID))
                            static_cast<HarmonyToolchain *>(tc)->syncAutodetectedWithParentToolchains();
                    }
                });
        return;
    }

    if (!mingwToolchainFromId(m_parentToolchainId)) {
        const Toolchains mingwTCs = mingwToolchains();
        m_parentToolchainId = mingwTCs.isEmpty() ? QByteArray() : mingwTCs.front()->id();
    }

    /* ** 仅订阅自动检测的工具链。 */
    ToolchainManager *tcManager = ToolchainManager::instance();
    m_mingwToolchainAddedConnection
        = connect(tcManager, &ToolchainManager::toolchainsRegistered,
                  this, &HarmonyToolchain::onMingwToolchainsRegistered);
    m_thisToolchainRemovedConnection
        = connect(tcManager, &ToolchainManager::toolchainsDeregistered,
                  this, &HarmonyToolchain::onMingwToolchainsDeregistered);
}

void HarmonyToolchain::onMingwToolchainsRegistered(const Toolchains &toolchains)
{
    if (mingwToolchainFromId(m_parentToolchainId))
        return;
    for (Toolchain * const tc : toolchains) {
        if (tc->typeId() == ProjectExplorer::Constants::MINGW_TOOLCHAIN_TYPEID) {
            m_parentToolchainId = tc->id();
            break;
        }
    }
}

void HarmonyToolchain::onMingwToolchainsDeregistered(const Toolchains &toolchains)
{
    if (toolchains.contains(this)) {
        QObject::disconnect(m_thisToolchainRemovedConnection);
        QObject::disconnect(m_mingwToolchainAddedConnection);
        return;
    }
    const bool parentRemoved = Utils::anyOf(toolchains, [this](const Toolchain *tc) {
        return m_parentToolchainId == tc->id();
    });
    if (parentRemoved) {
        const Toolchains mingwTCs = mingwToolchains();
        m_parentToolchainId = mingwTCs.isEmpty() ? QByteArray() : mingwTCs.front()->id();
    }
}

static Abis guessGccAbi(const QString &m, const ProjectExplorer::Macros &macros)
{
    Abis abiList;

    Abi guessed = Abi::abiFromTargetTriplet(m);
    if (guessed.isNull())
        return abiList;

    Abi::Architecture arch = guessed.architecture();
    Abi::OS os = guessed.os();
    Abi::OSFlavor flavor = guessed.osFlavor();
    Abi::BinaryFormat format = guessed.binaryFormat();
    int width = guessed.wordWidth();

    const Macro sizeOfMacro = Utils::findOrDefault(macros, [](const Macro &m) { return m.key == "__SIZEOF_SIZE_T__"; });
    if (sizeOfMacro.isValid() && sizeOfMacro.type == MacroType::Define)
        width = sizeOfMacro.value.toInt() * 8;
    const Macro &mscVerMacro = Utils::findOrDefault(macros, [](const Macro &m) { return m.key == "_MSC_VER"; });
    if (mscVerMacro.type == MacroType::Define) {
        const int msvcVersion = mscVerMacro.value.toInt();
        flavor = Abi::flavorForMsvcVersion(msvcVersion);
    }

    if (os == Abi::DarwinOS) {
        /* ** Apple 支持 PPC 和 x86！ */
        abiList << Abi(arch, os, flavor, format, width);
        abiList << Abi(arch, os, flavor, format, width == 64 ? 32 : 64);
    } else if (arch == Abi::X86Architecture && (width == 0 || width == 64)) {
        abiList << Abi(arch, os, flavor, format, 64);
        if (width != 64 || (!m.contains("mingw")
                            && ToolchainManager::detectionSettings().detectX64AsX32)) {
            abiList << Abi(arch, os, flavor, format, 32);
        }
    } else {
        abiList << Abi(arch, os, flavor, format, width);
    }
    return abiList;
}


static HarmonyToolchain::DetectedAbisResult guessGccAbi(const FilePath &path,
                                                    const Environment &env,
                                                    const Macros &macros,
                                                    const QStringList &extraArgs = {})
{
    if (path.isEmpty())
        return HarmonyToolchain::DetectedAbisResult();

    QStringList arguments = extraArgs;
    arguments << "-dumpmachine";

    Result<QString> result = runGcc(path, arguments, env);
    QTC_ASSERT_RESULT(result, return {});

    QString machine = result->section('\n', 0, 0, QString::SectionSkipEmpty);
    if (machine.isEmpty()) {
        /* ** ICC 在 macOS 上未实现 -dumpmachine 选项。 */
        if (HostOsInfo::isMacHost() && (path.fileName() == "icc" || path.fileName() == "icpc"))
            return HarmonyToolchain::DetectedAbisResult({Abi::hostAbi()});
        return HarmonyToolchain::DetectedAbisResult(); // no need to continue if running failed once...
    }
    return HarmonyToolchain::DetectedAbisResult(guessGccAbi(machine, macros), machine);
}
HarmonyToolchain::DetectedAbisResult HarmonyToolchain::detectSupportedAbis() const
{
    Environment env = compilerCommand().deviceEnvironment();
    addToEnvironment(env);
    ProjectExplorer::Macros macros = createMacroInspectionRunner()({}).macros;
    return guessGccAbi(findLocalCompiler(compilerCommand(), env),
                       env,
                       macros,
                       platformCodeGenFlags());
}

/* ** 从编译器路径推断 NDK 目录。
** 典型路径结构：.../native/llvm/bin/clang → .../native
** 备用结构：    .../native/llvm/...       → .../native（基于 /llvm/ 分割） */
static FilePath inferNdkLocation(const QString &compilerPath)
{
    QStringList parts = compilerPath.split("/native/llvm/");
    if (parts.size() > 1)
        return FilePath::fromString(parts.first() + "/native");

    parts = compilerPath.split("/llvm/");
    const QString basePath = parts.size() > 1 ? parts.first() : QString();
    if (basePath.endsWith("/native"))
        return FilePath::fromString(basePath);

    return {};
}

FilePath HarmonyToolchain::ndkLocation() const
{
    /* ** 如果 NDK 位置为空，尝试从编译器路径推断 */
    if (m_ndkLocation.isEmpty())
        m_ndkLocation = inferNdkLocation(compilerCommand().toFSPathString());
    return m_ndkLocation;
}

FilePath HarmonyToolchain::makeCommand(const Utils::Environment &environment) const
{
    /* ** Unix/macOS：与 GccToolchain::mingwAwareMakeCommand 思路相同 — 优先 PATH，包含 gmake（BSD）。 */
    if (!HostOsInfo::isWindowsHost()) {
        const QStringList names{QStringLiteral("make"), QStringLiteral("gmake")};
        for (const QString &name : names) {
            const FilePath found = environment.searchInPath(name);
            if (!found.isEmpty())
                return found;
        }
        return FilePath("make");
    }
    const FilePath makePath = HarmonyConfig::makeLocation();
    if (makePath.isEmpty())
        return {};
    const FilePath makeExecutable = makePath / "bin" / "mingw32-make.exe";
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
        setToolchainConstructor([] { return new HarmonyToolchain; });   // NOSONAR (cpp:S5025)
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

/* ** 查找或创建一个工具链条目，并将其重置到给定的编译器路径。 */
static Toolchain *processToolchain(const FilePath &compilerCommand,
                                   const Id &lang,
                                   const QString &target,
                                   const Abi &abi,
                                   const FilePath &ndkLocation,
                                   const QString &displayName,
                                   const ToolchainList &alreadyKnown,
                                   QList<Toolchain *> &newToolchains)
{
    Toolchain *tc = findToolchain(compilerCommand, lang, target, alreadyKnown);
    if (tc) {
        if (tc->displayName() != displayName)
            tc->setDisplayName(displayName);
    } else {
        qCDebug(harmonyToolchainLog) << "New Clang toolchain found" << abi.toString() << lang
                                     << "for NDK" << ndkLocation;
        auto atc = new HarmonyToolchain();  // NOSONAR (cpp:S5025) - 交由 ToolchainManager 管理生命周期
        atc->setNdkLocation(ndkLocation);
        atc->setOriginalTargetTriple(target);
        atc->setLanguage(lang);
        atc->setTargetAbi(abi);
        atc->setPlatformCodeGenFlags({"-target", target});
        atc->setPlatformLinkerFlags({"-target", target});
        atc->setDisplayName(displayName);
        tc = atc;
        newToolchains << tc;
    }
    if (auto gccTc = dynamic_cast<HarmonyToolchain *>(tc))
        gccTc->resetToolchain(compilerCommand);
    tc->setDetectionSource(DetectionSource(DetectionSource::FromSdk));
    return tc;
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

    for (const FilePath &ndkLocation : ndkLocations) {
        const FilePath clangPath = HarmonyConfig::clangPathFromNdk(ndkLocation);
        if (!clangPath.exists())
            qCDebug(harmonyToolchainLog) << "Clang toolchains detection fails. Can not find Clang" << clangPath;

        for (const Id &lang : LanguageIds) {
            FilePath compilerCommand = clangPath;
            if (lang == ProjectExplorer::Constants::CXX_LANGUAGE_ID)
                compilerCommand = clangPlusPlusPath(clangPath);

            if (!compilerCommand.exists()) {
                qCDebug(harmonyToolchainLog) << "Skipping Clang toolchain. Can not find compiler" << compilerCommand;
                continue;
            }

            const QPair<QVersionNumber, QVersionNumber> versionPair
                = HarmonyConfig::getVersion(HarmonyConfig::releaseFile(ndkLocation));
            const QString customStr = isCustom ? "Custom " : QString();

            auto targetItr = clangTargets().constBegin();
            while (targetItr != clangTargets().constEnd()) {
                const Abi &abi = targetItr.value();
                const QString target = targetItr.key();
                const QString displayName(customStr + QString("Harmony Clang (%1, API %2 %3)")
                                              .arg(HarmonyConfig::displayName(abi),
                                                   versionPair.first.toString(),
                                                   versionPair.second.toString()));
                processToolchain(compilerCommand, lang, target, abi,
                                 ndkLocation, displayName, alreadyKnown, newToolchains);
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
