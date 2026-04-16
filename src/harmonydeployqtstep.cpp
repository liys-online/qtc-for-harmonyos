#include "harmonydeployqtstep.h"
#include "harmonyconfigurations.h"
#include "harmonydeviceinfo.h"
#include "harmonylogcategories.h"
#include "harmonydevice.h"
#include "harmonyutils.h"
#include "hdcsocketclient.h"
#include "ohosconstants.h"
#include "ohostr.h"

#include <coreplugin/icore.h>
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

#include <QtTaskTree/qconditional.h>
#include <QtTaskTree/QSingleTaskTreeRunner>

#include <utils/algorithm.h>
#include <utils/async.h>
#include <utils/commandline.h>
#include <utils/environment.h>
#include <utils/fileutils.h>
#include <utils/layoutbuilder.h>
#include <utils/qtcassert.h>
#include <utils/qtcprocess.h>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QPushButton>
using namespace ProjectExplorer;
using namespace Utils;
using namespace QtTaskTree;
using namespace std::chrono_literals;
namespace Ohos::Internal {

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
    /* ** BuildStep 接口 */
    void fromMap(const Store &map) override;
    bool init() final;


    GroupItem runRecipe() final;
    Group deployRecipe();
    QWidget *createConfigWidget() final;

    void stdOutput(const QString &line);
    void stdError(const QString &line);

    void reportWarningOrError(const QString &message, Task::TaskType type);

    // deployRecipe helpers
    SetupResult uninstallSetup(Process &process);
    void uninstallDone(const Process &process);
    SetupResult installSetupHap(Process &process, DeployErrorFlags *flagsPtr);
    SetupResult installSetup(Process &process, DeployErrorFlags *flagsPtr);
    bool installDone(const Process &process, DeployErrorFlags *flagsPtr);
    SetupResult askForUninstallSetup(DeployErrorFlags *flagsPtr);
    static QString buildUninstallErrorMessage(const DeployErrorFlags &errorFlags);
    void onInstallHapClicked();
    QString m_serialNumber;
    FilePath m_hapPath;

    BoolAspect m_uninstallPreviousPackage{this};
    bool m_uninstallPreviousPackageRun = false;
    CommandLine m_harmonydeployqtArgs;
    FilePath m_hdcPath;
    FilePath m_command;
    FilePath m_workingDirectory;
    Environment m_environment;

    QSingleTaskTreeRunner m_taskTreeRunner;
};

HarmonyDeployQtStep::HarmonyDeployQtStep(BuildStepList *parent, Id id)
    : BuildStep(parent, id)
{
    setImmutable(true);
    setUserExpanded(true);

    m_uninstallPreviousPackage.setSettingsKey(Constants::HarmonyDeployUninstallPreviousPackageKey);
    m_uninstallPreviousPackage.setLabel(Tr::tr("Uninstall the existing app before deployment"),
                                        BoolAspect::LabelPlacement::AtCheckBox);
    m_uninstallPreviousPackage.setValue(false);
}

