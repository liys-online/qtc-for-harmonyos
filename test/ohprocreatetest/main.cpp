#include <QtCore/QCoreApplication>
#include <QtCore/qglobal.h>
#include <ohprojectecreator/ohprojectecreator.h>
#include <QFile>
#include <QDir>
#include <QTimer>
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    auto *ohPro = OhProjecteCreator::instance();
    QObject::connect(ohPro, &OhProjecteCreator::signalCreateFinished, [](bool success, const QString &msg){
        qDebug() << success << msg;
        OhProjecteCreator::destroy();
        QCoreApplication::quit();
    });
    OhProjecteCreator::ProjecteInfo proInfo;
    proInfo.projectPath = QCoreApplication::applicationDirPath()+"/testProject";
    proInfo.targetSdkVersion = 20;
    proInfo.compatibleSdkVersion = 15;
    proInfo.qtHostPath = "D:/Qt/5.15.12/ohos15_arm64-v8a";
    proInfo.cmakeListPath = "./CMakeLists.txt";
    proInfo.deviceTypes = {"2in1"};
    QDir dir(proInfo.projectPath);
    if(dir.exists()){
        dir.removeRecursively();
    }
    QTimer::singleShot(0, ohPro, [ohPro, proInfo] {
        ohPro->create(proInfo);
    });
    return a.exec();
}
