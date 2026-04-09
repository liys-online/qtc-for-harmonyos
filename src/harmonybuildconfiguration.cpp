#include "harmonybuildconfiguration.h"
#include "harmonyutils.h"
#include "ohostr.h"
#include "ohosconstants.h"

#include <cmakeprojectmanager/cmakebuildconfiguration.h>
#include <cmakeprojectmanager/cmakeconfigitem.h>
#include <cmakeprojectmanager/cmakekitaspect.h>
#include <cmakeprojectmanager/cmakeprojectconstants.h>

#include <coreplugin/icore.h>

#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/buildsystem.h>
#include <projectexplorer/kit.h>
#include <projectexplorer/projectexplorerconstants.h>

#include <qtsupport/qtkitaspect.h>

#include <utils/filepath.h>
#include <utils/id.h>
#include <utils/layoutbuilder.h>

#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>

using namespace ProjectExplorer;
using namespace Utils;

namespace Ohos::Internal {

namespace {

static QString cmakeKitCacheValue(const Kit *kit, const QByteArray &key)
{
    if (!kit)
        return {};
    const CMakeProjectManager::CMakeConfig cfg = CMakeProjectManager::CMakeConfigurationKitAspect::configuration(kit);
    const CMakeProjectManager::CMakeConfigItem item = cfg.value(key);
    if (item.isNull())
        return {};
    return item.expandedValue(kit);
}

class HarmonyCMakeSummaryWidget final : public QWidget
{
public:
    explicit HarmonyCMakeSummaryWidget(BuildConfiguration *bc);

private:
    void refresh();
    void showEvent(QShowEvent *event) override;

    QPointer<BuildConfiguration> m_bc;
    QLabel *m_qtLabel = nullptr;
    QLabel *m_ndkLabel = nullptr;
    QLabel *m_kitOhosArchLabel = nullptr;
    QLabel *m_projectOhosArchLabel = nullptr;
    QLabel *m_toolchainLabel = nullptr;
};

HarmonyCMakeSummaryWidget::HarmonyCMakeSummaryWidget(BuildConfiguration *bc)
    : m_bc(bc)
{
    setWindowTitle(Tr::tr("Harmony"));

    m_qtLabel = new QLabel(this);   // NOSONAR (cpp:S5025) - parented, will auto-delete
    m_qtLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_qtLabel->setWordWrap(true);

    m_ndkLabel = new QLabel(this);  // NOSONAR (cpp:S5025) - parented, will auto-delete
    m_ndkLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_ndkLabel->setWordWrap(true);

    m_kitOhosArchLabel = new QLabel(this);  // NOSONAR (cpp:S5025) - parented, will auto-delete
    m_kitOhosArchLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);

    m_projectOhosArchLabel = new QLabel(this);  // NOSONAR (cpp:S5025) - parented, will auto-delete
    m_projectOhosArchLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_projectOhosArchLabel->setWordWrap(true);

    m_toolchainLabel = new QLabel(this);  // NOSONAR (cpp:S5025) - parented, will auto-delete
    m_toolchainLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_toolchainLabel->setWordWrap(true);

    auto *harmonyPrefs = new QPushButton(Tr::tr("Open Harmony Settings…"), this);  // NOSONAR (cpp:S5025)
    harmonyPrefs->setToolTip(
        Tr::tr("Open Harmony preferences (SDK paths, DevEco, ohpm, default device types)."));
    connect(harmonyPrefs, &QPushButton::clicked, this, [] {
        Core::ICore::showSettings(Constants::HARMONY_SETTINGS_ID);
    });

    auto *kitPrefs = new QPushButton(Tr::tr("Configure Current Kit…"), this);  // NOSONAR (cpp:S5025)
    kitPrefs->setToolTip(Tr::tr("Open the Kits options page for this build configuration's kit."));
    connect(kitPrefs, &QPushButton::clicked, this, [this] {
        if (!m_bc)
            return;
        if (Kit *k = m_bc->kit())
            Core::ICore::showSettings(ProjectExplorer::Constants::KITS_SETTINGS_PAGE_ID, k->id());
    });

    auto *refreshBtn = new QPushButton(Tr::tr("Refresh"), this);  // NOSONAR (cpp:S5025) - parented, will auto-delete
    refreshBtn->setToolTip(
        Tr::tr("Reload labels from the kit and CMake project data (for example after configuring CMake)."));
    connect(refreshBtn, &QPushButton::clicked, this, [this] { refresh(); });