void HarmonyDeployQtStep::fromMap(const Store &map)
{
    Store m = map;
    const Key newK(Constants::HarmonyDeployUninstallPreviousPackageKey);
    const Key oldK("UninstallPreviousPackage");
    if (!m.contains(newK) && m.contains(oldK))
        m.insert(newK, m.value(oldK));
    BuildStep::fromMap(m);
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
    qCDebug(harmonyDeployLog) << "Target architecture:" << harmonyABIs
                           << "Min target API" << minTargetApi;

    const BuildSystem *bs = buildSystem();
    QStringList selectedAbis;
    /* ** QStringList selectedAbis = bs->property(Constants::HarmonyAbis).toStringList(); */

    const QString dataBuildKey = Internal::buildKeyForCMakeExtraData(buildConfiguration());
    // if (selectedAbis.isEmpty())
    //     selectedAbis = bs->extraData(buildKey, Constants::HarmonyAbis).toStringList();

    if (selectedAbis.isEmpty()) {
        const QString arch = bs->extraData(dataBuildKey, Constants::OHOS_ARCH).toString().trimmed();
        if (!arch.isEmpty())
            selectedAbis.append(arch);
    }
    selectedAbis.removeAll(QString());
    selectedAbis.removeDuplicates();

    const auto dev = std::dynamic_pointer_cast<const HarmonyDevice>(RunDeviceKitAspect::device(kit()));
    if (!dev) {
        reportWarningOrError(Tr::tr("No valid deployment device is set."), Task::Error);
        return false;
    }

    /* ** TODO: 直接使用 HarmonyDevice 而非 HarmonyDeviceInfo。 */
    const HarmonyDeviceInfo info = HarmonyDevice::harmonyDeviceInfoFromDevice(dev);

    if (!info.isValid()) {
        reportWarningOrError(Tr::tr("The deployment device \"%1\" is invalid.")
                                 .arg(dev->displayName()), Task::Error);
        return false;
    }

    if (!selectedAbis.isEmpty() && !dev->canSupportAbis(selectedAbis)) {
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
                                               "might not run correctly. Add it from Projects > Build > "
                                               "Build Steps > qmake > ABIs.")
                                            .arg(info.cpuAbi.first())));
    }

    /* ** m_avdName = info.avdName; */
    m_serialNumber = info.serialNumber.trimmed();
    qCDebug(harmonyDeployLog) << "Selected device info:" << info;

    if (m_serialNumber.isEmpty()) {
        reportWarningOrError(
            Tr::tr("No device serial number is available for \"%1\". Connect a device, refresh the "
                   "device list, or select another kit/run device in Projects.")
                .arg(dev->displayName()),
            Task::Error);
        return false;
    }

    Internal::setDeviceSerialNumber(buildConfiguration(), m_serialNumber);
    Internal::setDeviceApiLevel(buildConfiguration(), info.sdk);
    Internal::setDeviceAbis(buildConfiguration(), info.cpuAbi);

    emit addOutput(Tr::tr("Deploying to %1").arg(m_serialNumber), OutputFormat::NormalMessage);

    m_uninstallPreviousPackageRun = m_uninstallPreviousPackage();

    /* ** 无需 ProjectNode：HAP 路径来自 harmonyBuildDirectory / findBuiltHapPackage。 */
    m_hdcPath = HarmonyConfig::hdcToolPath();
    if (!m_hdcPath.isExecutableFile()) {
        reportWarningOrError(
            Tr::tr("The hdc tool is missing or not executable.\n"
                   "Expected path: %1\n"
                   "Install the Harmony/OpenHarmony SDK toolchains, set the default SDK in "
                   "**Preferences → HarmonyOS**, and ensure \"hdc\" exists under the SDK toolchains directory.")
                .arg(m_hdcPath.toUserOutput()),
            Task::Error);
        return false;
    }

    const FilePath ohbuildDirectory = harmonyBuildDirectory(buildConfiguration());
    if (!ohbuildDirectory.isReadableDir()) {
        reportWarningOrError(
            Tr::tr("The Harmony application directory does not exist or is not readable: \"%1\".\n"
                   "Build the project first (including the \"Build Harmony HAP\" step) so that the "
                   "\"%2\" folder is generated.")
                .arg(ohbuildDirectory.toUserOutput(),
                     QString::fromLatin1(Constants::HARMONY_BUILD_DIRECTORY)),
            Task::Error);
        return false;
    }

    QString hapDiagnostic;
    m_hapPath = findBuiltHapPackage(ohbuildDirectory, &hapDiagnostic);
    if (!m_hapPath.isEmpty() && m_hapPath.isReadableFile())
        qCDebug(harmonyDeployLog).noquote() << "Resolved HAP for deploy:" << m_hapPath.toUserOutput();
    if (m_hapPath.isEmpty() || !m_hapPath.isReadableFile()) {
        reportWarningOrError(
            Tr::tr("Cannot find a readable HAP package under \"%1\".\n"
                   "Run the \"Build Harmony HAP\" step first. The deploy step checks, in order:\n"
                   "• each module's \"build/default/outputs/default\" (from build-profile.json5 when present; "
                   "entry-type modules first),\n"
                   "• the classic \"entry/build/default/outputs/default\" layout,\n"
                   "• the newest \"*.hap\" file anywhere under the ohpro directory.\n\n"
                   "Details:\n%2")
                .arg(ohbuildDirectory.toUserOutput(),
                     hapDiagnostic.isEmpty() ? Tr::tr("(no diagnostic)") : hapDiagnostic),
            Task::Error);
        return false;
    }

    emit addOutput(Tr::tr("HAP package: %1").arg(m_hapPath.toUserOutput()), OutputFormat::NormalMessage);

    m_command = m_hdcPath;

    m_environment = buildConfiguration()->environment();
    return true;
}

