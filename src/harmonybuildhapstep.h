#ifndef HARMONYBUILDHAPSTEP_H
#define HARMONYBUILDHAPSTEP_H

#include <projectexplorer/abstractprocessstep.h>

namespace Utils {
class OutputFormatter;
}

namespace Ohos::Internal {

class HarmonyBuildHapStep : public ProjectExplorer::AbstractProcessStep
{
    Q_OBJECT
public:
    HarmonyBuildHapStep(ProjectExplorer::BuildStepList *bc, Utils::Id id);

    void fromMap(const Utils::Store &map) override;
    void toMap(Utils::Store &map) const override;

    Utils::FilePath ohProjectPath() const;

    QString buildTargetSdk() const;
    void setBuildTargetSdk(const QString &sdk);

    QString buildToolsVersion() const;
    void setBuildToolsVersion(const QString &version);

    QString entryLibOverride() const;
    void setEntryLibOverride(const QString &lib);
    /** Returns the override if set, otherwise auto-detects from the active build key. */
    QString resolvedEntryLib() const;

    QStringList moduleDeviceTypes() const;
    void setModuleDeviceTypes(const QStringList &deviceTypes);
    /** For \c OhProjecteCreator: step line → else Kit → else global preferences (default \c 2in1). */
    QStringList effectiveModuleDeviceTypes() const;

protected:
    QtTaskTree::GroupItem hvigorBuildTask();
    QtTaskTree::GroupItem syncProjectTask();
    QtTaskTree::GroupItem ohpmInstallTask();
    bool setupHvigorProcess(Utils::Process &process);
    /** 创建 build/ohpro，返回规范绝对路径供 hvigor/Node（避免 uv_cwd EPERM）。 */
    bool prepareOhProDirectory(Utils::FilePath *outCwd, QString *errorMessage);

Q_SIGNALS:
    void createTemplates();
private:
    QWidget *createConfigWidget() override;
    bool init() override;
    void setupOutputFormatter(Utils::OutputFormatter *formatter) override;
    QtTaskTree::GroupItem runRecipe() override;
    friend class HarmonyBuildHapWidget;
    QString m_buildTargetSdk;
    QString m_buildToolsVersion;
    QString m_entryLibOverride;
    QStringList m_ohModuleDeviceTypes;
};

void setupHarmonyBuildHapStep();
} // namespace Ohos::Internal
#endif // HARMONYBUILDHAPSTEP_H
