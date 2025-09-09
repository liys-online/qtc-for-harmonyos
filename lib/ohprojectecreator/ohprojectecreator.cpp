#include "ohprojectecreator.h"
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

/**
 * @brief setJsonValue 递归设置 JSON 对象中的值，支持嵌套对象和数组
 * @param obj 要修改的 JSON 对象
 * @param path 字段路径列表，如 ["app", "products[0]", "targetSdkVersion"]
 * @param value 要设置的值
 * @param index 当前处理的路径索引，初始调用时为 0
 * @return
 */
bool setJsonValue(QJsonObject &obj, const QStringList &path, const QJsonValue &value, int index = 0)
{
    if (index >= path.size())
        return false;

    QString key = path[index];

    /*
     * 数组字段匹配正则，如 products[0] 或 products[]
     */
    QRegularExpression arrayRegex(R"(^(.*)\[(\d*)\]$)");
    QRegularExpressionMatch match = arrayRegex.match(key);

    if (match.hasMatch()) {

        /*
         * 数组字段，可能是具体索引如 products[0]，也可能是追加如 products[]
         */
        QString arrKey = match.captured(1);
        QString idxStr = match.captured(2);

        QJsonArray arr = obj[arrKey].toArray();

        if (index == path.size() - 1) {
            /*
             * 最后一个字段，直接设置值
             */
            if (idxStr.isEmpty()) {
                /*
                 * [] → 追加
                 */
                arr.append(value);
            } else {
                int idx = idxStr.toInt();
                if (idx < arr.size()) {
                    arr[idx] = value;
                } else {
                    /*
                     * 越界时扩展数组
                     */
                    while (arr.size() < idx) arr.append(QJsonValue());
                    arr.append(value);
                }
            }
            obj[arrKey] = arr;
            return true;
        } else {
            /*
             * 递归进入数组元素
             */
            int idx = idxStr.toInt();
            if (idx >= arr.size()) {
                while (arr.size() <= idx) arr.append(QJsonObject());
            }
            QJsonValue elem = arr[idx];
            if (!elem.isObject()) elem = QJsonObject();

            QJsonObject childObj = elem.toObject();
            bool ok = setJsonValue(childObj, path, value, index + 1);
            arr[idx] = childObj;
            obj[arrKey] = arr;
            return ok;
        }
    }

    /*
     * 普通Object字段
     */
    if (index == path.size() - 1) {
        obj[key] = value;
        return true;
    } else {
        QJsonObject childObj = obj[key].toObject();
        bool ok = setJsonValue(childObj, path, value, index + 1);
        obj[key] = childObj;
        return ok;
    }
}
/**
 * @brief modifyJsonFile 修改 JSON 文件中的指定字段，支持嵌套对象和数组
 * @param filePath JSON 文件路径
 * @param changes 要修改的字段和值的映射，键为字段路径，如 "app.products[0].targetSdkVersion"
 * @return
 */
bool modifyJsonFile(const QString &filePath, const QMap<QString, QJsonValue> &changes)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return false;
    }

    QJsonObject rootObj = doc.object();

    for (auto it = changes.constBegin(); it != changes.constEnd(); ++it) {
        QStringList path = it.key().split('.');
        setJsonValue(rootObj, path, it.value());
    }

    QFileInfo fi(filePath);
    QDir dir = fi.dir();
    if (!dir.exists() && !dir.mkpath(".")) {
        return false;
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    file.write(QJsonDocument(rootObj).toJson(QJsonDocument::Indented));
    file.close();

    return true;
}

bool copyDir(const QString &srcPath, const QString &dstPath, bool overwrite = false)
{
    QDir srcDir(srcPath);
    if (!srcDir.exists()) {
        return false;
    }

    QDir dstDir(dstPath);
    if (!dstDir.exists()) {
        if (!dstDir.mkpath(".")) {
            return false;
        }
    }

    QFileInfoList entries = srcDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const QFileInfo &entry : std::as_const(entries)) {
        QString srcFilePath = entry.filePath();
        QString dstFilePath = dstDir.filePath(entry.fileName());

        if (entry.isDir()) {
            if (!copyDir(srcFilePath, dstFilePath, overwrite))
                return false;
        } else {
            if (QFile::exists(dstFilePath) && overwrite) {
                QFile::remove(dstFilePath);
            }
            if (!QFile::copy(srcFilePath, dstFilePath)) {
                return false;
            }
        }
    }
    return true;
}

class OhProjecteCreatorPrivate
{
public:
    OhProjecteCreatorPrivate() {}
    ~OhProjecteCreatorPrivate() {}
    bool initProjectDir()
    {
        if (m_proInfo.projectPath.isEmpty()) {
            return false;
        }

        QFile projectTreeInfo(":/ohprojectecreator/project_tree.json");
        if (!projectTreeInfo.open(QIODevice::ReadOnly)) {
            return false;
        }
        QByteArray data = projectTreeInfo.readAll();
        projectTreeInfo.close();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) {
            return false;
        }

