#include "harmonybuildhapstep.h"
#include "harmonyhvigoroutputparser.h"
#include "harmonylogcategories.h"
#include "harmonyqtversion.h"
#include "ohosconstants.h"
#include "ohostr.h"
#include "harmonyutils.h"
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/buildstep.h>
#include <projectexplorer/task.h>
#include <projectexplorer/buildsteplist.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/processparameters.h>
#include <projectexplorer/buildsystem.h>
#include <projectexplorer/runconfigurationaspects.h>
#include <projectexplorer/runcontrol.h>
#include <projectexplorer/buildconfiguration.h>

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QGridLayout>
#include <QHash>
#include <QSignalBlocker>
#include <QUrl>
#include <QWidget>
#include <qpushbutton.h>
#include <utils/algorithm.h>
#include <utils/layoutbuilder.h>
#include <utils/qtcprocess.h>
#include <utils/outputformatter.h>
#include <qtsupport/qtkitaspect.h>

#include <ohprojectecreator/ohprojectecreator.h>

#include "harmonyconfigurations.h"

#include <projectexplorer/kit.h>
#include <utils/id.h>
#include <QVariant>

/* ** 勿在此 using namespace QtTaskTree：会与 Layouting::Group（layoutbuilder）歧义。 */
using namespace Utils;
using ProjectExplorer::BuildSystemTask;

namespace Ohos::Internal {
namespace {

QString harmonyNodeMissingUserHint()
{
    return Tr::tr(
        "Could not find Node.js (hvigor/ohpm require it). "
        "Set the DevEco Studio path in Harmony preferences (bundled Node is often "
        "…/tools/node/bin/node), or install Node.js and ensure \"node\" is on your PATH "
        "when Qt Creator starts.");
}

void applyDevecoAndJavaEnv(Environment &evn)
{
    const FilePath deveco = HarmonyConfig::devecoStudioLocation();
    if (!deveco.isEmpty())
        evn.set("DEVECO_SDK_HOME", deveco.toFSPathString());

    const FilePath javaHome = HarmonyConfig::javaLocation();
    const FilePath javaExe = (javaHome / "bin" / "java").withExecutableSuffix();
    if (!javaHome.isEmpty() && javaExe.isExecutableFile()) {
        evn.set("JAVA_HOME", javaHome.toFSPathString());
        evn.appendOrSetPath({javaHome / "bin"});
    }
}

void applyHvigorWorkingDirectoryEnv(Environment &evn, const FilePath &absoluteCwd)
{
    if (absoluteCwd.isEmpty())
        return;
    const QString p = absoluteCwd.nativePath();
    evn.set(QStringLiteral("PWD"), p);
    /* ** Node/libuv 与部分 hvigor 子进程依赖 cwd；与 PWD 一致可减少 macOS 上 uv_cwd EPERM */
    evn.set(QStringLiteral("INIT_CWD"), p);
}
} // namespace
[[maybe_unused]] static void createOhPro(ProjectExplorer::BuildSystem *buildsystem, const QString &path)
{
    using namespace QtSupport;
    if (!buildsystem)
        return;
    auto *ohPro = OhProjecteCreator::instance();
    OhProjecteCreator::ProjecteInfo proInfo;
    proInfo.projectPath = path;
    proInfo.targetSdkVersion = HarmonyConfig::devecoStudioVersion().first;
    if(auto *project = buildsystem->project())
    {
        QString projFile = project->projectFilePath().toUserOutput();
        if(projFile.endsWith("CMakeLists.txt")) {
            proInfo.cmakeListPath = projFile;
        }
    }
    if(auto *kit = buildsystem->kit())
    {
        auto ohQt = static_cast<const HarmonyQtVersion *>(QtKitAspect::qtVersion(kit));
        if(ohQt) {
            proInfo.compatibleSdkVersion = ohQt->supportOhVersion().majorVersion();
            proInfo.qtHostPath = ohQt->hostPrefixPath().toUserOutput();
        }
    }
    proInfo.deviceTypes = HarmonyConfig::ohModuleDeviceTypes();
    ohPro->create(proInfo);
}
class HarmonyBuildHapWidget : public QWidget
{
public:
    explicit HarmonyBuildHapWidget(HarmonyBuildHapStep *step)
        : QWidget{},
        m_step{step}
    {
        qCDebug(harmonyBuildLog) << "HarmonyBuildHapWidget setup";
        QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);

        using namespace Layouting;
        using namespace Utils;

        // Application Signature Group

        // auto keystoreLocationChooser = new PathChooser;
        // keystoreLocationChooser->setExpectedKind(PathChooser::File);
        // keystoreLocationChooser->lineEdit()->setReadOnly(true);
        // keystoreLocationChooser->setFilePath(m_step->keystorePath());
        // keystoreLocationChooser->setInitialBrowsePathBackup(FileUtils::homePath());
        // keystoreLocationChooser->setPromptDialogFilter(Tr::tr("Keystore files (*.keystore *.jks)"));
        // keystoreLocationChooser->setPromptDialogTitle(Tr::tr("Select Keystore File"));
        // connect(keystoreLocationChooser, &PathChooser::textChanged, this, [this, keystoreLocationChooser] {
        //     const FilePath file = keystoreLocationChooser->unexpandedFilePath();
        //     m_step->setKeystorePath(file);
        //     m_signPackageCheckBox->setChecked(!file.isEmpty());
        //     if (!file.isEmpty())
        //         setCertificates();
        // });

