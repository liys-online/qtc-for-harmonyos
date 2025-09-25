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

#include "harmonybuildconfiguration.h"
#include "harmonybuildhapstep.h"
#include "harmonysettingswidget.h"
#include "harmonyconfigurations.h"
#include "harmonydevice.h"
#include "harmonyqtversion.h"
#include "harmonytoolchain.h"
#include "ohosconstants.h"

#include <QTranslator>

namespace Ohos::Internal {

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

        connect(KitManager::instance(), &KitManager::kitsLoaded, this, &OhosPlugin::kitsRestored,
                Qt::SingleShotConnection);
        connect(ProjectManager::instance(), &ProjectManager::activeBuildConfigurationChanged,
            this, &OhosPlugin::addBuildHapStepForOhBuild);
    }

    void kitsRestored()
    {
        // Add code here to handle kits being restored
        HarmonyConfigurations::registerNewToolchains();
        // HarmonyConfigurations::registerQtVersions();
        HarmonyConfigurations::updateAutomaticKitList();
        connect(QtSupport::QtVersionManager::instance(), &QtSupport::QtVersionManager::qtVersionsChanged,
                HarmonyConfigurations::instance(), [] {
                    HarmonyConfigurations::registerNewToolchains();
                    HarmonyConfigurations::updateAutomaticKitList();
                });

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
                Core::MessageManager::writeSilently(QString("Add Harmony Build Hap Step to BuildConfiguration: %1")
                                                   .arg(ohQt->displayName()));
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
