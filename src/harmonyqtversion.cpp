#include "harmonyqtversion.h"
#include "harmonyconfigurations.h"
#include "ohosconstants.h"
#include <utils/filepath.h>
#include <utils/environment.h>
#include "ohostr.h"
using namespace Utils;
namespace Ohos::Internal {
HarmonyQtVersion::HarmonyQtVersion() {}

bool HarmonyQtVersion::supportsMultipleQtAbis() const
{
    return qtVersion() == QVersionNumber(5, 15, 12);
}

void HarmonyQtVersion::addToBuildEnvironment(const ProjectExplorer::Kit *k, Utils::Environment &env) const
{
    QtVersion::addToBuildEnvironment(k, env);
    const FilePath sdkLocation = HarmonyConfig::sdkLocation();
    if (sdkLocation.exists())
    {
        const FilePath sdkPath = sdkLocation.pathAppended("default/openharmony");
        env.set(Constants::OHOS_SDK_ENV_VAR, sdkPath.toUserOutput());
    }
}

QSet<Id> HarmonyQtVersion::targetDeviceTypes() const
{
    return {Id(Constants::HARMONY_DEVICE_TYPE)};
}

QString HarmonyQtVersion::description() const
{
    return Tr::tr("HarmonyOS");
}

QString HarmonyQtVersion::defaultUnexpandedDisplayName() const
{
    return Tr::tr("Qt %{Qt:Version} for HarmonyOS");
}

// Factory

class HarmonyQtVersionFactory : public QtSupport::QtVersionFactory
{
public:
    HarmonyQtVersionFactory()
    {
        setQtVersionCreator([] { return new HarmonyQtVersion; });
        setSupportedType(Constants::HARMONY_QT_TYPE);
        setPriority(90);

        // setRestrictionChecker([](const SetupData &setup) {
        //     return !setup.config.contains("harmony-no-sdk")
        //     && (setup.config.contains("harmony")
        //        || setup.platforms.contains("harmony"));
        // });
    }
};

void setupHarmonyQtVersion()
{
    static HarmonyQtVersionFactory factory;
}

} // Ohos::Internal
