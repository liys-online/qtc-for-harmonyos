#include "harmonyrunconfiguration.h"
#include "harmonyconfigurations.h"
#include "harmonyutils.h"
#include "ohosconstants.h"
#include "ohostr.h"
#include <projectexplorer/runconfiguration.h>
#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/buildsystem.h>
#include <projectexplorer/devicesupport/devicekitaspects.h>
#include <projectexplorer/environmentkitaspect.h>
#include <projectexplorer/project.h>
#include <projectexplorer/runconfigurationaspects.h>
#include <projectexplorer/task.h>
#include <projectexplorer/target.h>
#include <utils/commandline.h>
using namespace ProjectExplorer;
using namespace Utils;
namespace Ohos::Internal {

/** P1-12: canonical \c settingsKey() matches \c setId(); \a legacyProjectStoreKey is read if canonical absent. */
class HarmonyMigratingStringAspect final : public StringAspect
{
public:
    HarmonyMigratingStringAspect(AspectContainer *container, const Key &legacyProjectStoreKey)
        : StringAspect(container), m_legacyProjectStoreKey(legacyProjectStoreKey)
    {}

    void fromMap(const Store &map) override
    {
        Store copy = map;
        if (!m_legacyProjectStoreKey.isEmpty() && !copy.contains(settingsKey())
            && copy.contains(m_legacyProjectStoreKey))
            copy.insert(settingsKey(), copy.value(m_legacyProjectStoreKey));
        StringAspect::fromMap(copy);
    }

private:
    Key m_legacyProjectStoreKey;
};

class BaseStringListAspect final : public Utils::StringAspect
{
public:
    explicit BaseStringListAspect(AspectContainer *container, const Key &legacyProjectStoreKey = {})
        : StringAspect(container), m_legacyProjectStoreKey(legacyProjectStoreKey)
    {}

    void fromMap(const Store &map) final
    {
        Store copy = map;
        if (!m_legacyProjectStoreKey.isEmpty() && !copy.contains(settingsKey())
            && copy.contains(m_legacyProjectStoreKey))
            copy.insert(settingsKey(), copy.value(m_legacyProjectStoreKey));
        /* ** Qt Creator 5.0 之前的兼容处理：将 QStringList 读取为 QString */
        setValue(copy.value(settingsKey()).toStringList().join('\n'));
    }

