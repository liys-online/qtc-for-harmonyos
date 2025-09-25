#ifndef HARMONYBUILDHAPSTEP_H
#define HARMONYBUILDHAPSTEP_H

#include <projectexplorer/abstractprocessstep.h>

namespace Ohos::Internal {

class HarmonyBuildHapStep : public ProjectExplorer::AbstractProcessStep
{
    Q_OBJECT
public:
    HarmonyBuildHapStep(ProjectExplorer::BuildStepList *bc, Utils::Id id);

    void fromMap(const Utils::Store &map) override;
    void toMap(Utils::Store &map) const override;

    QString buildTargetSdk() const;
    void setBuildTargetSdk(const QString &sdk);

    QString buildToolsVersion() const;
    void setBuildToolsVersion(const QString &version);
protected:
    Tasking::GroupItem defaultProcessTask();
    Tasking::GroupItem syncProjectTask();
    Tasking::GroupItem ohpmInstallTask();
    bool setupProcess(Utils::Process &process);

Q_SIGNALS:
    void createTemplates();
private:
    QWidget *createConfigWidget() override;
    bool init() override;
    Tasking::GroupItem runRecipe() override;
    friend class HarmonyBuildHapWidget;
    QString m_buildTargetSdk;
    QString m_buildToolsVersion;
};

void setupHarmonyBuildHapStep();
} // namespace Harmony::Internal
#endif // HARMONYBUILDHAPSTEP_H
