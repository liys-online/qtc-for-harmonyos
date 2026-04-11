#ifndef HARMONYSETTINGSWIDGET_H
#define HARMONYSETTINGSWIDGET_H
#include <coreplugin/dialogs/ioptionspage.h>
#include <projectexplorer/toolchainmanager.h>

#include <QHash>

class QListWidget;
class QPushButton;
class QLineEdit;
class QCheckBox;
class QWidget;
namespace Ohos::Internal {
class SummaryWidget;
class HarmonySettingsWidget final : public Core::IOptionsPageWidget
{
public:
    HarmonySettingsWidget();
private:
    /**
     * @brief showEvent
     * 显示事件
     * @param event
     */
    void showEvent(QShowEvent *event) override;
    /**
     * @brief updateSdkList
     * 更新SDK列表
     */
    void updateSdkList();
    /**
     * @brief reloadQmakeListFromConfig
     * 从 HarmonyConfig 重新填充 Qt for Harmony qmake 列表（供外部 applyConfig 后刷新 UI）
     */
    void reloadQmakeListFromConfig();
    /**
     * @brief openDownloadUrl
     * 打开Harmony Sdk下载链接
     */
    void openDownloadUrl();
    /**
     * @brief openMakeDownloadUrl
     * 打开Make下载链接
     */
    void openMakeDownloadUrl();
    /**
     * @brief addSdkItem
     * 添加SDK项
     */
    void addSdkItem();
    /**
     * @brief addQmakeItem
     * 添加Qmake项
     */
    void addQmakeItem();
    /**
     * @brief checkSdkItem
     * 检查SDK项
     * @param sdkLocation
     */
    void checkSdkItem(const Utils::FilePath &sdkLocation);
    /**
     * @brief checkQmakeItem
     * 检查Qmake项
     * @param qmakeLocation
     */
    void checkQmakeItem(const Utils::FilePath &qmakeLocation);
    /**
     * @brief isDefaultSdkSelected
     * 是否选择了默认SDK
     * @return
     */
    bool isDefaultSdkSelected() const;
    /**
     * @brief setAllOk
     * 设置所有正常
     */
    void setAllOk();
    /**
     * @brief validateSdk
     * 验证SDK
     */
    void validateSdk();
    /**
     * @brief findGccToolChain
     * 查找GCC工具链
     * @return
     */
    ProjectExplorer::Toolchain *findGccToolChain() const;

private Q_SLOTS:
    /**
     * @brief onDevecoStudioPathChanged
     * 当Deveco Studio路径改变时
     */
    void onDevecoStudioPathChanged();
    /**
     * @brief onMakePathChanged
     * 当Make路径改变时
     */
    void onMakePathChanged();
    void onOhosSdkRootChanged();
    void reloadOhpmControlsFromConfig();
    void reloadOhModuleDeviceTypesFromConfig();
    void persistOhModuleDeviceTypesFromCheckBoxes();

    /**
     * @brief m_makePathChooser
     * Make路径选择器
     */
    Utils::PathChooser *m_makePathChooser = nullptr;
    /**
     * @brief m_devecoStudioPathChooser
     * Deveco Studio路径选择器
     */
    Utils::PathChooser *m_devecoStudioPathChooser = nullptr;
    Utils::PathChooser *m_ohosSdkRootChooser = nullptr;
    QLineEdit *m_ohpmRegistryEdit = nullptr;
    QCheckBox *m_ohpmStrictSslCheck = nullptr;
    QWidget *m_ohModuleDeviceTypesWidget = nullptr;
    QHash<QString, QCheckBox *> m_moduleDeviceTypeCheckBoxes;
    /**
     * @brief m_makeDefaultSdkButton
     * 设置默认SDK按钮
     */
    QPushButton *m_makeDefaultSdkButton = nullptr;
    /**
     * @brief m_sdkListWidget
     * SDK列表控件
     */
    QListWidget *m_sdkListWidget = nullptr;
    /**
     * @brief m_qmakeListWidget
     * Qmake列表控件
     */
    QListWidget *m_qmakeListWidget = nullptr;
    /**
     * @brief m_isInitialReloadDone
     * 是否是初始重新加载
     */
    bool m_isInitialReloadDone = false;
    /**
     * @brief m_harmonySummary
     * Harmony Sdk配置总结控件
     */
    SummaryWidget *m_harmonySummary = nullptr;
    /**
     * @brief m_qtForHarmonySummary
     * Qt for Harmony配置总结控件
     */
    SummaryWidget *qtForHarmonySummary = nullptr;

};
enum HarmonyValidation {
    DevecoPathExistsAndWritableRow,
    OhosSdkManagerRootRow,
    HdcToolsInstalledRow,
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
