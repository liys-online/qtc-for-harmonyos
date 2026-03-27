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

/// Streams the Harmony run's device shell script over hdc daemon TCP (P2-15 phase 3).
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