        std::function<void(const QJsonObject &, const QString &)> createFromJson;
        createFromJson = [&createFromJson](const auto &obj, const auto &path) {
            QDir dir;
            if (!dir.exists(path)) {
                dir.mkpath(path);
            }
            if (obj.contains("dir")) {
                QJsonObject subDirs = obj["dir"].toObject();
                for (auto it = subDirs.begin(); it != subDirs.end(); ++it) {
                    QString subPath = path + "/" + it.key();
                    createFromJson(it.value().toObject(), subPath);
                }
            }
        };
        createFromJson(doc.object(), m_proInfo.projectPath);
        return true;
    }
    bool initProjectFile() const
    {
        std::function<bool(const QString &, const QString &)> createFromResource;

        createFromResource = [this, &createFromResource](const QString &rootPath, const QString &curPath) -> bool {
            QDir dir(curPath);
            QFileInfoList list = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                                                   QDir::DirsFirst | QDir::Name);
            bool res = true;
            for (const QFileInfo &fi : std::as_const(list)) {
                if (fi.isDir()) {
                    /*
                     * 递归处理子目录
                     */
                    res = createFromResource(rootPath, fi.filePath());
                } else {
                    /*
                     * 计算相对路径：始终基于 rootPath
                     */
                    QString relativePath = QDir(rootPath).relativeFilePath(fi.filePath());
                    QString targetPath = m_proInfo.projectPath + "/" + relativePath;
                    QDir().mkpath(QFileInfo(targetPath).absolutePath());
                    if(QFile::exists(targetPath)) {
                        return true;
                    }
                    res = QFile::copy(fi.filePath(), targetPath);
                    /*
                     * 确保文件可写
                     */
                    QFile::setPermissions(targetPath, QFile::WriteOwner | QFile::ReadOwner);
                }
                if (!res)
                    break;
            }
            return res;
        };
        /*
         * 从资源目录复制文件到目标目录
         */
        return createFromResource(":/ohprojectecreator/ohpro",":/ohprojectecreator/ohpro");
    }
    bool createBuildProfile()
    {
        /*
         * 用QJsonDocument创建build-profile.json5文件
         */
        QJsonDocument doc;
        QJsonObject rootObj;
        QJsonObject app;
        app.insert("signingConfigs", QJsonArray());
        QJsonArray products;

        QJsonObject defaultProduct;
        defaultProduct["name"] = "default";
        defaultProduct["signingConfig"] = "default";
        if (m_proInfo.runtimeOS == OhProjecteCreator::HarmonyOS) {
            defaultProduct["targetSdkVersion"] = sdkVersionMap.value(m_proInfo.targetSdkVersion, "6.0.0(20)");
            defaultProduct["compatibleSdkVersion"] = sdkVersionMap.value(m_proInfo.compatibleSdkVersion, "6.0.0(20)");
            defaultProduct["runtimeOS"] = "HarmonyOS";
        } else {
            defaultProduct["targetSdkVersion"] = m_proInfo.targetSdkVersion;
            defaultProduct["compatibleSdkVersion"] = m_proInfo.compatibleSdkVersion;
            defaultProduct["runtimeOS"] = "OpenHarmony";
        }
        QJsonObject buildOption;
        QJsonObject strictMode;
        strictMode["caseSensitiveCheck"] = true;
        strictMode["useNormalizedOHMUrl"] = true;
        buildOption["strictMode"] = strictMode;
        defaultProduct["buildOption"] = buildOption;
        products.append(defaultProduct);
        app["products"] = products;


        QJsonArray buildModeSet {QJsonObject::fromVariantMap({{"name", "debug"}}),
                                 QJsonObject::fromVariantMap({{"name", "release"}})};
        app["buildModeSet"] = buildModeSet;
        QJsonArray modules;
        QJsonObject entryModule;
        entryModule["name"] = "entry";
        entryModule["srcPath"] = "./entry";
        QJsonObject target;
        target["name"] = "default";
        target["applyToProducts"] = QJsonArray{"default"};
        entryModule["targets"] = QJsonArray{target};
        modules.append(entryModule);
        rootObj["app"] = app;
        rootObj["modules"] = modules;

        doc.setObject(rootObj);

        if (QFile file(m_proInfo.projectPath + "/build-profile.json5");
            file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(doc.toJson(QJsonDocument::Indented));
            file.close();
            file.setPermissions(QFile::WriteOwner | QFile::ReadOwner);
            return true;
        }
        return false;
    }

    bool updateEntryBuildProfile()
    {
        QMap<QString, QJsonValue> changes;
        QStringList arguments;
        if(!m_proInfo.qtHostPath.isEmpty()){
            arguments.append(QString("-DCMAKE_PREFIX_PATH=%1").arg(m_proInfo.qtHostPath));
        }
        if (!m_proInfo.cmakeListPath.isEmpty()) {
            m_proInfo.externalNativeOptions.insert("path", m_proInfo.cmakeListPath);
        }
        m_proInfo.externalNativeOptions.insert("arguments", arguments.join(" "));
        changes.insert("buildOption.externalNativeOptions", QJsonObject::fromVariantMap(m_proInfo.externalNativeOptions));
        return modifyJsonFile(m_proInfo.projectPath + "/entry/build-profile.json5", changes);
    }

    bool updateEntryModule()
    {
        QMap<QString, QJsonValue> changes;
        changes.insert("module.deviceTypes", QJsonArray::fromStringList(m_proInfo.deviceTypes));
        return modifyJsonFile(m_proInfo.projectPath + "/entry/src/main/module.json5", changes);
    }

    void setAppName(const QString &appName) const
    {
        QVariantMap appInfo;
        appInfo.insert("name", "app_name");
        appInfo.insert("value", appName);
        QMap<QString, QJsonValue> changes;
        changes.insert("string[0]", QJsonObject::fromVariantMap(appInfo));
        modifyJsonFile(m_proInfo.projectPath + "/AppScope/resources/base/element/string.json", changes);
    }
    void setBundleName(const QString &bundleName) const
    {
        QMap<QString, QJsonValue> changes;
        changes.insert("app.bundleName", bundleName);
        modifyJsonFile(m_proInfo.projectPath + "/AppScope/app.json5", changes);
    }

    void createEtsFile() const
    {
        if (m_proInfo.qtHostPath.isEmpty()) {
            return;
        }
        QString srcEtsPath = m_proInfo.qtHostPath + "/openharmony/qtbase";
        QString dstEtsPath = m_proInfo.projectPath + "/entry/src/main/ets";
        copyDir(srcEtsPath, dstEtsPath, true);
    }
    static OhProjecteCreator *m_instance;
    OhProjecteCreator::ProjecteInfo m_proInfo;
    const QMap<int, QString> sdkVersionMap = {
        {20, "6.0.0(20)"},
        {19, "5.1.1(19)"},
        {18, "5.1.0(18)"},
        {17, "5.0.5(17)"},
        {16, "5.0.4(16)"},
        {15, "5.0.3(15)"},
        {14, "5.0.2(14)"},
        {13, "5.0.1(13)"},
        {12, "5.0.0(12)"}
    };
};

