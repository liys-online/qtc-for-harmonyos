#ifndef OHPROJECTECREATOR_H
#define OHPROJECTECREATOR_H

#include <QObject>
#include <QVariant>

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
    static void destroy();
Q_SIGNALS:
    void signalCreateFinished(bool success, const QString &msg);
private:
    explicit OhProjecteCreator(QObject *parent = nullptr);
    ~OhProjecteCreator();
    OhProjecteCreatorPrivate * const m_p = nullptr;
};

#endif // OHPROJECTECREATOR_H
