#include "harmonyrunconfiguration.h"
#include "harmonyconfigurations.h"
#include "harmonyutils.h"
#include "ohosconstants.h"
#include "ohostr.h"
#include <projectexplorer/runconfiguration.h>
#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/buildsystem.h>
#include <projectexplorer/environmentkitaspect.h>
#include <projectexplorer/project.h>
#include <projectexplorer/runconfigurationaspects.h>
#include <projectexplorer/target.h>
#include <utils/commandline.h>
using namespace ProjectExplorer;
using namespace Utils;
namespace Ohos::Internal {
class BaseStringListAspect final : public Utils::StringAspect
{
public:
    explicit BaseStringListAspect(AspectContainer *container)
        : StringAspect(container)
    {}

    void fromMap(const Store &map) final
    {
        // Pre Qt Creator 5.0 hack: Reads QStringList as QString
        setValue(map.value(settingsKey()).toStringList().join('\n'));
    }

    void toMap(Store &map) const final
    {
        // Pre Qt Creator 5.0 hack: Writes QString as QStringList
        map.insert(settingsKey(), value().split('\n'));
    }
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
        amStartArgs.setSettingsKey("Harmony.AaStartArgsKey");
        amStartArgs.setLabelText(Tr::tr("Ability assistant start arguments:"));
        amStartArgs.setDisplayStyle(StringAspect::LineEditDisplay);
        amStartArgs.setHistoryCompleter("Harmony.AaStartArgs.History");

        preStartShellCmd.setDisplayStyle(StringAspect::TextEditDisplay);
        preStartShellCmd.setId(Constants::HARMONY_PRESTARTSHELLCMDLIST);
        preStartShellCmd.setSettingsKey("Harmony.PreStartShellCmdListKey");
        preStartShellCmd.setLabelText(Tr::tr("Pre-launch on-device shell commands:"));

        postStartShellCmd.setDisplayStyle(StringAspect::TextEditDisplay);
        postStartShellCmd.setId(Constants::HARMONY_POSTFINISHSHELLCMDLIST);
        postStartShellCmd.setSettingsKey("Harmony.PostStartShellCmdListKey");
        postStartShellCmd.setLabelText(Tr::tr("Post-quit on-device shell commands:"));

        setUpdater([this] {
            const ProjectExplorer::BuildTargetInfo bti = buildTargetInfo();
            setDisplayName(bti.displayName);
            setDefaultDisplayName(bti.displayName);
        });

        setCommandLineGetter([this, bc] {
            CommandLine cmd;
            const FilePath hdc = HarmonyConfig::hdcToolPath();
            if (hdc.isEmpty() || !hdc.exists())
                return cmd;

            const QString pkgName = packageName(bc);
            cmd.setExecutable(hdc);
            cmd.addArg("shell");

            QString startCommand;
            const QString userArgs = amStartArgs();
            if (!userArgs.trimmed().isEmpty()) {
                startCommand = "aa start " + userArgs.trimmed();
            } else {
                startCommand = "aa start";
                if (!pkgName.isEmpty()) {
                    startCommand += " -b " + pkgName + " -a EntryAbility";
                }
            }

            const QStringList preCmds = preStartShellCmd().split('\n', Qt::SkipEmptyParts);
            QStringList allDeviceCommands;
            for (const QString &pre : preCmds) {
                const QString trimmed = pre.trimmed();
                if (!trimmed.isEmpty())
                    allDeviceCommands << trimmed;
            }
            allDeviceCommands << startCommand;

            // NOTE: postStartShellCmd requires a dedicated run worker lifecycle hook.
            // Here we only execute pre-commands and launch command in one shell session.
            cmd.addArg(allDeviceCommands.join(" ; "));

            return cmd;
        });
    }

    EnvironmentAspect environment{this};
    ArgumentsAspect extraAppArgs{this};
    StringAspect amStartArgs{this};
    BaseStringListAspect preStartShellCmd{this};
    BaseStringListAspect postStartShellCmd{this};
};
class HarmonyRunConfigurationFactory : public ProjectExplorer::RunConfigurationFactory
{
public:
    HarmonyRunConfigurationFactory()
    {
        registerRunConfiguration<HarmonyRunConfiguration>(Ohos::Constants::HARMONY_RUNCONFIG_ID);
        addSupportedTargetDeviceType(Ohos::Constants::HARMONY_DEVICE_TYPE);
    }
};

void setupHarmonyRunConfiguration()
{
    static HarmonyRunConfigurationFactory theHarmonyRunConfigurationFactory;
}

}