    using namespace Layouting;
    Column {
        Group {
            title(Tr::tr("Harmony CMake summary")),
            Form {
                Tr::tr("Qt version:"), m_qtLabel, br,
                Tr::tr("OpenHarmony NDK (Kit):"), m_ndkLabel, br,
                Tr::tr("OHOS_ARCH (Kit CMake):"), m_kitOhosArchLabel, br,
                Tr::tr("OHOS_ARCH (CMake target / project):"), m_projectOhosArchLabel, br,
                Tr::tr("CMAKE_TOOLCHAIN_FILE (Kit):"), m_toolchainLabel, br,
            },
        },
        Row { harmonyPrefs, kitPrefs, refreshBtn, st },
        st,
    }.attachTo(this);

    if (m_bc)
        connect(m_bc.data(), &BuildConfiguration::kitChanged, this, [this] { refresh(); });
}

void HarmonyCMakeSummaryWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refresh();
}

void HarmonyCMakeSummaryWidget::refresh()
{
    if (!m_bc) {
        m_qtLabel->setText(Tr::tr("No build configuration."));
        return;
    }

    Kit *kit = m_bc->kit();
    if (!kit) {
        m_qtLabel->setText(Tr::tr("No kit."));
        m_ndkLabel->clear();
        m_kitOhosArchLabel->clear();
        m_projectOhosArchLabel->clear();
        m_toolchainLabel->clear();
        return;
    }

    if (const QtSupport::QtVersion *qv = QtSupport::QtKitAspect::qtVersion(kit)) {
        m_qtLabel->setText(qv->displayName());
        m_qtLabel->setToolTip(qv->qmakeFilePath().toUserOutput());
    } else {
        m_qtLabel->setText(Tr::tr("No Qt version is set for this kit."));
        m_qtLabel->setToolTip({});
    }

    const FilePath ndk = FilePath::fromSettings(kit->value(Id(Constants::HARMONY_KIT_NDK)));
    if (ndk.isEmpty()) {
        m_ndkLabel->setText(Tr::tr("Not set"));
        m_ndkLabel->setToolTip({});
    } else {
        m_ndkLabel->setText(ndk.toUserOutput());
        m_ndkLabel->setToolTip(ndk.toUserOutput());
    }

    const QString kitArch = cmakeKitCacheValue(kit, QByteArrayLiteral("OHOS_ARCH"));
    if (kitArch.isEmpty()) {
        m_kitOhosArchLabel->setText(Tr::tr("Not set in Kit CMake configuration"));
        m_kitOhosArchLabel->setToolTip(
            Tr::tr("Harmony kits normally set OHOS_ARCH automatically. Check Kit → CMake configuration."));
    } else {
        m_kitOhosArchLabel->setText(kitArch);
        m_kitOhosArchLabel->setToolTip(kitArch);
    }

    QString projArch = kit->value(Constants::OHOS_ARCH).toString();
    if (projArch.isEmpty()) {
        m_projectOhosArchLabel->setText(
            Tr::tr("Not available (configure CMake or select an application target)"));
        m_projectOhosArchLabel->setToolTip({});
    } else {
        m_projectOhosArchLabel->setText(projArch);
        m_projectOhosArchLabel->setToolTip(projArch);
    }

    const QString toolchain = cmakeKitCacheValue(kit, QByteArrayLiteral("CMAKE_TOOLCHAIN_FILE"));
    if (toolchain.isEmpty()) {
        m_toolchainLabel->setText(Tr::tr("Not set in Kit CMake configuration"));
        m_toolchainLabel->setToolTip({});
    } else {
        m_toolchainLabel->setText(toolchain);
        m_toolchainLabel->setToolTip(toolchain);
    }
}

} // namespace

class HarmonyCMakeBuildConfiguration final : public CMakeProjectManager::CMakeBuildConfiguration
{
public:
    HarmonyCMakeBuildConfiguration(Target *target, Id id);

private:
    QList<QWidget *> createSubConfigWidgets() final;
};

HarmonyCMakeBuildConfiguration::HarmonyCMakeBuildConfiguration(Target *target, Id id)
    : CMakeBuildConfiguration(target, id)
{}

QList<QWidget *> HarmonyCMakeBuildConfiguration::createSubConfigWidgets()
{
    QList<QWidget *> result;
    result.append(new HarmonyCMakeSummaryWidget(this));
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

void setupHarmonyBuildConfiguration()
{
    static HarmonyCMakeBuildConfigurationFactory theHarmonyCMakeBuildConfigurationFactory;
}

} // namespace Ohos::Internal
