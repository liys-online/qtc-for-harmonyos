#include "harmonydeployqtstep.h"
#include "harmonyconfigurations.h"
#include "harmonydeviceinfo.h"
#include "harmonydevice.h"
#include "harmonyutils.h"
#include "ohosconstants.h"
#include "ohostr.h"

#include <coreplugin/messagemanager.h>

#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/buildstep.h>
#include <projectexplorer/taskhub.h>
#include <projectexplorer/buildsystem.h>
#include <projectexplorer/buildsteplist.h>
#include <projectexplorer/devicesupport/idevice.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/devicesupport/devicekitaspects.h>

#include <qtsupport/baseqtversion.h>
#include <qtsupport/qtkitaspect.h>

#include <solutions/tasking/tasktree.h>
#include <solutions/tasking/tasktreerunner.h>
#include <solutions/tasking/conditional.h>

#include <utils/algorithm.h>
#include <utils/async.h>
#include <utils/commandline.h>
#include <utils/environment.h>
#include <utils/fileutils.h>
#include <utils/layoutbuilder.h>
#include <utils/qtcassert.h>
#include <utils/qtcprocess.h>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QPushButton>
using namespace ProjectExplorer;
using namespace Utils;
using namespace Tasking;
using namespace std::chrono_literals;
namespace Ohos::Internal {

static Q_LOGGING_CATEGORY(deployStepLog, "qtc.harmony.build.harmonydeployqtstep", QtWarningMsg)

const char UninstallPreviousPackageKey[] = "UninstallPreviousPackage";

const QLatin1String InstallFailedInconsistentCertificatesString("INSTALL_PARSE_FAILED_INCONSISTENT_CERTIFICATES");
const QLatin1String InstallFailedUpdateIncompatible("INSTALL_FAILED_UPDATE_INCOMPATIBLE");
const QLatin1String InstallFailedPermissionModelDowngrade("INSTALL_FAILED_PERMISSION_MODEL_DOWNGRADE");
const QLatin1String InstallFailedVersionDowngrade("INSTALL_FAILED_VERSION_DOWNGRADE");

enum DeployErrorFlag
{
    NoError = 0,
    InconsistentCertificates = 0x0001,
    UpdateIncompatible = 0x0002,
    PermissionModelDowngrade = 0x0004,
    VersionDowngrade = 0x0008
};

Q_DECLARE_FLAGS(DeployErrorFlags, DeployErrorFlag)

static DeployErrorFlags parseDeployErrors(const QString &deployOutputLine)
{
    DeployErrorFlags errorCode = NoError;

    if (deployOutputLine.contains(InstallFailedInconsistentCertificatesString))
        errorCode |= InconsistentCertificates;
    if (deployOutputLine.contains(InstallFailedUpdateIncompatible))
        errorCode |= UpdateIncompatible;
    if (deployOutputLine.contains(InstallFailedPermissionModelDowngrade))
        errorCode |= PermissionModelDowngrade;
    if (deployOutputLine.contains(InstallFailedVersionDowngrade))
        errorCode |= VersionDowngrade;

    return errorCode;
}

class HarmonyDeployQtStep final : public BuildStep
{
public:
    HarmonyDeployQtStep(BuildStepList *parent, Id id);

private:
    // BuildStep interface
    bool init() final;


    Tasking::GroupItem runRecipe() final;
    Group deployRecipe();
    QWidget *createConfigWidget() final;

    void stdOutput(const QString &line);
    void stdError(const QString &line);

    void reportWarningOrError(const QString &message, Task::TaskType type);
    QString m_serialNumber;
    FilePath m_hapPath;

