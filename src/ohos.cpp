#include "harmonyqtcdefs.h"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/icontext.h>
#include <coreplugin/icore.h>
#include <coreplugin/messagemanager.h>

#include <projectexplorer/kitmanager.h>
#include <projectexplorer/buildsystem.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/deployconfiguration.h>

#include <extensionsystem/iplugin.h>

#include <qtsupport/qtversionmanager.h>
#include <qtsupport/qtkitaspect.h>

#include <utils/infobar.h>

#include <QTimer>

#include "harmonybuildconfiguration.h"
#include "harmonybuildhapstep.h"
#include "harmonysettingswidget.h"
#include "harmonyconfigurations.h"
#include "harmonydevice.h"
#include "harmonyqtversion.h"
#include "harmonytoolchain.h"
#include "harmonyrunconfiguration.h"
#include "harmonyrunner.h"
#include "harmonydeployqtstep.h"
#include "ohosconstants.h"
#include "ohostr.h"

#include <QTranslator>
#include <QLoggingCategory>

namespace {
static Q_LOGGING_CATEGORY(harmonyPluginLog, "qtc.harmony.plugin", QtWarningMsg)

const char kSetupHarmonySdkInfoBarId[] = "Harmony.ConfigureSdkSetup";
const char kSetupHarmonyQtInfoBarId[] = "Harmony.ConfigureQtSetup";

static bool hasHarmonyQtVersions()
{
    using namespace QtSupport;
    return !QtVersionManager::versions([](const QtVersion *v) {
               return dynamic_cast<const Ohos::Internal::HarmonyQtVersion *>(v) != nullptr;
           }).isEmpty();
}

static bool hasRegisteredValidHarmonySdk()
{
    using namespace Utils;
    using namespace Ohos::Internal;
    const FilePath def = HarmonyConfig::defaultSdk().cleanPath();
    if (!def.isEmpty() && HarmonyConfig::isValidSdk(def))
        return true;
    for (const QString &s : HarmonyConfig::getSdkList()) {
        const FilePath fp = FilePath::fromUserInput(s).cleanPath();
        if (!fp.isEmpty() && HarmonyConfig::isValidSdk(fp))
            return true;
    }
    return HarmonyConfig::isValidSdk(HarmonyConfig::effectiveOhosSdkRoot().cleanPath());
}

} // namespace

namespace Ohos::Internal {

class HarmonyDeployConfigurationFactory final : public DeployConfigurationFactory
{
public:
    HarmonyDeployConfigurationFactory()
    {
        setConfigBaseId("Qt4ProjectManager.HarmonyDeployConfiguration2");
        addSupportedTargetDeviceType(Constants::HARMONY_DEVICE_TYPE);
        setDefaultDisplayName(Tr::tr("Deploy to HarmonyOS Device"));
        addInitialStep(Constants::HARMONY_DEPLOY_QT_ID);
    }
};

void setupHarmonyDeployConfiguration()
{
    static HarmonyDeployConfigurationFactory theHarmonyDeployConfigurationFactory;
}

class OhosPlugin final : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "Harmony.json")

