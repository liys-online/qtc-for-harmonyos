#include "harmonysettingswidget.h"
#include "ohostr.h"
#include "ohosconstants.h"
#include <coreplugin/icore.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <utils/utilsicons.h>
#include <utils/layoutbuilder.h>
#include <utils/hostosinfo.h>
#include <utils/pathchooser.h>
#include <utils/detailswidget.h>
#include <utils/guiutils.h>
#include <QLabel>
#include <QToolButton>
#include <QDesktopServices>
#include "harmonyconfigurations.h"
#include "harmonysdkmanagerdialog.h"
#include "harmonyqttsdkmanagerdialog.h"
#include <QMessageBox>
#include <QVBoxLayout>
#include <QTimer>
#include <QPushButton>
#include <QListWidget>
#include <QStandardPaths>
#include <QFileDialog>

namespace Ohos::Internal {

class SummaryWidget : public QWidget
{
    class RowData {
    public:
        InfoLabel *m_infoLabel = nullptr;
        bool m_valid = false;
        QString m_validText;
    };

public:
    SummaryWidget(const QMap<int, QString> &validationPoints, const QString &validText,
                  const QString &invalidText, DetailsWidget *detailsWidget) :
        QWidget(detailsWidget),
        m_validText(validText),
        m_invalidText(invalidText),
        m_detailsWidget(detailsWidget)
    {
        QTC_CHECK(m_detailsWidget);
        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(22, 0, 0, 12);
        layout->setSpacing(4);
        for (auto itr = validationPoints.cbegin(); itr != validationPoints.cend(); ++itr) {
            RowData data;
            data.m_infoLabel = new InfoLabel(itr.value());
            data.m_validText = itr.value();
            layout->addWidget(data.m_infoLabel);
            m_validationData[itr.key()] = data;
            setPointValid(itr.key(), false);
        }
        m_detailsWidget->setWidget(this);
        setContentsMargins(0, 0, 0, 0);
    }


    void setPointValid(int key, bool valid, const QString &errorText = {})
    {
        if (!m_validationData.contains(key))
            return;
        RowData &data = m_validationData[key];
        data.m_valid = valid;
        data.m_infoLabel->setType(valid ? InfoLabel::Ok : InfoLabel::NotOk);
        data.m_infoLabel->setText(valid || errorText.isEmpty() ? data.m_validText : errorText);
        updateUi();
    }

    bool rowsOk(const QList<int> &keys) const
    {
        auto isKeyValid = [this](const auto &key) {
            return m_validationData[key].m_valid;
        };
        return std::all_of(keys.cbegin(), keys.cend(), isKeyValid);
    }

    bool allRowsOk() const
    {
        return rowsOk(m_validationData.keys());
    }

    void setInfoText(const QString &text)
    {
        m_infoText = text;
        updateUi();
    }

    void setInProgressText(const QString &text)
    {
        m_detailsWidget->setIcon({});
        m_detailsWidget->setSummaryText(QString("%1...").arg(text));
        m_detailsWidget->setState(DetailsWidget::Collapsed);
    }

    void setSetupOk(bool ok)
    {
        m_detailsWidget->setState(ok ? DetailsWidget::Collapsed : DetailsWidget::Expanded);
    }