    BoolAspect m_uninstallPreviousPackage{this};
    bool m_uninstallPreviousPackageRun = false;
    CommandLine m_harmonydeployqtArgs;
    FilePath m_hdcPath;
    FilePath m_command;
    FilePath m_workingDirectory;
    Environment m_environment;
};

HarmonyDeployQtStep::HarmonyDeployQtStep(BuildStepList *parent, Id id)
    : BuildStep(parent, id)
{
    setImmutable(true);
    setUserExpanded(true);

    m_uninstallPreviousPackage.setSettingsKey(UninstallPreviousPackageKey);
    m_uninstallPreviousPackage.setLabel(Tr::tr("Uninstall the existing app before deployment"),
                                        BoolAspect::LabelPlacement::AtCheckBox);
    m_uninstallPreviousPackage.setValue(false);
}

bool HarmonyDeployQtStep::init()
{
    QtSupport::QtVersion *qtVersion = QtSupport::QtKitAspect::qtVersion(kit());
    if (!qtVersion) {
        reportWarningOrError(Tr::tr("The Qt version for kit %1 is invalid.").arg(kit()->displayName()),
                             Task::Error);
        return false;
    }

    m_harmonydeployqtArgs = {};

    const QStringList harmonyABIs = applicationAbis(kit());
    if (harmonyABIs.isEmpty()) {
        reportWarningOrError(Tr::tr("No HarmonyOS architecture (ABI) is set by the project."),
                             Task::Error);
        return false;
    }

    emit addOutput(Tr::tr("Initializing deployment to HarmonyOS device/simulator"),
                   OutputFormat::NormalMessage);

    const RunConfiguration *rc = buildConfiguration()->activeRunConfiguration();
    QTC_ASSERT(rc, reportWarningOrError(Tr::tr("The kit's run configuration is invalid."), Task::Error);
               return false);

    const int minTargetApi = minimumSDK(buildConfiguration());
    qCDebug(deployStepLog) << "Target architecture:" << harmonyABIs
                           << "Min target API" << minTargetApi;

    const BuildSystem *bs = buildSystem();
    QStringList selectedAbis;
    // QStringList selectedAbis = bs->property(Constants::HarmonyAbis).toStringList();

    const QString buildKey = buildConfiguration()->activeBuildKey();
    // if (selectedAbis.isEmpty())
    //     selectedAbis = bs->extraData(buildKey, Constants::HarmonyAbis).toStringList();

    if (selectedAbis.isEmpty()) {
        selectedAbis.append(bs->extraData(buildKey, Constants::OHOS_ARCH).toString());}

    const auto dev = std::dynamic_pointer_cast<const HarmonyDevice>(RunDeviceKitAspect::device(kit()));
    if (!dev) {
        reportWarningOrError(Tr::tr("No valid deployment device is set."), Task::Error);
        return false;
    }

    // TODO: use HarmonyDevice directly instead of HarmonyDeviceInfo.
    const HarmonyDeviceInfo info = HarmonyDevice::harmonyDeviceInfoFromDevice(dev);

    if (!info.isValid()) {
        reportWarningOrError(Tr::tr("The deployment device \"%1\" is invalid.")
                                 .arg(dev->displayName()), Task::Error);
        return false;
    }

    if (!dev->canSupportAbis(selectedAbis)) {
        const QString error = Tr::tr("The deployment device \"%1\" does not support the "
                                     "architectures used by the kit.\n"
                                     "The kit supports \"%2\", but the device uses \"%3\".")
                                  .arg(dev->displayName(), selectedAbis.join(", "), dev->supportedAbis().join(", "));
        reportWarningOrError(error, Task::Error);
        return false;
    }

    if (!dev->canHandleDeployments()) {
        reportWarningOrError(Tr::tr("The deployment device \"%1\" is disconnected.")
                                 .arg(dev->displayName()), Task::Error);
        return false;
    }

    if (qtVersion->supportsMultipleQtAbis() && !info.cpuAbi.isEmpty()
        && !selectedAbis.contains(info.cpuAbi.first())) {
        TaskHub::addTask(DeploymentTask(Task::Warning,
                                        Tr::tr("HarmonyOS: The main ABI of the deployment device (%1) is not selected. The app "
                                               "execution or debugging might not work properly. Add it from Projects > Build > "
                                               "Build Steps > qmake > ABIs.")
                                            .arg(info.cpuAbi.first())));
    }

    // m_avdName = info.avdName;
    m_serialNumber = info.serialNumber;
    qCDebug(deployStepLog) << "Selected device info:" << info;

    Internal::setDeviceSerialNumber(buildConfiguration(), m_serialNumber);
    Internal::setDeviceApiLevel(buildConfiguration(), info.sdk);
    Internal::setDeviceAbis(buildConfiguration(), info.cpuAbi);

    emit addOutput(Tr::tr("Deploying to %1").arg(m_serialNumber), OutputFormat::NormalMessage);

    m_uninstallPreviousPackageRun = m_uninstallPreviousPackage();

    const ProjectNode *node = project()->findNodeForBuildKey(buildKey);
    if (!node) {
        reportWarningOrError(Tr::tr("The deployment step's project node is invalid."), Task::Error);
        return false;
    }
    auto ohbuildDirectory = harmonyBuildDirectory(buildConfiguration());
    m_hapPath = ohbuildDirectory.pathAppended("entry/build/default/outputs/default/entry-default-signed.hap");
    if (!m_hapPath.isEmpty()) {
        m_command = HarmonyConfig::hdcToolPath();
    } /*else {
        FilePath jsonFile = HarmonyQtVersion::harmonyDeploymentSettings(buildConfiguration());
        if (jsonFile.isEmpty()) {
            reportWarningOrError(Tr::tr("Cannot find the harmonydeployqt input JSON file."),
                                 Task::Error);
            return false;
        }
        m_command = qtVersion->hostBinPath();
        if (m_command.isEmpty()) {
            reportWarningOrError(Tr::tr("Cannot find the harmonydeployqt tool."), Task::Error);
            return false;
        }
        m_command = m_command.pathAppended("harmonydeployqt").withExecutableSuffix();

        m_workingDirectory = harmonyBuildDirectory(buildConfiguration());

        // clang-format off
        m_harmonydeployqtArgs.addArgs({"--verbose",
                                       "--output", m_workingDirectory.path(),
                                       "--no-build",
                                       "--input", jsonFile.path()});
        // clang-format on

        m_harmonydeployqtArgs.addArg("--gradle");

        if (buildType() == BuildConfiguration::Release)
            m_harmonydeployqtArgs.addArgs({"--release"});

        const auto harmonyBuildHapStep =
            buildConfiguration()->buildSteps()->firstOfType<HarmonyBuildHapStep>();
        if (harmonyBuildHapStep && harmonyBuildHapStep->signPackage()) {
            // The harmonydeployqt tool is not really written to do stand-alone installations.
            // This hack forces it to use the correct filename for the apk file when installing
            // as a temporary fix until harmonydeployqt gets the support. Since the --sign is
            // only used to get the correct file name of the apk, its parameters are ignored.
            m_harmonydeployqtArgs.addArgs({"--sign", "foo", "bar"});
        }
    }*/

    m_environment = buildConfiguration()->environment();

    m_hdcPath = HarmonyConfig::hdcToolPath();
    return true;
}

Tasking::GroupItem HarmonyDeployQtStep::runRecipe()
{
    return Tasking::Group {
        deployRecipe()
    };
}

Group HarmonyDeployQtStep::deployRecipe()
{
    const Storage<DeployErrorFlags> storage;
    const auto onUninstallSetup = [this](Process &process) {
        if (m_hapPath.isEmpty())
            return SetupResult::StopWithSuccess;
        if (!m_uninstallPreviousPackageRun)
            return SetupResult::StopWithSuccess;

        QTC_ASSERT(buildConfiguration()->activeRunConfiguration(), return SetupResult::StopWithError);

        const QString packageName = Internal::packageName(buildConfiguration());
        if (packageName.isEmpty()) {
            reportWarningOrError(
                Tr::tr("Cannot find the package name from app.json5 nor "
                       "build.gradle files at \"%1\".")
                    .arg(harmonyBuildDirectory(buildConfiguration()).toUserOutput() + "/AppScope/app.json5"),
                Task::Error);
            return SetupResult::StopWithError;
        }
        const QString msg = Tr::tr("Uninstalling the previous package \"%1\".").arg(packageName);
        qCDebug(deployStepLog) << msg;
        emit addOutput(msg, OutputFormat::NormalMessage);
        const CommandLine cmd{m_hdcPath, {hdcSelector(m_serialNumber), "uninstall", packageName}};
        emit addOutput(Tr::tr("Package deploy: Running command \"%1\".").arg(cmd.toUserOutput()),
                       OutputFormat::NormalMessage);
        process.setCommand(cmd);
        return SetupResult::Continue;
    };
    const auto onUninstallDone = [this](const Process &process) {
        reportWarningOrError(process.exitMessage(), Task::Error);
    };

    const auto onInstallSetup = [this, storage](Process &process) {
        CommandLine cmd(m_command);
        if (m_hapPath.isEmpty()) {
            cmd.addArgs(m_harmonydeployqtArgs.arguments(), CommandLine::Raw);
            if (m_uninstallPreviousPackageRun)
                cmd.addArg("--install");
            else
                cmd.addArg("--reinstall");

            if (!m_serialNumber.isEmpty() && !m_serialNumber.startsWith("????"))
                cmd.addArgs({"--device", m_serialNumber});
        } else {
            QTC_ASSERT(buildConfiguration()->activeRunConfiguration(), return SetupResult::StopWithError);
            cmd.addArgs(hdcSelector(m_serialNumber));
            cmd.addArgs({"install", m_hapPath.nativePath()});
        }

        process.setCommand(cmd);
        process.setWorkingDirectory(m_workingDirectory);
        process.setEnvironment(m_environment);
        process.setUseCtrlCStub(true);

        DeployErrorFlags *flagsPtr = storage.activeStorage();
        process.setStdOutLineCallback([this, flagsPtr](const QString &line) {
            *flagsPtr |= parseDeployErrors(line);
            stdOutput(line);
        });
        process.setStdErrLineCallback([this, flagsPtr](const QString &line) {
            *flagsPtr |= parseDeployErrors(line);
            stdError(line);
        });
        emit addOutput(Tr::tr("Starting: \"%1\"").arg(cmd.toUserOutput()), OutputFormat::NormalMessage);
        return SetupResult::Continue;
    };
    const auto onInstallDone = [this, storage](const Process &process) {
        const QProcess::ExitStatus exitStatus = process.exitStatus();
        const int exitCode = process.exitCode();

        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            emit addOutput(Tr::tr("The process \"%1\" exited normally.").arg(m_command.toUserOutput()),
                           OutputFormat::NormalMessage);
        } else if (exitStatus == QProcess::NormalExit) {
            const QString error = Tr::tr("The process \"%1\" exited with code %2.")
            .arg(m_command.toUserOutput(), QString::number(exitCode));
            reportWarningOrError(error, Task::Error);
        } else {
            const QString error = Tr::tr("The process \"%1\" crashed.").arg(m_command.toUserOutput());
            reportWarningOrError(error, Task::Error);
        }

        if (*storage != NoError) {
            if (m_uninstallPreviousPackageRun) {
                reportWarningOrError(
                    Tr::tr("Installing the app failed even after uninstalling the previous one."),
                    Task::Error);
                *storage = NoError;
                return false;
            }
        } else if (exitCode != 0 || exitStatus != QProcess::NormalExit) {
            // Set the deployError to Failure when no deployError code was detected
            // but the adb tool failed otherwise relay the detected deployError.
            reportWarningOrError(Tr::tr("Installing the app failed with an unknown error."), Task::Error);
            return false;
        }
        return true;
    };