OhProjecteCreator *OhProjecteCreatorPrivate::m_instance = nullptr;
OhProjecteCreator *OhProjecteCreator::instance()
{

    OhProjecteCreatorPrivate::m_instance = OhProjecteCreatorPrivate::m_instance ? OhProjecteCreatorPrivate::m_instance
                                                                                : new OhProjecteCreator;
    return OhProjecteCreatorPrivate::m_instance;
}

void OhProjecteCreator::create(const ProjecteInfo &projectPath)
{
    m_p->m_proInfo = projectPath;
    if (!m_p->initProjectDir()) {
        emit signalCreateFinished(false, "Failed to create project directory structure.");
        return;
    }
    if (!m_p->initProjectFile()) {
        emit signalCreateFinished(false, "Failed to initialize project files.");
        return;
    }
    if (!m_p->createBuildProfile()) {
        emit signalCreateFinished(false, "Failed to create build-profile.json5.");
        return;
    }
    if (!m_p->updateEntryBuildProfile())
    {
        emit signalCreateFinished(false, "Failed to update entry/build-profile.json5.");
        return;
    }
    if (!m_p->updateEntryModule())
    {
        emit signalCreateFinished(false, "Failed to update entry/src/main/module.json5.");
        return;
    }
    m_p->setAppName(projectPath.appName);
    m_p->setBundleName(projectPath.bundleName);
    m_p->createEtsFile();
    switch (projectPath.runtimeOS) {
    case OhProjecteCreator::HarmonyOS:
        emit signalCreateFinished(true, "Hamony Project created successfully.");
        break;
    case OpenHarmony:
        emit signalCreateFinished(true, "OpenHarmony Project created successfully.");
        break;
    }
}
void OhProjecteCreator::destroy()
{
    if (OhProjecteCreatorPrivate::m_instance) {
        delete OhProjecteCreatorPrivate::m_instance;
        OhProjecteCreatorPrivate::m_instance = nullptr;
    }
}

OhProjecteCreator::OhProjecteCreator(QObject *parent)
    : QObject(parent),
      m_p(new OhProjecteCreatorPrivate)
{}

OhProjecteCreator::~OhProjecteCreator()
{
    qDebug() << "OhProjecteCreator destructor called.";
    delete m_p;
}

OhProjecteCreator::ProjecteInfo::ProjecteInfo() {
    externalNativeOptions.insert("path",  "");
    externalNativeOptions.insert("arguments", "");
    externalNativeOptions.insert("cppFlags", "");
}