        // auto keystoreCreateButton = new QPushButton(Tr::tr("Create..."));
        // connect(keystoreCreateButton, &QAbstractButton::clicked, this, [this, keystoreLocationChooser] {
        //     const auto data = executeKeystoreCertificateDialog();
        //     if (!data)
        //         return;
        //     keystoreLocationChooser->setFilePath(data->keystoreFilePath);
        //     m_step->setKeystorePath(data->keystoreFilePath);
        //     m_step->setKeystorePassword(data->keystorePassword);
        //     m_step->setCertificateAlias(data->certificateAlias);
        //     m_step->setCertificatePassword(data->certificatePassword);
        //     setCertificates();
        // });

        // m_signPackageCheckBox = new QCheckBox(Tr::tr("Sign package"));
        // m_signPackageCheckBox->setChecked(m_step->signPackage());

        // m_signingDebugWarningLabel = new InfoLabel(Tr::tr("Signing a debug package"),
        //                                            InfoLabel::Warning);
        // m_signingDebugWarningLabel->hide();
        // m_signingDebugWarningLabel->setSizePolicy(QSizePolicy::MinimumExpanding,
        //                                           QSizePolicy::Preferred);

        // m_certificatesAliasComboBox = new QComboBox;
        // m_certificatesAliasComboBox->setEnabled(false);
        // m_certificatesAliasComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);

        using namespace Layouting;
        PushButton openDevEcoButton {
            text(Tr::tr("Signing the HAP in DevEco Studio")),
            Layouting::toolTip(Tr::tr("Processes Hvigor project information and persistently stores it in "
                                      "./hvigor/outputs/sync/output.json.")),
            onClicked(this, std::bind(&HarmonyBuildHapWidget::openDevEco, this))
        };
        Group signPackageGroup {
            title(Tr::tr("Application Signature")),
            Form {
                openDevEcoButton, br,
            //     Tr::tr("Keystore:"), keystoreLocationChooser, keystoreCreateButton, br,
            //     m_signPackageCheckBox, br,
            //     Tr::tr("Certificate alias:"), m_certificatesAliasComboBox,
            //     m_signingDebugWarningLabel, st, br,
            }
        };

        // connect(m_signPackageCheckBox, &QAbstractButton::toggled,
        //         this, &HarmonyBuildHapWidget::signPackageCheckBoxToggled);

        // auto updateAlias = [this](int idx) {
        //     QString alias = m_certificatesAliasComboBox->itemText(idx);
        //     if (!alias.isEmpty())
        //         m_step->setCertificateAlias(alias);
        // };

        // connect(m_certificatesAliasComboBox, &QComboBox::activated, this, updateAlias);
        // connect(m_certificatesAliasComboBox, &QComboBox::currentIndexChanged, this, updateAlias);


        /* ** Application group */

        const int devecoApiLevel = HarmonyConfig::devecoStudioVersion().first;
        QList<int> buildToolsVersions = OhProjecteCreator::availableApiLevels();
        QStringList versions = HarmonyConfig::apiLevelNamesFor(buildToolsVersions);
        versions.removeDuplicates();

        // 两个下拉框共享同一 API 等级列表
        m_targetSDKComboBox = new QComboBox(this);  // NOSONAR (cpp:S5025) - parented, will auto-delete
        m_buildToolsSdkComboBox = new QComboBox(this);  // NOSONAR (cpp:S5025) - parented, will auto-delete
        for (int i = 0; i < versions.size(); ++i) {
            const QVariant v = QVariant::fromValue(buildToolsVersions.at(i));
            m_targetSDKComboBox->addItem(versions.at(i), v);
            m_buildToolsSdkComboBox->addItem(versions.at(i), v);
        }

        // 默认选中 DevEco Studio 对应版本；有已保存值则恢复保存值
        const auto resolveInitIdx = [&](const QString &stored) {
            if (!stored.isEmpty()) {
                const auto idx = versions.indexOf(stored);
                if (idx >= 0)
                    return static_cast<int>(idx);
            }
            if (buildToolsVersions.isEmpty())
                return 0;
            const auto devecoIdx = buildToolsVersions.indexOf(devecoApiLevel);
            return static_cast<int>(devecoIdx >= 0 ? devecoIdx : versions.size() - 1);
        };
        m_targetSDKComboBox->setCurrentIndex(resolveInitIdx(m_step->buildTargetSdk()));
        m_buildToolsSdkComboBox->setCurrentIndex(resolveInitIdx(m_step->buildToolsVersion()));

