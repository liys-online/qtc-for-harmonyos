#include "harmonybuildconfiguration.h"
#include "ohostr.h"
#include "ohosconstants.h"
#include <cmakeprojectmanager/cmakebuildconfiguration.h>
#include <cmakeprojectmanager/cmakeprojectconstants.h>
namespace Ohos::Internal {

class HarmonyCMakeBuildConfiguration final : public CMakeProjectManager::CMakeBuildConfiguration
{
public:
    HarmonyCMakeBuildConfiguration(ProjectExplorer::Target *target, Utils::Id id);
private:
    void addSubConfigWidgets(const BuildConfiguration::WidgetAdder &adder) final;

};

HarmonyCMakeBuildConfiguration::HarmonyCMakeBuildConfiguration(ProjectExplorer::Target *target, Utils::Id id)
    : CMakeBuildConfiguration(target, id)
{

}

void HarmonyCMakeBuildConfiguration::addSubConfigWidgets(const WidgetAdder &adder)
{
    // Ownership of this widget is with BuildSettingsWidget
    adder(new QLabel("Harmony Settings"),
          Tr::tr("Harmony Settings"));
    // CMakeBuildConfiguration::addSubConfigWidgets(adder);
}

class HarmonyCMakeBuildConfigurationFactory final : public CMakeProjectManager::CMakeBuildConfigurationFactory
{
public:
    HarmonyCMakeBuildConfigurationFactory()
    {
        registerBuildConfiguration<HarmonyCMakeBuildConfiguration>(
            CMakeProjectManager::Constants::CMAKE_BUILDCONFIGURATION_ID);
        addSupportedTargetDeviceType(Constants::HARMONY_DEVICE_TYPE);
    }
};

// Factories
void setupHarmonyBuildConfiguration()
{
    static HarmonyCMakeBuildConfigurationFactory theHarmonyCMakeBuildConfigurationFactory;
}



}