GroupItem HarmonyDeployQtStep::runRecipe()
{
    return Group {
        deployRecipe()
    };
}

SetupResult HarmonyDeployQtStep::uninstallSetup(Process &)
{
    if (m_hapPath.isEmpty() || !m_uninstallPreviousPackageRun)
        return SetupResult::StopWithSuccess;

    QTC_ASSERT(buildConfiguration()->activeRunConfiguration(), return SetupResult::StopWithError);

    const QString packageName = Internal::packageName(buildConfiguration());
    if (packageName.isEmpty()) {
        reportWarningOrError(
            Tr::tr("Cannot read the application bundle name from \"%1\".\n"
                   "Ensure AppScope/app.json5 exists and contains app.bundleName.")
                .arg((harmonyBuildDirectory(buildConfiguration()) / "AppScope" / "app.json5")
                         .toUserOutput()),
            Task::Error);
        return SetupResult::StopWithError;
    }

    const QString msg = Tr::tr("Uninstalling the previous package \"%1\".").arg(packageName);
    qCDebug(harmonyDeployLog) << msg;
    emit addOutput(msg, OutputFormat::NormalMessage);

    QStringList uargs = hdcSelector(m_serialNumber);
    uargs << QStringLiteral("uninstall") << packageName;
    const HdcShellSyncResult r = HdcSocketClient::runSyncWithCliFallback(
        m_serialNumber,
        QStringLiteral("uninstall ") + packageName,
        m_hdcPath.toUserOutput(),
        uargs,
        120000);
    if (r.isOk()) {
        const QString t = r.standardOutput.trimmed();
        for (const QString &rawLine : t.split('\n')) {
            const QString ln = rawLine.trimmed();
            if (!ln.isEmpty())
                emit addOutput(ln, OutputFormat::Stdout, DontAppendNewline);
        }
        return SetupResult::StopWithSuccess;
    }
    reportWarningOrError(r.errorMessage, Task::Warning);
    return SetupResult::StopWithSuccess;
}

void HarmonyDeployQtStep::uninstallDone(const Process &process)
{
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        reportWarningOrError(process.exitMessage(), Task::Warning);
}

SetupResult HarmonyDeployQtStep::installSetupHap(Process &process, DeployErrorFlags *flagsPtr)
{
    QTC_ASSERT(buildConfiguration()->activeRunConfiguration(), return SetupResult::StopWithError);

    QStringList iargs = hdcSelector(m_serialNumber);
    iargs << QStringLiteral("install") << m_hapPath.nativePath();

    const auto rejectSock = [](const HdcShellSyncResult &sock) -> bool {
        if (!sock.isOk())
            return false;
        DeployErrorFlags f = NoError;
        for (const QString &rawLine : sock.standardOutput.split('\n')) {
            const QString ln = rawLine.trimmed();
            if (!ln.isEmpty())
                f |= parseDeployErrors(ln);
        }
        return f != NoError;
    };

    const HdcShellSyncResult r = HdcSocketClient::runSyncWithCliFallback(
        m_serialNumber,
        QStringLiteral("install ") + m_hapPath.nativePath(),
        m_hdcPath.toUserOutput(),
        iargs,
        600000,
        {},
        rejectSock);
    if (r.isOk()) {
        for (const QString &rawLine : r.standardOutput.split('\n')) {
            const QString ln = rawLine.trimmed();
            if (!ln.isEmpty()) {
                *flagsPtr |= parseDeployErrors(ln);
                emit addOutput(ln, OutputFormat::Stdout, DoAppendNewline);
            }
        }
        return SetupResult::StopWithSuccess;
    }
    reportWarningOrError(r.errorMessage, Task::Error);
    Q_UNUSED(process)
    return SetupResult::StopWithError;
}