        connect(m_targetSDKComboBox, &QComboBox::activated, this,
                [this](int idx) {
                    m_step->setBuildTargetSdk(m_targetSDKComboBox->itemText(idx));
                    OhProjecteCreator::updateBuildProfileSdkVersions(
                        m_step->ohProjectPath().toUserOutput(),
                        m_targetSDKComboBox->currentData().toInt(),
                        m_buildToolsSdkComboBox->currentData().toInt());
                });
        connect(m_buildToolsSdkComboBox, &QComboBox::activated, this,
                [this](int idx) {
                    m_step->setBuildToolsVersion(m_buildToolsSdkComboBox->itemText(idx));
                    OhProjecteCreator::updateBuildProfileSdkVersions(
                        m_step->ohProjectPath().toUserOutput(),
                        m_targetSDKComboBox->currentData().toInt(),
                        m_buildToolsSdkComboBox->currentData().toInt());
                });

        auto createHarmonyTemplatesButton = new QPushButton(Tr::tr("Create Templates"), this);   // NOSONAR (cpp:S5025)
        createHarmonyTemplatesButton->setToolTip(
            Tr::tr("Create an HarmonyOS package for Custom Java code, assets, and Gradle configurations."));
        connect(createHarmonyTemplatesButton, &QAbstractButton::clicked,
                this, [this] { createTemplatesFromUi(); });
        connect(m_step, &HarmonyBuildHapStep::createTemplates, createHarmonyTemplatesButton, &QPushButton::click);

        m_entryLibEdit = new QLineEdit(this);  // NOSONAR (cpp:S5025) - parented, will auto-delete
        m_entryLibEdit->setPlaceholderText(m_step->resolvedEntryLib());
        m_entryLibEdit->setText(m_step->entryLibOverride());
        m_entryLibEdit->setToolTip(
            Tr::tr("Override the shared library name written into EntryAbility.ets "
                   "(e.g. libmyapp.so). Leave empty to auto-detect from the active build target."));
        connect(m_entryLibEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
            m_step->setEntryLibOverride(text);
            m_entryLibEdit->setPlaceholderText(m_step->resolvedEntryLib());
        });

        auto applyEntryLibButton = new QPushButton(Tr::tr("Apply"), this);  // NOSONAR (cpp:S5025)
        applyEntryLibButton->setToolTip(
            Tr::tr("Write the entry library name directly into the existing EntryAbility.ets "
                   "without re-creating all templates."));
        connect(applyEntryLibButton, &QAbstractButton::clicked, this, [this] {
            const QString ohproPath = m_step->ohProjectPath().toUserOutput();
            OhProjecteCreator::patchEntryAbilityLib(ohproPath, m_step->resolvedEntryLib());
        });

        m_followKitModuleDeviceTypes = new QCheckBox(  // NOSONAR (cpp:S5025) - parented, will auto-delete
            Tr::tr("Use Harmony preferences and Kit for module device types (no step override)"), this);
        m_followKitModuleDeviceTypes->setToolTip(
            Tr::tr("When checked, the build step does not store an override; effective device types use the "
                   "Kit field, then global Harmony preferences (empty preference means 2in1). Uncheck to pick "
                   "presets for this step only."));
        m_moduleDeviceTypesOverrideWidget = new QWidget(this);  // NOSONAR (cpp:S5025) - parented, will auto-delete
        {
            auto *grid = new QGridLayout(m_moduleDeviceTypesOverrideWidget);  // NOSONAR (cpp:S5025)
            grid->setContentsMargins(0, 0, 0, 0);
            setupDeviceTypeCheckBoxGrid(grid);
        }
        connect(m_followKitModuleDeviceTypes, &QCheckBox::toggled,
                this, &HarmonyBuildHapWidget::onFollowKitToggled);
        reloadModuleDeviceTypesUiFromStep();

        Group applicationGroup {
            title(Tr::tr("Application")),
            Form {
                Tr::tr("Harmony compatible Sdk Version:"), m_buildToolsSdkComboBox, br,
                Tr::tr("Harmony target Sdk Version:"), m_targetSDKComboBox, br,
                m_followKitModuleDeviceTypes, br,
                Tr::tr("Override module device types:"), m_moduleDeviceTypesOverrideWidget, br,
                Tr::tr("Entry library name:"), m_entryLibEdit, applyEntryLibButton, br,
                Tr::tr("Harmony customization:"), createHarmonyTemplatesButton,
            }
        };

