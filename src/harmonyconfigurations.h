#ifndef HARMONYCONFIGURATIONS_H
#define HARMONYCONFIGURATIONS_H

#include <QObject>
#include <utils/filepath.h>
#include <projectexplorer/toolchain.h>
namespace Ohos::Internal {
class HarmonyConfigurations : public QObject
{
    Q_OBJECT
public:
    static HarmonyConfigurations *instance();
    static void applyConfig();
    static void registerNewToolchains();
    static void registerQtVersions();
private:
    friend void setupHarmonyConfigurations();
    explicit HarmonyConfigurations(QObject *parent = nullptr);

    void load();
    void save();

    static void updateHarmonyDevice();
signals:
    void aboutToUpdate();
    void updated();
};

namespace HarmonyConfig {
Utils::FilePath makeLocation();
void setMakeLocation(const Utils::FilePath &makeLocation);
Utils::FilePath sdkLocation();
void setSdkLocation(const Utils::FilePath &sdkLocation);
Utils::FilePath ndkLocation();
void setNdkLocation(const Utils::FilePath &ndkLocation);
Utils::FilePath devecoStudioLocation();
void setDevecoStudioLocation(const Utils::FilePath &devecoStudioLocation);
Utils::FilePath qmakeLocation();
void setQmakeLocation(const Utils::FilePath &qmakeLocation);
Utils::FilePath toolchainPathFromNdk(
    const Utils::FilePath &ndkLocation);
Utils::FilePath clangPathFromNdk(const Utils::FilePath &ndkLocation);
QLatin1String displayName(const ProjectExplorer::Abi &abi);
QVersionNumber sdkVersion();
QVersionNumber ndkVersion();
bool setVersion(const Utils::FilePath &releaseFile);

Utils::FilePath hdcToolPath();
}
void setupHarmonyConfigurations();
} // namespace OhosConfig
#endif // HARMONYCONFIGURATIONS_H
