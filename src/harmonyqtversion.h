#ifndef HARMONYQTVERSION_H
#define HARMONYQTVERSION_H
#include <qtsupport/baseqtversion.h>
#include <qtsupport/qtversionfactory.h>
namespace Ohos::Internal {
class HarmonyQtVersion : public QtSupport::QtVersion
{
public:
    HarmonyQtVersion();
    bool supportsMultipleQtAbis() const override;
    void addToBuildEnvironment(const ProjectExplorer::Kit *k, Utils::Environment &env) const override;
    QSet<Utils::Id> targetDeviceTypes() const override;
    QString description() const override;
    QString defaultUnexpandedDisplayName() const;
    bool isHarmonyQtVersion() const { return true; };
    QVersionNumber supportOhVersion() const;
};
void setupHarmonyQtVersion();
} // Ohos::Internal

#endif // HARMONYQTVERSION_H
