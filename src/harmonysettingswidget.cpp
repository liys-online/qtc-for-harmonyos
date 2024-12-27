#include "harmonysettingswidget.h"
#include "ohostr.h"
#include "ohosconstants.h"
#include <projectexplorer/projectexplorerconstants.h>
#include <utils/utilsicons.h>
#include <utils/layoutbuilder.h>
#include <utils/pathchooser.h>
#include <utils/detailswidget.h>
#include <QToolButton>
#include <QDesktopServices>
#include "harmonyconfigurations.h"
#include <QMessageBox>
#include <QVBoxLayout>
#include <QTimer>
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

    template<class T>
    void setPointValid(int key, const expected_str<T> &test)
    {
        setPointValid(key, test.has_value(), test.has_value() ? QString{} : test.error());
    }

    void setPointValid(int key, bool valid, const QString errorText = {})
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
        for (auto key : keys) {
            if (!m_validationData[key].m_valid)
                return false;
        }
        return true;
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
        m_detailsWidget->setSummaryText(ok ? QString("%1 %2").arg(m_validText).arg(m_infoText)
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
    m_makePathChooser = new PathChooser;
    m_makePathChooser->setFilePath(HarmonyConfig::makeLocation());
    m_makePathChooser->setPromptDialogTitle(Tr::tr("Select Make Location"));
    auto downloadmake = new QToolButton;
    downloadmake->setIcon(downloadIcon);
    downloadmake->setToolTip(Tr::tr("Open mingw download URL in the system's browser."));
#ifdef Q_OS_WIN
    m_devecoStudioPathChooser = new PathChooser;
    m_devecoStudioPathChooser->setFilePath(HarmonyConfig::devecoStudioLocation());
    m_devecoStudioPathChooser->setPromptDialogTitle(Tr::tr("Select Deveco Studio Folder"));
#endif

    auto downloadSdkToolButton = new QToolButton;
    downloadSdkToolButton->setIcon(downloadIcon);
    downloadSdkToolButton->setToolTip(Tr::tr("Open OpenHarmony SDK download URL in the system's browser."));

    m_sdkLocationPathChooser = new PathChooser;
    m_sdkLocationPathChooser->setFilePath(HarmonyConfig::sdkLocation());
    m_sdkLocationPathChooser->setPromptDialogTitle(Tr::tr("Select OpenHarmony SDK Folder"));

    auto downloadDevecoStudio = new QToolButton;
    downloadDevecoStudio->setIcon(downloadIcon);
    downloadDevecoStudio->setToolTip(Tr::tr("Open Deveco Studio download URL in the system's browser."));
    m_qmakePathChooser = new PathChooser;
    m_qmakePathChooser->setExpectedKind(PathChooser::Kind::File);
    m_qmakePathChooser->setPromptDialogFilter(Tr::tr("Qmake binaries (qmake.exe)"));
    m_qmakePathChooser->setFilePath(HarmonyConfig::qmakeLocation());
    m_qmakePathChooser->setPromptDialogTitle(Tr::tr("Select Qt for Harmony qmake binaries Location"));
    auto downloadQtForHarmony = new QToolButton;
    downloadQtForHarmony->setIcon(downloadIcon);
    downloadQtForHarmony->setToolTip(Tr::tr("Open Qt for Harmony download URL in the system's browser."));
    auto qtForHarmonyDetailsWidget = new DetailsWidget;
    auto harmonyDetailsWidget = new DetailsWidget;
    const QMap<int, QString> harmonyValidationPoints = {
#ifdef Q_OS_WIN
        {DevecoPathExistsAndWritableRow, Tr::tr("Deveco Studio path exists and is writable")},
#endif
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
                Tr::tr("Make location:"),
                m_makePathChooser,
                downloadmake,
                br,
#ifdef Q_OS_WIN
                Tr::tr("Deveco studio location:"),
                m_devecoStudioPathChooser,
                downloadDevecoStudio,
                br,
#endif
                Tr::tr("OpenHarmony SDK location:"),
                m_sdkLocationPathChooser,
                downloadSdkToolButton,
                br,
                Span(4, harmonyDetailsWidget), br,

            }
        },
        Group {
            title(Tr::tr("Qt for Harmony Settings")),
            Grid {
                Tr::tr("Qt for Harmony qmake location:"),
                m_qmakePathChooser,
                downloadQtForHarmony,
                br,

                Span(4, qtForHarmonyDetailsWidget)
            }
        },
        st
    }.attachTo(this);

    connect(m_sdkLocationPathChooser, &PathChooser::rawPathChanged, this, &HarmonySettingsWidget::onSdkPathChanged);
