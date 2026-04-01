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
         * @brief syncToolchainsQtAndKits
         * 刷新自动检测工具链、按 qmake 列表注册/清理 Harmony Qt、更新自动 Kit、移除无效工具链。
         * \c applyConfig() 在保存与设备更新后调用本函数；\c kitsLoaded 在无完整 apply 时也走此路径。
         */
        static void syncToolchainsQtAndKits();
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
        /**
         * Write HarmonyConfig to application settings. Safe to call after mutating paths/lists;
         * no-op if HarmonyConfigurations has not been constructed yet (e.g. during load()).
         */
        static void persistSettings();
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
        void save() const;
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
    QStringList apiLevelNamesFor(const QList<int> &apiLevels);
        QString apiLevelNameFor(const int apiLevel);
        /**
         * @brief makeLocation
         * @return
         * Windows：MinGW 安装根目录（其下 \c bin/mingw32-make.exe）。非 Windows 下为空，构建时使用
         * 当前 Kit/环境的 \c PATH 中的 \c make（与 Qt Creator GCC 工具链行为一致）。
         */
        Utils::FilePath makeLocation();
        /**
         * @brief setMakeLocation
         * @param makeLocation
         * 设置 make 路径（仅 Windows MinGW 根目录有意义）
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
         * @brief ohosSdkRoot
         * HarmonyOS SDK root for SDK Manager: `.temp/` for downloads, `<api_version>/` for unpack.
         */
        Utils::FilePath ohosSdkRoot();
        void setOhosSdkRoot(const Utils::FilePath &path);
        /**
         * Primary catalog URL: non-empty \c QT_OH_BINARY_CATALOG_URL overrides the built-in
         * GitCode default (see \c Constants::QtOhBinaryCatalogDefaultGitcodeUrl).
         */
        QString effectiveQtOhBinaryCatalogUrl();
        /** Default SDK root if none stored (env, then same pattern as Android: Library/... on macOS, etc.). */
        Utils::FilePath defaultOhosSdkPath();
        /** Stored path, or default when empty — for SDK Manager without opening settings first. */
        Utils::FilePath effectiveOhosSdkRoot();
        /**
         * Scan @p sdkRoot: register each immediate subdirectory (skips `.temp` and hidden names)
         * that passes isValidSdk(); if @p sdkRoot itself is a valid SDK, register it too.
         * @return Number of paths newly appended to the SDK list.
         */
        int registerDownloadedSdksUnder(const Utils::FilePath &sdkRoot);
        /**
         * @brief isValidSdk
         * @param sdkLocation
         * @return
         * 是否是有效的SDK
         */
        bool isValidSdk(const QString &sdkLocation);
        /** \overload 使用 FilePath，避免从磁盘枚举到的路径经 QString 往返时解析不一致。 */
        bool isValidSdk(const Utils::FilePath &sdkLocation);
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
         * 获取 DevEco Studio 路径。在 macOS 上为 \c *.app 包路径（例如 \c /Applications/DevEco-Studio.app），
         * 不再要求选到 \c Contents；访问 tools/jbr/sdk 时在实现内解析到 \c Contents。
         */
        Utils::FilePath devecoStudioLocation();
        Utils::FilePath devecoStudioLocation(const Utils::FilePath &root);
        /**
         * @brief setDevecoStudioLocation
         * @param devecoStudioLocation
         * 设置Deveco Studio路径
         */
        void setDevecoStudioLocation(const Utils::FilePath &devecoStudioLocation);
        /** Valid if \c Contents/tools exists or IDE launcher under \c bin/ or \c MacOS/ (accepts macOS \c *.app or \c Contents). */
        bool isValidDevecoStudioRoot(const Utils::FilePath &root);
        /**
         * If stored Deveco path is empty: try \c DEVECO_STUDIO_HOME, then on macOS scan
         * \c /Applications etc. for \c *DevEco*.app (stores the \c .app path, not \c Contents).
         * @return true if a path was detected and stored (caller may persist settings).
         */
        bool tryAutoDetectDevecoStudio();

        Utils::FilePath devecostudioExeLocation();
        Utils::FilePath devecostudioExeLocation(const Utils::FilePath &devecoLocation);
        QPair<int, QVersionNumber> devecoStudioVersion();
        Utils::FilePath devecoToolsLocation();
        Utils::FilePath devecoSdkLocation();
        Utils::FilePath hvigorwJsLocation();
        Utils::FilePath nodeLocation();
        Utils::FilePath ohpmJsLocation();
        Utils::FilePath javaLocation();

        /** Default registry passed to \c ohpm install \c --registry when user override is empty. */
        QString defaultOhpmRegistryUrl();
        /** User override (may be empty). */
        QString ohpmRegistryUrl();
        /** Non-empty trimmed user URL, else \c defaultOhpmRegistryUrl(). */
        QString effectiveOhpmRegistryUrl();
        void setOhpmRegistryUrl(const QString &url);
        bool ohpmStrictSsl();
        void setOhpmStrictSsl(bool on);

        /**
         * \c module.json5 \c deviceTypes for \c OhProjecteCreator: user list from settings; if empty, \c {"2in1"}.
         */
        QStringList ohModuleDeviceTypes();
        /** Raw stored list (may be empty); for preferences UI. */
        QStringList ohModuleDeviceTypesUserList();
        void setOhModuleDeviceTypes(const QStringList &types);

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
        bool isValidReleaseFile(const Utils::FilePath &ndkLocation);
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
        QStringList getAbis(const QString &device);
    }
    void setupHarmonyConfigurations();
} // namespace OhosConfig
#endif // HARMONYCONFIGURATIONS_H