        /* ** main layout */
        Column {
            signPackageGroup,
            applicationGroup,
            noMargin
        }.attachTo(this);
    }

    void openDevEco()
    {
        auto projectPath = m_step->ohProjectPath().toUserOutput();
        openDevEcoProject(projectPath);
    }

    void reloadModuleDeviceTypesUiFromStep()
    {
        if (!m_followKitModuleDeviceTypes || m_moduleDeviceTypeCheckBoxes.isEmpty())
            return;
        const bool followKit = m_step->moduleDeviceTypes().isEmpty();
        {
            const QSignalBlocker b(m_followKitModuleDeviceTypes);
            m_followKitModuleDeviceTypes->setChecked(followKit);
        }
        m_moduleDeviceTypesOverrideWidget->setEnabled(!followKit);
        const QStringList tokens = m_step->moduleDeviceTypes();
        for (const QString &id : ohModuleDeviceTypePresetIds()) {
            if (QCheckBox *cb = m_moduleDeviceTypeCheckBoxes.value(id)) {
                const QSignalBlocker b(cb);
                cb->setChecked(!followKit && tokens.contains(id));
            }
        }
    }

    void applyModuleDeviceTypesOverrideFromCheckBoxes()
    {
        QStringList out;
        for (const QString &id : ohModuleDeviceTypePresetIds()) {
            if (QCheckBox *cb = m_moduleDeviceTypeCheckBoxes.value(id))
                if (cb->isChecked())
                    out.append(id);
        }
        if (m_step->moduleDeviceTypes() != out) {
            m_step->setModuleDeviceTypes(out);
            const QString ohproPath = m_step->ohProjectPath().toUserOutput();
            OhProjecteCreator::updateModuleDeviceTypes(ohproPath, m_step->moduleDeviceTypes());
        }
    }

    void createTemplatesFromUi() const;
    void onFollowKitToggled(bool followKit);
    void setupDeviceTypeCheckBoxGrid(QGridLayout *grid);

private:
    HarmonyBuildHapStep *m_step = nullptr;
    QLineEdit *m_entryLibEdit = nullptr;
    QComboBox *m_targetSDKComboBox = nullptr;
    QComboBox *m_buildToolsSdkComboBox = nullptr;
    QCheckBox *m_followKitModuleDeviceTypes = nullptr;
    QWidget *m_moduleDeviceTypesOverrideWidget = nullptr;
    QHash<QString, QCheckBox *> m_moduleDeviceTypeCheckBoxes;
};

void HarmonyBuildHapWidget::createTemplatesFromUi() const
{
    using namespace QtSupport;
    const auto *buildsystem = m_step->buildSystem();
    if (!buildsystem)
        return;
    auto *ohPro = OhProjecteCreator::instance();
    OhProjecteCreator::ProjecteInfo proInfo;
    proInfo.projectPath = m_step->ohProjectPath().toUserOutput();
    proInfo.targetSdkVersion = m_targetSDKComboBox->currentData().toInt();
    if (proInfo.targetSdkVersion < 0)
        proInfo.targetSdkVersion = HarmonyConfig::devecoStudioVersion().first;
    if (const auto *project = buildsystem->project()) {
        const QString projFile = project->projectFilePath().toUserOutput();
        if (projFile.endsWith("CMakeLists.txt"))
            proInfo.cmakeListPath = projFile;
    }
    if (const auto *kit = buildsystem->kit()) {
        const auto *ohQt = static_cast<const HarmonyQtVersion *>(QtKitAspect::qtVersion(kit));
        if (ohQt) {
            proInfo.compatibleSdkVersion = m_buildToolsSdkComboBox->currentData().toInt();
            if (proInfo.compatibleSdkVersion < 0)
                proInfo.compatibleSdkVersion = ohQt->supportOhVersion().majorVersion();
            proInfo.qtHostPath = ohQt->hostPrefixPath().toUserOutput();
        }
    }
    proInfo.deviceTypes = m_step->effectiveModuleDeviceTypes();
    proInfo.entrylib = m_step->resolvedEntryLib();
    ohPro->create(proInfo);
}

void HarmonyBuildHapWidget::onFollowKitToggled(bool followKit)
{
    if (followKit) {
        m_step->setModuleDeviceTypes(QStringList{});
        for (QCheckBox *cb : std::as_const(m_moduleDeviceTypeCheckBoxes)) {
            if (cb) {
                const QSignalBlocker b(cb);
                cb->setChecked(false);
            }
        }
        m_moduleDeviceTypesOverrideWidget->setEnabled(false);
    } else {
        m_moduleDeviceTypesOverrideWidget->setEnabled(true);
        const QStringList eff = m_step->effectiveModuleDeviceTypes();
        for (const QString &id : ohModuleDeviceTypePresetIds()) {
            if (QCheckBox *cb = m_moduleDeviceTypeCheckBoxes.value(id)) {
                const QSignalBlocker b(cb);
                cb->setChecked(eff.contains(id));
            }
        }
        applyModuleDeviceTypesOverrideFromCheckBoxes();
    }
}

void HarmonyBuildHapWidget::setupDeviceTypeCheckBoxGrid(QGridLayout *grid)
{
    int i = 0;
    for (const QString &id : ohModuleDeviceTypePresetIds()) {
        auto *cb = new QCheckBox(id, this);  // NOSONAR (cpp:S5025) - parented, will auto-delete
        m_moduleDeviceTypeCheckBoxes.insert(id, cb);
        grid->addWidget(cb, i / 3, i % 3);
        ++i;
        connect(cb, &QCheckBox::toggled, this, [this] {
            if (!m_followKitModuleDeviceTypes->isChecked())
                applyModuleDeviceTypesOverrideFromCheckBoxes();
        });
    }
}

