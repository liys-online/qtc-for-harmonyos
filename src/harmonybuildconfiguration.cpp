#include "harmonybuildconfiguration.h"
#include "ohostr.h"
#include "ohosconstants.h"
#include <cmakeprojectmanager/cmakebuildconfiguration.h>
#include <cmakeprojectmanager/cmakeprojectconstants.h>

#include <QLabel>

namespace Ohos::Internal {

class HarmonyCMakeBuildConfiguration final : public CMakeProjectManager::CMakeBuildConfiguration
{
public:
    HarmonyCMakeBuildConfiguration(ProjectExplorer::Target *target, Utils::Id id);

private:
    QList<QWidget *> createSubConfigWidgets() final;
};

HarmonyCMakeBuildConfiguration::HarmonyCMakeBuildConfiguration(ProjectExplorer::Target *target, Utils::Id id)
    : CMakeBuildConfiguration(target, id)
{

}

QList<QWidget *> HarmonyCMakeBuildConfiguration::createSubConfigWidgets()
{
    QList<QWidget *> result;
    auto *harmonyLabel = new QLabel(Tr::tr("Harmony Settings"));
    harmonyLabel->setObjectName(QStringLiteral("HarmonyBuildConfigurationSettingsLabel"));
    result.append(harmonyLabel);
    result.append(CMakeProjectManager::CMakeBuildConfiguration::createSubConfigWidgets());
    return result;
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
