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
Utils::FilePath defaultSdk();
void setdefaultSdk(const Utils::FilePath &sdkLocation);
bool isValidSdk(const QString &sdkLocation);
QStringList &getSdkList();
void addSdk(const QString &sdk);
void removeSdkList(const QString &sdk);
QList<Utils::FilePath> ndkLocations();
// void setNdkLocation(const Utils::FilePath &ndkLocation);
Utils::FilePath devecoStudioLocation();
void setDevecoStudioLocation(const Utils::FilePath &devecoStudioLocation);
QStringList &getQmakeList();
void addQmake(const QString &qmake);
void removeQmake(const QString &qmake);

Utils::FilePath toolchainPathFromNdk( const Utils::FilePath &ndkLocation);
Utils::FilePath clangPathFromNdk(const Utils::FilePath &ndkLocation);
QLatin1String displayName(const ProjectExplorer::Abi &abi);
Utils::FilePath ndkLocation(const Utils::FilePath &sdkLocation);
Utils::FilePath releaseFile(const Utils::FilePath &ndkLocation);
QPair<QVersionNumber, QVersionNumber> getVersion(const Utils::FilePath &releaseFile);

Utils::FilePath hdcToolPath();
int getSDKVersion(const QString &device);
QString getProductModel(const QString &device);
QString getDeviceName(const QString &device);
QString getAbis(const QString &device);
}
void setupHarmonyConfigurations();
} // namespace OhosConfig
#endif // HARMONYCONFIGURATIONS_H
