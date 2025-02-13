#ifndef HARMONYDEVICEMANAGER_H
#define HARMONYDEVICEMANAGER_H

#include <QObject>
#include <QGlobalStatic>

#include <solutions/tasking/tasktreerunner.h>

using namespace Tasking;
namespace Ohos::Internal {
class HarmonyDeviceManager : public QObject
{
    Q_OBJECT
public:
    explicit HarmonyDeviceManager(QObject *parent = nullptr);
    ~HarmonyDeviceManager();
    /**
     * @brief queryDevice
     * 查询设备
     */
    void queryDevice();
    /**
     * @brief setupDevicesWatcher
     * 设置设备监视器
     */
    void setupDevicesWatcher();
private:
    Q_DISABLE_COPY(HarmonyDeviceManager)
};
Q_GLOBAL_STATIC(HarmonyDeviceManager, instance)
} // namespace Ohos::Internal
#endif // HARMONYDEVICEMANAGER_H
