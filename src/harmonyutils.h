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

/** Bundle name for debug / `aa`（运行配置覆盖优先，否则 \c AppScope/app.json5）。 */
QString harmonyDebuggerBundleName(const ProjectExplorer::BuildConfiguration *bc);
/** Ability 名（运行配置覆盖优先，否则从 module.json5 推断，再退回 EntryAbility）。 */
QString harmonyDebuggerAbilityName(const ProjectExplorer::BuildConfiguration *bc);

/**
 * 设备序列号：构建配置里缓存的部署目标优先，否则当前 Kit 所选运行设备。
 */
QString harmonyEffectiveDeviceSerial(const ProjectExplorer::BuildConfiguration *bc);

/**
 * 供调试选择 \c lldb-server 架构：在 \c hapDevicePreferredAbi（ohpro \c entry/libs、ProjectNode）基础上，
 * 回退 **CMake \c OHOS_ARCH**（含合成 build key）、**Kit \c applicationAbis**、**部署缓存的设备 ABI**。
 */
QString harmonyDebuggerPreferredAbi(const ProjectExplorer::BuildConfiguration *bc);

/**
 * 查询应用主进程 PID（供 LLDB \c attach）。依次尝试 \c bm dump、\c aa dump、\c ps 解析。
 * @return 有效 PID，失败返回 \c -1
 */
qint64 harmonyQueryApplicationPid(const Utils::FilePath &hdc,
                                  const QString &serialNumber,
                                  const QString &bundleName,
                                  QString *sourceOut = nullptr);

/**
 * Best-effort verification that \a pid still exists and belongs to \a bundleName.
 */
bool harmonyPidLooksLikeBundleProcess(const Utils::FilePath &hdc,
                                      const QString &serialNumber,
                                      const QString &bundleName,
                                      qint64 pid);

/**
 * @name Native 调试设备壳环境探测（DEBUG-TASKS 0.2）
 *
 * 通过 `hdc shell id` / `getenforce` 粗判 **root vs user**、**SELinux 状态**。集成上主路径为 **§7.2**；§7.1 仅工程机/手工。
 * 判定规则与 `HARMONY-LLDB-DEBUG.md` §5（官方矩阵）一致。
 * @{
 */
enum class HarmonyShellUidClass {
    Unknown,
    /** `uid=0`（常见 `uid=0(root)`） */
    Root,
    /** 非 0 uid，多为 user / shell 账户 */
    NonRoot,
};

enum class HarmonySelinuxState {
    Unknown,
    Enforcing,
    Permissive,
    Disabled,
};

enum class HarmonyNativeDebugRecipeKind {
    Unknown,
    /** 更适合官方 §7.1：root shell 下推送 lldb-server、TCP 监听（仍可能需 `setenforce 0`） */
    FavorRootTcpListening,
    /** 更适合官方 §7.2：非 root 时常需 debug HAP + `aa` + abstract socket */
    FavorUserHapAbstractSocket,
};

struct HarmonyNativeDebugShellProbe {
    HarmonyShellUidClass uidClass = HarmonyShellUidClass::Unknown;
    HarmonySelinuxState selinux = HarmonySelinuxState::Unknown;
    QString idOutput;
    QString getenforceOutput;
    bool hdcMissing = false;
    bool idCommandOk = false;
    bool getenforceCommandOk = false;
};

HarmonyNativeDebugShellProbe probeNativeDebugShellEnvironment(const QString &deviceSerial);
HarmonyNativeDebugRecipeKind nativeDebugRecipeKind(const HarmonyNativeDebugShellProbe &probe);
/** @} */

}

#endif // HARMONYUTILS_H