namespace {
void migrateLegacyHarmonyBuildHapMap(Utils::Store &m)
{
    const Key newTs(Constants::HarmonyBuildHapTargetSdkKey);
    const Key oldTs("BuildTargetSdk");
    if (!m.contains(newTs) && m.contains(oldTs))
        m.insert(newTs, m.value(oldTs));
    const Key newTv(Constants::HarmonyBuildHapBuildToolsVersionKey);
    const Key oldTv("BuildToolsVersion");
    if (!m.contains(newTv) && m.contains(oldTv))
        m.insert(newTv, m.value(oldTv));
}
} // namespace

HarmonyBuildHapStep::HarmonyBuildHapStep(ProjectExplorer::BuildStepList *bc, Utils::Id id)
    : AbstractProcessStep(bc, id)
{
    qCDebug(harmonyBuildLog) << "HarmonyBuildHapStep setup";
}

void HarmonyBuildHapStep::fromMap(const Utils::Store &map)
{
    Utils::Store m = map;
    migrateLegacyHarmonyBuildHapMap(m);
    m_buildTargetSdk = m.value(Key(Constants::HarmonyBuildHapTargetSdkKey)).toString();
    m_buildToolsVersion = m.value(Key(Constants::HarmonyBuildHapBuildToolsVersionKey)).toString();
    m_entryLibOverride = m.value(Key(Constants::HarmonyBuildHapEntryLibOverrideKey)).toString();
    m_ohModuleDeviceTypes = m.value(Constants::HarmonyBuildOhModuleDeviceTypesLine).toStringList();
    if (m_buildTargetSdk.isEmpty()) {
        m_buildTargetSdk = HarmonyConfig::apiLevelNameFor(HarmonyConfig::devecoStudioVersion().first);
    }
    ProjectExplorer::BuildStep::fromMap(m);

}

void HarmonyBuildHapStep::toMap(Utils::Store &map) const
{
    ProjectExplorer::AbstractProcessStep::toMap(map);
    map.insert(Key(Constants::HarmonyBuildHapTargetSdkKey), m_buildTargetSdk);
    map.insert(Key(Constants::HarmonyBuildHapBuildToolsVersionKey), m_buildToolsVersion);
    map.insert(Key(Constants::HarmonyBuildHapEntryLibOverrideKey), m_entryLibOverride);
    map.insert(Constants::HarmonyBuildOhModuleDeviceTypesLine, m_ohModuleDeviceTypes);
}

FilePath HarmonyBuildHapStep::ohProjectPath() const
{
    return (buildDirectory().cleanPath() / "ohpro").cleanPath();
}

bool HarmonyBuildHapStep::prepareOhProDirectory(FilePath *outCwd, QString *errorMessage)
{
    if (!outCwd || !errorMessage)
        return false;
    *outCwd = {};
    const FilePath ohPro = ohProjectPath();
    if (const Result<> mk = ohPro.ensureWritableDir(); !mk) {
        *errorMessage = Tr::tr("Could not create Harmony OHOS wrapper directory \"%1\". %2")
                            .arg(ohPro.toUserOutput(), mk.error());
        return false;
    }
    if (!ohPro.isReadableDir()) {
        *errorMessage = Tr::tr("Harmony project directory is not accessible: \"%1\"")
                            .arg(ohPro.toUserOutput());
        return false;
    }
    FilePath cwd = ohPro.canonicalPath();
    if (cwd.isEmpty()) {
        cwd = ohPro.absoluteFilePath();
        if (cwd.isEmpty())
            cwd = ohPro;
    }
    if (!cwd.isReadableDir()) {
        *errorMessage = Tr::tr("Could not resolve a usable absolute path for \"%1\".")
                            .arg(ohPro.toUserOutput());
        return false;
    }
    *outCwd = cwd;
    return true;
}

QtTaskTree::GroupItem HarmonyBuildHapStep::hvigorBuildTask()
{
    const auto onSetup = [this](Process &process) {
        return setupHvigorProcess(process) ? QtTaskTree::SetupResult::Continue
                                     : QtTaskTree::SetupResult::StopWithError;
    };
    const auto onDone = [this](const Process &process) { handleProcessDone(process); };
    return ProcessTask(onSetup, onDone);
}