    void toMap(Store &map) const final
    {
        /* ** Qt Creator 5.0 之前的兼容处理：将 QString 写出为 QStringList */
        map.insert(settingsKey(), value().split('\n'));
    }

private:
    Key m_legacyProjectStoreKey;
};
class HarmonyRunConfiguration : public RunConfiguration
{
public:
    HarmonyRunConfiguration(ProjectExplorer::BuildConfiguration *bc, Utils::Id id)
        : ProjectExplorer::RunConfiguration(bc, id)
    {
        environment.addSupportedBaseEnvironment(Tr::tr("Clean Environment"), {});

        extraAppArgs.addOnChanged(this, [this, bc] {
            if (bc->target()->buildConfigurations().first()->buildType() == BuildConfiguration::BuildType::Release) {
                const QString buildKey = bc->activeBuildKey();
                bc->buildSystem()->setExtraData(buildKey,
                                                Ohos::Constants::HarmonyApplicationArgs,
                                                extraAppArgs());
            }
        });

        amStartArgs.setId(Constants::HARMONY_AA_START_ARGS);
        amStartArgs.setSettingsKey(Constants::HARMONY_AA_START_ARGS);
        amStartArgs.setLabelText(Tr::tr("Ability assistant start arguments:"));
        amStartArgs.setDisplayStyle(StringAspect::LineEditDisplay);
        amStartArgs.setHistoryCompleter("Harmony.AaStartArgs.History");

        preStartShellCmd.setDisplayStyle(StringAspect::TextEditDisplay);
        preStartShellCmd.setId(Constants::HARMONY_PRESTARTSHELLCMDLIST);
        preStartShellCmd.setSettingsKey(Constants::HARMONY_PRESTARTSHELLCMDLIST);
        preStartShellCmd.setLabelText(Tr::tr("Pre-launch on-device shell commands:"));

        postStartShellCmd.setDisplayStyle(StringAspect::TextEditDisplay);
        postStartShellCmd.setId(Constants::HARMONY_POSTFINISHSHELLCMDLIST);
        postStartShellCmd.setSettingsKey(Constants::HARMONY_POSTFINISHSHELLCMDLIST);
        postStartShellCmd.setLabelText(Tr::tr("Post-quit on-device shell commands:"));

        bundleNameOverride.setId(Constants::HARMONY_RUN_BUNDLE_OVERRIDE);
        bundleNameOverride.setSettingsKey(Constants::HARMONY_RUN_BUNDLE_OVERRIDE);
        bundleNameOverride.setLabelText(Tr::tr("Bundle name override:"));
        bundleNameOverride.setToolTip(Tr::tr("If empty, the bundle name from AppScope/app.json5 is used."));
        bundleNameOverride.setDisplayStyle(StringAspect::LineEditDisplay);
        bundleNameOverride.setHistoryCompleter("Harmony.Run.BundleName.History");

        abilityNameOverride.setId(Constants::HARMONY_RUN_ABILITY_OVERRIDE);
        abilityNameOverride.setSettingsKey(Constants::HARMONY_RUN_ABILITY_OVERRIDE);
        abilityNameOverride.setLabelText(Tr::tr("Ability name override:"));
        abilityNameOverride.setToolTip(
            Tr::tr("If empty, the first suitable ability name is read from module.json5 under the ohpro "
                   "directory (entry module preferred, then first exported or first ability). "
                   "If none is found, EntryAbility is used."));
        abilityNameOverride.setDisplayStyle(StringAspect::LineEditDisplay);
        abilityNameOverride.setHistoryCompleter("Harmony.Run.AbilityName.History");

        hilogEnabled.setId(Constants::HARMONY_HILOG_ENABLED);
        hilogEnabled.setSettingsKey(Constants::HARMONY_HILOG_ENABLED);
        hilogEnabled.setLabelText(Tr::tr("Stream hilog to output panel:"));
        hilogEnabled.setToolTip(Tr::tr("Attaches an hdc hilog reader filtered by the application PID that "
                                       "streams on-device log output to the Application Output panel "
                                       "while the application is running."));
        hilogEnabled.setDefaultValue(true);

        hilogFilter.setId(Constants::HARMONY_HILOG_FILTER);
        hilogFilter.setSettingsKey(Constants::HARMONY_HILOG_FILTER);
        hilogFilter.setLabelText(Tr::tr("hilog filter:"));
        hilogFilter.setToolTip(Tr::tr("Extra arguments appended to the hilog command. "
                                      "The application PID filter (-P) is added automatically. "
                                      "Leave empty to stream all of the application's logs."));
        hilogFilter.setPlaceHolderText(Tr::tr("Auto-filtered by PID; e.g. \"-T MyTag\" or \"-e pattern\""));
        hilogFilter.setDisplayStyle(StringAspect::LineEditDisplay);
        hilogFilter.setHistoryCompleter("Harmony.Run.HilogFilter.History");

        hilogEnabled.addOnChanged(this, [this] {
            hilogFilter.setEnabled(hilogEnabled());
        });

        startupBreak.setId(Constants::HARMONY_DEBUG_STARTUP_BREAK);
        startupBreak.setSettingsKey(Constants::HARMONY_DEBUG_STARTUP_BREAK);
        startupBreak.setLabelText(Tr::tr("Enable startup breakpoints (aa start -D)"));
        startupBreak.setToolTip(Tr::tr(
            "Use 'aa start -D' to pause the ArkTS/JS VM before native code runs, "
            "allowing breakpoints to be set in C++ startup code (main(), constructors, etc.). "
            "The ohos_lldb_debug_cli bridge binary must be next to the Qt Creator executable.\n"
            "Disable this option only when attaching to an already running process."));
        startupBreak.setDefaultValue(true);

        setUpdater([this] {
            const ProjectExplorer::BuildTargetInfo bti = buildTargetInfo();
            /*
            ** Qt for OH / HAP 工程在 CMake 侧常为 MODULE_LIBRARY 等，不会进入 applicationTargets；
            ** FixedRunConfigurationFactory 提供无 buildKey 的条目，此时 displayName 为空，用默认名。
            */
            const QString name = bti.displayName.isEmpty()
                ? Tr::tr("Harmony Application")
                : bti.displayName;
            setDisplayName(name);
            setDefaultDisplayName(name);
        });

        setCommandLineGetter([this, bc] {
            CommandLine cmd;
            const FilePath hdc = HarmonyConfig::hdcToolPath();
            if (hdc.isEmpty() || !hdc.exists())
                return cmd;

            QString pkgName = bundleNameOverride().trimmed();
            if (pkgName.isEmpty())
                pkgName = packageName(bc);

            QString abilityName = abilityNameOverride().trimmed();
            if (abilityName.isEmpty())
                abilityName = defaultHarmonyAbilityName(bc);
            if (abilityName.isEmpty())
                abilityName = QStringLiteral("EntryAbility");

            cmd.setExecutable(hdc);
            cmd.addArg("shell");

            QString startCommand;
            const QString userArgs = amStartArgs();
            if (!userArgs.trimmed().isEmpty()) {
                startCommand = "aa start " + userArgs.trimmed();
            } else {
                startCommand = "aa start";
                if (!pkgName.isEmpty())
                    startCommand += " -b " + pkgName + " -a " + abilityName;
            }

            const QStringList preCmds = preStartShellCmd().split('\n', Qt::SkipEmptyParts);
            QStringList allDeviceCommands;
            for (const QString &pre : preCmds) {
                const QString trimmed = pre.trimmed();
                if (!trimmed.isEmpty())
                    allDeviceCommands << trimmed;
            }
            allDeviceCommands << startCommand;

            /*
            ** `aa start` 立即返回；若设备 shell 随即退出，hdc 完成且 RunControl 的任务树
            ** 在 UI 认为运行已激活之前结束（Stop 按鈕保持禁用）。
            ** 保持 shell 会话开启，直到用户停止运行（与 Android 的 PID 等待思路相同）。
            */
            allDeviceCommands << QStringLiteral("while true; do sleep 2; done");

            /* ** postStartShellCmd（退出后命令）在 hdc 会话结束时运行——见 harmonyrunner.cpp。 */
            cmd.addArg(allDeviceCommands.join(" ; "));

            return cmd;
        });
    }