#ifdef Q_OS_WIN
    connect(m_devecoStudioPathChooser, &PathChooser::rawPathChanged,
        this, &HarmonySettingsWidget::onDevecoStudioPathChanged);
#endif
    connect(m_makePathChooser, &PathChooser::rawPathChanged, this, &HarmonySettingsWidget::onMakePathChanged);
    connect(m_qmakePathChooser, &PathChooser::rawPathChanged, this, &HarmonySettingsWidget::onQmakePathChanged);

    connect(downloadmake, &QAbstractButton::clicked, this, &HarmonySettingsWidget::openMakeDownloadUrl);
    connect(downloadDevecoStudio, &QAbstractButton::clicked, this, &HarmonySettingsWidget::openDownloadUrl);
    connect(downloadSdkToolButton, &QAbstractButton::clicked, this, &HarmonySettingsWidget::openDownloadUrl);
    setOnApply([] { HarmonyConfigurations::applyConfig(); });
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

void HarmonySettingsWidget::setAllOk()
{
    m_harmonySummary->setPointValid(AllEssentialsInstalledRow, true);
    if (!m_harmonySummary->allRowsOk())
    {
        m_harmonySummary->setPointValid(AllEssentialsInstalledRow, false);
    }
}

void HarmonySettingsWidget::onSdkPathChanged()
{
    const FilePath sdkPath = m_sdkLocationPathChooser->filePath().cleanPath();
    if (sdkPath.isReadableDir())
    {
        m_harmonySummary->setPointValid(SdkPathExistsAndWritableRow, true);

    }
    else
    {
        m_harmonySummary->setPointValid(SdkPathExistsAndWritableRow, false
            , Tr::tr("SDK path does not exist or is not writable"));
    }
    FilePath ndkPath = sdkPath / "default" / "openharmony" / "native";
    if (ndkPath.isReadableDir())
    {
        FilePath packagejsonPath = ndkPath / "oh-uni-package.json";
        if (packagejsonPath.isReadableFile())
        {
            if(HarmonyConfig::setVersion(packagejsonPath))
            {
                HarmonyConfig::setSdkLocation(sdkPath);
                HarmonyConfig::setNdkLocation(ndkPath);
                m_harmonySummary->setPointValid(SdkToolsInstalledRow, true);
                setAllOk();
                return;
            }
        }
    }
    m_harmonySummary->setPointValid(SdkToolsInstalledRow, false
        , Tr::tr("SDK tools are not installed"));
    setAllOk();
}

void HarmonySettingsWidget::onDevecoStudioPathChanged()
{
    const FilePath devecoStudioPath = m_devecoStudioPathChooser->filePath().cleanPath();
    FilePath devecoStudioExe = devecoStudioPath.pathAppended("bin/devecostudio64.exe");

    if (devecoStudioExe.isExecutableFile())
    {
        m_harmonySummary->setPointValid(DevecoPathExistsAndWritableRow, true);
        HarmonyConfig::setDevecoStudioLocation(devecoStudioPath);
    }
    else
    {
        m_harmonySummary->setPointValid(DevecoPathExistsAndWritableRow, false
            , Tr::tr("Deveco Studio path does not exist or is not writable"));
    }
    setAllOk();
}

void HarmonySettingsWidget::onMakePathChanged()
{
    const FilePath makePath = m_makePathChooser->filePath().cleanPath();
    HarmonyConfig::setMakeLocation(makePath);
}

void HarmonySettingsWidget::onQmakePathChanged()
{
    const FilePath qmakePath = m_qmakePathChooser->filePath().cleanPath();
    if (qmakePath.isExecutableFile())
    {
        qtForHarmonySummary->setPointValid(QMakeToolsInstalledRow, true);
        HarmonyConfig::setQmakeLocation(qmakePath);
    }
    else
    {
        qtForHarmonySummary->setPointValid(QMakeToolsInstalledRow, false
            , Tr::tr("Qt for Harmony qmake does not exist"));
    }
}

void HarmonySettingsWidget::validateSdk()
{
    onSdkPathChanged();
    onDevecoStudioPathChanged();
    onMakePathChanged();
    onQmakePathChanged();
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
    setCategory(ProjectExplorer::Constants::DEVICE_SETTINGS_CATEGORY);
    setWidgetCreator([] { return new HarmonySettingsWidget; });
}

void setupHarmonySettingsPage()
{
    static HarmonySettingsPage settingsPage;
}

}