QtTaskTree::GroupItem HarmonyBuildHapStep::syncProjectTask()
{
    const auto onSyncProject = [this] (Process &process){
        if (const FilePath node = HarmonyConfig::nodeLocation(); node.isExecutableFile())
        {
            if (auto hvigorwJs = HarmonyConfig::hvigorwJsLocation(); hvigorwJs.exists())
            {
                const CommandLine command {node, {hvigorwJs.toUserOutput(),
                                                 "--sync",
                                                 "-p",
                                                 "product=default",
                                                 "--analyze=normal",
                                                 "--parallel",
                                                 "--incremental",
                                                 "--no_daemon",
                                                 }};

                FilePath ohProCwd;
                QString prepErr;
                if (!prepareOhProDirectory(&ohProCwd, &prepErr)) {
                    emit addOutput(prepErr, OutputFormat::ErrorMessage);
                    emit addTask(BuildSystemTask(ProjectExplorer::Task::Error, prepErr));
                    return QtTaskTree::SetupResult::StopWithError;
                }

                process.setUseCtrlCStub(HostOsInfo::isWindowsHost());
                process.setWorkingDirectory(ohProCwd);
                auto evn = buildEnvironment();
                applyDevecoAndJavaEnv(evn);
                applyHvigorWorkingDirectoryEnv(evn, ohProCwd);
                process.setEnvironment(evn);
                process.setCommand(command);
                if (buildEnvironment().hasKey("VSLANG"))
                    process.setUtf8StdOutCodec();

                process.setStdOutCallback([this](const QString &s){
                    emit addOutput(s, OutputFormat::Stdout, DontAppendNewline);
                });

                process.setStdErrCallback([this](const QString &s){
                    emit addOutput(s, OutputFormat::Stderr, DontAppendNewline);
                });

                connect(&process, &Process::started, this, [this, &process] {
                    emit addOutput(Tr::tr("Starting: \"%1\"").arg(process.commandLine().toUserOutput()),
                                   OutputFormat::NormalMessage);
                });
                return QtTaskTree::SetupResult::Continue;
            }
            else
            {
                const QString err = Tr::tr("Could not find hvigorw.js file, "
                                           "please check your HarmonyOS SDK configuration.");
                emit addOutput(err, OutputFormat::ErrorMessage);
                emit addTask(BuildSystemTask(ProjectExplorer::Task::Error, err));
                return QtTaskTree::SetupResult::StopWithError;
            }
        }
        else
        {
            const QString err = harmonyNodeMissingUserHint();
            emit addOutput(err, OutputFormat::ErrorMessage);
            emit addTask(BuildSystemTask(ProjectExplorer::Task::Error, err));
            return QtTaskTree::SetupResult::StopWithError;
        }
    };

    const auto onSyncProjectDone = [this](const Process &process) { handleProcessDone(process); };
    return ProcessTask(onSyncProject, onSyncProjectDone);
}

QtTaskTree::GroupItem HarmonyBuildHapStep::ohpmInstallTask()
{
    const auto onOhpmInstall = [this] (Process &process){
        if (const FilePath node = HarmonyConfig::nodeLocation(); node.isExecutableFile())
        {
            if (auto ohpmJs = HarmonyConfig::ohpmJsLocation(); ohpmJs.exists())
            {
                const QString registryStr = HarmonyConfig::effectiveOhpmRegistryUrl();
                const QUrl registryUrl = QUrl::fromUserInput(registryStr);
                if (!registryUrl.isValid() || registryUrl.scheme().isEmpty()) {
                    const QString err = Tr::tr("Invalid ohpm registry URL in Harmony preferences: \"%1\"")
                                            .arg(registryStr);
                    emit addOutput(err, OutputFormat::ErrorMessage);
                    emit addTask(BuildSystemTask(ProjectExplorer::Task::Error, err));
                    return QtTaskTree::SetupResult::StopWithError;
                }
                const bool strictSsl = HarmonyConfig::ohpmStrictSsl();
                const CommandLine command{node,
                                          {ohpmJs.toUserOutput(),
                                           QStringLiteral("install"),
                                           QStringLiteral("--all"),
                                           QStringLiteral("--registry"),
                                           registryUrl.toString(),
                                           QStringLiteral("--strict_ssl"),
                                           strictSsl ? QStringLiteral("true") : QStringLiteral("false")}};

                FilePath ohProCwd;
                QString prepErr;
                if (!prepareOhProDirectory(&ohProCwd, &prepErr)) {
                    emit addOutput(prepErr, OutputFormat::ErrorMessage);
                    emit addTask(BuildSystemTask(ProjectExplorer::Task::Error, prepErr));
                    return QtTaskTree::SetupResult::StopWithError;
                }

                process.setUseCtrlCStub(HostOsInfo::isWindowsHost());
                process.setWorkingDirectory(ohProCwd);
                auto evn = buildEnvironment();
                applyDevecoAndJavaEnv(evn);
                applyHvigorWorkingDirectoryEnv(evn, ohProCwd);
                process.setEnvironment(evn);
                process.setCommand(command);
                if (buildEnvironment().hasKey("VSLANG"))
                    process.setUtf8StdOutCodec();

                process.setStdOutCallback([this](const QString &s){
                    emit addOutput(s, OutputFormat::Stdout, DontAppendNewline);
                });

                process.setStdErrCallback([this](const QString &s){
                    emit addOutput(s, OutputFormat::Stderr, DontAppendNewline);
                });

                connect(&process, &Process::started, this, [&process, this] {
                    emit addOutput(Tr::tr("Starting: \"%1\"").arg(process.commandLine().toUserOutput()),
                                   OutputFormat::NormalMessage);
                });
                return QtTaskTree::SetupResult::Continue;
            }
            else
            {
                const QString err = Tr::tr("Could not find ohpm executable, "
                                           "please check your HarmonyOS SDK configuration.");
                emit addOutput(err, OutputFormat::ErrorMessage);
                emit addTask(BuildSystemTask(ProjectExplorer::Task::Error, err));
                return QtTaskTree::SetupResult::StopWithError;
            }
        }
        else
        {
            const QString err = harmonyNodeMissingUserHint();
            emit addOutput(err, OutputFormat::ErrorMessage);
            emit addTask(BuildSystemTask(ProjectExplorer::Task::Error, err));
            return QtTaskTree::SetupResult::StopWithError;
        }
    };

    const auto onOhpmInstallDone = [this](const Process &process) { handleProcessDone(process); };
    return ProcessTask(onOhpmInstall, onOhpmInstallDone);
}