public:
    OhosPlugin() = default;

    ~OhosPlugin() final
    {

    }

    void initialize() final
    {
        using namespace ProjectExplorer;
        loadTranslations();
        setupHarmonyConfigurations();
        setupHarmonyDevice();
        setupHarmonyQtVersion();
        setupHarmonySettingsPage();
        setupHarmonyBuildConfiguration();
        setupHarmonyToolchain();
        setupHarmonyBuildHapStep();

        setupHarmonyDeployConfiguration();
        setupHarmonyDeployQtStep();

        setupHarmonyRunConfiguration();
        setupHarmonyRunWorker();
        connect(KitManager::instance(), &KitManager::kitsLoaded, this, &OhosPlugin::kitsRestored,
                Qt::SingleShotConnection);
        connect(ProjectManager::instance(), &ProjectManager::activeBuildConfigurationChanged,
            this, &OhosPlugin::addBuildHapStepForOhBuild);
    }

    void kitsRestored()
    {
        if (HarmonyConfig::registerDownloadedSdksUnder(HarmonyConfig::effectiveOhosSdkRoot()) > 0)
            HarmonyConfigurations::applyConfig();

        askUserAboutHarmonySdkSetupIfNeeded();

        HarmonyConfigurations::registerNewToolchains();
        // HarmonyConfigurations::registerQtVersions();
        HarmonyConfigurations::updateAutomaticKitList();
        connect(QtSupport::QtVersionManager::instance(), &QtSupport::QtVersionManager::qtVersionsChanged,
                HarmonyConfigurations::instance(), [] {
                    HarmonyConfigurations::registerNewToolchains();
                    HarmonyConfigurations::updateAutomaticKitList();
                });
    }

    void askUserAboutHarmonySdkSetupIfNeeded()
    {
        const bool hasQt = hasHarmonyQtVersions();
        const bool hasSdk = hasRegisteredValidHarmonySdk();

        // Fully configured — nothing to suggest.
        if (hasQt && hasSdk)
            return;

        // User has not started Harmony setup (no Qt and no SDK path) — stay quiet.
        if (!hasQt && !hasSdk)
            return;

        Utils::InfoBar *infoBar = Core::ICore::popupInfoBar();

        if (hasQt && !hasSdk) {
            if (!infoBar->canInfoBeAdded(Utils::Id(kSetupHarmonySdkInfoBarId)))
                return;

            Utils::InfoBarEntry entry(
                Utils::Id(kSetupHarmonySdkInfoBarId),
                Tr::tr("Add an OpenHarmony / HarmonyOS SDK path and default SDK in preferences, "
                       "or set %1 / %2 so kits, hdc, and toolchains can be resolved.")
                    .arg(QString::fromLatin1(Constants::OHOS_SDK_HOME_ENV_VAR),
                         QString::fromLatin1(Constants::OHOS_SDK_ENV_VAR)),
                Utils::InfoBarEntry::GlobalSuppression::Enabled);
            entry.setTitle(Tr::tr("Configure Harmony SDK?"));
            entry.setInfoType(Utils::InfoLabel::Information);
            entry.addCustomButton(
                Tr::tr("Open Harmony Settings"),
                [this] {
                    QTimer::singleShot(0, this, [] {
                        Core::ICore::showSettings(Constants::HARMONY_SETTINGS_ID);
                    });
                },
                {},
                Utils::InfoBarEntry::ButtonAction::SuppressPersistently);
            infoBar->addInfo(entry);
            return;
        }

        // hasSdk && !hasQt — SDK is set up but no Harmony Qt registered yet.
        QTC_CHECK(hasSdk && !hasQt);
        if (!infoBar->canInfoBeAdded(Utils::Id(kSetupHarmonyQtInfoBarId)))
            return;

        Utils::InfoBarEntry entry(
            Utils::Id(kSetupHarmonyQtInfoBarId),
            Tr::tr("Harmony SDK paths look valid, but no Qt for Harmony is registered. "
                   "Add a qmake path on the Harmony settings page, or register the qmake in "
                   "Edit > Preferences > Kits > Qt Versions."),
            Utils::InfoBarEntry::GlobalSuppression::Enabled);
        entry.setTitle(Tr::tr("Add Harmony Qt?"));
        entry.setInfoType(Utils::InfoLabel::Information);
        entry.addCustomButton(
            Tr::tr("Open Harmony Settings"),
            [this] {
                QTimer::singleShot(0, this, [] {
                    Core::ICore::showSettings(Constants::HARMONY_SETTINGS_ID);
                });
            },
            {},
            Utils::InfoBarEntry::ButtonAction::SuppressPersistently);
        infoBar->addInfo(entry);
    }
    void addBuildHapStepForOhBuild(ProjectExplorer::BuildConfiguration *bc)
    {
        using namespace QtSupport;
        if(!bc) return;
        if(auto *buildsystem = bc->buildSystem())
        {
            auto *ohQt = dynamic_cast<const HarmonyQtVersion *>(QtKitAspect::qtVersion(buildsystem->kit()));
            if(ohQt && ohQt->isHarmonyQtVersion())
            {
                qCDebug(harmonyPluginLog) << "Add Harmony Build Hap Step to BuildConfiguration:"
                                          << ohQt->displayName();
                if(auto *steps = bc->buildSteps(); !steps->contains(Constants::HARMONY_BUILD_HAP_ID))
                    steps->appendStep(Constants::HARMONY_BUILD_HAP_ID);
            }
        }
    }
    void loadTranslations() {
        QTranslator *translator = new QTranslator();
        if (translator->load(":/Harmony_zh_CN.qm")) {
            QCoreApplication::installTranslator(translator);
        }
    }
    void extensionsInitialized() final
    {

    }

    ShutdownFlag aboutToShutdown() final
    {
        return SynchronousShutdown;
    }
};

} // namespace Ohos::Internal

#include <ohos.moc>
