// Copyright (C) 2026 Li-Yaosong
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QObject>
#include <QString>
#include <QtTaskTree/qtasktree.h>

namespace ProjectExplorer {
class RunControl;
}

namespace Ohos::Internal {

/* ** 通过 hdc 守护进程 TCP 流式传输 Harmony 运行的设备 Shell 脚本（P2-15 阶段三）。 */
class HarmonyMainRunSocketTask final : public QObject
{
    Q_OBJECT
public:
    void setContext(ProjectExplorer::RunControl *runControl,
                    QString serial,
                    QString deviceShellScript);

    void start();
    void stopShell();

Q_SIGNALS:
    void sessionStarted();
    void done(QtTaskTree::DoneResult result);

private:
    void onClientFinished();
    void finalizeDone(QtTaskTree::DoneResult result);

    ProjectExplorer::RunControl *m_runControl = nullptr;
    QString m_serial;
    QString m_script;
    class HdcSocketClient *m_client = nullptr;
    bool m_sessionReady = false;
    bool m_emittedDone = false;
};

} // namespace Ohos::Internal
