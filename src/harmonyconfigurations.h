#ifndef HARMONYCONFIGURATIONS_H
#define HARMONYCONFIGURATIONS_H

#include <QObject>
#include <utils/filepath.h>
#include <projectexplorer/toolchain.h>
namespace Ohos::Internal {
    class HarmonyConfigurations : public QObject
    {
        Q_OBJECT
    public:
        static HarmonyConfigurations *instance();
        /**
         * @brief applyConfig
         * 应用配置
         */
        static void applyConfig();
        /**
         * @brief registerNewToolchains
         * 注册新的工具链
         */
        static void registerNewToolchains();
        /**
         * @brief registerQtVersions
         * 注册Qt版本
         */
        static void registerQtVersions();
        /**
         * @brief removeOldToolchains
         * 移除旧的工具链
         */
        static void removeOldToolchains();
        /**
         * @brief updateAutomaticKitList
         * 更新自动Kit列表
         */
        static void updateAutomaticKitList();
    private:
        friend void setupHarmonyConfigurations();
        explicit HarmonyConfigurations(QObject *parent = nullptr);

        /**
         * @brief load
         * 加载配置
         */
        void load();
        /**
         * @brief save
         * 保存配置
         */
        void save();
        /**
         * @brief updateHarmonyDevice
         * 更新Harmony设备
         */
        static void updateHarmonyDevice();
    signals:
        void aboutToUpdate();
        void updated();
    };

    namespace HarmonyConfig {
        /**
         * @brief makeLocation
         * @return
         * 获取make路径
         */
        Utils::FilePath makeLocation();
        /**
         * @brief setMakeLocation
         * @param makeLocation
         * 设置make路径
         */
        void setMakeLocation(const Utils::FilePath &makeLocation);
        /**
         * @brief defaultSdk
         * @return
         * 获取默认的SDK
         */
        Utils::FilePath defaultSdk();
        /**
         * @brief setdefaultSdk
         * @param sdkLocation
         * 设置默认的SDK
         */
        void setdefaultSdk(const Utils::FilePath &sdkLocation);
        /**
         * @brief isValidSdk
         * @param sdkLocation
         * @return
         * 是否是有效的SDK
         */
        bool isValidSdk(const QString &sdkLocation);
        /**
         * @brief sdkLocations
         * @return
         * SDK路径列表
         */
        QStringList &getSdkList();
        /**
         * @brief addSdk
         * @param sdk
         * 添加SDK
         */
        void addSdk(const QString &sdk);
        /**
         * @brief removeSdk
         * @param sdk
         * 移除SDK
         */
        void removeSdkList(const QString &sdk);
        /**
         * @brief ndkLocations
         * @return
         * NDK路径列表
         */
        QList<Utils::FilePath> ndkLocations();
        /**
         * @brief devecoStudioLocation
         * @return
         * 获取Deveco Studio路径
         */
        Utils::FilePath devecoStudioLocation();
        /**
         * @brief setDevecoStudioLocation
         * @param devecoStudioLocation
         * 设置Deveco Studio路径
         */
        void setDevecoStudioLocation(const Utils::FilePath &devecoStudioLocation);
        /**
         * @brief getQmakeList
         * @return
         * qmake路径列表
         */
        QStringList &getQmakeList();
        /**
         * @brief addQmake
         * @param qmake
         * 添加qmake
         */
        void addQmake(const QString &qmake);
        /**
         * @brief removeQmake
         * @param qmake
         * 移除qmake
         */
        void removeQmake(const QString &qmake);
        /**
         * @brief toolchainPathFromNdk
         * @param ndkLocation
         * @return
         * 从NDK获取工具链路径
         */
        Utils::FilePath toolchainPathFromNdk( const Utils::FilePath &ndkLocation);
        /**
         * @brief clangPathFromNdk
         * @param ndkLocation
         * @return
         * 从NDK获取clang路径
         */
        Utils::FilePath clangPathFromNdk(const Utils::FilePath &ndkLocation);
        Utils::FilePath toolchainFilePath(const Utils::FilePath &ndkLocation);
        /**
         * @brief displayName
         * @param abi
         * @return
         * 显示名称
         */
        QLatin1String displayName(const ProjectExplorer::Abi &abi);
        /**
         * @brief abi
         * @param arch
         * @return
         * 获取ABI
         */
        ProjectExplorer::Abi abi(const QLatin1String &arch);
        /**
         * @brief ndkLocation
         * @param sdkLocation
         * @return
         * NDK路径
         */
        Utils::FilePath ndkLocation(const Utils::FilePath &sdkLocation);
        /**
         * @brief releaseFile
         * @param ndkLocation
         * @return
         * 发布文件
         */
        Utils::FilePath releaseFile(const Utils::FilePath &ndkLocation);
        /**
         * @brief getVersion
         * @param releaseFile
         * @return
         * 获取版本
         */
        QPair<QVersionNumber, QVersionNumber> getVersion(const Utils::FilePath &releaseFile);
        /**
         * @brief hdcToolPath
         * @return
         * HDC工具路径
         */
        Utils::FilePath hdcToolPath();
        /**
         * @brief getSDKVersion
         * @param device
         * @return
         * 获取设备SDK版本
         */
        int getSDKVersion(const QString &device);
        /**
         * @brief getProductModel
         * @param device
         * @return
         * 获取产品型号
         */
        QString getProductModel(const QString &device);
        /**
         * @brief getDeviceName
         * @param device
         * @return
         * 获取设备名称
         */
        QString getDeviceName(const QString &device);
        /**
         * @brief getAbis
         * @param device
         * @return
         * 获取ABI
         */
        QString getAbis(const QString &device);
    }
    void setupHarmonyConfigurations();
} // namespace OhosConfig
#endif // HARMONYCONFIGURATIONS_H
