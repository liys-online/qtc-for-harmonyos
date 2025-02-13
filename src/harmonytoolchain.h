#ifndef HARMONYTOOLCHAIN_H
#define HARMONYTOOLCHAIN_H

#include <projectexplorer/gcctoolchain.h>
namespace Ohos::Internal {
using ToolchainList = QList<ProjectExplorer::Toolchain *>;
class HarmonyToolchain : public ProjectExplorer::GccToolchain
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

    void setNdkLocation(const Utils::FilePath &ndkLocation);
    Utils::FilePath ndkLocation() const;
private:
    mutable Utils::FilePath m_ndkLocation;
};
ToolchainList autodetectToolchains(const ToolchainList &alreadyKnown);
ToolchainList autodetectToolchainsFromNdk(const ToolchainList &alreadyKnown,
                                           const QList<Utils::FilePath> &ndkLocations,
                                           const bool isCustom = false);
void setupHarmonyToolchain();
} // namespace Ohos

#endif // HARMONYTOOLCHAIN_H
