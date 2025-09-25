#include "harmonybuildhapstep.h"
#include "harmonyqtversion.h"
#include "ohosconstants.h"
#include "ohostr.h"
#include "harmonyutils.h"
#include <coreplugin/messagemanager.h>

#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/buildstep.h>
#include <projectexplorer/buildsteplist.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/processparameters.h>
#include <projectexplorer/buildsystem.h>
#include <projectexplorer/runconfigurationaspects.h>
#include <projectexplorer/runcontrol.h>

#include <qpushbutton.h>
#include <utils/algorithm.h>
#include <utils/layoutbuilder.h>
#include <utils/qtcprocess.h>

#include <qtsupport/qtkitaspect.h>

#include <ohprojectecreator/ohprojectecreator.h>

#include "harmonyconfigurations.h"

namespace Ohos::Internal {
static void createOhPro(ProjectExplorer::BuildSystem *buildsystem, const QString &path)
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
    proInfo.deviceTypes = {"2in1"};
    ohPro->create(proInfo);
}
class HarmonyBuildHapWidget : public QWidget
{
public:
    explicit HarmonyBuildHapWidget(HarmonyBuildHapStep *step)
        : QWidget{},
        m_step{step}
    {
        Core::MessageManager::writeSilently("HarmonyBuildHapWidget setup");
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


        // Application group

        QList<int>  buildToolsVersions = {HarmonyConfig::devecoStudioVersion().first};
        QStringList versions = HarmonyConfig::apiLevelNamesFor(buildToolsVersions);

        versions.removeDuplicates();

        auto targetSDKComboBox = new QComboBox();
        for(int i = 0; i < versions.size(); ++i) {
            targetSDKComboBox->addItem(versions.at(i), QVariant::fromValue(buildToolsVersions.at(i)));
        }
        // targetSDKComboBox->setCurrentIndex(versions.indexOf(m_step->buildTargetSdk()));

        connect(targetSDKComboBox, &QComboBox::activated, this, [this, targetSDKComboBox](int idx) {
            const QString sdk = targetSDKComboBox->itemText(idx);
            m_step->setBuildTargetSdk(sdk);
        });
        if (!buildToolsVersions.isEmpty()) {
            const int initIdx = (m_step->buildTargetSdk().isEmpty())
            ? buildToolsVersions.indexOf(buildToolsVersions.last())
            : versions.indexOf(m_step->buildTargetSdk());
            targetSDKComboBox->setCurrentIndex(initIdx);
        }

        auto *ohQt = dynamic_cast<const HarmonyQtVersion *>(QtSupport::QtKitAspect::qtVersion(m_step->kit()));
        const int minApiSupported = ohQt->supportOhVersion().majorVersion();
        QList<int> apiLevels = {minApiSupported};
        QStringList targets = HarmonyConfig::apiLevelNamesFor(apiLevels);
        targets.removeDuplicates();
        auto buildToolsSdkComboBox = new QComboBox();
        for(int i = 0; i < targets.size(); ++i) {
            buildToolsSdkComboBox->addItem(targets.at(i), QVariant::fromValue(apiLevels.at(i)));
        }
        connect(buildToolsSdkComboBox, &QComboBox::activated, this,
                [this, buildToolsSdkComboBox](int idx) {
                    m_step->setBuildToolsVersion(buildToolsSdkComboBox->itemText(idx));
                });

        if (!apiLevels.isEmpty()) {
            const int initIdx = (m_step->buildToolsVersion().isEmpty())
            ? apiLevels.indexOf(apiLevels.last())
            : targets.indexOf(m_step->buildToolsVersion());
            buildToolsSdkComboBox->setCurrentIndex(initIdx);
        }

        auto createHarmonyTemplatesButton = new QPushButton(Tr::tr("Create Templates"));
        createHarmonyTemplatesButton->setToolTip(
            Tr::tr("Create an HarmonyOS package for Custom Java code, assets, and Gradle configurations."));
        connect(createHarmonyTemplatesButton, &QAbstractButton::clicked, this, [this, targetSDKComboBox, buildToolsSdkComboBox] {
            using namespace QtSupport;
            auto *buildsystem = m_step->buildSystem();
            if (!buildsystem)
                return;
            auto *ohPro = OhProjecteCreator::instance();
            OhProjecteCreator::ProjecteInfo proInfo;
            proInfo.projectPath = m_step->buildDirectory().toUserOutput() + "/ohpro";
            proInfo.targetSdkVersion = targetSDKComboBox->currentData().toInt();
            if(proInfo.targetSdkVersion < 0) {
                proInfo.targetSdkVersion = HarmonyConfig::devecoStudioVersion().first;
            }
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
                    proInfo.compatibleSdkVersion =buildToolsSdkComboBox->currentData().toInt();
                    if(proInfo.compatibleSdkVersion < 0) {
                        proInfo.compatibleSdkVersion = ohQt->supportOhVersion().majorVersion();
                    }
                    proInfo.qtHostPath = ohQt->hostPrefixPath().toUserOutput();
                }
            }
            proInfo.deviceTypes = {"2in1"};
            const QString buildKey = m_step->buildConfiguration()->activeBuildKey();
            proInfo.entrylib = buildKey.isEmpty() ? "libentry.so" : QString("lib%1.so").arg(buildKey);
            ohPro->create(proInfo);
        });
        connect(m_step, &HarmonyBuildHapStep::createTemplates, createHarmonyTemplatesButton, &QPushButton::click);
        Group applicationGroup {
            title(Tr::tr("Application")),
            Form {
                Tr::tr("Harmony compatible Sdk Version:"), buildToolsSdkComboBox, br,
                Tr::tr("Harmony target Sdk Version:"), targetSDKComboBox, br,
                Tr::tr("Harmony customization:"), createHarmonyTemplatesButton,
            }
        };

        // main layout
        Column {
            signPackageGroup,
            applicationGroup,
            noMargin
        }.attachTo(this);
    }

    void openDevEco()
    {
        auto projectPath = m_step->buildDirectory() / "ohpro";
        openDevEcoProject(projectPath.toUserOutput());
    }
