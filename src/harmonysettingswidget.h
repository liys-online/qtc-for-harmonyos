#ifndef HARMONYSETTINGSWIDGET_H
#define HARMONYSETTINGSWIDGET_H
#include <coreplugin/dialogs/ioptionspage.h>
#include <projectexplorer/toolchainmanager.h>
using namespace Utils;
using namespace ProjectExplorer;
class QListWidget;
class QPushButton;
namespace Ohos::Internal {
class SummaryWidget;
class HarmonySettingsWidget final : public Core::IOptionsPageWidget
{
public:
    HarmonySettingsWidget();
private:
    void showEvent(QShowEvent *event) override;

    void updateSdkList();
    void openDownloadUrl();
    void openMakeDownloadUrl();
    void addSdkItem();
    void addQmakeItem();
    void checkSdkItem(QString sdkLocation);
    void checkQmakeItem(QString qmakeLocation);
    bool isDefaultSdkSelected() const;
    void setAllOk();
    void onDevecoStudioPathChanged();
    void onMakePathChanged();
    void validateSdk();
    Toolchain *findGccToolChain() const;
    PathChooser *m_makePathChooser = nullptr;
    PathChooser *m_devecoStudioPathChooser = nullptr;
    QPushButton *m_makeDefaultSdkButton = nullptr;
    QListWidget *m_sdkListWidget = nullptr;
    QListWidget *m_qmakeListWidget = nullptr;

    bool m_isInitialReloadDone = false;
    SummaryWidget *m_harmonySummary = nullptr;
    SummaryWidget *qtForHarmonySummary = nullptr;

};
enum HarmonyValidation {
#ifdef Q_OS_WIN
    DevecoPathExistsAndWritableRow,
#endif
    SdkPathExistsAndWritableRow,
    SdkToolsInstalledRow,
    QMakeToolsInstalledRow,
    AllEssentialsInstalledRow,
};
class HarmonySettingsPage final : public Core::IOptionsPage
{
public:
    HarmonySettingsPage();
};
void setupHarmonySettingsPage();

} // namespace Ohos::Internal

#endif // HARMONYSETTINGSWIDGET_H
