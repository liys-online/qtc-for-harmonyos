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

        bundleNameOverride.setId(Constants::HARMONY_RUN_BUNDLE_OVERRIDE);
        bundleNameOverride.setSettingsKey("Harmony.Run.BundleNameOverrideKey");
        bundleNameOverride.setLabelText(Tr::tr("Bundle name override:"));
        bundleNameOverride.setToolTip(Tr::tr("If empty, the bundle name from AppScope/app.json5 is used."));
        bundleNameOverride.setDisplayStyle(StringAspect::LineEditDisplay);
        bundleNameOverride.setHistoryCompleter("Harmony.Run.BundleName.History");

        abilityNameOverride.setId(Constants::HARMONY_RUN_ABILITY_OVERRIDE);
        abilityNameOverride.setSettingsKey("Harmony.Run.AbilityNameOverrideKey");
        abilityNameOverride.setLabelText(Tr::tr("Ability name override:"));
        abilityNameOverride.setToolTip(
            Tr::tr("If empty, the first suitable ability name is read from module.json5 under the ohpro "
                   "directory (entry module preferred, then first exported or first ability). "
                   "If none is found, EntryAbility is used."));
        abilityNameOverride.setDisplayStyle(StringAspect::LineEditDisplay);
        abilityNameOverride.setHistoryCompleter("Harmony.Run.AbilityName.History");

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

            // `aa start` returns immediately; if the device shell exits right away, hdc finishes and
            // RunControl's task tree ends before the UI treats the run as active (Stop stays disabled).
            // Keep the shell session open until the user stops the run (same idea as Android's PID wait).
            allDeviceCommands << QStringLiteral("while true; do sleep 2; done");

            // postStartShellCmd (Post-quit…) runs when the hdc session ends — see harmonyrunner.cpp.
            cmd.addArg(allDeviceCommands.join(" ; "));

            return cmd;
        });
    }

    EnvironmentAspect environment{this};
    ArgumentsAspect extraAppArgs{this};
    StringAspect amStartArgs{this};
    BaseStringListAspect preStartShellCmd{this};
    BaseStringListAspect postStartShellCmd{this};
    StringAspect bundleNameOverride{this};
    StringAspect abilityNameOverride{this};
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