private:
    HarmonyBuildHapStep *m_step = nullptr;
};

const char BuildTargetSdkKey[] = "BuildTargetSdk";
const char BuildToolsVersionKey[] = "BuildToolsVersion";
HarmonyBuildHapStep::HarmonyBuildHapStep(ProjectExplorer::BuildStepList *bc, Utils::Id id)
    : AbstractProcessStep(bc, id)
{
    Core::MessageManager::writeSilently("HarmonyBuildHapStep setup");
}

void HarmonyBuildHapStep::fromMap(const Utils::Store &map)
{
    m_buildTargetSdk = map.value(BuildTargetSdkKey).toString();
     m_buildToolsVersion = map.value(BuildToolsVersionKey).toString();
    if (m_buildTargetSdk.isEmpty()) {
        m_buildTargetSdk = HarmonyConfig::apiLevelNameFor(HarmonyConfig::devecoStudioVersion().first);
    }
    ProjectExplorer::BuildStep::fromMap(map);

}

void HarmonyBuildHapStep::toMap(Utils::Store &map) const
{
    ProjectExplorer::AbstractProcessStep::toMap(map);
    map.insert(BuildTargetSdkKey, m_buildTargetSdk);
    map.insert(BuildToolsVersionKey, m_buildToolsVersion);
}

Tasking::GroupItem HarmonyBuildHapStep::defaultProcessTask()
{
    using namespace Tasking;
    using namespace Utils;
    const auto onSetup = [this](Process &process) {
        return setupProcess(process) ? SetupResult::Continue : SetupResult::StopWithError;
    };
    const auto onDone = [this](const Process &process) { handleProcessDone(process); };
    return ProcessTask(onSetup, onDone);
}