    const auto onAskForUninstallSetup = [this, storage] {
        const DeployErrorFlags &errorFlags = *storage;
        QString uninstallMsg = Tr::tr("Deployment failed with the following errors:") + "\n\n";
        if (errorFlags & InconsistentCertificates)
            uninstallMsg += InstallFailedInconsistentCertificatesString + '\n';
        if (errorFlags & UpdateIncompatible)
            uninstallMsg += InstallFailedUpdateIncompatible + '\n';
        if (errorFlags & PermissionModelDowngrade)
            uninstallMsg += InstallFailedPermissionModelDowngrade + '\n';
        if (errorFlags & VersionDowngrade)
            uninstallMsg += InstallFailedVersionDowngrade + '\n';
        uninstallMsg += '\n';
        uninstallMsg.append(Tr::tr("Uninstalling the installed package may solve the issue.") + '\n');
        uninstallMsg.append(Tr::tr("Do you want to uninstall the existing package?"));

        if (QMessageBox::critical(nullptr, Tr::tr("Install failed"), uninstallMsg,
                                  QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
            m_uninstallPreviousPackageRun = true;
            return SetupResult::Continue;
        }
        return SetupResult::StopWithSuccess;
    };

    return Group {
        storage,
        Group {
            ProcessTask(onUninstallSetup, onUninstallDone, CallDoneIf::Error).withTimeout(2min),
            ProcessTask(onInstallSetup, onInstallDone),
            onGroupDone(DoneResult::Success)
        },
        If ([storage] { return *storage != NoError; }) >> Then {
            onGroupSetup(onAskForUninstallSetup),
            ProcessTask(onUninstallSetup, onUninstallDone, CallDoneIf::Error).withTimeout(2min),
            ProcessTask(onInstallSetup, onInstallDone),
            onGroupDone(DoneResult::Success)
        }
    };
}

QWidget *HarmonyDeployQtStep::createConfigWidget()
{
    auto widget = new QWidget;
    auto installCustomApkButton = new QPushButton(widget);
    installCustomApkButton->setText(Tr::tr("Install a HAP File"));

    connect(installCustomApkButton, &QAbstractButton::clicked, this, [this] {
        const FilePath packagePath
            = FileUtils::getOpenFilePath(Tr::tr("Qt HarmonyOS Installer"),
                                         FileUtils::homePath(),
                                         Tr::tr("HarmonyOS package (*.hap)"));
        if (packagePath.isEmpty())
            return;

        // TODO: Write error messages on all the early returns below.

        const QStringList appAbis = applicationAbis(kit());
        if (appAbis.isEmpty())
            return;

        const IDevice::ConstPtr device = RunDeviceKitAspect::device(kit());
        const HarmonyDeviceInfo info = HarmonyDevice::harmonyDeviceInfoFromDevice(device);
        if (!info.isValid()) // aborted
            return;

        const Tasking::Storage<QString> serialNumberStorage;

        const auto onSetup = [serialNumberStorage, info] {
            if (info.type == IDevice::Emulator)
                return SetupResult::Continue;
            if (info.serialNumber.isEmpty())
                return SetupResult::StopWithError;
            *serialNumberStorage = info.serialNumber;
            return SetupResult::StopWithSuccess;
        };
        const auto onDone = [serialNumberStorage, info](Tasking::DoneWith result) {
            if (info.type == IDevice::Emulator && serialNumberStorage->isEmpty()) {
                Core::MessageManager::writeDisrupting(Tr::tr("Starting HarmonyOS virtual device failed."));
                return false;
            }
            return result == DoneWith::Success;
        };

        const auto onHdcSetup = [serialNumberStorage, packagePath](Process &process) {
            const CommandLine cmd{HarmonyConfig::hdcToolPath(),
                                  {hdcSelector(*serialNumberStorage),
                                   "install", packagePath.toUserOutput()}};
            process.setCommand(cmd);
            Core::MessageManager::writeSilently(
                Tr::tr("HarmonyOS package installation started with command \"%1\".")
                    .arg(cmd.toUserOutput()));
        };
        const auto onHdcDone = [](const Process &process, Tasking::DoneWith result) {
            if (result == DoneWith::Success) {
                Core::MessageManager::writeSilently(
                    Tr::tr("HarmonyOS package installation finished with success."));
            } else {
                Core::MessageManager::writeDisrupting(Tr::tr("HarmonyOS package installation failed.")
                                                      + '\n' + process.cleanedStdErr());
            }
        };

        const Tasking::Group recipe {
            serialNumberStorage,
            Tasking::Group {
                Tasking::onGroupSetup(onSetup),
            //     startAvdRecipe(info.avdName, serialNumberStorage),
                Tasking::onGroupDone(onDone)
            },
            ProcessTask(onHdcSetup, onHdcDone)
        };

        TaskTreeRunner *runner = new TaskTreeRunner;
        runner->setParent(target());
        runner->start(recipe);
    });

    using namespace Layouting;

    Form {
        m_uninstallPreviousPackage, br,
        installCustomApkButton,
        noMargin
    }.attachTo(widget);

    return widget;
}

void HarmonyDeployQtStep::stdOutput(const QString &line)
{
    emit addOutput(line, BuildStep::OutputFormat::Stdout, BuildStep::DontAppendNewline);
}

void HarmonyDeployQtStep::stdError(const QString &line)
{
    emit addOutput(line, BuildStep::OutputFormat::Stderr, BuildStep::DontAppendNewline);

    QString newOutput = line;
    static const QRegularExpression re("^(\\n)+");
    newOutput.remove(re);

    if (newOutput.isEmpty())
        return;

    if (newOutput.startsWith("warning", Qt::CaseInsensitive)
        || newOutput.startsWith("note", Qt::CaseInsensitive)
        || newOutput.startsWith(QLatin1String("All files should be loaded."))) {
        TaskHub::addTask(DeploymentTask(Task::Warning, newOutput));
    } else {
        TaskHub::addTask(DeploymentTask(Task::Error, newOutput));
    }
}

void HarmonyDeployQtStep::reportWarningOrError(const QString &message, Task::TaskType type)
{
    qCDebug(deployStepLog).noquote() << message;
    emit addOutput(message, OutputFormat::ErrorMessage);
    TaskHub::addTask(DeploymentTask(type, message));
}

// HarmonyDeployQtStepFactory
class HarmonyDeployQtStepFactory final : public BuildStepFactory
{
public:
    HarmonyDeployQtStepFactory()
    {
        registerStep<HarmonyDeployQtStep>(Constants::HARMONY_DEPLOY_QT_ID);
        setSupportedStepList(ProjectExplorer::Constants::BUILDSTEPS_DEPLOY);
        setSupportedDeviceType(Constants::HARMONY_DEVICE_TYPE);
        setRepeatable(false);
        setDisplayName(Tr::tr("Deploy to HarmonyOS device"));
    }
};
void setupHarmonyDeployQtStep()
{
    static HarmonyDeployQtStepFactory theHarmonyDeployQtStepFactory;
}

} // namespace Ohos::Internal
