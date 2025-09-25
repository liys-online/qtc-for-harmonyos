#ifndef HARMONYUTILS_H
#define HARMONYUTILS_H

#include <QtCore/qglobal.h>
namespace ProjectExplorer {
class BuildConfiguration;
class Target;
class Kit;
}

namespace QtSupport { class QtVersion; }

namespace Utils {
class FilePath;
}
namespace Ohos::Internal {

bool openDevEcoProject(const QString &path);

QString packageName(const ProjectExplorer::BuildConfiguration *bc);

QString deviceSerialNumber(const ProjectExplorer::BuildConfiguration *bc);
void setDeviceSerialNumber(ProjectExplorer::BuildConfiguration *bc,
                           const QString &deviceSerialNumber);

QString hapDevicePreferredAbi(const ProjectExplorer::BuildConfiguration *bc);
void setDeviceAbis(ProjectExplorer::BuildConfiguration *bc, const QStringList &deviceAbis);

int deviceApiLevel(const ProjectExplorer::BuildConfiguration *bc);
void setDeviceApiLevel(ProjectExplorer::BuildConfiguration *bc, int level);


Utils::FilePath appJson5Path(const ProjectExplorer::BuildConfiguration *bc);
void setAppJson5Path(ProjectExplorer::BuildConfiguration *bc, const Utils::FilePath &path);

int minimumSDK(const ProjectExplorer::BuildConfiguration *bc);
int minimumSDK(const ProjectExplorer::Kit *kit);
int defaultMinimumSDK(const QtSupport::QtVersion *qtVersion);

bool isQt5CmakeProject(const ProjectExplorer::Target *target);

Utils::FilePath harmonyBuildDirectory(const ProjectExplorer::BuildConfiguration *bc);
Utils::FilePath buildDirectory(const ProjectExplorer::BuildConfiguration *bc);
QStringList applicationAbis(const ProjectExplorer::Kit *k);

QStringList hdcSelector(const QString &serialNumber);

}

#endif // HARMONYUTILS_H
