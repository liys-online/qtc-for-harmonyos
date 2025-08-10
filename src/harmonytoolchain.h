#ifndef HARMONYTOOLCHAIN_H
#define HARMONYTOOLCHAIN_H

#include <projectexplorer/gcctoolchain.h>
#include <projectexplorer/headerpath.h>
namespace Ohos::Internal {
using ToolchainList = QList<ProjectExplorer::Toolchain *>;
class HarmonyToolchain : public ProjectExplorer::Toolchain
{
public:
    explicit HarmonyToolchain();
    /**
     * @brief addToEnvironment
     * 添加到环境变量
     * @param env
     */
    void addToEnvironment(Utils::Environment &env) const override;
    /**
     * @brief suggestedMkspecList
     * 建议的mkspec列表
     * @return
     */
    QStringList suggestedMkspecList() const override;
    /**
     * @brief makeCommand
     * make命令
     * @param environment
     * @return
     */
    Utils::FilePath makeCommand(const Utils::Environment &environment) const override;


    class DetectedAbisResult {
    public:
        DetectedAbisResult() = default;
        DetectedAbisResult(const ProjectExplorer::Abis &supportedAbis, const QString &originalTargetTriple = {}) :
            supportedAbis(supportedAbis),
            originalTargetTriple(originalTargetTriple)
        { }

        ProjectExplorer::Abis supportedAbis;
        QString originalTargetTriple;
    };

    void toMap(Utils::Store &data) const override;
    void fromMap(const Utils::Store &data) override;
    void resetToolchain(const Utils::FilePath &);
    bool isValid() const override;
    
    void setNdkLocation(const Utils::FilePath &ndkLocation);
    Utils::FilePath ndkLocation() const;

    QVersionNumber apiVersion() const;

    void setPlatformCodeGenFlags(const QStringList &);
    QStringList platformCodeGenFlags() const;

    void setPlatformLinkerFlags(const QStringList &);

    // Reinterpret options for compiler drivers inheriting from GccToolchain (e.g qcc) to apply -Wp option
    // that passes the initial options directly down to the gcc compiler
    using OptionsReinterpreter = std::function<QStringList(const QStringList &options)>;
    void setOptionsReinterpreter(const OptionsReinterpreter &optionsReinterpreter);

    using ExtraHeaderPathsFunction = std::function<void(ProjectExplorer::HeaderPaths &)>;
    void initExtraHeaderPathsFunction(ExtraHeaderPathsFunction &&extraHeaderPathsFunction) const;

    void setPriority (int priority) { m_priority = priority; }
    int priority() const override { return m_priority; }

    Utils::LanguageExtensions languageExtensions(const QStringList &cxxflags) const override;
    Utils::WarningFlags warningFlags(const QStringList &cflags) const override;
    MacroInspectionRunner createMacroInspectionRunner() const override;
    BuiltInHeaderPathsRunner createBuiltInHeaderPathsRunner(const Utils::Environment &env) const override;
    QList<Utils::OutputLineParser *> createOutputParsers() const override;

    void setOriginalTargetTriple(const QString &targetTriple);
    virtual QString originalTargetTriple() const override;
protected:
    QString defaultDisplayName() const;
    Utils::LanguageExtensions defaultLanguageExtensions() const;
    virtual DetectedAbisResult detectSupportedAbis() const;
    QStringList m_platformCodeGenFlags = {};
    QStringList m_platformLinkerFlags = {};

    OptionsReinterpreter m_optionsReinterpreter = [](const QStringList &v) { return v; };
    mutable ExtraHeaderPathsFunction m_extraHeaderPathsFunction = [](ProjectExplorer::HeaderPaths &) {};

private:
    void syncAutodetectedWithParentToolchains();
    mutable ProjectExplorer::Abis m_supportedAbis;
    mutable QString m_originalTargetTriple;
    mutable Utils::FilePath m_ndkLocation;
    mutable QVersionNumber m_apiVersion;
    mutable Utils::FilePath m_installDir;
    QByteArray m_parentToolchainId;
    int m_priority = PriorityNormal;
    QMetaObject::Connection m_mingwToolchainAddedConnection;
    QMetaObject::Connection m_thisToolchainRemovedConnection;
};
ToolchainList autodetectToolchains(const ToolchainList &alreadyKnown);
ToolchainList autodetectToolchainsFromNdk(const ToolchainList &alreadyKnown,
                                           const QList<Utils::FilePath> &ndkLocations,
                                           const bool isCustom = false);
void setupHarmonyToolchain();
} // namespace Ohos

#endif // HARMONYTOOLCHAIN_H
