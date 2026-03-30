// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "arktsdebugbridge.h"

#include <QCoreApplication>

#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("arktsdebugbridge"));

    /*
    ** 用法：arktsdebugbridge <connectPort> <pandaPort> <signalFile>
    **
    ** connectPort ：本地 TCP 端口，已由 hdc fport 转发至 ark:<pid>@<bundle>
    ** pandaPort   ：本地 TCP 端口，已由 hdc fport 转发至 ark:<pid>@Debugger
    ** signalFile  ：LLDB 通过 'script' 命令创建的信号文件路径，
    **              用于通知 bridge 发送 {"type":"connected"}
    */
    if (argc != 4) {
        std::cerr << "Usage: arktsdebugbridge <connectPort> <pandaPort> <signalFile>\n";
        return 1;
    }

    const quint16 connectPort = QByteArray(argv[1]).toUShort();
    const quint16 pandaPort   = QByteArray(argv[2]).toUShort();
    const QString signalFile  = QString::fromLocal8Bit(argv[3]);

    ArkTSDebugBridge bridge;
    QObject::connect(&bridge, &ArkTSDebugBridge::logMessage, [](const QString &m) {
        std::cout << m.toStdString() << "\n";
        std::cout.flush();
    });
    QObject::connect(&bridge, &ArkTSDebugBridge::errorOccurred, [](const QString &m) {
        std::cerr << m.toStdString() << "\n";
        std::cerr.flush();
    });
    QObject::connect(&bridge, &ArkTSDebugBridge::finished, qApp, &QCoreApplication::quit);
    bridge.start(connectPort, pandaPort, signalFile);
    return app.exec();
}