SetupResult HarmonyDeployQtStep::installSetup(Process &process, DeployErrorFlags *flagsPtr)
{
    CommandLine cmd(m_command);
    if (!m_hapPath.isEmpty())
        return installSetupHap(process, flagsPtr);

    cmd.addArgs(m_harmonydeployqtArgs.arguments(), CommandLine::Raw);
    cmd.addArg(m_uninstallPreviousPackageRun ? QString("--install") : QString("--reinstall"));
    if (!m_serialNumber.isEmpty() && !m_serialNumber.startsWith("????"))
        cmd.addArgs({"--device", m_serialNumber});

    process.setCommand(cmd);
    process.setWorkingDirectory(m_workingDirectory);
    process.setEnvironment(m_environment);
    process.setUseCtrlCStub(true);
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
}

bool HarmonyDeployQtStep::installDone(const Process &process, DeployErrorFlags *flagsPtr)
{
    const QProcess::ExitStatus exitStatus = process.exitStatus();
    const int exitCode = process.exitCode();

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        emit addOutput(Tr::tr("The process \"%1\" exited normally.").arg(m_command.toUserOutput()),
                       OutputFormat::NormalMessage);
    } else if (exitStatus == QProcess::NormalExit) {
        reportWarningOrError(Tr::tr("The process \"%1\" exited with code %2.")
                                 .arg(m_command.toUserOutput(), QString::number(exitCode)),
                             Task::Error);
    } else {
        reportWarningOrError(Tr::tr("The process \"%1\" crashed.").arg(m_command.toUserOutput()),
                             Task::Error);
    }

    if (*flagsPtr != NoError) {
        if (m_uninstallPreviousPackageRun) {
            reportWarningOrError(
                Tr::tr("Installing the app failed even after uninstalling the previous one."),
                Task::Error);
            *flagsPtr = NoError;
            return false;
        }
    } else if (exitCode != 0 || exitStatus != QProcess::NormalExit) {
        /*
        ** 当未检测到 deployError 代码但 adb 工具失败时，将 deployError 设置为 Failure；
        ** 否则转发已检测到的 deployError。
        */
        reportWarningOrError(Tr::tr("Installing the app failed with an unknown error."), Task::Error);
        return false;
    }
    return true;
}

