#ifndef HARMONYDEVICEMANAGER_H
#define HARMONYDEVICEMANAGER_H

#include <QObject>
#include <QGlobalStatic>

namespace Ohos::Internal {
class HarmonyDeviceManager : public QObject
{
    Q_OBJECT
public:
    explicit HarmonyDeviceManager(QObject *parent = nullptr);
    ~HarmonyDeviceManager() override;
    /**
     * @brief queryDevice
     * 查询设备
     */
    void queryDevice() const;
    /**
     * @brief setupDevicesWatcher
     * 设置设备监视器
     */
    void setupDevicesWatcher() const;
private:
    Q_DISABLE_COPY(HarmonyDeviceManager)
};
Q_GLOBAL_STATIC(HarmonyDeviceManager, instance)
} // namespace Ohos::Internal
#endif // HARMONYDEVICEMANAGER_H
