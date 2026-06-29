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
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/target.h>
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
#include "harmonydebugsupport.h"
#include "harmonyrunner.h"
#include "harmonydeployqtstep.h"
#include "harmonylogcategories.h"
#include "ohosconstants.h"
#include "ohostr.h"

#include <QTranslator>

namespace {

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

using namespace ProjectExplorer;
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

    ~OhosPlugin() final  = default;
    
    using ExtensionSystem::IPlugin::initialize;
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
        setupHarmonyDebugWorker();

        connect(KitManager::instance(), &KitManager::kitsLoaded, this, &OhosPlugin::kitsRestored,
                Qt::SingleShotConnection);
        connect(ProjectManager::instance(), &ProjectManager::activeBuildConfigurationChanged,
            this, &OhosPlugin::addBuildHapStepForOhBuild);
        /* ** 项目打开后（含启动时恢复的项目）：为其所有 BC 补充 HAP 步骤 */
        connect(ProjectManager::instance(), &ProjectManager::projectAdded,
            this, [this](ProjectExplorer::Project *project) {
                ensureHapStepForAllBcs(project);
            });
    }

    void kitsRestored()
    {
        if (HarmonyConfig::registerDownloadedSdksUnder(HarmonyConfig::effectiveOhosSdkRoot()) > 0)
            HarmonyConfigurations::applyConfig();
        else
            HarmonyConfigurations::syncToolchainsQtAndKits();

        /*
         * Kit 加载完成后，activeBuildConfigurationChanged 可能已在 Kit 就绪前触发（项目作为
         * "最近项目"在启动期间加载），导致 HarmonyQtVersion 检测失败、步骤未被添加。
         * 此处对所有已加载项目做一次补偿遍历。
         */
        ensureHapStepForAllBcs();

        askUserAboutHarmonySdkSetupIfNeeded();

        connect(QtSupport::QtVersionManager::instance(), &QtSupport::QtVersionManager::qtVersionsChanged,
                HarmonyConfigurations::instance(), [] {
                    HarmonyConfigurations::syncToolchainsQtAndKits();
                });
    }

    void askUserAboutHarmonySdkSetupIfNeeded()
    {
        const bool hasQt = hasHarmonyQtVersions();
        const bool hasSdk = hasRegisteredValidHarmonySdk();

        /* ** 已完全配置——无需任何提示。 */
        if (hasQt && hasSdk)
            return;

        /* ** 用户尚未开始 Harmony 配置（无 Qt 且无 SDK 路径）——保持静默。 */
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

        /* ** hasSdk && !hasQt —— SDK 已配置但尚未注册 Harmony Qt。 */
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
    /** 遍历 project（或所有已加载项目）的全部 Target/BC，确保 HAP 步骤存在。 */
    void ensureHapStepForAllBcs(ProjectExplorer::Project *project = nullptr)
    {
        const QList<ProjectExplorer::Project *> projects =
            project ? QList{project} : ProjectManager::projects();
        for (ProjectExplorer::Project *p : projects) {
            for (ProjectExplorer::Target *t : p->targets()) {
                for (ProjectExplorer::BuildConfiguration *bc : t->buildConfigurations())
                    addBuildHapStepForOhBuild(bc);
            }
        }
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
        const QString lang = Core::ICore::userInterfaceLanguage();
        if (lang.isEmpty() || lang.startsWith(QLatin1String("en")))
            return; // English is the source language; no translation file needed.
        QTranslator translator;
        if (translator.load(QLatin1String(":/Harmony_") + lang + QLatin1String(".qm"))) {
            QCoreApplication::installTranslator(&translator);
        }
    }
    void extensionsInitialized() final
    {
        return;
    }

    ShutdownFlag aboutToShutdown() final
    {
        return SynchronousShutdown;
    }
};

} // namespace Ohos::Internal

#include "ohos.moc"
