#ifndef HARMONYUTILS_H
#define HARMONYUTILS_H

#include <QtCore/qglobal.h>
#include <QString>
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

/**
 * Default UIAbility name for `aa start -a` from ohpro module.json5 files:
 * prefers module \c type \c entry, then first exported ability, else first ability.
 * Empty if nothing could be parsed.
 */
QString defaultHarmonyAbilityName(const ProjectExplorer::BuildConfiguration *bc);

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

/**
 * Active CMake/build-system key for \c extraData lookups. When the run configuration uses the
 * synthetic \c HARMONY_DEFAULT_RUN_BUILD_KEY (no \c ProjectNode), falls back to the first
 * \c applicationTargets entry that has a project node.
 */
QString buildKeyForCMakeExtraData(const ProjectExplorer::BuildConfiguration *bc);

/**
 * Locate a built \c .hap under the ohpro root: \c build-profile.json5 \c modules (entry-type first),
 * then classic \c entry/…/outputs/default, then newest \c *.hap under the tree.
 * \a diagnosticOut optional multi-line trace of what was tried (for deploy error UI).
 */
Utils::FilePath findBuiltHapPackage(const Utils::FilePath &ohProRoot, QString *diagnosticOut = nullptr);

QStringList applicationAbis(const ProjectExplorer::Kit *k);

QStringList hdcSelector(const QString &serialNumber);

/**
 * Delegates to @c HdcSocketClient::preferCliFromEnvironment() (@c QTC_HARMONY_HDC_USE_CLI).
 * When @c true, @c HdcSocketClient::runSyncWithCliFallback skips the daemon and runs @c hdc.exe only.
 */
bool harmonyHdcShellPreferCli();

/**
 * 设备序列号：构建配置里缓存的部署目标优先，否则当前 Kit 所选运行设备。
 */
QString harmonyEffectiveDeviceSerial(const ProjectExplorer::BuildConfiguration *bc);

/** Split comma-separated \c module.json5 \c deviceTypes tokens (trim, skip empty). */
QStringList parseOhModuleDeviceTypesLine(const QString &line);
QString joinOhModuleDeviceTypesLine(const QStringList &types);
/** Preset \c deviceTypes ids for UI checkboxes (OpenHarmony \c module.json5 common values). */
QStringList ohModuleDeviceTypePresetIds();

}

#endif // HARMONYUTILS_H