bool HarmonyBuildHapStep::setupHvigorProcess(Utils::Process &process)
{
    using namespace Utils;
    using namespace ProjectExplorer;
    auto *params = processParameters();
    FilePath workingDir = params->effectiveWorkingDirectory();
    if (!workingDir.exists() && !workingDir.createDir()) {
        emit addOutput(Tr::tr("Could not create directory \"%1\"").arg(workingDir.toUserOutput()),
                       OutputFormat::ErrorMessage);
        return false;
    }
    if (FilePath canonical = workingDir.canonicalPath(); !canonical.isEmpty())
        workingDir = canonical;
    if (!params->effectiveCommand().isExecutableFile()) {
        emit addOutput(Tr::tr("The program \"%1\" does not exist or is not executable.")
                           .arg(params->effectiveCommand().toUserOutput()),
                       OutputFormat::ErrorMessage);
        return false;
    }

    process.setUseCtrlCStub(HostOsInfo::isWindowsHost());
    process.setWorkingDirectory(workingDir);
    /*
    ** 在环境变量中强制设置 PWD，因为部分构建工具依赖此变量。
    ** PWD 可能与 getcwd 不同（后者会解析符号链接）。
    ** 例如，Clang 使用 PWD 生成调试信息中的路径，参见 QTCREATORBUG-23788。
    */
    Environment envWithPwd = params->environment();
    qCDebug(harmonyBuildLog) << "Environment PATH prepared";
    envWithPwd.set(QStringLiteral("PWD"), workingDir.nativePath());
    envWithPwd.set(QStringLiteral("INIT_CWD"), workingDir.nativePath());
    process.setProcessMode(params->processMode());
    // if (const auto runAsRoot = aspect<RunAsRootAspect>(); runAsRoot && runAsRoot->value()) {
    //     RunControl::provideAskPassEntry(envWithPwd);
    //     process.setRunAsRoot(true);
    // }
    process.setEnvironment(envWithPwd);
    process.setCommand({params->effectiveCommand(), params->effectiveArguments(),
                        CommandLine::Raw});

    if (buildEnvironment().hasKey("VSLANG"))
        process.setUtf8StdOutCodec();

    process.setStdOutCallback([this](const QString &s){
        emit addOutput(s, OutputFormat::Stdout, DontAppendNewline);
    });

    process.setStdErrCallback([this](const QString &s){
        emit addOutput(s, OutputFormat::Stderr, DontAppendNewline);
    });

    connect(&process, &Process::started, this, [params, this] {
        emit addOutput(Tr::tr("Starting: \"%1\" %2")
                           .arg(params->effectiveCommand().toUserOutput(), params->prettyArguments()),
                       OutputFormat::NormalMessage);
    });
    return true;
}

QString HarmonyBuildHapStep::buildTargetSdk() const
{
    return m_buildTargetSdk;
}

void HarmonyBuildHapStep::setBuildTargetSdk(const QString &sdk)
{
    if (m_buildTargetSdk != sdk) {
        m_buildTargetSdk = sdk;
    }
}

QString HarmonyBuildHapStep::buildToolsVersion() const
{
    return m_buildToolsVersion;
}

void HarmonyBuildHapStep::setBuildToolsVersion(const QString &version)
{
    if (m_buildToolsVersion != version) {
        m_buildToolsVersion = version;
    }
}

QString HarmonyBuildHapStep::entryLibOverride() const
{
    return m_entryLibOverride;
}

void HarmonyBuildHapStep::setEntryLibOverride(const QString &lib)
{
    m_entryLibOverride = lib;
}

QString HarmonyBuildHapStep::resolvedEntryLib() const
{
    if (!m_entryLibOverride.trimmed().isEmpty())
        return m_entryLibOverride.trimmed();
    const QString buildKey = buildConfiguration() ? buildConfiguration()->activeBuildKey() : QString();
    const bool isDefaultKey = buildKey.isEmpty() || buildKey == QLatin1String(Constants::HARMONY_DEFAULT_RUN_BUILD_KEY);
    return isDefaultKey ? QStringLiteral("libentry.so") : QString("lib%1.so").arg(buildKey);
}

QStringList HarmonyBuildHapStep::moduleDeviceTypes() const
{
    return m_ohModuleDeviceTypes;
}

void HarmonyBuildHapStep::setModuleDeviceTypes(const QStringList &deviceTypes)
{
    m_ohModuleDeviceTypes = deviceTypes;
}

