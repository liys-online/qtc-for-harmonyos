#ifndef HARMONYTOOLCHAIN_H
#define HARMONYTOOLCHAIN_H

#include <projectexplorer/gcctoolchain.h>
namespace Ohos::Internal {
using ToolchainList = QList<ProjectExplorer::Toolchain *>;
class HarmonyToolchain : public ProjectExplorer::GccToolchain
{
public:
    explicit HarmonyToolchain();

    void addToEnvironment(Utils::Environment &env) const override;
    QStringList suggestedMkspecList() const override;
    Utils::FilePath makeCommand(const Utils::Environment &environment) const override;

    void setNdkLocation(const Utils::FilePath &ndkLocation);
    Utils::FilePath ndkLocation() const;
private:
    mutable Utils::FilePath m_ndkLocation;
};
ToolchainList autodetectToolchains(const ToolchainList &alreadyKnown);
ToolchainList autodetectToolchainsFromNdk(const ToolchainList &alreadyKnown,
                                           const Utils::FilePath &ndkLocation,
                                           const bool isCustom = false);
void setupHarmonyToolchain();
} // namespace Ohos

#endif // HARMONYTOOLCHAIN_H
