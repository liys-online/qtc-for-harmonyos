#include "harmonyqtversion.h"
#include "harmonyconfigurations.h"
#include "ohosconstants.h"
#include <utils/filepath.h>
#include <utils/environment.h>
#include "ohostr.h"
using namespace Utils;
namespace Ohos::Internal {
HarmonyQtVersion::HarmonyQtVersion() {}

bool HarmonyQtVersion::supportsMultipleQtAbis() const
{
    return qtVersion() == QVersionNumber(5, 15, 12);
}

void HarmonyQtVersion::addToBuildEnvironment(const ProjectExplorer::Kit *k, Utils::Environment &env) const
{
    QtVersion::addToBuildEnvironment(k, env);
    const QStringList sdkList = HarmonyConfig::getSdkList();
    if (!sdkList.isEmpty())
    {
        for(const QString &sdk : sdkList)
        {
            FilePath releaseFile = HarmonyConfig::releaseFile(HarmonyConfig::ndkLocation(FilePath::fromString(sdk)));
            if(HarmonyConfig::getVersion(releaseFile).first == supportOhVersion())
            {
                const FilePath sdkPath = FilePath::fromString(sdk).pathAppended("default/openharmony");
                env.set(Constants::OHOS_SDK_ENV_VAR, sdkPath.toUserOutput());
            }
        }
    }
}

QSet<Id> HarmonyQtVersion::targetDeviceTypes() const
{
    return {Id(Constants::HARMONY_DEVICE_TYPE)};
}

QString HarmonyQtVersion::description() const
{
    return Tr::tr("HarmonyOS");
}

QString HarmonyQtVersion::defaultUnexpandedDisplayName() const
{
    return Tr::tr("Qt %{Qt:Version} for HarmonyOS");
}

QVersionNumber HarmonyQtVersion::supportOhVersion() const
{
    FilePath headerDir = headerPath();
    FilePath qconfigHeader = headerDir.pathAppended(Constants::Q_CONFIG_H);
    if (qconfigHeader.exists())
    {
        QFile qconfigFile(qconfigHeader.toString());
        if (qconfigFile.open(QIODevice::ReadOnly))
        {
            QTextStream in(&qconfigFile);
            while (!in.atEnd())
            {
                QString line = in.readLine();
                if (line.contains(Constants::OHOS_SDK_VERSION))
                {
                    QStringList list = line.split(" ");
                    QString version = list.at(2);
                    return QVersionNumber::fromString(version);
                }
            }
        }
    }
    return QVersionNumber();
}

// Factory

class HarmonyQtVersionFactory : public QtSupport::QtVersionFactory
{
public:
    HarmonyQtVersionFactory()
    {
        setQtVersionCreator([] { return new HarmonyQtVersion; });
        setSupportedType(Constants::HARMONY_QT_TYPE);
        setPriority(90);

        // setRestrictionChecker([](const SetupData &setup) {
        //     return !setup.config.contains("harmony-no-sdk")
        //     && (setup.config.contains("harmony")
        //        || setup.platforms.contains("harmony"));
        // });
    }
};

void setupHarmonyQtVersion()
{
    static HarmonyQtVersionFactory factory;
}

} // Ohos::Internal
