#ifndef HARMONYQTVERSION_H
#define HARMONYQTVERSION_H
#include <qtsupport/baseqtversion.h>
#include <qtsupport/qtversionfactory.h>
namespace Ohos::Internal {
class HarmonyQtVersion : public QtSupport::QtVersion
{
public:
    HarmonyQtVersion();
    /** True when qmake / Qt metadata exposes more than one target ABI (no fixed Qt version number). */
    bool supportsMultipleQtAbis() const override;
    /**
     * @brief addToBuildEnvironment
     * 添加到构建环境
     * @param k
     * @param env
     */
    void addToBuildEnvironment(const ProjectExplorer::Kit *k, Utils::Environment &env) const override;
    /**
     * @brief targetDeviceTypes
     * 目标设备类型
     * @return
     */
    QSet<Utils::Id> targetDeviceTypes() const override;
    /**
     * @brief description
     * 描述
     * @return
     */
    QString description() const override;
    /**
     * @brief isHarmonyQtVersion
     * 是否是Harmony Qt版本
     * @return
     */
    bool isHarmonyQtVersion() const { return true; };
    /**
     * @brief supportOhVersion
     * 支持的Harmony Sdk版本
     * @return
     */
    QVersionNumber supportOhVersion() const;

    const QStringList ohosAbis() const;

    ProjectExplorer::Abi targetAbi() const;
};
void setupHarmonyQtVersion();
} // Ohos::Internal

#endif // HARMONYQTVERSION_H