QStringList HarmonyBuildHapStep::effectiveModuleDeviceTypes() const
{
    if (!m_ohModuleDeviceTypes.isEmpty())
        return m_ohModuleDeviceTypes;
    const auto *bc = buildConfiguration();
    const ProjectExplorer::Kit *k = bc ? bc->kit() : nullptr;
    if (k) {
        const QVariant v = k->value(Id(Constants::HARMONY_KIT_MODULE_DEVICE_TYPES));
        if (v.canConvert<QStringList>()) {
            const QStringList kl = v.toStringList();
            if (!kl.isEmpty())
                return kl;
        }
    }
    return HarmonyConfig::ohModuleDeviceTypes();
}

QWidget *HarmonyBuildHapStep::createConfigWidget()
{
    return new HarmonyBuildHapWidget(this);  // NOSONAR (cpp:S5025) - parented, will auto-delete
}

void HarmonyBuildHapStep::setupOutputFormatter(OutputFormatter *formatter)
{
#ifdef ENABLE_HVIGOR_OUTPUT_PARSING
    if (auto *bc = buildConfiguration()) {
        formatter->addSearchDir(harmonyBuildDirectory(bc));
        formatter->addSearchDir(bc->buildDirectory());
        if (ProjectExplorer::Project *proj = project())
            formatter->addSearchDir(proj->projectDirectory());
    }
    formatter->addLineParser(new HarmonyHvigorOhpmOutputParser());
#endif
    AbstractProcessStep::setupOutputFormatter(formatter);
}

bool HarmonyBuildHapStep::init()
{
    if (!AbstractProcessStep::init()) {
        const QString err = Tr::tr("\"%1\" step failed initialization.").arg(displayName());
        emit addOutput(err, OutputFormat::ErrorMessage);
        emit addTask(BuildSystemTask(ProjectExplorer::Task::Error, err));
        return false;
    }
    if (const FilePath node = HarmonyConfig::nodeLocation(); node.isExecutableFile())
    {
        if (auto hvigorwJs = HarmonyConfig::hvigorwJsLocation(); hvigorwJs.exists())
        {
            buildDirectory(); // ensure build directory exists
            FilePath ohProCwd;
            QString prepErr;
            if (!prepareOhProDirectory(&ohProCwd, &prepErr)) {
                emit addOutput(prepErr, OutputFormat::ErrorMessage);
                emit addTask(BuildSystemTask(ProjectExplorer::Task::Error, prepErr));
                return false;
            }
            auto *params = processParameters();

            params->setWorkingDirectory(ohProCwd);
            auto evn = params->environment();
            applyDevecoAndJavaEnv(evn);
            applyHvigorWorkingDirectoryEnv(evn, ohProCwd);
            params->setEnvironment(evn);
            qCDebug(harmonyBuildLog) << "Step in directory:"
                                        << params->workingDirectory().toUserOutput();
            // params->setCommandLine({node, {hvigorwJs.toUserOutput(), "--help"}});
            params->setCommandLine({node, {hvigorwJs.toUserOutput(),
                                           "--mode",
                                           "module",
                                           "-p",
                                           "product=default",
                                           "assembleHap",
                                           "--analyze=normal",
                                           "--parallel",
                                           "--incremental",
                                           "--no_daemon",
                                          }});
            const auto ohProPath = params->workingDirectory().toUserOutput();
            if (!QFile::exists(ohProPath + "/build-profile.json5")) {
                // createOhPro(buildSystem(), ohProPath);
                emit createTemplates();
            } else {
                qCDebug(harmonyBuildLog) << Tr::tr("OhPro already exists in %1").arg(ohProPath);
            }

        }
        else
        {
            const QString err = Tr::tr("Could not find hvigorw.js file, "
                                       "please check your HarmonyOS SDK configuration.");
            emit addOutput(err, OutputFormat::ErrorMessage);
            emit addTask(BuildSystemTask(ProjectExplorer::Task::Error, err));
            return false;
        }
    }
    else
    {
        const QString err = harmonyNodeMissingUserHint();
        emit addOutput(err, OutputFormat::ErrorMessage);
        emit addTask(BuildSystemTask(ProjectExplorer::Task::Error, err));
        return false;
    }
    return true;
}

QtTaskTree::GroupItem HarmonyBuildHapStep::runRecipe()
{
    return QtTaskTree::Group{
        ignoreReturnValue() ? QtTaskTree::finishAllAndSuccess : QtTaskTree::stopOnError,
        syncProjectTask(),
        ohpmInstallTask(),
        hvigorBuildTask()};
}


class HarmonyBuildHapStepFactory final : public ProjectExplorer::BuildStepFactory
{
public:
    HarmonyBuildHapStepFactory()
        : BuildStepFactory{}
    {
        registerStep<HarmonyBuildHapStep>(Constants::HARMONY_BUILD_HAP_ID);
        setSupportedDeviceType(Constants::HARMONY_DEVICE_TYPE);
        setSupportedStepList(ProjectExplorer::Constants::BUILDSTEPS_BUILD);
        setDisplayName(Tr::tr("Build Harmony HAP"));
        setRepeatable(false);
    }

};

void setupHarmonyBuildHapStep()
{
    static HarmonyBuildHapStepFactory theHarmonyBuildHapStepFactory;
}

} // namespace Ohos::Internal