SetupResult HarmonyDeployQtStep::askForUninstallSetup(DeployErrorFlags *flagsPtr)
{
    QString uninstallMsg = Tr::tr("Deployment failed with the following errors:") + "\n\n";
    uninstallMsg += buildUninstallErrorMessage(*flagsPtr);
    uninstallMsg += '\n';
    uninstallMsg.append(Tr::tr("Uninstalling the installed package may solve the issue.") + '\n');
    uninstallMsg.append(Tr::tr("Do you want to uninstall the existing package?"));

    if (QMessageBox::critical(Core::ICore::dialogParent(), Tr::tr("Install failed"), uninstallMsg,
                              QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
        m_uninstallPreviousPackageRun = true;
        return SetupResult::Continue;
    }
    return SetupResult::StopWithSuccess;
}

QString HarmonyDeployQtStep::buildUninstallErrorMessage(const DeployErrorFlags &errorFlags)
{
    QString msg;
    if (errorFlags & InconsistentCertificates)
        msg += InstallFailedInconsistentCertificatesString + '\n';
    if (errorFlags & UpdateIncompatible)
        msg += InstallFailedUpdateIncompatible + '\n';
    if (errorFlags & PermissionModelDowngrade)
        msg += InstallFailedPermissionModelDowngrade + '\n';
    if (errorFlags & VersionDowngrade)
        msg += InstallFailedVersionDowngrade + '\n';
    return msg;
}

Group HarmonyDeployQtStep::deployRecipe()
{
    const Storage<DeployErrorFlags> storage;
    const auto onUninstallSetup = [this](Process &p) { return uninstallSetup(p); };
    const auto onUninstallDone  = [this](const Process &p) { uninstallDone(p); };
    const auto onInstallSetup   = [this, storage](Process &p) {
        return installSetup(p, storage.activeStorage());
    };
    const auto onInstallDone    = [this, storage](const Process &p) {
        return installDone(p, storage.activeStorage());
    };
    const auto onAskForUninstall = [this, storage] {
        return askForUninstallSetup(storage.activeStorage());
    };

    return Group {
        storage,
        Group {
            ProcessTask(onUninstallSetup, onUninstallDone, CallDone::OnError).withTimeout(2min),
            ProcessTask(onInstallSetup, onInstallDone),
            onGroupDone(DoneResult::Success)
        },
        If ([storage] { return *storage != NoError; }) >> Then {
            onGroupSetup(onAskForUninstall),
            ProcessTask(onUninstallSetup, onUninstallDone, CallDone::OnError).withTimeout(2min),
            ProcessTask(onInstallSetup, onInstallDone),
            onGroupDone(DoneResult::Success)
        }
    };
}

static bool hapSockHasErrors(const HdcShellSyncResult &sock)
{
    if (!sock.isOk())
        return false;
    DeployErrorFlags f = NoError;
    for (const QString &rawLine : sock.standardOutput.split('\n')) {
        const QString ln = rawLine.trimmed();
        if (!ln.isEmpty())
            f |= parseDeployErrors(ln);
    }
    return f != NoError;
}

static SetupResult hapInstallSetup(const QString &serialNumber,
                                   const FilePath &packagePath,
                                   const FilePath &hdcPath,
                                   const QPointer<QWidget> &dlgParentPtr,
                                   Process &)
{
    QStringList args = hdcSelector(serialNumber);
    args << QLatin1String("install") << packagePath.toUserOutput();
    const HdcShellSyncResult r = HdcSocketClient::runSyncWithCliFallback(
        serialNumber,
        QStringLiteral("install ") + packagePath.nativePath(),
        hdcPath.toUserOutput(),
        args,
        600000,
        {},
        hapSockHasErrors);
    if (r.isOk()) {
        Core::MessageManager::writeFlashing(Tr::tr("HarmonyOS package was installed successfully."));
        qCDebug(harmonyDeployLog).noquote() << "Manual HAP install OK:" << packagePath.toUserOutput();
        return SetupResult::StopWithSuccess;
    }
    const QString err = r.errorMessage;
    QMetaObject::invokeMethod(
        qApp,
        [dlgParentPtr, err]() {
            if (QWidget *p = dlgParentPtr.data())
                QMessageBox::warning(p, Tr::tr("Install HAP"),
                                     Tr::tr("Installation failed.\n\n%1").arg(err));
            TaskHub::addTask(DeploymentTask(Task::Error,
                                            Tr::tr("HAP installation failed: %1").arg(err)));
        },
        Qt::QueuedConnection);
    return SetupResult::StopWithError;
}

static void hapInstallDone(QWidget *dlgParent, const Process &process, DoneWith result)
{
    if (result == DoneWith::Success) {
        Core::MessageManager::writeFlashing(Tr::tr("HarmonyOS package was installed successfully."));
        return;
    }
    const QString detail = process.cleanedStdErr().trimmed();
    const QString msg = detail.isEmpty() ? process.exitMessage() : detail;
    QMessageBox::warning(dlgParent, Tr::tr("Install HAP"),
                         Tr::tr("Installation failed.\n\n%1").arg(msg));
    TaskHub::addTask(DeploymentTask(Task::Error, Tr::tr("HAP installation failed: %1").arg(msg)));
}

void HarmonyDeployQtStep::onInstallHapClicked()
{
    QWidget *const dlgParent = Core::ICore::dialogParent();

    const FilePath packagePath
        = FileUtils::getOpenFilePath(Tr::tr("Select HAP Package"),
                                     FileUtils::homePath(),
                                     Tr::tr("HarmonyOS package (*.hap)"));
    if (packagePath.isEmpty())
        return; // user cancelled

    if (!packagePath.isReadableFile()) {
        QMessageBox::warning(dlgParent, Tr::tr("Install HAP"),
                             Tr::tr("The selected file cannot be read:\n%1").arg(packagePath.toUserOutput()));
        return;
    }

    const FilePath hdcPath = HarmonyConfig::hdcToolPath();
    if (!hdcPath.isExecutableFile()) {
        QMessageBox::warning(dlgParent, Tr::tr("Install HAP"),
                             Tr::tr("The hdc tool is missing or not executable:\n%1\n\n"
                                    "Configure the Harmony/OpenHarmony SDK and default SDK path under "
                                    "Edit → Preferences → HarmonyOS.")
                                 .arg(hdcPath.toUserOutput()));
        return;
    }

    const QStringList appAbis = applicationAbis(kit());
    if (appAbis.isEmpty()) {
        QMessageBox::warning(dlgParent, Tr::tr("Install HAP"),
                             Tr::tr("No HarmonyOS ABIs are defined for the current kit.\n"
                                    "Add a Qt for Harmony version to the kit and check the project configuration."));
        TaskHub::addTask(DeploymentTask(Task::Warning, Tr::tr("Install HAP: kit has no Harmony ABIs.")));
        return;
    }

    const IDevice::ConstPtr device = RunDeviceKitAspect::device(kit());
    if (!device) {
        QMessageBox::warning(dlgParent, Tr::tr("Install HAP"),
                             Tr::tr("No run device is selected.\n"
                                    "Select a HarmonyOS device in Projects → Run or in the kit settings."));
        return;
    }

    const HarmonyDeviceInfo info = HarmonyDevice::harmonyDeviceInfoFromDevice(device);
    if (!info.isValid()) {
        QMessageBox::warning(dlgParent, Tr::tr("Install HAP"),
                             Tr::tr("The current device is not valid for deployment.\n"
                                    "Refresh the device list (Devices) and ensure the device is connected."));
        return;
    }

    if (info.type == IDevice::Emulator) {
        QMessageBox::information(dlgParent, Tr::tr("Install HAP"),
                                 Tr::tr("Installing a HAP onto a HarmonyOS emulator from this dialog is not supported yet.\n"
                                        "Use a physical device, or install from DevEco Studio."));
        return;
    }

    const QString serialNumber = info.serialNumber.trimmed();
    if (serialNumber.isEmpty()) {
        QMessageBox::warning(dlgParent, Tr::tr("Install HAP"),
                             Tr::tr("The selected device has no serial number.\n"
                                    "Reconnect the device, run \"hdc list targets\", and refresh devices in Qt Creator."));
        return;
    }

    if (device->deviceState() != IDevice::DeviceReadyToUse) {
        QMessageBox::warning(dlgParent, Tr::tr("Install HAP"),
                             Tr::tr("The device \"%1\" is not ready (state: not connected or busy).\n"
                                    "Wait until the device is ready, then try again.")
                                 .arg(device->displayName()));
        return;
    }

    const QPointer<QWidget> dlgParentPtr(dlgParent);
    const auto onHdcSetup = [serialNumber, packagePath, hdcPath, dlgParentPtr](Process &p) {
        return hapInstallSetup(serialNumber, packagePath, hdcPath, dlgParentPtr, p);
    };
    const auto onHdcDone = [dlgParent](const Process &p, DoneWith r) {
        hapInstallDone(dlgParent, p, r);
    };
    m_taskTreeRunner.start(Group{ProcessTask(onHdcSetup, onHdcDone)});
}

QWidget *HarmonyDeployQtStep::createConfigWidget()
{
    auto widget = new QWidget;  // NOSONAR - parented, will be owned by the caller
    auto installCustomHapButton = new QPushButton(widget);  // NOSONAR - parented, will be owned by the caller
    installCustomHapButton->setText(Tr::tr("Install a HAP File…"));
    installCustomHapButton->setToolTip(
        Tr::tr("Install a .hap package on the run device selected in the active kit using hdc."));

    connect(installCustomHapButton, &QAbstractButton::clicked, this, [this] {
        onInstallHapClicked();
    });

    using namespace Layouting;

    Form {
        m_uninstallPreviousPackage, br,
        installCustomHapButton,
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
    qCDebug(harmonyDeployLog).noquote() << message;
    emit addOutput(message, OutputFormat::ErrorMessage);
    TaskHub::addTask(DeploymentTask(type, message));
}

/* ** HarmonyDeployQtStepFactory */
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