    EnvironmentAspect environment{this};
    ArgumentsAspect extraAppArgs{this};
    HarmonyMigratingStringAspect amStartArgs{this, Key("Harmony.AaStartArgsKey")};
    BaseStringListAspect preStartShellCmd{this, Key("Harmony.PreStartShellCmdListKey")};
    BaseStringListAspect postStartShellCmd{this, Key("Harmony.PostStartShellCmdListKey")};
    HarmonyMigratingStringAspect bundleNameOverride{this, Key("Harmony.Run.BundleNameOverrideKey")};
    HarmonyMigratingStringAspect abilityNameOverride{this, Key("Harmony.Run.AbilityNameOverrideKey")};
    BoolAspect hilogEnabled{this};
    HarmonyMigratingStringAspect hilogFilter{this, {}};
    BoolAspect startupBreak{this};
};
namespace {

bool harmonyRunConfigCanAddDefault(BuildConfiguration *bc)
{
    Target *target = bc->target();
    if (!target)
        return false;
    Project *project = target->project();
    Kit *kit = target->kit();
    if (!project || !kit)
        return false;
    if (containsType(project->projectIssues(kit), Task::TaskType::Error))
        return false;
    return RunDeviceTypeKitAspect::deviceTypeId(kit) == Ohos::Constants::HARMONY_DEVICE_TYPE;
}
} // namespace

class HarmonyRunConfigurationFactory final : public RunConfigurationFactory
{
public:
    HarmonyRunConfigurationFactory()
    {
        registerRunConfiguration<HarmonyRunConfiguration>(Ohos::Constants::HARMONY_RUNCONFIG_ID);
        addSupportedTargetDeviceType(Ohos::Constants::HARMONY_DEVICE_TYPE);
    }

protected:
    QList<RunConfigurationCreationInfo> availableCreators(BuildConfiguration *bc) const override
    {
        QList<RunConfigurationCreationInfo> items = RunConfigurationFactory::availableCreators(bc);
        if (!items.isEmpty())
            return items;
        /* ** CMake 的 applicationTargets 仅含 Executable；Qt for OH 常为 MODULE_LIBRARY，列表为空。 */
        if (!harmonyRunConfigCanAddDefault(bc))
            return items;
        /*
        ** 在尚无解析数据时若仍返回 AlwaysCreate，Qt Creator 无法把已有 RC 记入 existing
        **（见 BuildConfiguration::updateDefaultRunConfigurations），会导致每次打开项目重复添加。
        */
        if (!bc->buildSystem()->hasParsingData())
            return items;

        RunConfigurationCreationInfo rci;
        rci.factory = this;
        rci.buildKey = QString::fromLatin1(Constants::HARMONY_DEFAULT_RUN_BUILD_KEY);
        rci.displayName = decoratedTargetName(Tr::tr("Harmony Application"), bc->kit());
        rci.creationMode = RunConfigurationCreationInfo::AlwaysCreate;
        items.append(rci);
        return items;
    }

    bool supportsBuildKey(BuildConfiguration *bc, const QString &key) const override
    {
        if (key == QString::fromLatin1(Constants::HARMONY_DEFAULT_RUN_BUILD_KEY))
            return harmonyRunConfigCanAddDefault(bc);
        return RunConfigurationFactory::supportsBuildKey(bc, key);
    }
};

void setupHarmonyRunConfiguration()
{
    static HarmonyRunConfigurationFactory theHarmonyRunConfigurationFactory;
}

}
