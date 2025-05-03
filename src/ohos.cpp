// #include "ohosconstants.h"
// #include "ohostr.h"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/icontext.h>
#include <coreplugin/icore.h>
#include <projectexplorer/kitmanager.h>
#include <extensionsystem/iplugin.h>

#include <QTranslator>
#include "harmonysettingswidget.h"
#include "harmonyconfigurations.h"
#include "harmonydevice.h"
#include "harmonyqtversion.h"
#include "harmonytoolchain.h"
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
        loadTranslations();
        setupHarmonyConfigurations();
        setupHarmonyDevice();
        setupHarmonyQtVersion();
        setupHarmonySettingsPage();
        setupHarmonyToolchain();
        connect(KitManager::instance(), &KitManager::kitsLoaded, this, &OhosPlugin::kitsRestored,
                Qt::SingleShotConnection);
    }

    void kitsRestored()
    {
        // Add code here to handle kits being restored
        HarmonyConfigurations::registerNewToolchains();
        HarmonyConfigurations::registerQtVersions();
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

// private:
//     void triggerAction()
//     {
//         QMessageBox::information(Core::ICore::mainWindow(),
//                                  Tr::tr("Action Triggered"),
//                                  Tr::tr("This is an action from Ohos."));
//     }
};

} // namespace Ohos::Internal

#include <ohos.moc>