Tasking::GroupItem HarmonyBuildHapStep::syncProjectTask()
{
    using namespace Tasking;
    using namespace Utils;
    const auto onSyncProject = [this] (Process &process){
        if (auto node = HarmonyConfig::nodeLocation(); node.exists())
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

                process.setUseCtrlCStub(HostOsInfo::isWindowsHost());
                process.setWorkingDirectory(buildDirectory() / "ohpro");
                auto evn = buildEnvironment();
                evn.set("DEVECO_SDK_HOME", HarmonyConfig::devecoStudioLocation().toFSPathString());
                process.setEnvironment(evn);
                process.setCommand(command);
                if (buildEnvironment().hasKey("VSLANG"))
                    process.setUtf8StdOutCodec();

                process.setStdOutCallback([this](const QString &s){
                    emit addOutput(s + "\033[0m", OutputFormat::Stdout, DontAppendNewline);
                });

                process.setStdErrCallback([this](const QString &s){
                    emit addOutput(s + "\033[0m", OutputFormat::Stderr, DontAppendNewline);
                });

                connect(&process, &Process::started, this, [this, &process] {
                    emit addOutput(Tr::tr("Starting: \"%1\"").arg(process.commandLine().toUserOutput()),
                                   OutputFormat::NormalMessage);
                });
                return Tasking::SetupResult::Continue;
            }
            else
            {
                Core::MessageManager::writeSilently(Tr::tr("Could not find hvigorw.js file, "
                                                           "please check your HarmonyOS SDK configuration."));
                return Tasking::SetupResult::StopWithError;
            }
        }
        else
        {
            Core::MessageManager::writeSilently(Tr::tr("Could not find node executable, "
                                                       "please check your HarmonyOS SDK configuration."));
            return Tasking::SetupResult::StopWithError;
        }
    };

    const auto onSyncProjectDone = [this](const Process &process) { handleProcessDone(process); };
    return ProcessTask(onSyncProject, onSyncProjectDone);
}

Tasking::GroupItem HarmonyBuildHapStep::ohpmInstallTask()
{
    using namespace Tasking;
    using namespace Utils;
    const auto onOhpmInstall = [this] (Process &process){
        if (auto node = HarmonyConfig::nodeLocation(); node.exists())
        {
            if (auto ohpmJs = HarmonyConfig::ohpmJsLocation(); ohpmJs.exists())
            {
                /*
                 * TODO: 未来支持用户自定义ohpm源
                 */
                QUrl registryUrl = QUrl::fromUserInput("https://ohpm.openharmony.cn/ohpm");
                /*
                 * TODO: 未来支持用户自定义是否开启ssl
                 */
                bool strictSsl = true;
                const CommandLine command {node, {ohpmJs.toUserOutput(),
                                                 "install",
                                                 "--all",
                                                 "--registry",
                                                 registryUrl.toString(),
                                                 "--strict_ssl",
                                                 strictSsl ? "true" : "false"
                                                }};

                process.setUseCtrlCStub(HostOsInfo::isWindowsHost());
                process.setWorkingDirectory(buildDirectory() / "ohpro");
                auto evn = buildEnvironment();
                evn.set("DEVECO_SDK_HOME", HarmonyConfig::devecoStudioLocation().toFSPathString());
                process.setEnvironment(evn);
                process.setCommand(command);
                if (buildEnvironment().hasKey("VSLANG"))
                    process.setUtf8StdOutCodec();

                process.setStdOutCallback([this](const QString &s){
                    emit addOutput(s + "\033[0m", OutputFormat::Stdout, DontAppendNewline);
                });

                process.setStdErrCallback([this](const QString &s){
                    emit addOutput(s + "\033[0m", OutputFormat::Stderr, DontAppendNewline);
                });

                connect(&process, &Process::started, this, [&process, this] {
                    emit addOutput(Tr::tr("Starting: \"%1\"").arg(process.commandLine().toUserOutput()),
                                   OutputFormat::NormalMessage);
                });
                return Tasking::SetupResult::Continue;
            }
            else
            {
                Core::MessageManager::writeSilently(Tr::tr("Could not find ohpm executable, "
                                                           "please check your HarmonyOS SDK configuration."));
                return Tasking::SetupResult::StopWithError;
            }
        }
        else
        {
            Core::MessageManager::writeSilently(Tr::tr("Could not find node executable, "
                                                       "please check your HarmonyOS SDK configuration."));
            return Tasking::SetupResult::StopWithError;
        }
    };

    const auto onOhpmInstallDone = [this](const Process &process) { handleProcessDone(process); };
    return ProcessTask(onOhpmInstall, onOhpmInstallDone);
}

