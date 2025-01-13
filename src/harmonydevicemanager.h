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
    ~HarmonyDeviceManager() = default;

    void setupDevicesWatcher();
private:
    TaskTreeRunner m_hdcDeviceWatcherRunner;
    Q_DISABLE_COPY(HarmonyDeviceManager)
};
Q_GLOBAL_STATIC(HarmonyDeviceManager, instance)
} // namespace Ohos::Internal
#endif // HARMONYDEVICEMANAGER_H
