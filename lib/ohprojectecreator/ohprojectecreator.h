#ifndef OHPROJECTECREATOR_H
#define OHPROJECTECREATOR_H

#include <QObject>
#include <QVariant>
#include <QSharedPointer>

class OhProjecteCreatorPrivate;
class OhProjecteCreator : public QObject
{
    Q_OBJECT
public:
    enum RuntimeOS
    {
        HarmonyOS,
        OpenHarmony,
    };

    struct ProjecteInfo
    {
        ProjecteInfo();
        QString projectPath = "";
        QString appName = "MyApplication";
        QString bundleName = "com.example.myapplication";
        QString entrylib = "libentry.so";
        int targetSdkVersion = 20;
        int compatibleSdkVersion = -1;
        RuntimeOS runtimeOS = HarmonyOS;
        QVariantMap externalNativeOptions = {};
        QStringList deviceTypes = {};
        QString qtHostPath = "";
        QString cmakeListPath = "";
    };

    static OhProjecteCreator *instance();
    void create(const ProjecteInfo &projectPath);
    static QString versionForApiLevel(int apiLevel);
    static int apiLevelForVersion(const QString &version);
    static int latestApiLevel();
    static int defaultApiLevel();
    static QList<int> availableApiLevels();
    /**
     * @brief 若 ohproPath/build-profile.json5 存在，就地更新
     *        app.products[0].targetSdkVersion / compatibleSdkVersion 字段。
     * @return 文件存在且修改成功时返回 true，文件不存在时返回 false（静默）。
     */
    static bool updateBuildProfileSdkVersions(const QString &ohproPath,
                                              int targetSdkVersion,
                                              int compatibleSdkVersion);
    /**
     * @brief Patches the launchApplication value in an already-created EntryAbility.ets.
     * @param ohproPath  Path to the ohpro root directory (contains entry/src/main/ets/...).
     * @param libName    New shared library name, e.g. "libmyapp.so".
     * @return true on success; false if the file does not exist or cannot be written.
     */
    static bool patchEntryAbilityLib(const QString &ohproPath, const QString &libName);

    static bool updateModuleDeviceTypes(const QString &ohproPath, const QStringList &deviceTypes);
Q_SIGNALS:
    void signalCreateFinished(bool success, const QString &msg);
private:
    explicit OhProjecteCreator(QObject *parent = nullptr);
    ~OhProjecteCreator();
    QSharedPointer<OhProjecteCreatorPrivate> m_p = nullptr;
};

#endif // OHPROJECTECREATOR_H