bool HarmonyBuildHapStep::setupProcess(Utils::Process &process)
{
    using namespace Utils;
    using namespace ProjectExplorer;
    auto *params = processParameters();
    const FilePath workingDir = params->effectiveWorkingDirectory();
    if (!workingDir.exists() && !workingDir.createDir()) {
        emit addOutput(Tr::tr("Could not create directory \"%1\"").arg(workingDir.toUserOutput()),
                       OutputFormat::ErrorMessage);
        return false;
    }
    if (!params->effectiveCommand().isExecutableFile()) {
        emit addOutput(Tr::tr("The program \"%1\" does not exist or is not executable.")
                           .arg(params->effectiveCommand().toUserOutput()),
                           OutputFormat::ErrorMessage);
        return false;
    }

    process.setUseCtrlCStub(HostOsInfo::isWindowsHost());
    process.setWorkingDirectory(workingDir);
    // Enforce PWD in the environment because some build tools use that.
    // PWD can be different from getcwd in case of symbolic links (getcwd resolves symlinks).
    // For example Clang uses PWD for paths in debug info, see QTCREATORBUG-23788
    Environment envWithPwd = params->environment();
    Core::MessageManager::writeSilently("Environment: PATH" + Environment::valueFromPathList(envWithPwd.path(),OsType::OsTypeWindows));
    envWithPwd.set("PWD", workingDir.path());
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
        emit addOutput(s + "\033[0m", OutputFormat::Stdout, DontAppendNewline);
    });

    process.setStdErrCallback([this](const QString &s){
        emit addOutput(s + "\033[0m", OutputFormat::Stderr, DontAppendNewline);
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

QWidget *HarmonyBuildHapStep::createConfigWidget()
{
    return new HarmonyBuildHapWidget(this);
}

bool HarmonyBuildHapStep::init()
{
    if (!AbstractProcessStep::init()) {
        Core::MessageManager::writeSilently(Tr::tr("\"%1\" step failed initialization.").arg(displayName()));
        return false;
    }
    if (auto node = HarmonyConfig::nodeLocation(); node.exists())
    {
        if (auto hvigorwJs = HarmonyConfig::hvigorwJsLocation(); hvigorwJs.exists())
        {
            buildDirectory(); // ensure build directory exists
            auto *params = processParameters();

            params->setWorkingDirectory(buildDirectory() / "ohpro");
            auto evn = params->environment();
            evn.set("DEVECO_SDK_HOME", HarmonyConfig::devecoStudioLocation().toFSPathString());
            evn.set("JAVA_HOME", HarmonyConfig::javaLocation().toFSPathString());
            evn.appendOrSetPath({HarmonyConfig::javaLocation() / "bin"});
            params->setEnvironment(evn);
            Core::MessageManager::writeSilently(Tr::tr("Step in directory: %1").arg(params->workingDirectory().toUserOutput()));
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
            auto OhProPath = params->workingDirectory().toUserOutput();
            !QFile::exists(OhProPath + "/build-profile.json5") ?
                // createOhPro(buildSystem(), OhProPath) :
                emit createTemplates():
                Core::MessageManager::writeSilently(Tr::tr("OhPro already exists in %1").arg(OhProPath));

        }
        else
        {
            Core::MessageManager::writeSilently(Tr::tr("Could not find hvigorw.js file, "
                                                       "please check your HarmonyOS SDK configuration."));
            return false;
        }
    }
    else
    {
        Core::MessageManager::writeSilently(Tr::tr("Could not find node executable, "
                                                   "please check your HarmonyOS SDK configuration."));
        return false;
    }
    return true;
}

Tasking::GroupItem HarmonyBuildHapStep::runRecipe()
{
    using namespace Tasking;
    return Group {
        ignoreReturnValue() ? finishAllAndSuccess : stopOnError,
        syncProjectTask(),
        ohpmInstallTask(),
        defaultProcessTask()
    };
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

} // namespace Harmony::Internal