    void setState(DetailsWidget::State state)
    {
        m_detailsWidget->setState(state);
    }

private:
    void updateUi() {
        bool ok = allRowsOk();
        m_detailsWidget->setIcon(ok ? Icons::OK.icon() : Icons::CRITICAL.icon());
        m_detailsWidget->setSummaryText(ok ? QString("%1 %2").arg(m_validText, m_infoText)
                                           : m_invalidText);
    }
    QString m_validText;
    QString m_invalidText;
    QString m_infoText;
    DetailsWidget *m_detailsWidget = nullptr;
    QMap<int, RowData> m_validationData;
};

HarmonySettingsWidget::HarmonySettingsWidget()
{
    setWindowTitle(Tr::tr("Harmony Configuration"));
    const QIcon downloadIcon = Icons::ONLINE.icon();
    const bool showMakeControls = HostOsInfo::isWindowsHost();

    auto *makeLocationLabel = new QLabel(Tr::tr("Make location:"));
    makeLocationLabel->setVisible(showMakeControls);

    m_makePathChooser = new PathChooser;
    m_makePathChooser->setFilePath(HarmonyConfig::makeLocation());
    m_makePathChooser->setPromptDialogTitle(Tr::tr("Select Make Location"));
    m_makePathChooser->setVisible(showMakeControls);
    auto downloadmake = new QToolButton;
    downloadmake->setIcon(downloadIcon);
    downloadmake->setToolTip(Tr::tr("Open mingw download URL in the system's browser."));
    downloadmake->setVisible(showMakeControls);
    m_devecoStudioPathChooser = new PathChooser;
    if (HarmonyConfig::devecoStudioLocation().isEmpty())
        HarmonyConfig::tryAutoDetectDevecoStudio();
    m_devecoStudioPathChooser->setFilePath(HarmonyConfig::devecoStudioLocation());
    m_devecoStudioPathChooser->setPromptDialogTitle(Tr::tr("Select Deveco Studio Folder"));

    m_ohosSdkRootChooser = new PathChooser;
    m_ohosSdkRootChooser->setExpectedKind(PathChooser::Directory);
    if (HarmonyConfig::ohosSdkRoot().isEmpty())
        HarmonyConfig::setOhosSdkRoot(HarmonyConfig::defaultOhosSdkPath());
    m_ohosSdkRootChooser->setFilePath(HarmonyConfig::ohosSdkRoot());
    m_ohosSdkRootChooser->setPromptDialogTitle(Tr::tr("Select HarmonyOS SDK Folder"));
    m_ohosSdkRootChooser->setToolTip(
        Tr::tr("Root directory for SDK Manager: archives download to .temp/ here, and unpack to "
               "<API version>/ subfolders (same layout idea as the Android SDK location)."));

    auto downloadSdkToolButton = new QToolButton;
    downloadSdkToolButton->setIcon(downloadIcon);
    downloadSdkToolButton->setToolTip(Tr::tr("Open OpenHarmony SDK download URL in the system's browser."));

    auto addSDKButton = new QPushButton(Tr::tr("Add..."));
    addSDKButton->setToolTip(Tr::tr("Add the selected SDK. The toolchains "
                                          "and debuggers will be created automatically."));
    auto removeSDKButton = new QPushButton(Tr::tr("Remove"));
    removeSDKButton->setEnabled(false);
    removeSDKButton->setToolTip(Tr::tr("Remove the selected SDK if it has been added manually."));

    auto manageSdkPackagesBtn = new QPushButton(Tr::tr("Manage SDK Packages…"));
    manageSdkPackagesBtn->setToolTip(
        Tr::tr("Download OpenHarmony SDK components using the official package index (same API as the "
               "Qt for OHOS build scripts)."));

    m_makeDefaultSdkButton = new QPushButton;

    m_sdkListWidget = new QListWidget;
    m_sdkListWidget->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    m_sdkListWidget->setIconSize(QSize(16, 16));
    m_sdkListWidget->setResizeMode(QListView::Adjust);
    m_sdkListWidget->setModelColumn(0);
    m_sdkListWidget->setSortingEnabled(false);

    auto downloadDevecoStudio = new QToolButton;
    downloadDevecoStudio->setIcon(downloadIcon);
    downloadDevecoStudio->setToolTip(Tr::tr("Open Deveco Studio download URL in the system's browser."));

    m_qmakeListWidget = new QListWidget;
    m_qmakeListWidget->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    m_qmakeListWidget->setIconSize(QSize(16, 16));
    m_qmakeListWidget->setResizeMode(QListView::Adjust);
    m_qmakeListWidget->setModelColumn(0);
    m_qmakeListWidget->setSortingEnabled(false);

    auto addQmakeButton = new QPushButton(Tr::tr("Add..."));
    addQmakeButton->setToolTip(Tr::tr("Add the selected qmake. The toolchains "
                                          "and debuggers will be created automatically."));
    auto removeQmakeButton = new QPushButton(Tr::tr("Remove"));
    removeQmakeButton->setEnabled(false);

    auto downloadQtForHarmony = new QToolButton;
    downloadQtForHarmony->setIcon(downloadIcon);
    downloadQtForHarmony->setToolTip(Tr::tr("Open Qt for Harmony download URL in the system's browser."));
    auto manageQtOhSdkBtn = new QPushButton(Tr::tr("Manage Qt for OpenHarmony SDK…"));
    manageQtOhSdkBtn->setToolTip(
        Tr::tr("Browse Qt for OpenHarmony packages from the binary catalog JSON "
               "(qt-oh-binary-catalog.v1.json), loaded from GitCode with GitHub fallback."));
    auto qtForHarmonyDetailsWidget = new DetailsWidget;
    auto harmonyDetailsWidget = new DetailsWidget;
    const QMap<int, QString> harmonyValidationPoints = {
        {DevecoPathExistsAndWritableRow, Tr::tr("Deveco Studio path exists and is writable")},
        {OhosSdkManagerRootRow, Tr::tr("HarmonyOS SDK location exists and is writable")},
        {SdkPathExistsAndWritableRow, Tr::tr("SDK path exists and is writable")},
        {SdkToolsInstalledRow, Tr::tr("SDK tools installed")},
        {AllEssentialsInstalledRow, Tr::tr("All essentials installed")},
    };
    m_harmonySummary = new SummaryWidget(harmonyValidationPoints, Tr::tr("Harmony settings are OK."),
                                         Tr::tr("Harmony settings have errors."), harmonyDetailsWidget);

    const QMap<int, QString> qtForHarmonyValidationPoints = {
        {QMakeToolsInstalledRow, Tr::tr("Qt for Harmony qmake exists")},
    };
    qtForHarmonySummary = new SummaryWidget(qtForHarmonyValidationPoints, Tr::tr("Qt for Harmony settings are OK."),
                                            Tr::tr("Qt for Harmony settings have errors."), qtForHarmonyDetailsWidget);
    using namespace Layouting;

    Column {
        Tr::tr("All changes on this page take effect immediately."),
        Group {
            title(Tr::tr("Harmony Settings")),
            Grid {
                makeLocationLabel,
                m_makePathChooser,
                downloadmake,
                br,
                Tr::tr("Deveco studio location:"),
                m_devecoStudioPathChooser,
                downloadDevecoStudio,
                br,
                Tr::tr("HarmonyOS SDK location:"),
                m_ohosSdkRootChooser,
                st,
                br,
                Column { Tr::tr("OpenHarmony SDK list:"), st },
                m_sdkListWidget,
                Column {
                    addSDKButton,
                    removeSDKButton,
                    m_makeDefaultSdkButton,
                    manageSdkPackagesBtn,
                },
                downloadSdkToolButton,
                br,
                Span(4, harmonyDetailsWidget), br,

            }
        },
        Group {
            title(Tr::tr("Qt for Harmony Settings")),
            Grid {
                Column {Tr::tr("Qt for Harmony qmake list:"), st },
                m_qmakeListWidget,
                Column {
                    addQmakeButton,
                    removeQmakeButton,
                    manageQtOhSdkBtn,
                },
                downloadQtForHarmony,
                br,

                Span(4, qtForHarmonyDetailsWidget)
            }
        },
        st
    }.attachTo(this);

    connect(m_devecoStudioPathChooser, &PathChooser::rawPathChanged,
        this, &HarmonySettingsWidget::onDevecoStudioPathChanged);
    connect(m_ohosSdkRootChooser, &PathChooser::rawPathChanged,
            this, &HarmonySettingsWidget::onOhosSdkRootChanged);
    connect(m_sdkListWidget, &QListWidget::currentTextChanged,
            this, [this, removeSDKButton](const QString &sdk) {
                setAllOk();
                removeSDKButton->setEnabled(HarmonyConfig::getSdkList().contains(sdk));
            });
    connect(addSDKButton, &QPushButton::clicked, this,
            &HarmonySettingsWidget::addSdkItem);

    connect(removeSDKButton, &QPushButton::clicked, this, [this, removeSDKButton] {
        if (isDefaultSdkSelected())
            HarmonyConfig::setdefaultSdk({});
        HarmonyConfig::removeSdkList(m_sdkListWidget->currentItem()->text());
        m_sdkListWidget->takeItem(m_sdkListWidget->currentRow());
    });
    connect(m_makeDefaultSdkButton, &QPushButton::clicked, this, [this] {
        const FilePath defaultSdk = isDefaultSdkSelected()
        ? FilePath()
        : FilePath::fromUserInput(m_sdkListWidget->currentItem()->text());
        HarmonyConfig::setdefaultSdk(defaultSdk);
        setAllOk();
    });

    connect(m_qmakeListWidget, &QListWidget::currentTextChanged,
            this, [this, removeQmakeButton](const QString &qmake) {
                setAllOk();
                removeQmakeButton->setEnabled(HarmonyConfig::getQmakeList().contains(qmake));
            });
    connect(addQmakeButton, &QPushButton::clicked, this,
            &HarmonySettingsWidget::addQmakeItem);

    connect(removeQmakeButton, &QPushButton::clicked, this, [this, removeQmakeButton] {
        HarmonyConfig::removeQmake(m_qmakeListWidget->currentItem()->text());
        m_qmakeListWidget->takeItem(m_qmakeListWidget->currentRow());
    });

    connect(m_makePathChooser, &PathChooser::rawPathChanged, this, &HarmonySettingsWidget::onMakePathChanged);

    connect(downloadmake, &QAbstractButton::clicked, this, &HarmonySettingsWidget::openMakeDownloadUrl);
    connect(downloadDevecoStudio, &QAbstractButton::clicked, this, &HarmonySettingsWidget::openDownloadUrl);
    connect(downloadSdkToolButton, &QAbstractButton::clicked, this, &HarmonySettingsWidget::openDownloadUrl);
    connect(manageSdkPackagesBtn, &QPushButton::clicked, this, [] {
        executeHarmonySdkManagerDialog(Core::ICore::dialogParent());
    });
    connect(manageQtOhSdkBtn, &QPushButton::clicked, this, [] {
        executeHarmonyQtOhSdkManagerDialog(Core::ICore::dialogParent());
    });
    setOnApply([] { HarmonyConfigurations::applyConfig(); });
    // 1) IOptionsPageWidget 无 Aspect/dirtyChecker 时 isDirty() 默认 true。
    // 2) Utils::markSettingsDirty() 会把整个「偏好设置」的 m_isDirty 置 true；切换子页时会弹
    //    “The previous page contains unsaved changes.”，与 (1) 无关。本页已用 HarmonyConfig 即时写盘，
    //    validateSdk() 也会在首次显示时跑一遍，故不得调用 markSettingsDirty()。
    setDirtyChecker([] { return false; });

    connect(HarmonyConfigurations::instance(), &HarmonyConfigurations::updated, this, [this] {
        reloadQmakeListFromConfig();
        updateSdkList();
        setAllOk();
    }, Qt::QueuedConnection);
}

void HarmonySettingsWidget::showEvent(QShowEvent *event)
{
    Q_UNUSED(event)
    if (!m_isInitialReloadDone) {
        QTimer::singleShot(0, this, [this] {
            validateSdk();
        });
        m_isInitialReloadDone = true;
    }
}

void HarmonySettingsWidget::updateSdkList()
{
    m_sdkListWidget->clear();
    const QStringList sdkList = HarmonyConfig::getSdkList();
    for (const QString &sdk : sdkList) {
        m_sdkListWidget->addItem(new QListWidgetItem(Icons::UNLOCKED.icon(), sdk));
    }
}

void HarmonySettingsWidget::reloadQmakeListFromConfig()
{
    m_qmakeListWidget->clear();
    for (const QString &qmake : HarmonyConfig::getQmakeList()) {
        if (!qmake.isEmpty())
            m_qmakeListWidget->addItem(new QListWidgetItem(Icons::UNLOCKED.icon(), qmake));
    }

    bool hasValid = false;
    for (const QString &qmake : HarmonyConfig::getQmakeList()) {
        if (!qmake.isEmpty() && FilePath::fromString(qmake).isExecutableFile()) {
            hasValid = true;
            break;
        }
    }
    qtForHarmonySummary->setPointValid(
        QMakeToolsInstalledRow,
        hasValid,
        hasValid ? QString() : Tr::tr("Qt for Harmony qmake does not exist"));
}

void HarmonySettingsWidget::openDownloadUrl()
{
    QDesktopServices::openUrl(QUrl::fromUserInput(
        "https://developer.huawei.com/consumer/cn/download/"));
}

void HarmonySettingsWidget::openMakeDownloadUrl()
{
    QDesktopServices::openUrl(QUrl::fromUserInput(
        "https://download.qt.io/development_releases/prebuilt/mingw_64/"));
}

void HarmonySettingsWidget::addSdkItem()
{
    const QString homePath = QStandardPaths::standardLocations(QStandardPaths::HomeLocation)
    .constFirst();
    const QString sdkPath = QFileDialog::getExistingDirectory(Core::ICore::dialogParent(),
                                                              Tr::tr("Select an NDK"),
                                                              homePath);

    checkSdkItem(sdkPath);
}

void HarmonySettingsWidget::addQmakeItem()
{
    const QString homePath = QStandardPaths::standardLocations(QStandardPaths::HomeLocation)
    .constFirst();
    const QString qmakePath = QFileDialog::getOpenFileName(Core::ICore::dialogParent(),
                                                           Tr::tr("Select Qt for Harmony qmake"),
                                                           homePath,
                                                           Tr::tr("qmake*"));
    checkQmakeItem(qmakePath);
}

void HarmonySettingsWidget::checkSdkItem(QString sdkLocation)
{
    if (FilePath::fromString(sdkLocation).isReadableDir())
    {
        m_harmonySummary->setPointValid(SdkPathExistsAndWritableRow, true);

    }
    else
    {
        m_harmonySummary->setPointValid(SdkPathExistsAndWritableRow, false
                                        , Tr::tr("SDK path does not exist or is not writable"));
    }

    if (HarmonyConfig::isValidSdk(sdkLocation)) {
        HarmonyConfig::addSdk(sdkLocation);
        if (m_sdkListWidget->findItems(sdkLocation, Qt::MatchExactly).size() == 0) {
            m_sdkListWidget->addItem(new QListWidgetItem(Icons::UNLOCKED.icon(), sdkLocation));
            if (HarmonyConfig::defaultSdk().isEmpty()) {
                HarmonyConfig::setdefaultSdk(FilePath::fromString(sdkLocation));
            }
        }
        m_harmonySummary->setPointValid(SdkToolsInstalledRow, true);
        setAllOk();
    } else if (!sdkLocation.isEmpty()) {
        QMessageBox::warning(
            Core::ICore::dialogParent(),
            Tr::tr("Add Custom SDK"),
            Tr::tr("The selected path has an invalid SDK. This might mean that the path contains space "
                   "characters, or that it does not have a \"toolchains\" sub-directory, or that the "
                   "SDK metadata could not be retrieved because of a missing "
                   "\"oh-uni-package.json\" file."));
        m_harmonySummary->setPointValid(SdkToolsInstalledRow, false
                                        , Tr::tr("SDK tools are not installed"));
        setAllOk();
    }
}

void HarmonySettingsWidget::checkQmakeItem(QString qmakeLocation)
{
    if (FilePath::fromString(qmakeLocation).isExecutableFile())
    {
        qtForHarmonySummary->setPointValid(QMakeToolsInstalledRow, true);
        HarmonyConfig::addQmake(qmakeLocation);
        if (m_qmakeListWidget->findItems(qmakeLocation, Qt::MatchExactly).size() == 0) {
            m_qmakeListWidget->addItem(new QListWidgetItem(Icons::UNLOCKED.icon(), qmakeLocation));
        }
    }
    else
    {
        qtForHarmonySummary->setPointValid(QMakeToolsInstalledRow, false
                                           , Tr::tr("Qt for Harmony qmake does not exist"));
    }
}

bool HarmonySettingsWidget::isDefaultSdkSelected() const
{
    if (!HarmonyConfig::defaultSdk().isEmpty()) {
        if (const QListWidgetItem *item = m_sdkListWidget->currentItem()) {
            return FilePath::fromUserInput(item->text()) == HarmonyConfig::defaultSdk();
        }
    }
    return false;
}

void HarmonySettingsWidget::setAllOk()
{
    m_harmonySummary->setPointValid(AllEssentialsInstalledRow, true);
    if (!m_harmonySummary->allRowsOk())
    {
        m_harmonySummary->setPointValid(AllEssentialsInstalledRow, false);
    }
    // Mark default entry in NDK list widget
    {
        const QFont font = m_sdkListWidget->font();
        QFont markedFont = font;
        markedFont.setItalic(true);
        for (int row = 0; row < m_sdkListWidget->count(); ++row) {
            QListWidgetItem *item = m_sdkListWidget->item(row);
            const bool isDefaultNdk =
                FilePath::fromUserInput(item->text()) == HarmonyConfig::defaultSdk();
            item->setFont(isDefaultNdk ? markedFont : font);
            item->setIcon((isDefaultNdk ? Icons::LOCKED : Icons::UNLOCKED).icon());
        }
    }

    m_makeDefaultSdkButton->setEnabled(m_sdkListWidget->count() > 0);
    m_makeDefaultSdkButton->setText(isDefaultSdkSelected() ? Tr::tr("Unset Default")
                                                           : Tr::tr("Make Default"));
}

void HarmonySettingsWidget::onDevecoStudioPathChanged()
{
    const FilePath devecoStudioPath = m_devecoStudioPathChooser->filePath().cleanPath();

    if (HarmonyConfig::isValidDevecoStudioRoot(devecoStudioPath)) {
        m_harmonySummary->setPointValid(DevecoPathExistsAndWritableRow, true);
        HarmonyConfig::setDevecoStudioLocation(devecoStudioPath);
    } else {
        QString hint;
        if (HostOsInfo::isMacHost()) {
            hint = Tr::tr("\n\nOn macOS, select the application bundle (e.g. DevEco-Studio.app under "
                          "/Applications), or set DEVECO_STUDIO_HOME to that .app path.");
        }
        m_harmonySummary->setPointValid(
            DevecoPathExistsAndWritableRow,
            false,
            Tr::tr("Deveco Studio path is not valid (expect \"tools\" or IDE launcher under bin/ or "
                   "MacOS/).%1")
                .arg(hint));
    }
    setAllOk();
}

void HarmonySettingsWidget::onMakePathChanged()
{
    if (!HostOsInfo::isWindowsHost())
        return;
    const FilePath makePath = m_makePathChooser->filePath().cleanPath();
    HarmonyConfig::setMakeLocation(makePath);
}

void HarmonySettingsWidget::onOhosSdkRootChanged()
{
    const FilePath p = m_ohosSdkRootChooser->filePath().cleanPath();
    if (p.isEmpty()) {
        HarmonyConfig::setOhosSdkRoot({});
        m_harmonySummary->setPointValid(
            OhosSdkManagerRootRow,
            false,
            Tr::tr("Set HarmonyOS SDK location for SDK Manager (.temp/ downloads and <API>/ unpack)."));
        setAllOk();
        return;
    }
    if (!p.exists()) {
        if (!p.createDir()) {
            m_harmonySummary->setPointValid(OhosSdkManagerRootRow, false,
                                            Tr::tr("Cannot create HarmonyOS SDK directory"));
            setAllOk();
            return;
        }
    }
    if (p.isWritableDir()) {
        HarmonyConfig::setOhosSdkRoot(p);
        m_harmonySummary->setPointValid(OhosSdkManagerRootRow, true);
        if (HarmonyConfig::registerDownloadedSdksUnder(HarmonyConfig::effectiveOhosSdkRoot()) > 0)
            HarmonyConfigurations::applyConfig();
    } else {
        m_harmonySummary->setPointValid(OhosSdkManagerRootRow, false,
                                        Tr::tr("HarmonyOS SDK directory is not writable"));
    }
    setAllOk();
}

void HarmonySettingsWidget::validateSdk()
{
    onDevecoStudioPathChanged();
    onOhosSdkRootChanged();
    onMakePathChanged();
    for(const QString &qmake : qAsConst(HarmonyConfig::getQmakeList()))
    {
        checkQmakeItem(qmake);
    }
    if (HarmonyConfig::registerDownloadedSdksUnder(HarmonyConfig::effectiveOhosSdkRoot()) > 0)
        HarmonyConfigurations::applyConfig();
    updateSdkList();
    for(const QString &sdk : qAsConst(HarmonyConfig::getSdkList()))
    {
        checkSdkItem(sdk);
    }
    setAllOk();
}

Toolchain *HarmonySettingsWidget::findGccToolChain() const
{
    const Abi targetAbi = Abi(Abi::X86Architecture, Abi::WindowsOS, Abi::WindowsMSysFlavor, Abi::PEFormat, 64);
    QList<Toolchain *>  toolchains = ToolchainManager::findToolchains(targetAbi);
    if (toolchains.isEmpty())
        return nullptr;
    return toolchains.first();
}

HarmonySettingsPage::HarmonySettingsPage()
{
    setId(Constants::HARMONY_SETTINGS_ID);
    setDisplayName(Tr::tr("HarmonyOS"));
    setCategory(ProjectExplorer::Constants::SDK_SETTINGS_CATEGORY);
    setAutoApply(); // 与 Android 设置页一致；多数选项已即时写入 HarmonyConfig
    setWidgetCreator([] { return new HarmonySettingsWidget; });
}

void setupHarmonySettingsPage()
{
    static HarmonySettingsPage settingsPage;
}

}
